// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
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

#include <cassert>
#include <cstring>
#include <unordered_map>
#include "SimpleBCStateTransfer.hpp"
#include "categorization/kv_blockchain.h"

using namespace concord::kvbc;

// This should be the same as test config
const uint32_t kMaxBlockSize = 1024;

namespace bftEngine {

namespace bcst {

struct Block {
  char block[kMaxBlockSize];
  uint32_t actualSize;
  StateTransferDigest digest;
};

class TestAppState : public IAppState {
 public:
  bool hasBlock(uint64_t blockId) const override {
    auto it = blocks_.find(blockId);
    return it != blocks_.end();
  }

  bool getBlock(uint64_t blockId, char* outBlock, uint32_t outBlockMaxSize, uint32_t* outBlockActualSize) override {
    auto it = blocks_.find(blockId);
    if (it == blocks_.end()) return false;
    std::memcpy(outBlock, it->second.block, it->second.actualSize);
    *outBlockActualSize = it->second.actualSize;
    return true;
  };

  std::future<bool> getBlockAsync(uint64_t blockId,
                                  char* outBlock,
                                  uint32_t outBlockMaxSize,
                                  uint32_t* outBlockActualSize) override {
    std::future<bool> future = std::async(
        std::launch::async, [&]() { return getBlock(blockId, outBlock, outBlockMaxSize, outBlockActualSize); });
    return future;
  }

  bool getPrevDigestFromBlock(uint64_t blockId, StateTransferDigest* outPrevBlockDigest) override {
    ConcordAssert(blockId > 0);
    auto it = blocks_.find(blockId - 1);
    if (it == blocks_.end()) return false;
    std::memcpy(outPrevBlockDigest, &it->second.digest, BLOCK_DIGEST_SIZE);
    return true;
  };

  void getPrevDigestFromBlock(const char* blockData,
                              const uint32_t blockSize,
                              StateTransferDigest* outPrevBlockDigest) override {
    auto view = std::string_view{blockData, blockSize};
    const auto rawBlock = categorization::RawBlock::deserialize(view);
    std::memcpy(outPrevBlockDigest, rawBlock.data.parent_digest.data(), BLOCK_DIGEST_SIZE);
  }

  bool putBlock(const uint64_t blockId, const char* block, const uint32_t blockSize, bool lastBlock) override {
    ConcordAssert(blockId < last_block_);
    Block bl;
    computeBlockDigest(blockId, block, blockSize, &bl.digest);
    memcpy(&bl.block, block, blockSize);
    bl.actualSize = blockSize;
    last_block_ = blockId;
    blocks_.emplace(blockId, bl);
    return true;
  }

  std::future<bool> putBlockAsync(uint64_t blockId,
                                  const char* block,
                                  const uint32_t blockSize,
                                  bool lastBlock = true) override {
    std::future<bool> future =
        std::async(std::launch::async, [&]() { return putBlock(blockId, block, blockSize, lastBlock); });
    return future;
  }

  // TODO(AJS): How does this differ from getLastBlockNum?
  uint64_t getLastReachableBlockNum() const override { return last_block_; }
  uint64_t getGenesisBlockNum() const override { return 1; }
  uint64_t getLastBlockNum() const override { return last_block_; };

 private:
  uint64_t last_block_;
  std::unordered_map<uint64_t, Block> blocks_;
};

}  // namespace bcst

}  // namespace bftEngine
