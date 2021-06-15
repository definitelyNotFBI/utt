// Concord
//
// Copyright (c) 2018-2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <cstdint>
#include "test_comm_config.hpp"
#include "test_parameters.hpp"
#include "state.hpp"
#include "communication/CommFactory.hpp"
#include "communication/CommDefs.hpp"
#include "SimpleClient.hpp"
#include "histogram.hpp"
#include "misc.hpp"
#include "assertUtils.hpp"
#include "msg_receiver.h"

using namespace bftEngine;
using namespace bft::communication;
using namespace std;

#define test_assert(statement, message)                                                                          \
  {                                                                                                              \
    if (!(statement)) {                                                                                          \
      LOG_FATAL(clientLogger, "assert fail with message: " << message); /* NOLINT(bugprone-macro-parentheses) */ \
      ConcordAssert(false);                                                                                      \
    }                                                                                                            \
  }

class SimpleTestClient {
 private:
  ClientParams cp;
  logging::Logger clientLogger;

 public:
  SimpleTestClient(ClientParams& clientParams, logging::Logger& logger) : cp{clientParams}, clientLogger{logger} {}

  bool run() {
    // This client's index number. Must be larger than the largest replica index
    // number.
    const uint16_t id = cp.clientId;

    // Concord clients must tag each request with a unique sequence number. This
    // generator handles that for us.
    std::unique_ptr<SeqNumberGeneratorForClientRequests> pSeqGen =
        SeqNumberGeneratorForClientRequests::createSeqNumberGeneratorForClientRequests();

    TestCommConfig testCommConfig(clientLogger);
    // Configure, create, and start the Concord client to use.
#ifdef USE_COMM_PLAIN_TCP
    PlainTcpConfig conf = testCommConfig.GetTCPConfig(false, id, cp.numOfClients, cp.numOfReplicas, cp.configFileName);
#elif USE_COMM_TLS_TCP
    TlsTcpConfig conf = testCommConfig.GetTlsTCPConfig(false, id, cp.numOfClients, cp.numOfReplicas, cp.configFileName);
#else
    PlainUdpConfig conf = testCommConfig.GetUDPConfig(false, id, cp.numOfClients, cp.numOfReplicas, cp.configFileName);
#endif

    LOG_INFO(clientLogger,
             "ClientParams: clientId: " << cp.clientId << ", numOfReplicas: " << cp.numOfReplicas << ", numOfClients: "
                                        << cp.numOfClients << ", numOfIterations: " << cp.numOfOperations
                                        << ", fVal: " << cp.numOfFaulty << ", cVal: " << cp.numOfSlow);

    ICommunication* comm = bft::communication::CommFactory::create(conf);

    SimpleClient* client = SimpleClient::createSimpleClient(comm, id, cp.numOfFaulty, cp.numOfSlow);
    auto aggregator = std::make_shared<concordMetrics::Aggregator>();
    client->setAggregator(aggregator);

    bft::client::MsgReceiver receiver;
    receiver.activate(10000000);
    // Breaking change
    // comm->setReceiver(id, &receiver);
    // End breaking change
    // Comment the above for a safe version

    comm->Start();

    // hack that copies the behaviour of the protected function of SimpleClient
    while (true) {
      auto readyReplicas = 0;
      this_thread::sleep_for(1s);
      for (int i = 0; i < cp.numOfReplicas; ++i)
        readyReplicas += (comm->getCurrentConnectionStatus(i) == ConnectionStatus::Connected);
      if (readyReplicas >= cp.numOfReplicas - cp.numOfFaulty) break;
    }

    concordUtils::Histogram hist;
    hist.Clear();

    LOG_INFO(clientLogger, "Starting " << cp.numOfOperations);

    // Perform this check once all parameters configured.
    if (3 * cp.numOfFaulty + 2 * cp.numOfSlow + 1 != cp.numOfReplicas) {
      LOG_FATAL(clientLogger,
                "Number of replicas is not equal to 3f + 2c + 1 :"
                " f="
                    << cp.numOfFaulty << ", c=" << cp.numOfSlow << ", numOfReplicas=" << cp.numOfReplicas);
      exit(-1);
    }

    for (uint32_t i = 1; i <= cp.numOfOperations; i++) {
      // the python script that runs the client needs to know how many
      // iterations has been done - that's the reason we use printf and not
      // logging module - to keep the output exactly as we expect.
      if (i > 0 && i % 100 == 0) {
        printf("Iterations count: 100\n");
        printf("Total iterations count: %i\n", i);
      }

      uint64_t start = get_monotonic_time();
      // Read the latest value every readMod-th operation.

      // Prepare request parameters.
      const uint32_t kRequestLength = 1;
      const OpType requestBuffer[kRequestLength] = {OpType::Mint};
      const char* rawRequestBuffer = reinterpret_cast<const char*>(requestBuffer);
      const uint32_t rawRequestLength = sizeof(OpType) * kRequestLength;

      const uint64_t requestSequenceNumber = pSeqGen->generateUniqueSequenceNumberForRequest();

      const uint64_t timeout = SimpleClient::INFINITE_TIMEOUT;

      const uint32_t kReplyBufferLength = 1;
      OpType replyBuffer[kReplyBufferLength];
      uint32_t lengthOfReplyBuffer = sizeof(OpType) * kReplyBufferLength;
      uint32_t actualReplyLength = 0;

      client->sendRequest(EMPTY_FLAGS_REQ,
                          rawRequestBuffer,
                          rawRequestLength,
                          requestSequenceNumber,
                          timeout,
                          lengthOfReplyBuffer,
                          reinterpret_cast<char*>(replyBuffer),
                          actualReplyLength);
                          
      // printf("Successfully sent data over to the replicas\n");

      // Mint should respond with sizeof(OpType) of data.
      test_assert(actualReplyLength == lengthOfReplyBuffer, "actualReplyLength != " << actualReplyLength);
      test_assert(replyBuffer[0] == OpType::MintAck, "Mint was not acknowledged");

      uint64_t end = get_monotonic_time();
      uint64_t elapsedMicro = end - start;

      if (cp.measurePerformance) {
        hist.Add(elapsedMicro);
        LOG_INFO(clientLogger, "RAWLatencyMicro " << elapsedMicro << " Time " << (uint64_t)(end / 1e3));
      }
    }

    // After all requests have been issued, stop communication and clean up.
    comm->Stop();
    std::string metric_comp_name = "clientMetrics_" + std::to_string(id);
    LOG_INFO(clientLogger,
             "clientMetrics::retransmissions " << aggregator->GetCounter(metric_comp_name, "retransmissions").Get());
    LOG_INFO(
        clientLogger,
        "clientMetrics::retransmissionTimer " << aggregator->GetGauge(metric_comp_name, "retransmissionTimer").Get());
    test_assert(aggregator->GetCounter(metric_comp_name, "retransmissions").Get() >= 0, "retransmissions <" << 0);
    test_assert(aggregator->GetGauge(metric_comp_name, "retransmissionTimer").Get() >= 0, "retransmissionTimer <" << 0);
    delete client;
    delete comm;

    if (cp.measurePerformance) {
      LOG_INFO(clientLogger,
               std::endl
                   << "Performance info from client " << cp.clientId << std::endl
                   << hist.ToString());
    }

    LOG_INFO(clientLogger, "test done, iterations: " << cp.numOfOperations);
    return true;
  }

};
