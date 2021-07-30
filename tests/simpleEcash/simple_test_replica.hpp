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
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Logging4cplus.hpp"
#include "PrimitiveTypes.hpp"
#include "assertUtils.hpp"
#include "OpenTracing.hpp"
#include "communication/CommFactory.hpp"
#include "Replica.hpp"
#include "ReplicaConfig.hpp"
#include "ControlStateManager.hpp"
#include "SimpleStateTransfer.hpp"
#include "FileStorage.hpp"
#include "rocksdb/native_client.h"
#include "commonDefs.h"
#include "simple_test_replica_behavior.hpp"
#include "threshsign/IThresholdSigner.h"
#include "threshsign/IThresholdVerifier.h"
#include "state.hpp"

#include "utt/Bank.h"
#include "utt/Coin.h"
#include "utt/Params.h"

using namespace bftEngine;
using namespace bft::communication;
using namespace std;

// NOLINTNEXTLINE(misc-definitions-in-headers)
logging::Logger replicaLogger = logging::getLogger("simpletest.replica");

#define test_assert_replica(statement, message)                                                                   \
  {                                                                                                               \
    if (!(statement)) {                                                                                           \
      LOG_FATAL(replicaLogger, "assert fail with message: " << message); /* NOLINT(bugprone-macro-parentheses) */ \
      ConcordAssert(false);                                                                                       \
    }                                                                                                             \
  }


// The replica state machine.
class SimpleAppState : public IRequestsHandler {
 public:
  libutt::BankShareSK *shareSK = nullptr;
  libutt::Params *p = nullptr;
  uint16_t replicaId = 0;

  // (TODO) DB Stress test

  // Baselines: ZCash (no anonymity budget) vs UTT,
  // Veksel?
  // BFT Baselines, FastPay(RUST) baseline, Coconut, SNARKs

 public:
  SimpleAppState(uint16_t numCl, uint16_t numRep)
      : statePtr{new SimpleAppState::State[numCl]}, numOfClients{numCl}, numOfReplicas{numRep} {
      }
  ~SimpleAppState() { delete[] statePtr; }
  bool verifyMintTx(ExecutionRequest &req) {
    // Our read-write request includes one eight-byte argument, in addition to
    // the request type.
    if (req.requestSize <= sizeof(UTT_Msg)) {
      LOG_WARN(replicaLogger, "Invalid size: Got " << req.requestSize << ", Expected: " << sizeof(OpType));
      req.outExecutionStatus = -1;
      return false;
    } 

    // We only support the WRITE operation in read-write mode.
    const OpType *pReqId = reinterpret_cast<const OpType *>(req.request);
    if (*pReqId == OpType::Mint) {
      auto mint_req = reinterpret_cast<const MintMsg*>(req.request+sizeof(UTT_Msg));
      printf("Got a mint transaction to mint %lu money\n", mint_req->val);
    } else {
      printf("Got something else\n");
    }
    return true;
  }

  // Only fills the common parts without RSI
  // store the sig locally so we can ship it on execution
  bool preExecuteMintTx(ExecutionRequest &req) {
    printf("B: Printing request\n");
    for(auto i = 0u; i < req.requestSize; i++) {
      printf("%02x", req.request[i]);
    }
    printf("\n");

    // Is the size good?
    test_assert_replica(req.maxReplySize >= sizeof(UTT_Msg)+sizeof(MintAckMsg), 
            "maxReplySize < " << sizeof(OpType));

    // Since the request will be deleted after pre-processing, store the request message as the reply
    req.outReplicaSpecificInfoSize = 0;
    req.outActualReplySize = req.requestSize;
    std::memcpy(req.outReply, req.request, req.requestSize);

    // std::cout << "Max Reply Size is:" << req.maxReplySize << std::endl;
    req.outExecutionStatus = 0;

    return true;
  }

  bool PostExecuteMintTx(ExecutionRequest &req) {
    printf("A: Printing request for client: %u\n", req.clientId);
    for(auto i = 0u; i < req.requestSize; i++) {
      printf("%02x", req.request[i]);
    }
    printf("\n");

    auto mint_req = reinterpret_cast<const MintMsg*>(req.request+sizeof(UTT_Msg));
    
    std::stringstream ss;
    ss.rdbuf()->pubsetbuf(
      const_cast<char*>(req.request+sizeof(UTT_Msg)+sizeof(MintMsg)), 
      mint_req->cc_buf_len
    );
    
    libutt::EPK empty_coin;
    ss >> empty_coin;
    ss.clear();

    // TODO: Check if the value is actually zero
    if (!libutt::EpkProof::verify(*p, empty_coin)) {
      printf("verification failed\n");
      std::cout << "Params:" << *p << std::endl;
      std::cout << "Got:" << empty_coin << std::endl;
      return false;
    }

    auto *pRet = reinterpret_cast<UTT_Msg*>(req.outReply);
    pRet->type = OpType::MintAck;

    // TODO: Add value
    libutt::CoinComm cc(*p, empty_coin, mint_req->val);
    libutt::CoinSigShare coin = shareSK->sign(*p, cc);
    ss << coin;

    auto ack = reinterpret_cast<MintAckMsg*>(req.outReply+sizeof(UTT_Msg));
    ack->coin_sig_share_size = ss.str().size();
    ack->coin_id = mint_req->coin_id;
    std::memcpy(req.outReply+sizeof(UTT_Msg)+sizeof(MintAckMsg), ss.str().data(), ss.str().size());
    
    // The last two parts MintAckMsg and the CoinSigShare are replicaSpecific information
    req.outReplicaSpecificInfoSize = ack->coin_sig_share_size + sizeof(MintAckMsg);
    req.outActualReplySize = req.outReplicaSpecificInfoSize + sizeof(UTT_Msg);

    // std::cout << "Max Reply Size is:" << req.maxReplySize << std::endl;
    req.outExecutionStatus = 0;
    return true;
  }

  // This call will execute a Mint and return the changed state
  bool RawExecuteMintTx(ExecutionRequest &req) {
    // Reply with the number of times we've modified the register.
    test_assert_replica(req.maxReplySize >= sizeof(UTT_Msg)+sizeof(MintAckMsg), "maxReplySize < " << sizeof(OpType));
    auto mint_req = reinterpret_cast<const MintMsg*>(req.request+sizeof(UTT_Msg));
    std::stringstream ss;
    ss.rdbuf()->pubsetbuf(
      const_cast<char*>(req.request+sizeof(UTT_Msg)+sizeof(MintMsg)), 
      mint_req->cc_buf_len
    );
    libutt::EPK empty_coin;
    ss >> empty_coin;
    ss.clear();

    // DONE: Check if the value is actually zero
    if (!libutt::EpkProof::verify(*p, empty_coin)) {
      printf("verification failed\n");
      std::cout << "Got:" << empty_coin << std::endl;
      return false;
    }

    auto *pRet = reinterpret_cast<UTT_Msg*>(req.outReply);
    pRet->type = OpType::MintAck;

    // TODO(try) libutt::Fr(long)
    // TODO: Add value
    libutt::CoinComm cc(*p, empty_coin, mint_req->val);
    libutt::CoinSigShare coin = shareSK->sign(*p, cc);
    // Alin: Send r to the client if the bank was one entity; 
    // We can also set r=0 since the client will re-randomize it anyways.
    ss << coin;

    auto ack = reinterpret_cast<MintAckMsg*>(req.outReply+sizeof(UTT_Msg));
    ack->coin_sig_share_size = ss.str().size();
    ack->coin_id = mint_req->coin_id;
    std::memcpy(req.outReply+sizeof(UTT_Msg)+sizeof(MintAckMsg), ss.str().data(), ss.str().size());
    
    // The last two parts MintAckMsg and the CoinSigShare are replicaSpecific information
    req.outReplicaSpecificInfoSize = ack->coin_sig_share_size + sizeof(MintAckMsg);
    req.outActualReplySize = req.outReplicaSpecificInfoSize + sizeof(UTT_Msg);

    req.outExecutionStatus = 0;
    return true;
  }
  
  // Handler for the upcall from Concord-BFT.
  // This function will be called twice
  // The first time is during pre-execution.
  // The second time is after consensus.
  void execute(ExecutionRequestsQueue &requests,
               const std::string &batchCid,
               concordUtils::SpanWrapper &parent_span) override {
    for (auto &req : requests) {
      // These are invalid transactions, Ignore.
      if (req.outExecutionStatus != 1) {
        LOG_INFO(replicaLogger, "Executing an invalid transaction");
        continue;
      };

      // Not currently used
      // req.outReplicaSpecificInfoSize = 0;

      if ((req.flags & MsgFlag::PRE_PROCESS_FLAG)) {
        printf("Pre-executing\n");
        // We are being executed in pre-processing
        auto res = verifyMintTx(req);
        if (!res) printf("Got an invalid mint tx\n");
        preExecuteMintTx(req);
        return;
      } 

      // We are not in pre-processing and we have not pre-processed the transaction
      if (!(req.flags & MsgFlag::HAS_PRE_PROCESSED_FLAG)) {
        printf("Called without pre-execution\n");
        auto res = verifyMintTx(req);
        if (!res) printf("Got an invalid mint tx\n");
        RawExecuteMintTx(req);
        return;
      }

      PostExecuteMintTx(req);
      printf("out reply size is: %u", req.outActualReplySize);
    }
  }

  struct State {
    std::shared_ptr<concord::storage::rocksdb::NativeClient> client;
  };

  State *statePtr;

  uint16_t numOfClients;
  uint16_t numOfReplicas;

  bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *st = nullptr;
};

class SimpleTestReplica {
 private:
  ICommunication *comm;
  bftEngine::IReplica::IReplicaPtr replica = nullptr;
  const ReplicaConfig &replicaConfig;
  std::thread *runnerThread = nullptr;
  ISimpleTestReplicaBehavior *behaviorPtr;
  IRequestsHandler *statePtr;
  bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *inMemoryST_;
  // UTT Params
  // libutt::Params *p = nullptr;
  // libutt::BankShareSK *shareSK = nullptr;

 public:
  SimpleTestReplica(ICommunication *commObject,
                    IRequestsHandler *state,
                    const ReplicaConfig &rc,
                    ISimpleTestReplicaBehavior *behvPtr,
                    bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *inMemoryST,
                    MetadataStorage *metaDataStorage)
      : comm{commObject}, replicaConfig{rc}, behaviorPtr{behvPtr}, statePtr(state), inMemoryST_(inMemoryST) {
    replica = IReplica::createNewReplica(rc,
                                         std::shared_ptr<bftEngine::IRequestsHandler>(state),
                                         inMemoryST,
                                         comm,
                                         metaDataStorage,
                                         std::make_shared<concord::performance::PerformanceManager>(),
                                         nullptr /*SecretsManagerEnc*/);
  }

  ~SimpleTestReplica() {
    // TODO(DD): Reset manually because apparently the order matters - fixit
    replica.reset();
    if (comm) {
      comm->Stop();
      delete comm;
    }
    if (behaviorPtr) {
      delete behaviorPtr;
    }
    if (statePtr) {
      delete statePtr;
    }
  }

  uint16_t get_replica_id() { return replicaConfig.replicaId; }

  void start() {
    // for (auto &logger: logging::Logger::getCurrentLoggers()) {
    //   logger.setLogLevel(log4cplus::TRACE_LOG_LEVEL);
    // }

    replica->start();
    ControlStateManager::instance(inMemoryST_).disable();
  }

  void stop() {
    replica->stop();
    if (runnerThread) {
      runnerThread->join();
    }
    LOG_INFO(replicaLogger, "replica " << replicaConfig.replicaId << " stopped");
  }

  bool isRunning() { return replica->isRunning(); }

  void run() {
    if (replica->isRunning() && behaviorPtr->to_be_restarted()) {
      uint32_t initialSleepBetweenRestartsMillis = behaviorPtr->get_initial_sleep_between_restarts_ms();
      LOG_INFO(replicaLogger, "Restarting replica in " << initialSleepBetweenRestartsMillis << " ms");
      std::this_thread::sleep_for(std::chrono::milliseconds(initialSleepBetweenRestartsMillis));
    }
    while (replica->isRunning()) {
      bool toBeRestarted = behaviorPtr->to_be_restarted();
      if (toBeRestarted) {
        if (replica && replica->isRunning()) {
          uint32_t downTime = behaviorPtr->get_down_time_millis();
          LOG_INFO(replicaLogger, "Restarting replica");
          replica->restartForDebug(downTime);
          behaviorPtr->on_restarted();
          LOG_INFO(replicaLogger, "Replica restarted");
        }
      } else {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  }

  void run_non_blocking() { runnerThread = new std::thread(std::bind(&SimpleTestReplica::run, this)); }

  static SimpleTestReplica *create_replica(ISimpleTestReplicaBehavior *behv,
                                           ReplicaParams rp,
                                           MetadataStorage *metaDataStorage) {
    TestCommConfig testCommConfig(replicaLogger);
    ReplicaConfig &replicaConfig = ReplicaConfig::instance();
    testCommConfig.GetReplicaConfig(rp.replicaId, rp.keysFilePrefix, &replicaConfig);
    replicaConfig.numOfClientProxies = 0;
    replicaConfig.viewChangeProtocolEnabled = rp.viewChangeEnabled;
    replicaConfig.viewChangeTimerMillisec = rp.viewChangeTimeout;
    replicaConfig.preExecReqStatusCheckTimerMillisec = 200;
    replicaConfig.replicaId = rp.replicaId;
    replicaConfig.statusReportTimerMillisec = 10000;
    replicaConfig.concurrencyLevel = 1;
    replicaConfig.debugPersistentStorageEnabled =
        rp.persistencyMode == PersistencyMode::InMemory || rp.persistencyMode == PersistencyMode::File;

    replicaConfig.preExecutionFeatureEnabled = true;
    replicaConfig.numOfExternalClients = 10;
    replicaConfig.clientBatchingEnabled = true;

    // This is the state machine that the replica will drive.
    SimpleAppState *simpleAppState = new SimpleAppState(rp.numOfClients, rp.numOfReplicas);

#ifdef USE_COMM_PLAIN_TCP
    PlainTcpConfig conf =
        testCommConfig.GetTCPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
#elif USE_COMM_TLS_TCP
    TlsTcpConfig conf =
        testCommConfig.GetTlsTCPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
    LOG_INFO(replicaLogger, "GetTlsConfig"<< -1);
#else
    PlainUdpConfig conf =
        testCommConfig.GetUDPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
#endif
    printf("GetTlsConfig %d\n", 0);
    auto comm = bft::communication::CommFactory::create(conf);
    printf("GetTlsConfig %d\n", -1);

    bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *st =
        bftEngine::SimpleInMemoryStateTransfer::create(simpleAppState->statePtr,
                                                       sizeof(SimpleAppState::State) * rp.numOfClients,
                                                       replicaConfig.replicaId,
                                                       replicaConfig.fVal,
                                                       replicaConfig.cVal,
                                                       true);

    // Load UTT confs
    LOG_INFO(replicaLogger, "Initializing libutt");
    libutt::initialize(nullptr, 0);

    std::string ifile = "utt_pvt_replica_" + std::to_string(rp.replicaId);
    LOG_INFO(replicaLogger, "Opening iFile" << ifile);
    std::ifstream fin(ifile);

    if (fin.fail()) {
      throw std::runtime_error("Failed to open libutt params file");
    }

    simpleAppState->p = new libutt::Params;
    fin >> *simpleAppState->p;

    simpleAppState->shareSK = new libutt::BankShareSK;
    fin >> *simpleAppState->shareSK;

    simpleAppState->replicaId = rp.replicaId;

    simpleAppState->st = st;
    simpleAppState->statePtr = new SimpleAppState::State;

    using namespace concord::storage::rocksdb;
    simpleAppState->statePtr->client = NativeClient::newClient("local-db"+std::to_string(replicaConfig.replicaId), false, NativeClient::DefaultOptions{});
    SimpleTestReplica *replica = new SimpleTestReplica(comm, simpleAppState, replicaConfig, behv, st, metaDataStorage);
    return replica;
  }
};
