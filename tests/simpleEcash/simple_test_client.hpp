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

#include <log4cplus/loglevel.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "test_comm_config.hpp"
#include "test_parameters.hpp"
#include "bftclient/quorums.h"
#include "state.hpp"
#include "communication/CommFactory.hpp"
#include "communication/CommDefs.hpp"
#include "SimpleClient.hpp"
#include "histogram.hpp"
#include "misc.hpp"
#include "assertUtils.hpp"
#include "msg_receiver.h"
#include "ecash_client.hpp"
#include "adapter_config.hpp"
#include "bftclient/bft_client.h"

#include "utt/Bank.h"

using namespace bftEngine;
using namespace bft::communication;
using namespace std;

#define PRE_EXEC_ENABLED true

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
  size_t ctr = 1;

 public:
  SimpleTestClient(ClientParams& clientParams, logging::Logger& logger) : cp{clientParams}, clientLogger{logger} {}

  bool run() {
    clientLogger.setLogLevel(log4cplus::ALL_LOG_LEVEL);
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

    bft::client::ClientConfig adapter_config = make_adapter_config(cp);
    // Load the params file (move it out)
    auto p = new libutt::Params;
    std::ifstream ifile("utt_pub_client.dat");
    ConcordAssert(!ifile.fail());

    ifile >> *p;
    std::vector<libutt::BankSharePK> bank_pks;
    std::cout<< "Params: " << *p << std::endl;
    size_t n =cp.numOfReplicas;
    for(size_t i=0; i<n;i++) {
      libutt::BankSharePK bspk;
      ifile >> bspk;
      bank_pks.push_back(bspk);
    }
    assertEqual(bank_pks.size(), cp.numOfReplicas);

    ifile.close();

    EcashClient client(comm, adapter_config, p, std::move(bank_pks));

    // MsgReceiver receiver;
    // receiver.activate(10000000);
    // // Breaking change
    // // comm->setReceiver(id, &receiver);
    // // End breaking change
    // // Comment the above for a safe version

    client.wait_for_connections();
    printf("Connected to all the replicas\n");

    concordUtils::Histogram hist;
    hist.Clear();

    LOG_INFO(clientLogger, "Starting " << cp.numOfOperations);

    // // Perform this check once all parameters configured.
    if (3 * cp.numOfFaulty + 2 * cp.numOfSlow + 1 != cp.numOfReplicas) {
      LOG_FATAL(clientLogger,
                "Number of replicas is not equal to 3f + 2c + 1 :"
                " f="
                    << cp.numOfFaulty << ", c=" << cp.numOfSlow << ", numOfReplicas=" << cp.numOfReplicas);
      exit(-1);
    }

    for (uint32_t i = 1; i <= cp.numOfOperations; i++) {
      bft::client::RequestConfig req_config;
      req_config.timeout = 100s;
      bft::client::WriteConfig write_config{req_config, bft::client::ByzantineSafeQuorum{}};
      write_config.request.pre_execute = PRE_EXEC_ENABLED;

      // the python script that runs the client needs to know how many
      // iterations has been done - that's the reason we use printf and not
      // logging module - to keep the output exactly as we expect.
      if (i > 0 && i % 100 == 0) {
        printf("Iterations count: 100\n");
        printf("Total iterations count: %i\n", i);
      }

      // printf("Starting Tx: %d\n", i);

      // Prepare request parameters.
      libutt::CoinComm cc = client.new_coin();
      bft::client::Msg test_message = UTT_Msg::new_mint_msg(1000, cc, ctr++);

      write_config.request.sequence_number = pSeqGen->generateUniqueSequenceNumberForRequest();

      // const uint64_t timeout = SimpleClient::INFINITE_TIMEOUT;
      uint64_t start = get_monotonic_time();

      auto x = client.send(write_config,std::move(test_message));
      
      uint64_t end = get_monotonic_time();
      uint64_t elapsedMicro = end - start;

      if (cp.measurePerformance) {
        hist.Add(elapsedMicro);
        // LOG_INFO(clientLogger, "RAWLatencyMicro " << elapsedMicro << " Time " << (uint64_t)(end / 1e3));
      }

      // printf("Got Tx: %d\n", i);

      // printf("Got %lu bytes of matched data\n", x.matched_data.size());
      // printf("Got %lu rsi pieces\n", x.rsi.size());
      // printf("Got %lu rsi data\n", x.rsi.begin()->second.size());
      auto status = client.verifyMintAckRSI(x);
      if(!status) {
        printf("Got invalid transactions from the replicas\n");
      }
      // printf("Finishing tx: %d\n", i);
    }

    if (cp.measurePerformance) {
      LOG_INFO(clientLogger,
               std::endl
                   << "Performance info from client " << cp.clientId << std::endl
                   << hist.ToString());
    }

    // // After all requests have been issued, stop communication and clean up.
    client.stop();
    
    // LOG_INFO(clientLogger, "test done, iterations: " << cp.numOfOperations);
    return true;
  }

};
