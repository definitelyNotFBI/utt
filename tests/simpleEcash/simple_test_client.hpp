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

#include "bftclient/base_types.h"
#define UTT_DEBUG

#include <log4cplus/loglevel.h>
#include <xassert/XAssert.h>
#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "bftclient/config.h"
#include "bftclient/exception.h"
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
#include "bftclient/seq_num_generator.h"

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

 public:
  SimpleTestClient(ClientParams& clientParams, logging::Logger& logger) 
    : cp{clientParams}, clientLogger{logger} 
  {}

  bool run() {
    #ifdef UTT_DEBUG
    // Enable all the logs (Use for debugging only)
    for (auto &logger: logging::Logger::getCurrentLoggers()) {
      logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
    }
    clientLogger.setLogLevel(log4cplus::ALL_LOG_LEVEL);
    #endif
    
    // This client's index number. Must be larger than the largest replica index
    // number.
    const uint16_t id = cp.clientId;

    // Concord clients must tag each request with a unique sequence number. This
    // generator handles that for us.
    bft::client::ClientId cid{cp.clientId};
    auto pSeqGen = bft::client::SeqNumberGenerator(cid);

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
             "ClientParams: clientId: " << cp.clientId << 
             ", numOfReplicas: " << cp.numOfReplicas << 
             ", numOfClients: " << cp.numOfClients << 
             ", numOfIterations: " << cp.numOfOperations << 
             ", fVal: " << cp.numOfFaulty << 
             ", cVal: " << cp.numOfSlow
            );

    ICommunication* comm = bft::communication::CommFactory::create(conf);

    bft::client::ClientConfig adapter_config = make_adapter_config(cp);
    // Load the params file (move it out)
    std::ifstream ifile("utt_pub_client.dat");
    ConcordAssert(!ifile.fail());

    libutt::Params p(ifile);
    std::vector<libutt::BankSharePK> bank_pks;
    size_t n =cp.numOfReplicas;
    for(size_t i=0; i<n;i++) {
      bank_pks.emplace_back(ifile);
    }
    libutt::BankPK main_pk(ifile);

    std::cout << "MainPK: " << main_pk << std::endl;

    ifile.close();

    auto genesis_file_id = cp.clientId - cp.numOfReplicas;
    auto genesis_file = "genesis/genesis_" + std::to_string(genesis_file_id);
    std::cout << "Reading genesis file: " << genesis_file << std::endl;

    std::ifstream gen_file(genesis_file);
    ConcordAssert(!gen_file.fail());

    std::vector<std::tuple<libutt::CoinSecrets, libutt::CoinComm, libutt::CoinSig>> my_initial_coins;

    for(auto j=0; j<2;j++) {
      libutt::CoinSecrets cs_temp(gen_file);
      libutt::CoinComm cc_temp(gen_file);
      libutt::CoinSig csign_temp(gen_file);

      my_initial_coins.push_back(std::make_tuple(cs_temp, cc_temp, csign_temp));
    }

    gen_file.close();
    
    // // TODO: Check if the main_pk is indeed the lagrange interpolation of the given pk shares
    //
    // auto ids = std::vector<size_t>(cp.numOfFaulty+1);
    // auto pks = std::vector<libutt::BankSharePK>(cp.numOfFaulty+1);
    // for(auto i=0; i< cp.numOfFaulty+1; i++) {
    //   ids.push_back(i);
    //   pks.push_back(bank_pks[i]);
    // }

    // auto bpk2 = libutt::BankThresholdKeygen::aggregate(cp.numOfReplicas, ids, pks);
    // assertEqual(bpk.X, bpk2.X);

    assertEqual(bank_pks.size(), cp.numOfReplicas);

    EcashClient client(
      clientLogger,
      comm, 
      adapter_config, 
      p, 
      std::move(bank_pks), 
      main_pk, 
      std::move(my_initial_coins)
    );

    client.wait_for_connections();

    LOG_INFO(clientLogger, "Connected to all the replicas\n");
    LOG_INFO(clientLogger, "Starting " << cp.numOfOperations);

    // // Perform this check once all parameters configured.
    if (3 * cp.numOfFaulty + 2 * cp.numOfSlow + 1 != cp.numOfReplicas) {
      LOG_FATAL(clientLogger,
                "Number of replicas is not equal to 3f + 2c + 1 :"
                " f="
                    << cp.numOfFaulty << ", c=" << cp.numOfSlow << ", numOfReplicas=" << cp.numOfReplicas);
      exit(-1);
    }

    concordUtils::Histogram hist;
    hist.Clear();

    std::vector<std::deque<bft::client::WriteRequest>> write_requests(cp.numOfOperations);

    for(uint32_t i = 0; i < cp.numOfOperations; i++) {
      for(uint32_t j = 0; j < cp.batch_size; j++) {
        auto& batch_req = write_requests[i];
        bft::client::WriteRequest req;
        req.config.request.timeout = 100s;
        req.config.request.pre_execute = PRE_EXEC_ENABLED;
        // req_config.max_reply_size = 100000;
        req.config.quorum = client::LinearizableQuorum{};
        req.request = client.NewMintTx();
        req.config.request.sequence_number = pSeqGen.unique();
        batch_req.push_back(req);
      }
    }

    LOG_INFO(clientLogger, "Finished preparing batches");

    for (uint32_t i = 1; i <= cp.numOfOperations; i++) {
      // the python script that runs the client needs to know how many
      // iterations has been done - that's the reason we use printf and not
      // logging module - to keep the output exactly as we expect.
      if (i > 0 && i % 100 == 0) {
        printf("Iterations count: 100\n");
        printf("Total iterations count: %i\n", i);
      }

      LOG_DEBUG(clientLogger, "Starting Tx: " << i);

      // const uint64_t timeout = SimpleClient::INFINITE_TIMEOUT;
      uint64_t start = get_monotonic_time();

      auto x = client.sendBatch(write_requests[i-1], std::to_string(cp.clientId));
      
      uint64_t end = get_monotonic_time();
      uint64_t elapsedMicro = end - start;

      if (cp.measurePerformance) {
        hist.Add(double(elapsedMicro)/cp.batch_size);
      }

      LOG_DEBUG(clientLogger, "Got Tx: " << i);
      LOG_DEBUG(clientLogger, 
        "Got" << x.size() << "responses for the batch");

      for(auto& [seq, resp]: x) {
        auto status = client.verifyMintAckRSI(resp);
        if(!status.has_value()) {
          printf("Got invalid transactions from the replicas\n");
          return false;
        }
      }
    }

    if (cp.measurePerformance) {
      LOG_INFO(clientLogger,
               std::endl
                   << "Performance info from client " << cp.clientId << std::endl
                   << hist.ToString());
    }

    // Start the second set of experiments for Payments
    hist.Clear();

    // #define BATCH_SIZE 10
    // size_t batch_size = BATCH_SIZE;

    // bft::client::RequestConfig req_config;
    // req_config.timeout = 2s;
    // req_config.max_reply_size = 100000;
    // bft::client::WriteConfig write_config{req_config, bft::client::ByzantineSafeQuorum{}};
    // write_config.request.pre_execute = PRE_EXEC_ENABLED;
    // std::deque<bft::client::WriteConfig> write_vec(batch_size);
    // for(size_t i=0; i< batch_size;i++) {
    //   write_vec.push_back(write_config);
    // }

    // for (uint32_t i = 1; i <= cp.numOfOperations; i++) {

    //   // the python script that runs the client needs to know how many
    //   // iterations has been done - that's the reason we use printf and not
    //   // logging module - to keep the output exactly as we expect.
    //   if (i > 0 && i % 100 == 0) {
    //     printf("Iterations count: 100\n");
    //     printf("Total iterations count: %i\n", i);
    //   }

    //   // Prepare request parameters.
    //   std::deque<bft::client::WriteRequest> write_req;
    //   for(size_t i=0;i<batch_size;i++) {
    //     bft::client::Msg test_message = client.NewTestPaymentTx();
    //     bft::client::WriteRequest req{write_vec[i], test_message};
    //     write_req.push_back(req);
    //   }
    //   // std::cout << "Sending: " << empty_coin << std::endl;
    //   // std::cout << "Coin id: " << coin_id << std::endl;

    //   write_config.request.sequence_number = pSeqGen->generateUniqueSequenceNumberForRequest();

    //   // const uint64_t timeout = SimpleClient::INFINITE_TIMEOUT;
    //   uint64_t start = get_monotonic_time();
    //   try {
    //     auto x = client.sendBatch(write_req, std::to_string(cp.clientId));
    //   } catch (client::BftClientException& e) {
    //     i--;
    //     continue;
    //   }
    //   i+= batch_size;
      
    //   uint64_t end = get_monotonic_time();
    //   uint64_t elapsedMicro = end - start;

    //   std::cout << "Finished Iteration" << std::endl;

    //   if (cp.measurePerformance) {
    //     hist.Add(elapsedMicro);
    //     // LOG_INFO(clientLogger, "RAWLatencyMicro " << elapsedMicro << " Time " << (uint64_t)(end / 1e3));
    //   }


    //   // printf("Got Tx: %d\n", i);

    //   // printf("Got %lu bytes of matched data\n", x.matched_data.size());
    //   // printf("Got %lu rsi pieces\n", x.rsi.size());
    //   // printf("Got %lu rsi data\n", x.rsi.begin()->second.size());
    //   // auto status = client.verifyPayAckRSI(x);
    //   // if(!status) {
    //     // printf("Got invalid transactions from the replicas\n");
    //     // return false;
    //   // }
    //   // IGNORE CLIENT VERIFICATION FOR NOW
    //   // THE GOAL IS TO STRESS TEST THE PAYMENTS ON THE SERVER SIDE, NOT ON THE CLIENT SIDE
    // }

    // if (cp.measurePerformance) {
    //   LOG_INFO(clientLogger,
    //            std::endl
    //                << "Performance info from client " << cp.clientId << std::endl
    //                << hist.ToString());
    // }

    // After all requests have been issued, stop communication and clean up.
    client.stop();
    
    LOG_INFO(clientLogger, "test done, iterations: " << cp.numOfOperations);
    return true;
  }

};
