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

#include "internalCommandsHandler.hpp"
#include "Logging4cplus.hpp"
#include "OpenTracing.hpp"
#include "assertUtils.hpp"
#include "misc.hpp"
#include "simpleKVBTestsBuilder.hpp"
#include "sliver.hpp"
#include "kv_types.hpp"
#include "block_metadata.hpp"
#include "sha_hash.hpp"
#include <unistd.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <variant>
#include "ReplicaConfig.hpp"

// #define AB_TESTING_PRE_POST_EXEC_TIME

#include "utt/Tx.h"

using namespace BasicRandomTests;
using namespace bftEngine;
using namespace concord::kvbc::categorization;

using concordUtils::Sliver;
using concord::kvbc::BlockId;
using concord::kvbc::KeyValuePair;
using concord::storage::SetOfKeyValuePairs;

using Hasher = concord::util::SHA3_256;
using Hash = Hasher::Digest;

const uint64_t LONG_EXEC_CMD_TIME_IN_SEC = 11;

template <typename Span>
static Hash span_hash(const Span &span) {
  return Hasher{}.digest(span.data(), span.size());
}

static const std::string &keyHashToCategory(const Hash &keyHash) {
  // If the most significant bit of a key's hash is set, use the VersionedKeyValueCategory. Otherwise, use the
  // BlockMerkleCategory.
  if (keyHash[0] & 0x80) {
    return VERSIONED_KV_CAT_ID;
  }
  return BLOCK_MERKLE_CAT_ID;
}

static const std::string &keyToCategory(const std::string &key) { return keyHashToCategory(span_hash(key)); }

static void add(std::string &&key,
                std::string &&value,
                VersionedUpdates &verUpdates,
                BlockMerkleUpdates &merkleUpdates) {
  const auto &cat = keyToCategory(key);
  if (cat == VERSIONED_KV_CAT_ID) {
    verUpdates.addUpdate(std::move(key), std::move(value));
    return;
  }
  merkleUpdates.addUpdate(std::move(key), std::move(value));
}

void InternalCommandsHandler::execute(InternalCommandsHandler::ExecutionRequestsQueue &requests,
                                      std::optional<Timestamp> timestamp,
                                      const std::string &batchCid,
                                      concordUtils::SpanWrapper &parent_span) {
  if (requests.empty()) return;

  // To handle block accumulation if enabled
  VersionedUpdates verUpdates;
  BlockMerkleUpdates merkleUpdates;

  // auto pre_execute = requests.back().flags & bftEngine::PRE_PROCESS_FLAG;

  for (auto &req : requests) {
    if (req.outExecutionStatus != 1) continue;
    req.outReplicaSpecificInfoSize = 0;
    int res;
    if (req.requestSize < sizeof(SimpleRequest)) {
      LOG_ERROR(m_logger,
                "The message is too small: requestSize is " << req.requestSize << ", required size is "
                                                            << sizeof(SimpleRequest));
      req.outExecutionStatus = -1;
      continue;
    }
    auto* request = (SimpleRequest*)req.request;
    res = 0;
    
    if(req.flags & MsgFlag::HAS_PRE_PROCESSED_FLAG) {
      switch(request->type) {
        case PAY: {
          // TODO
          res = postExecutePay(req.requestSize,
                                req.request, 
                                req.executionSequenceNum,
                                req.flags,
                                req.maxReplySize,
                                req.outReply,
                                req.outActualReplySize,
                                req.outReplicaSpecificInfoSize
          );
        } break;
        default: {
          LOG_ERROR(m_logger, "Got an invalid transaction type");
        } break;
      }
      if (!res) LOG_ERROR(m_logger, "Command post execution failed!");
      req.outExecutionStatus = res ? 0 : -1;
      continue;
    }

    if(req.flags & MsgFlag::PRE_PROCESS_FLAG) {
      switch(request->type) {
        case PAY: {
          // Testing: Only execute if primary
          res = preExecutePay(  req.requestSize,
                                req.request, 
                                req.executionSequenceNum,
                                req.flags,
                                req.maxReplySize,
                                req.outReply,
                                req.outActualReplySize,
                                req.outReplicaSpecificInfoSize
          );
        } break;
        default: {
          LOG_ERROR(m_logger, "Got an invalid transaction type");
        } break;
      }
      if (!res) LOG_ERROR(m_logger, "Command execution failed!");
      req.outExecutionStatus = res ? 0 : -1;
      continue;
    }

    LOG_FATAL(m_logger, "Unreachable");
    ConcordAssert(0);
    // bool readOnly = req.flags & MsgFlag::READ_ONLY_FLAG;
    // if (readOnly) {
    //   res = executeReadOnlyCommand(req.requestSize,
    //                                req.request,
    //                                req.maxReplySize,
    //                                req.outReply,
    //                                req.outActualReplySize,
    //                                req.outReplicaSpecificInfoSize);
    // } else {
    //   // Only if requests size is greater than 1 and other conditions are met, block accumulation is enabled.
    //   bool isBlockAccumulationEnabled =
    //       ((requests.size() > 1) && (!pre_execute && (req.flags & bftEngine::MsgFlag::HAS_PRE_PROCESSED_FLAG)));

    //   res = executeWriteCommand(req.requestSize,
    //                             req.request,
    //                             req.executionSequenceNum,
    //                             req.flags,
    //                             req.maxReplySize,
    //                             req.outReply,
    //                             req.outActualReplySize,
    //                             isBlockAccumulationEnabled,
    //                             verUpdates,
    //                             merkleUpdates,
    //                             req.outReplicaSpecificInfoSize);
    // }

    if (!res) LOG_ERROR(m_logger, "Command execution failed!");
    req.outExecutionStatus = res ? 0 : -1;
  }

  // if (!pre_execute && (merkleUpdates.size() > 0 || verUpdates.size() > 0)) {
  //   // Write Block accumulated requests
  //   writeAccumulatedBlock(requests, verUpdates, merkleUpdates);
  // }
}

void InternalCommandsHandler::addMetadataKeyValue(VersionedUpdates &updates, uint64_t sequenceNum) const {
  updates.addUpdate(std::string{concord::kvbc::IBlockMetadata::kBlockMetadataKeyStr},
                    m_blockMetadata->serialize(sequenceNum));
}

Sliver InternalCommandsHandler::buildSliverFromStaticBuf(char *buf) {
  char *newBuf = new char[KV_LEN];
  memcpy(newBuf, buf, KV_LEN);
  return Sliver(newBuf, KV_LEN);
}

std::optional<std::string> InternalCommandsHandler::get(const std::string &key, BlockId blockId) const {
  const auto v = m_storage->get(keyToCategory(key), key, blockId);
  if (!v) {
    return std::nullopt;
  }
  return std::visit([](const auto &v) { return v.data; }, *v);
}

std::string InternalCommandsHandler::getAtMost(const std::string &key, BlockId current) const {
  if (m_storage->getLastBlockId() == 0 || m_storage->getGenesisBlockId() == 0 || current == 0) {
    return std::string(KV_LEN, '\0');
  }

  auto value = std::string(KV_LEN, '\0');
  do {
    const auto v = get(key, current);
    if (v) {
      value = *v;
      break;
    }
    --current;
  } while (current);
  return value;
}

std::string InternalCommandsHandler::getLatest(const std::string &key) const {
  const auto v = m_storage->getLatest(keyToCategory(key), key);
  if (!v) {
    return std::string(KV_LEN, '\0');
  }
  return std::visit([](const auto &v) { return v.data; }, *v);
}

std::optional<BlockId> InternalCommandsHandler::getLatestVersion(const std::string &key) const {
  const auto v = m_storage->getLatestVersion(keyToCategory(key), key);
  if (!v) {
    return std::nullopt;
  }
  // We never delete keys in TesterReplica at that stage.
  ConcordAssert(!v->deleted);
  return v->version;
}

std::optional<std::map<std::string, std::string>> InternalCommandsHandler::getBlockUpdates(
    concord::kvbc::BlockId blockId) const {
  const auto updates = m_storage->getBlockUpdates(blockId);
  if (!updates) {
    return std::nullopt;
  }

  auto ret = std::map<std::string, std::string>{};

  {
    const auto verUpdates = updates->categoryUpdates(VERSIONED_KV_CAT_ID);
    if (verUpdates) {
      const auto &u = std::get<VersionedInput>(verUpdates->get());
      for (const auto &[key, valueWithFlags] : u.kv) {
        ret[key] = valueWithFlags.data;
      }
    }
  }

  {
    const auto merkleUpdates = updates->categoryUpdates(BLOCK_MERKLE_CAT_ID);
    if (merkleUpdates) {
      const auto &u = std::get<BlockMerkleInput>(merkleUpdates->get());
      for (const auto &[key, value] : u.kv) {
        ret[key] = value;
      }
    }
  }

  return ret;
}

void InternalCommandsHandler::writeAccumulatedBlock(ExecutionRequestsQueue &blockedRequests,
                                                    VersionedUpdates &verUpdates,
                                                    BlockMerkleUpdates &merkleUpdates) {
  // Only block accumulated requests will be processed here
  BlockId currBlock = m_storage->getLastBlockId();

  for (auto &req : blockedRequests) {
    if (req.flags & bftEngine::MsgFlag::HAS_PRE_PROCESSED_FLAG) {
      auto *reply = (SimpleReply_ConditionalWrite *)req.outReply;
      reply->latestBlock = currBlock + 1;
      LOG_INFO(
          m_logger,
          "ConditionalWrite message handled; writesCounter=" << m_writesCounter << " currBlock=" << reply->latestBlock);
    }
  }
  addBlock(verUpdates, merkleUpdates);
}

bool InternalCommandsHandler::verifyWriteCommand(uint32_t requestSize,
                                                 const SimpleCondWriteRequest &request,
                                                 size_t maxReplySize,
                                                 uint32_t &outReplySize) const {
  if (requestSize < sizeof(SimpleCondWriteRequest)) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize is " << requestSize << ", required size is "
                                                          << sizeof(SimpleCondWriteRequest));
    return false;
  }
  if (requestSize < sizeof(request)) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize is " << requestSize << ", required size is " << sizeof(request));
    return false;
  }
  if (maxReplySize < outReplySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize=" << outReplySize << ", maxReplySize=" << maxReplySize);
    return false;
  }
  return true;
}

void InternalCommandsHandler::addKeys(SimpleCondWriteRequest *writeReq,
                                      uint64_t sequenceNum,
                                      VersionedUpdates &verUpdates,
                                      BlockMerkleUpdates &merkleUpdates) {
  SimpleKV *keyValArray = writeReq->keyValueArray();
  for (size_t i = 0; i < writeReq->numOfWrites; i++) {
    add(std::string(keyValArray[i].simpleKey.key, KV_LEN),
        std::string(keyValArray[i].simpleValue.value, KV_LEN),
        verUpdates,
        merkleUpdates);
  }
  addMetadataKeyValue(verUpdates, sequenceNum);
}

void InternalCommandsHandler::addBlock(VersionedUpdates &verUpdates, BlockMerkleUpdates &merkleUpdates) {
  BlockId currBlock = m_storage->getLastBlockId();

  Updates updates;
  updates.add(VERSIONED_KV_CAT_ID, std::move(verUpdates));
  updates.add(BLOCK_MERKLE_CAT_ID, std::move(merkleUpdates));
  const auto newBlockId = m_blockAdder->add(std::move(updates));
  ConcordAssert(newBlockId == currBlock + 1);
}

bool InternalCommandsHandler::hasConflictInBlockAccumulatedRequests(
    const std::string &key,
    VersionedUpdates &blockAccumulatedVerUpdates,
    BlockMerkleUpdates &blockAccumulatedMerkleUpdates) const {
  auto itVersionUpdates = blockAccumulatedVerUpdates.getData().kv.find(key);
  if (itVersionUpdates != blockAccumulatedVerUpdates.getData().kv.end()) {
    return true;
  }

  auto itMerkleUpdates = blockAccumulatedMerkleUpdates.getData().kv.find(key);
  if (itMerkleUpdates != blockAccumulatedMerkleUpdates.getData().kv.end()) {
    return true;
  }
  return false;
}

// bool InternalCommandsHandler::executeUTT_Pay();

bool InternalCommandsHandler::preExecutePay(uint32_t requestSize, 
                    const char *request,
                    uint64_t sequenceNum, 
                    uint8_t flags,
                    size_t maxReplySize, 
                    char *outReply,
                    uint32_t &outReplySize, 
                    uint32_t &outReplicaSpecificInfoSize) {
  #ifdef AB_TESTING_PRE_POST_EXEC_TIME
  auto start = get_monotonic_time();
  #endif
  auto *writeReq = (SimplePayRequest *)request;
  LOG_INFO(m_logger,
           "PreExecuting Pay command:"
               << " type=" << writeReq->header.type << " seqNum=" << sequenceNum
               << " READ_ONLY_FLAG=" << ((flags & MsgFlag::READ_ONLY_FLAG) != 0 ? "true" : "false")
               << " PRE_PROCESS_FLAG=" << ((flags & MsgFlag::PRE_PROCESS_FLAG) != 0 ? "true" : "false")
               << " HAS_PRE_PROCESSED_FLAG=" << ((flags & MsgFlag::HAS_PRE_PROCESSED_FLAG) != 0 ? "true" : "false")
               << " ReplicaSpecificInfo=" << outReplicaSpecificInfoSize
               << " maxReplySize=" << maxReplySize 
               << " requestSize=" << requestSize
               );
  // Copy the request into response 
  // outReplicaSpecificInfoSize = 0;
  // outReplySize = requestSize;
  // std::memcpy(outReply, request, requestSize);
  // return true;
  
  if(writeReq->getSize() != requestSize) {
    LOG_ERROR(m_logger, "Got invalid size for UTT Pay");
    LOG_ERROR(m_logger, "Tx says: " << writeReq->getSize() << " , but I got only " << requestSize);
    return false;
  }
  std::stringstream ss;
  ss.write(reinterpret_cast<const char*>(writeReq->getTxBuf()), writeReq->tx_buf_len);
  libutt::Tx tx(ss);
  if(!tx.validate(mParams_->p, mParams_->main_pk, mParams_->reg_pk)) {
    LOG_ERROR(m_logger, "Payment transaction verification failed");
    return false;
  }
  // So far, the transaction looks valid
  // DONE: Check if we can reject early by checking the storage
  std::string value;
  // Now check if the nullifiers are already burnt
  for(auto& null: tx.getNullifiers()) {
    auto str = null;
    bool key_may_exist = client->rawDB().KeyMayExist(rocksdb::ReadOptions{}, client->defaultColumnFamily(), &value, nullptr);
    // If the key max exist returns true, then there is a possibility that the key still does not exist, but we need to call Get()
    if(key_may_exist) {
      auto status = client->rawDB().Get(rocksdb::ReadOptions{}, client->defaultColumnFamilyHandle(), str, &value);
      if (!status.IsNotFound()) {
        // The key exists, abort
        LOG_ERROR(m_logger, "The nullifier already exists in the database");
        return false;
      }
    }
  }

  // Copy the request into response 
  outReplicaSpecificInfoSize = 0;
  outReplySize = requestSize;
  std::memcpy(outReply, request, requestSize);
  
  #ifdef AB_TESTING_PRE_POST_EXEC_TIME
  auto end = get_monotonic_time();
  std::cout << "Pre-exec time: " << (end-start) << " us" << std::endl;
  #endif
  return true;
}

bool InternalCommandsHandler::postExecutePay(uint32_t requestSize, 
                    const char *request,
                    uint64_t sequenceNum, 
                    uint8_t flags,
                    size_t maxReplySize, 
                    char *outReply,
                    uint32_t &outReplySize, 
                    uint32_t &outReplicaSpecificInfoSize) {
  #ifdef AB_TESTING_PRE_POST_EXEC_TIME
  auto start = get_monotonic_time();
  #endif
  auto *writeReq = (SimplePayRequest *)request;
  LOG_INFO(m_logger,
           "PostExecuting Pay command:"
               << " type=" << writeReq->header.type << " seqNum=" << sequenceNum
               << " READ_ONLY_FLAG=" << ((flags & MsgFlag::READ_ONLY_FLAG) != 0 ? "true" : "false")
               << " PRE_PROCESS_FLAG=" << ((flags & MsgFlag::PRE_PROCESS_FLAG) != 0 ? "true" : "false")
               << " HAS_PRE_PROCESSED_FLAG=" << ((flags & MsgFlag::HAS_PRE_PROCESSED_FLAG) != 0 ? "true" : "false")
               << " ReplicaSpecificInfo=" << outReplicaSpecificInfoSize
               << " maxReplySize=" << maxReplySize 
               << " requestSize=" << requestSize
               );
  
  std::stringstream ss;
  ss.write(reinterpret_cast<const char*>(writeReq->getTxBuf()), writeReq->tx_buf_len);
  libutt::Tx tx(ss);
  // The transaction was validated during pre-execution
  // Done: Check again if the nullifier was updated
  std::string value;
  bool found;
  // for(auto& null: tx.getNullifiers()) {
  for(auto& null: {"test", "test", "test"}) {
    bool key_may_exist = client->rawDB().KeyMayExist(
                                          rocksdb::ReadOptions{}, 
                                          client->defaultColumnFamilyHandle(), 
                                          null,
                                          &value,
                                          &found);
    if(!key_may_exist) {
      continue;
    }
    if (found) {
      LOG_ERROR(m_logger, "The nullifier already exists in the database");
      return false;
    }
    if(key_may_exist) {
      // If the key max exist returns true, then there is a possibility that the key still does not exist, but we need to call Get()
      auto status = client->rawDB().Get(rocksdb::ReadOptions{}, 
                                        client->defaultColumnFamilyHandle(), 
                                        null, 
                                        &value);
      if (!status.IsNotFound()) {
        // The key exists, abort
        LOG_ERROR(m_logger, "The nullifier already exists in the database");
        return false;
      }
    }
  }

  // Clear and serialize the new tx
  ss.str(std::string());

  // DONE: Process the tx (The coins are still unspent)
  // Testing: Remove this and see if we can get better throughput
  // REMOVE ALL TX RELATED OPERATIONS FROM POST EXECUTION TO SEE IF THIS IS THE BOTTLENECK
  // for(size_t txoIdx = 0; txoIdx < tx.outs.size(); txoIdx++) {
    // auto sig = tx.shareSignCoin(txoIdx, mParams_->my_sk);
    // ss << sig << std::endl;
  // }

  // Add nullifiers to the DB
  for(auto& null: tx.getNullifiers()) {
    // HACK so that the client can re-use the same coin for testing
    auto num = val.fetch_add(1);
    auto str = null + std::to_string(num);
    LOG_DEBUG(m_logger, "Atomic Pointer Str: " << str);
    client->rawDB().Put(rocksdb::WriteOptions{}, str, str);
  }

  // Setup the response
  // auto response = (SimpleReply_Pay*)outReply;
  // response->header.type = BasicRandomTests::PAY;
  // auto ss_str = ss.str();
  // response->tx_len = ss_str.size();
  // std::memcpy(response->getTxBuf(), ss_str.data(), response->tx_len);

  // Copy the request into response 
  // TODO: RSI Bug
  // outReplicaSpecificInfoSize = response->getRSISize();
  // outReplySize = response->getSize();

  // TODO: RSI Patch
  // // To delete after fixing
  outReplicaSpecificInfoSize = 0;
  outReplySize = 100;
  // std::memcpy(outReply, (uint8_t*)writeReq, outReplySize);
  std::memset(outReply, 0, 100);
  // std::memset(outReply, 0, response->getSize());
  
  #ifdef AB_TESTING_PRE_POST_EXEC_TIME
  auto end = get_monotonic_time();
  std::cout << "Replica PostExec time: " << (end-start) << " ms"<< std::endl;
  #endif
  return true;
}

bool InternalCommandsHandler::executeWriteCommand(uint32_t requestSize,
                                                  const char *request,
                                                  uint64_t sequenceNum,
                                                  uint8_t flags,
                                                  size_t maxReplySize,
                                                  char *outReply,
                                                  uint32_t &outReplySize,
                                                  bool isBlockAccumulationEnabled,
                                                  VersionedUpdates &blockAccumulatedVerUpdates,
                                                  BlockMerkleUpdates &blockAccumulatedMerkleUpdates,
                                                  uint32_t &outReplicaSpecificInfoSize) {
  auto *writeReq = (SimpleCondWriteRequest *)request;
  LOG_INFO(m_logger,
           "Execute WRITE command:"
               << " type=" << writeReq->header.type << " seqNum=" << sequenceNum
               << " numOfWrites=" << writeReq->numOfWrites << " numOfKeysInReadSet=" << writeReq->numOfKeysInReadSet
               << " readVersion=" << writeReq->readVersion
               << " READ_ONLY_FLAG=" << ((flags & MsgFlag::READ_ONLY_FLAG) != 0 ? "true" : "false")
               << " PRE_PROCESS_FLAG=" << ((flags & MsgFlag::PRE_PROCESS_FLAG) != 0 ? "true" : "false")
               << " HAS_PRE_PROCESSED_FLAG=" << ((flags & MsgFlag::HAS_PRE_PROCESSED_FLAG) != 0 ? "true" : "false")
               << " BLOCK_ACCUMULATION_ENABLED=" << isBlockAccumulationEnabled);

  // Insert hook to execute UTT Messages
  if (writeReq->header.type == PAY) {
    return postExecutePay(requestSize, request, sequenceNum, flags, maxReplySize, outReply, outReplySize, outReplicaSpecificInfoSize);
  }

  if (!(flags & MsgFlag::HAS_PRE_PROCESSED_FLAG)) {
    bool result = verifyWriteCommand(requestSize, *writeReq, maxReplySize, outReplySize);
    if (!result) ConcordAssert(0);
    if (flags & MsgFlag::PRE_PROCESS_FLAG) {
      if (writeReq->header.type == LONG_EXEC_COND_WRITE) sleep(LONG_EXEC_CMD_TIME_IN_SEC);
      outReplySize = requestSize;
      memcpy(outReply, request, requestSize);
      return result;
    }
  }

  SimpleKey *readSetArray = writeReq->readSetArray();
  BlockId currBlock = m_storage->getLastBlockId();

  // Look for conflicts
  bool hasConflict = false;
  for (size_t i = 0; !hasConflict && i < writeReq->numOfKeysInReadSet; i++) {
    const auto key = std::string(readSetArray[i].key, KV_LEN);
    const auto latest_ver = getLatestVersion(key);
    hasConflict = (latest_ver && latest_ver > writeReq->readVersion);
    if (isBlockAccumulationEnabled && !hasConflict) {
      if (hasConflictInBlockAccumulatedRequests(key, blockAccumulatedVerUpdates, blockAccumulatedMerkleUpdates)) {
        hasConflict = true;
      }
    }
  }

  if (!hasConflict) {
    if (isBlockAccumulationEnabled) {
      // If Block Accumulation is enabled then blocks are added after all requests are processed
      addKeys(writeReq, sequenceNum, blockAccumulatedVerUpdates, blockAccumulatedMerkleUpdates);
    } else {
      // If Block Accumulation is not enabled then blocks are added after all requests are processed
      VersionedUpdates verUpdates;
      BlockMerkleUpdates merkleUpdates;
      addKeys(writeReq, sequenceNum, verUpdates, merkleUpdates);
      addBlock(verUpdates, merkleUpdates);
    }
  }

  ConcordAssert(sizeof(SimpleReply_ConditionalWrite) <= maxReplySize);
  auto *reply = (SimpleReply_ConditionalWrite *)outReply;
  reply->header.type = COND_WRITE;
  reply->success = (!hasConflict);
  if (!hasConflict)
    reply->latestBlock = currBlock + 1;
  else
    reply->latestBlock = currBlock;

  outReplySize = sizeof(SimpleReply_ConditionalWrite);
  ++m_writesCounter;

  if (!isBlockAccumulationEnabled)
    LOG_INFO(
        m_logger,
        "ConditionalWrite message handled; writesCounter=" << m_writesCounter << " currBlock=" << reply->latestBlock);
  return true;
}

bool InternalCommandsHandler::executeGetBlockDataCommand(
    uint32_t requestSize, const char *request, size_t maxReplySize, char *outReply, uint32_t &outReplySize) {
  auto *req = (SimpleGetBlockDataRequest *)request;
  LOG_INFO(m_logger, "Execute GET_BLOCK_DATA command: type=" << req->h.type << ", BlockId=" << req->block_id);

  auto minRequestSize = std::max(sizeof(SimpleGetBlockDataRequest), req->size());
  if (requestSize < minRequestSize) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize=" << requestSize << ", minRequestSize=" << minRequestSize);
    return false;
  }

  auto block_id = req->block_id;
  const auto updates = getBlockUpdates(block_id);
  if (!updates) {
    LOG_ERROR(m_logger, "GetBlockData: Failed to retrieve block ID " << block_id);
    return false;
  }

  // Each block contains a single metadata key holding the sequence number
  const int numMetadataKeys = 1;
  auto numOfElements = updates->size() - numMetadataKeys;
  size_t replySize = SimpleReply_Read::getSize(numOfElements);
  LOG_INFO(m_logger, "NUM OF ELEMENTS IN BLOCK = " << numOfElements);
  if (maxReplySize < replySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize=" << replySize << ", maxReplySize=" << maxReplySize);
    return false;
  }

  SimpleReply_Read *pReply = (SimpleReply_Read *)(outReply);
  outReplySize = replySize;
  memset(pReply, 0, replySize);
  pReply->header.type = READ;
  pReply->numOfItems = numOfElements;

  auto i = 0;
  for (const auto &[key, value] : *updates) {
    if (key != concord::kvbc::IBlockMetadata::kBlockMetadataKeyStr) {
      memcpy(pReply->items[i].simpleKey.key, key.data(), KV_LEN);
      memcpy(pReply->items[i].simpleValue.value, value.data(), KV_LEN);
      ++i;
    }
  }
  return true;
}

bool InternalCommandsHandler::executeReadCommand(
    uint32_t requestSize, const char *request, size_t maxReplySize, char *outReply, uint32_t &outReplySize) {
  auto *readReq = (SimpleReadRequest *)request;
  LOG_INFO(m_logger,
           "Execute READ command: type=" << readReq->header.type << ", numberOfKeysToRead="
                                         << readReq->numberOfKeysToRead << ", readVersion=" << readReq->readVersion);

  auto minRequestSize = std::max(sizeof(SimpleReadRequest), readReq->getSize());
  if (requestSize < minRequestSize) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize=" << requestSize << ", minRequestSize=" << minRequestSize);
    return false;
  }

  size_t numOfItems = readReq->numberOfKeysToRead;
  size_t replySize = SimpleReply_Read::getSize(numOfItems);

  if (maxReplySize < replySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize=" << replySize << ", maxReplySize=" << maxReplySize);
    return false;
  }

  auto *reply = (SimpleReply_Read *)(outReply);
  outReplySize = replySize;
  reply->header.type = READ;
  reply->numOfItems = numOfItems;

  SimpleKey *readKeys = readReq->keys;
  SimpleKV *replyItems = reply->items;
  for (size_t i = 0; i < numOfItems; i++) {
    memcpy(replyItems->simpleKey.key, readKeys->key, KV_LEN);
    auto value = std::string(KV_LEN, '\0');
    if (readReq->readVersion > m_storage->getLastBlockId()) {
      value = getLatest(std::string(readKeys->key, KV_LEN));
    } else {
      value = getAtMost(std::string(readKeys->key, KV_LEN), readReq->readVersion);
    }
    memcpy(replyItems->simpleValue.value, value.data(), KV_LEN);
    ++readKeys;
    ++replyItems;
  }
  ++m_readsCounter;
  LOG_INFO(m_logger, "READ message handled; readsCounter=" << m_readsCounter);
  return true;
}

bool InternalCommandsHandler::executeGetLastBlockCommand(uint32_t requestSize,
                                                         size_t maxReplySize,
                                                         char *outReply,
                                                         uint32_t &outReplySize) {
  LOG_INFO(m_logger, "GET LAST BLOCK!!!");

  if (requestSize < sizeof(SimpleGetLastBlockRequest)) {
    LOG_ERROR(m_logger,
              "The message is too small: requestSize is " << requestSize << ", required size is "
                                                          << sizeof(SimpleGetLastBlockRequest));
    return false;
  }

  outReplySize = sizeof(SimpleReply_GetLastBlock);
  if (maxReplySize < outReplySize) {
    LOG_ERROR(m_logger, "maxReplySize is too small: replySize=" << outReplySize << ", maxReplySize=" << maxReplySize);
    return false;
  }

  auto *reply = (SimpleReply_GetLastBlock *)(outReply);
  reply->header.type = GET_LAST_BLOCK;
  reply->latestBlock = m_storage->getLastBlockId();
  ++m_getLastBlockCounter;
  LOG_INFO(m_logger,
           "GetLastBlock message handled; getLastBlockCounter=" << m_getLastBlockCounter
                                                                << ", latestBlock=" << reply->latestBlock);
  return true;
}

bool InternalCommandsHandler::executeReadOnlyCommand(uint32_t requestSize,
                                                     const char *request,
                                                     size_t maxReplySize,
                                                     char *outReply,
                                                     uint32_t &outReplySize,
                                                     uint32_t &specificReplicaInfoOutReplySize) {
  auto *requestHeader = (SimpleRequest *)request;
  if (requestHeader->type == READ) {
    return executeReadCommand(requestSize, request, maxReplySize, outReply, outReplySize);
  } else if (requestHeader->type == GET_LAST_BLOCK) {
    return executeGetLastBlockCommand(requestSize, maxReplySize, outReply, outReplySize);
  } else if (requestHeader->type == GET_BLOCK_DATA) {
    return executeGetBlockDataCommand(requestSize, request, maxReplySize, outReply, outReplySize);
  } else {
    outReplySize = 0;
    LOG_ERROR(m_logger, "Illegal message received: requestHeader->type=" << requestHeader->type);
    return false;
  }
}

void InternalCommandsHandler::setPerformanceManager(
    std::shared_ptr<concord::performance::PerformanceManager> perfManager) {
  perfManager_ = perfManager;
}
