// Concord
//
// Copyright (c) 2018-2019 VMware, Inc. All Rights Reserved.
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

#include <fstream>
#include <memory>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <rocksdb/options.h>
#include <rocksdb/status.h>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "OpenTracing.hpp"
#include "replica/Params.hpp"
#include "ReplicaConfig.hpp"
#include "assertUtils.hpp"
#include "sliver.hpp"
#include "simpleKVBTestsBuilder.hpp"
#include "db_interfaces.h"
#include "block_metadata.hpp"
#include "KVBCInterfaces.h"
#include "ControlStateManager.hpp"
#include "replica/Params.hpp"
#include "bft.hpp"

static const std::string VERSIONED_KV_CAT_ID{"replica_tester_versioned_kv_category"};
static const std::string BLOCK_MERKLE_CAT_ID{"replica_tester_block_merkle_category"};

class InternalCommandsHandler : public concord::kvbc::ICommandsHandler {
 public:
  InternalCommandsHandler(concord::kvbc::IReader *storage,
                          concord::kvbc::IBlockAdder *blocksAdder,
                          concord::kvbc::IBlockMetadata *blockMetadata,
                          logging::Logger &logger)
      : m_storage(storage), m_blockAdder(blocksAdder), m_blockMetadata(blockMetadata), m_logger(logger) {
            auto& replicaConfig = bftEngine::ReplicaConfig::instance();

            // Setup ROCKSDB for libutt
            using namespace concord::storage::rocksdb;
            client = NativeClient::newClient("utt-db"+std::to_string(replicaConfig.replicaId), false, NativeClient::DefaultOptions{});
            
            // Setup libutt config here
            auto utt_params = replicaConfig.get(utt_bft::UTT_PARAMS_REPLICA_KEY, std::string());
            if (utt_params.empty()) {
                return;
            }
            std::string configFile = utt_params.append(std::to_string(replicaConfig.replicaId));
            LOG_INFO(m_logger, "Using UTT config:" << configFile);
            std::ifstream pfile(configFile);
            ConcordAssert(!pfile.fail());

            mParams_ = utt_bft::replica::Params(pfile);
        }

  virtual void execute(ExecutionRequestsQueue &requests,
                       std::optional<bftEngine::Timestamp> timestamp,
                       const std::string &batchCid,
                       concordUtils::SpanWrapper &parent_span) override;

  void setPerformanceManager(std::shared_ptr<concord::performance::PerformanceManager> perfManager) override;

 private:
  bool executeWriteCommand(uint32_t requestSize,
                           const char *request,
                           uint64_t sequenceNum,
                           uint8_t flags,
                           size_t maxReplySize,
                           char *outReply,
                           uint32_t &outReplySize,
                           bool isBlockAccumulationEnabled,
                           concord::kvbc::categorization::VersionedUpdates &blockAccumulatedVerUpdates,
                           concord::kvbc::categorization::BlockMerkleUpdates &blockAccumulatedMerkleUpdates,
                           uint32_t &outReplicaSpecificInfoSize);

  bool executeReadOnlyCommand(uint32_t requestSize,
                              const char *request,
                              size_t maxReplySize,
                              char *outReply,
                              uint32_t &outReplySize,
                              uint32_t &specificReplicaInfoOutReplySize);

  bool verifyWriteCommand(uint32_t requestSize,
                          const BasicRandomTests::SimpleCondWriteRequest &request,
                          size_t maxReplySize,
                          uint32_t &outReplySize) const;

  bool executeReadCommand(
      uint32_t requestSize, const char *request, size_t maxReplySize, char *outReply, uint32_t &outReplySize);

  bool executeGetBlockDataCommand(
      uint32_t requestSize, const char *request, size_t maxReplySize, char *outReply, uint32_t &outReplySize);

  bool executeGetLastBlockCommand(uint32_t requestSize, size_t maxReplySize, char *outReply, uint32_t &outReplySize);

  void addMetadataKeyValue(concord::kvbc::categorization::VersionedUpdates &updates, uint64_t sequenceNum) const;
  bool preExecuteMint(uint32_t requestSize, const char *request,
                    uint64_t sequenceNum, uint8_t flags,
                    size_t maxReplySize, char *outReply,
                    uint32_t &outReplySize, uint32_t &outReplicaSpecificInfoSize);
  bool postExecuteMint(uint32_t requestSize, const char *request,
                    uint64_t sequenceNum, uint8_t flags,
                    size_t maxReplySize, char *outReply,
                    uint32_t &outReplySize, uint32_t &outReplicaSpecificInfoSize);
  bool preExecutePay(uint32_t requestSize, const char *request,
                    uint64_t sequenceNum, uint8_t flags,
                    size_t maxReplySize, char *outReply,
                    uint32_t &outReplySize, uint32_t &outReplicaSpecificInfoSize);
  bool postExecutePay(uint32_t requestSize, const char *request,
                    uint64_t sequenceNum, uint8_t flags,
                    size_t maxReplySize, char *outReply,
                    uint32_t &outReplySize, uint32_t &outReplicaSpecificInfoSize);

 private:
  static concordUtils::Sliver buildSliverFromStaticBuf(char *buf);
  std::optional<std::string> get(const std::string &key, concord::kvbc::BlockId blockId) const;
  std::string getAtMost(const std::string &key, concord::kvbc::BlockId blockId) const;
  std::string getLatest(const std::string &key) const;
  std::optional<concord::kvbc::BlockId> getLatestVersion(const std::string &key) const;
  std::optional<std::map<std::string, std::string>> getBlockUpdates(concord::kvbc::BlockId blockId) const;
  void writeAccumulatedBlock(ExecutionRequestsQueue &blockedRequests,
                             concord::kvbc::categorization::VersionedUpdates &verUpdates,
                             concord::kvbc::categorization::BlockMerkleUpdates &merkleUpdates);
  void addBlock(concord::kvbc::categorization::VersionedUpdates &verUpdates,
                concord::kvbc::categorization::BlockMerkleUpdates &merkleUpdates);
  void addKeys(BasicRandomTests::SimpleCondWriteRequest *writeReq,
               uint64_t sequenceNum,
               concord::kvbc::categorization::VersionedUpdates &verUpdates,
               concord::kvbc::categorization::BlockMerkleUpdates &merkleUpdates);
  bool hasConflictInBlockAccumulatedRequests(
      const std::string &key,
      concord::kvbc::categorization::VersionedUpdates &blockAccumulatedVerUpdates,
      concord::kvbc::categorization::BlockMerkleUpdates &blockAccumulatedMerkleUpdates) const;

 private:
  concord::kvbc::IReader *m_storage;
  concord::kvbc::IBlockAdder *m_blockAdder;
  concord::kvbc::IBlockMetadata *m_blockMetadata;
  logging::Logger &m_logger;
  size_t m_readsCounter = 0;
  size_t m_writesCounter = 0;
  size_t m_getLastBlockCounter = 0;
  std::atomic_uint64_t val = 0;
  std::shared_ptr<concord::performance::PerformanceManager> perfManager_;
  std::optional<utt_bft::replica::Params> mParams_ = nullopt;
  std::shared_ptr<concord::storage::rocksdb::NativeClient> client = nullptr;
};
