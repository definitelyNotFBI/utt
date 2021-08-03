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
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
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
#include "utt/Utt.h"

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
  libutt::BankShareSK shareSK;
  libutt::Params p;
  libutt::BankPK bpk;
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

  bool verifyTx(ExecutionRequest& req) {
    if (req.requestSize <= sizeof(UTT_Msg)) {
      LOG_WARN(replicaLogger, "Invalid size: Got " << req.requestSize << ", Expected: " << sizeof(OpType));
      req.outExecutionStatus = -1;
      return false;
    } 

    const OpType *pReqId = reinterpret_cast<const OpType *>(req.request);
    if (*pReqId == OpType::Mint) {
      return verifyMintTx(req);
    } else if (*pReqId == OpType::Pay) {
      return verifyPayTx(req);
    } else {
      LOG_DEBUG(replicaLogger, "Unknown tx");
      return false;
    }
    return true;
  }

  bool verifyMintTx(ExecutionRequest &req) {
    auto mint_req = reinterpret_cast<const MintMsg*>(req.request+sizeof(UTT_Msg));
    LOG_DEBUG(replicaLogger, "Got a mint transaction to mint money: " << mint_req->val);

    return true;
  }
  bool verifyPayTx(ExecutionRequest &req) {
    LOG_DEBUG(replicaLogger, "Got a payment transaction");
    return true;
  }

  bool preExecuteTx(ExecutionRequest& req) {
    auto utt_msg = reinterpret_cast<const UTT_Msg*>(req.request);
    if(utt_msg->type == OpType::Mint) {
      return preExecuteMintTx(req);
    } else if (utt_msg->type == OpType::Pay) {
      return preExecutePayTx(req);
    } else {
      LOG_DEBUG(replicaLogger, "Unknown tx");
    }
    return false;
  }

  bool preExecutePayTx(ExecutionRequest& req) {
    LOG_DEBUG(replicaLogger, "Pre-executing payment tx");

    libutt::Tx tx;
    std::stringstream ss;
    // Tx tx(unsigned char*, size_t);
    ss.write(req.request + sizeof(UTT_Msg), req.requestSize - sizeof(UTT_Msg));
    ss >> tx;

    if (!tx.verify(p, bpk)) {
      LOG_DEBUG(replicaLogger, "Got an invalid tx");
      req.outExecutionStatus = -1;
      return false;
    }

    std::string value;
    // Now check if the nullifiers are already burnt
    for(auto& txin: tx.ins) {
      auto str = txin.null.toString();
      bool key_may_exist = statePtr->client->rawDB().KeyMayExist(rocksdb::ReadOptions{}, statePtr->client->defaultColumnFamily(), &value, nullptr);
      // If the key max exist returns true, then there is a possibility that the key still does not exist, but we need to call Get()
      if(key_may_exist) {
        auto status = statePtr->client->rawDB().Get(rocksdb::ReadOptions{}, statePtr->client->defaultColumnFamilyHandle(), str, &value);
        if (!status.IsNotFound()) {
          // The key exists, abort
          req.outExecutionStatus = -1;
          return false;
        }
      }
    }

    // If unspent, reply with the nullifiers
    // TODO: What to reply?
    req.outReplicaSpecificInfoSize = 0;
    req.outActualReplySize = req.requestSize;
    std::memcpy(req.outReply, req.request, req.requestSize);

    req.outExecutionStatus = 0;

    return true;
  }

  // Only fills the common parts without RSI
  // store the sig locally so we can ship it on execution
  bool preExecuteMintTx(ExecutionRequest &req) {
    // Since the request will be deleted after pre-processing, store the request message as the reply
    req.outReplicaSpecificInfoSize = 0;
    req.outActualReplySize = req.requestSize;
    std::memcpy(req.outReply, req.request, req.requestSize);

    // std::cout << "Max Reply Size is:" << req.maxReplySize << std::endl;
    req.outExecutionStatus = 0;

    return true;
  }

  bool PostExecuteTx(ExecutionRequest& req) {
    auto utt_msg = reinterpret_cast<const UTT_Msg*>(req.request);
    if(utt_msg->type == OpType::Mint) {
      return PostExecuteMintTx(req);
    } else if (utt_msg->type == OpType::Pay) {
      return PostExecutePayTx(req);
    } else {
      LOG_DEBUG(replicaLogger, "Unknown tx");
      return false;
    }
    return false;
  }

  bool PostExecutePayTx(ExecutionRequest& req) {
    LOG_DEBUG(replicaLogger, "Post Executing payment transaction");

    libutt::Tx tx;
    std::stringstream ss;
    ss.write(req.request + sizeof(UTT_Msg), req.requestSize - sizeof(UTT_Msg));
    ss >> tx;

    // We checked that the transaction is valid in pre-execution
    // Check again that the tx is unspent
    std::string value;
    for(auto& txin: tx.ins) {
      auto str = txin.null.toString();
      bool key_may_exist = statePtr->client->rawDB().KeyMayExist(rocksdb::ReadOptions{}, statePtr->client->defaultColumnFamily(), &value, nullptr);
      if(key_may_exist) {
        // If the key max exist returns true, then there is a possibility that the key still does not exist, but we need to call Get()
        auto status = statePtr->client->rawDB().Get(rocksdb::ReadOptions{}, statePtr->client->defaultColumnFamilyHandle(), str, &value);
        if (!status.IsNotFound()) {
          // The key exists, abort
          req.outExecutionStatus = -1;
          return false;
        }
      }
    }

    // Process the tx (The coins are still unspent)
    tx.process(p, shareSK);

    // Add nullifiers to the DB
    for(auto& txin: tx.ins) {
      // HACK so that the client can re-use the same coin
      auto str = txin.null.toString() + std::to_string(statePtr->val);
      LOG_DEBUG(replicaLogger, "Atomic Pointer Str: "<<str);
      statePtr->val.fetch_add(1);
      statePtr->client->rawDB().Put(rocksdb::WriteOptions{}, str, str);
    }

    // Send the signed coins back to the clients via RSI
    ss.clear();
    ss << tx;
    for(size_t i=0;i<2;i++) {
      ss << tx.outs[i].cc.get();
      ss << tx.outs[i].sig.get();
    }
    req.outReplicaSpecificInfoSize = 0;
    req.outExecutionStatus = 0;
    req.outActualReplySize = sizeof(UTT_Msg) + req.outReplicaSpecificInfoSize;
    auto pay_ack = reinterpret_cast<UTT_Msg*>(req.outReply);
    pay_ack->type = OpType::PayAck;
    // std::memcpy(req.outReply, req.request, 1);
    // LOG_DEBUG(replicaLogger, "Mem copying" << ss.str().size());
    // std::memcpy(req.outReply+sizeof(UTT_Msg), ss.str().data(), ss.str().size());

    return true;
  }

  bool PostExecuteMintTx(ExecutionRequest &req) {
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
    if (!libutt::EpkProof::verify(p, empty_coin)) {
      printf("verification failed\n");
      std::cout << "Params:" << p << std::endl;
      std::cout << "Got:" << empty_coin << std::endl;
      return false;
    }

    auto *pRet = reinterpret_cast<UTT_Msg*>(req.outReply);
    pRet->type = OpType::MintAck;

    // TODO: Add value
    libutt::CoinComm cc(p, empty_coin, mint_req->val);
    libutt::CoinSigShare coin = shareSK.sign(p, cc);
    ss << coin;

    std::cout << "Receiving: " << empty_coin << std::endl;
    std::cout << "Coin id: " << mint_req->coin_id << std::endl;


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

  bool RawExecuteTx(ExecutionRequest& req) {
    auto utt_msg = reinterpret_cast<const UTT_Msg*>(req.request);
    if(utt_msg->type == OpType::Mint) {
      return RawExecuteMintTx(req);
    } else if(utt_msg->type == OpType::Pay) {
      return RawExecutePayTx(req);
    }
    return false;
  }

  bool RawExecutePayTx(ExecutionRequest& req) {
    LOG_DEBUG(replicaLogger, "Raw executing a payment transaction");
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
    if (!libutt::EpkProof::verify(p, empty_coin)) {
      printf("verification failed\n");
      std::cout << "Got:" << empty_coin << std::endl;
      return false;
    }

    auto *pRet = reinterpret_cast<UTT_Msg*>(req.outReply);
    pRet->type = OpType::MintAck;

    std::cout << "Receiving: " << empty_coin << std::endl;
    std::cout << "Coin id: " << mint_req->coin_id << std::endl;

    // TODO(try) libutt::Fr(long) 
    // This works
    libutt::CoinComm cc(p, empty_coin, mint_req->val);
    libutt::CoinSigShare coin = shareSK.sign(p, cc);
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
        LOG_DEBUG(replicaLogger, "Pre-executing");
        // We are being executed in pre-processing
        auto res = verifyTx(req);
        if (!res) {
          LOG_ERROR(replicaLogger, "Got an invalid tx");
          continue;
        };
        preExecuteTx(req);
        return;
      } 

      // We are not in pre-processing and we have not pre-processed the transaction
      if (!(req.flags & MsgFlag::HAS_PRE_PROCESSED_FLAG)) {
        LOG_DEBUG(replicaLogger, "Called without pre-execution");
        auto res = verifyTx(req);
        if (!res) {
          LOG_ERROR(replicaLogger, "Got an invalid tx");
          return;
        } 
        RawExecuteTx(req);
        return;
      }

      PostExecuteTx(req);
      LOG_DEBUG(replicaLogger, "out reply size is: "<< req.outActualReplySize);
    }
  }

  struct State {
    std::atomic_uint64_t val = 0;
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
    // Set all the loggers to TRACE_LOG_LEVEL
    for (auto &logger: logging::Logger::getCurrentLoggers()) {
      logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
    }

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

    fin >> simpleAppState->p;
    fin >> simpleAppState->shareSK;
    fin >> simpleAppState->bpk;

    simpleAppState->replicaId = rp.replicaId;

    simpleAppState->st = st;
    simpleAppState->statePtr = new SimpleAppState::State;

    using namespace concord::storage::rocksdb;
    simpleAppState->statePtr->client = NativeClient::newClient("local-db"+std::to_string(replicaConfig.replicaId), false, NativeClient::DefaultOptions{});
    SimpleTestReplica *replica = new SimpleTestReplica(comm, simpleAppState, replicaConfig, behv, st, metaDataStorage);
    return replica;
  }
};
