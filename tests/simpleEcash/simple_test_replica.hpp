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

#include "Logging4cplus.hpp"
#include "assertUtils.hpp"
#include "OpenTracing.hpp"
#include "communication/CommFactory.hpp"
#include "Replica.hpp"
#include "ReplicaConfig.hpp"
#include "ControlStateManager.hpp"
#include "SimpleStateTransfer.hpp"
#include "FileStorage.hpp"
#include <cstdint>
#include <cstdio>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "commonDefs.h"
#include "simple_test_replica_behavior.hpp"
#include "threshsign/IThresholdSigner.h"
#include "threshsign/IThresholdVerifier.h"
#include "state.hpp"

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
 private:
 public:
  SimpleAppState(uint16_t numCl, uint16_t numRep)
      : statePtr{new SimpleAppState::State[numCl]}, numOfClients{numCl}, numOfReplicas{numRep} {}
  ~SimpleAppState() { delete[] statePtr; }
  bool verifyMintTx(ExecutionRequest &req) {
    // Our read-write request includes one eight-byte argument, in addition to
    // the request type.
    if (req.requestSize != sizeof(OpType)) {
      LOG_WARN(replicaLogger, "Invalid size: Got " << req.requestSize << ", Expected: " << sizeof(OpType));
      req.outExecutionStatus = -1;
      return false;
    } 

    // We only support the WRITE operation in read-write mode.
    const OpType *pReqId = reinterpret_cast<const OpType *>(req.request);
    if (*pReqId == OpType::Mint) {
      printf("Got a mint transaction\n");
    } else {
      printf("Got something else\n");
    }
    return true;
  }

  // This call will execute a Mint and return the changed state
  bool executeMintTx(ExecutionRequest &req, std::unordered_set<uint8_t> &partial_state) {
    // Reply with the number of times we've modified the register.
    test_assert_replica(req.maxReplySize >= sizeof(OpType), "maxReplySize < " << sizeof(OpType));
    OpType *pRet = const_cast<OpType *>(reinterpret_cast<const OpType *>(req.outReply));
    *pRet = OpType::MintAck;
    req.outActualReplySize = sizeof(OpType);

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
    printf("Test tag\n");
    std::unordered_set<uint8_t> updates;
    for (auto &req : requests) {
      // These are invalid transactions, Ignore.
      if (req.outExecutionStatus != 1) {
        LOG_INFO(replicaLogger, "Executing an invalid transaction");
        continue;
      };

      // Not currently used
      req.outReplicaSpecificInfoSize = 0;

      bool pre_executed = req.flags & MsgFlag::HAS_PRE_PROCESSED_FLAG;
      if (!pre_executed) {
        if (!verifyMintTx(req)) {
          continue;
        }
      }
      // By now, either via pre-execution or not, we would have finished verifying the command
      // Check for conflicts and update the system
      bool has_conflict = false;
      has_conflict = executeMintTx(req, updates);
      statePtr->spent_coins.merge(updates);
      updates.clear();
    }
  }

  struct State {
    std::unordered_set<uint8_t> spent_coins;
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
    for (auto &logger: logging::Logger::getCurrentLoggers()) {
      logger.setLogLevel(log4cplus::TRACE_LOG_LEVEL);
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
    replicaConfig.numOfClientProxies = rp.numOfClients;
    replicaConfig.viewChangeProtocolEnabled = rp.viewChangeEnabled;
    replicaConfig.viewChangeTimerMillisec = rp.viewChangeTimeout;
    replicaConfig.replicaId = rp.replicaId;
    replicaConfig.statusReportTimerMillisec = 10000;
    replicaConfig.concurrencyLevel = 1;
    replicaConfig.debugPersistentStorageEnabled =
        rp.persistencyMode == PersistencyMode::InMemory || rp.persistencyMode == PersistencyMode::File;

    // This is the state machine that the replica will drive.
    SimpleAppState *simpleAppState = new SimpleAppState(rp.numOfClients, rp.numOfReplicas);

#ifdef USE_COMM_PLAIN_TCP
    PlainTcpConfig conf =
        testCommConfig.GetTCPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
#elif USE_COMM_TLS_TCP
    TlsTcpConfig conf =
        testCommConfig.GetTlsTCPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
#else
    PlainUdpConfig conf =
        testCommConfig.GetUDPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
#endif
    auto comm = bft::communication::CommFactory::create(conf);

    bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *st =
        bftEngine::SimpleInMemoryStateTransfer::create(simpleAppState->statePtr,
                                                       sizeof(SimpleAppState::State) * rp.numOfClients,
                                                       replicaConfig.replicaId,
                                                       replicaConfig.fVal,
                                                       replicaConfig.cVal,
                                                       true);

    simpleAppState->st = st;
    SimpleTestReplica *replica = new SimpleTestReplica(comm, simpleAppState, replicaConfig, behv, st, metaDataStorage);
    return replica;
  }
};
