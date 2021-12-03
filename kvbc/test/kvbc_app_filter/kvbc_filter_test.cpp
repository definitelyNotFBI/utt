// Concord
//
// Copyright (c) 2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include <boost/detail/endian.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <cassert>
#include <exception>
#include <memory>
#include <string>
#include <vector>
#include "Logger.hpp"
#include "categorization/updates.h"
#include "endianness.hpp"
#include "gtest/gtest.h"
#include "kv_types.hpp"
#include "memorydb/client.h"
#include "memorydb/key_comparator.h"
#include "openssl_crypto.hpp"
#include "status.hpp"
#include "concord_kvbc.pb.h"

#include <cassert>
#include <exception>
#include <memory>
#include <string>
#include <vector>
#include "kvbc_app_filter/kvbc_app_filter.h"
#include "storage/test/storage_test_common.h"

using com::vmware::concord::kvbc::ValueWithTrids;

using boost::lockfree::spsc_queue;
using concord::kvbc::BlockId;
using concord::kvbc::EventGroupId;
using concord::kvbc::InvalidBlockRange;
using concord::kvbc::InvalidEventGroupRange;
using concord::kvbc::KvbAppFilter;
using concord::kvbc::KvbFilteredUpdate;
using concord::kvbc::KvbFilteredEventGroupUpdate;
using concord::kvbc::KvbUpdate;
using concord::kvbc::EgUpdate;
using concord::util::openssl_utils::computeSHA256Hash;

namespace {

constexpr auto kLastBlockId = BlockId{150};
static inline const std::string kGlobalEgIdKey{"_global_eg_id"};

std::string CreateTridKvbValue(const std::string &value, const std::vector<std::string> &trid_list) {
  ValueWithTrids proto;
  proto.set_value(value);
  for (const auto &trid : trid_list) {
    proto.add_trid(trid);
  }

  size_t size = proto.ByteSizeLong();
  std::string ret(size, '\0');
  proto.SerializeToArray(ret.data(), ret.size());

  return ret;
}

concord::kvbc::categorization::Event convertToEvent(std::string &&value, std::set<std::string> &&tags) {
  concord::kvbc::categorization::Event event;
  event.data = std::move(value);
  for (auto it = tags.begin(); it != tags.end();) {
    event.tags.emplace_back(tags.extract(it++).value());
  }
  return event;
}

void addEventToEventGroup(concord::kvbc::categorization::Event &&event,
                          concord::kvbc::categorization::EventGroup &event_group) {
  event_group.events.emplace_back(std::move(event));
}

class FakeStorage : public concord::kvbc::IReader {
 public:
  std::vector<KvbUpdate> data_;
  std::vector<EgUpdate> eg_data_;
  // given trid as key, the map returns the latest trid_event_group_id
  std::map<std::string, std::string> latest_trid_eg_id;
  // given trid_event_group_id as key, the map returns the global_event_group_id
  std::map<std::string, std::string> trid_event_group_id;

  std::optional<concord::kvbc::categorization::Value> get(const std::string &category_id,
                                                          const std::string &key,
                                                          BlockId block_id) const override {
    ADD_FAILURE() << "get() should not be called by this test";
    return {};
  }

  std::optional<concord::kvbc::categorization::Value> getLatest(const std::string &category_id,
                                                                const std::string &key) const override {
    if (category_id == concord::kvbc::categorization::kExecutionEventGroupIdsCategory) {
      // get latest trid event_group_id
      EXPECT_FALSE(latest_trid_eg_id.find(key) == latest_trid_eg_id.end());
      if (latest_trid_eg_id.find(key) == latest_trid_eg_id.end()) {
        throw std::runtime_error("kExecutionEventGroupIdsCategory" + key);
      }
      return concord::kvbc::categorization::VersionedValue{{4, latest_trid_eg_id.at(key)}};
    } else if (category_id == concord::kvbc::categorization::kExecutionTridEventGroupsCategory) {
      // get global event_group_id corresponding to trid event_group_id
      if (trid_event_group_id.find(key) == trid_event_group_id.end())
        throw std::runtime_error("kExecutionTridEventGroupsCategory" + key);
      return concord::kvbc::categorization::ImmutableValue{{4, trid_event_group_id.at(key)}};
    } else if (category_id == concord::kvbc::categorization::kExecutionGlobalEventGroupsCategory) {
      // get event group
      std::vector<uint8_t> output;
      if (concordUtils::fromBigEndianBuffer<uint64_t>(key.data()) - 1 >= eg_data_.size())
        throw std::runtime_error("kExecutionTridEventGroupsCategory" + key);
      auto event_group_input = eg_data_.at(concordUtils::fromBigEndianBuffer<uint64_t>(key.data()) - 1).event_group;
      concord::kvbc::categorization::serialize(output, event_group_input);
      return concord::kvbc::categorization::ImmutableValue{{4, std::string(output.begin(), output.end())}};
    } else {
      ADD_FAILURE() << "getLatest() should not be called by this test";
      return {};
    }
  }

  void multiGet(const std::string &category_id,
                const std::vector<std::string> &keys,
                const std::vector<BlockId> &versions,
                std::vector<std::optional<concord::kvbc::categorization::Value>> &values) const override {
    ADD_FAILURE() << "multiGet() should not be called by this test";
    values = {};
  }

  void multiGetLatest(const std::string &category_id,
                      const std::vector<std::string> &keys,
                      std::vector<std::optional<concord::kvbc::categorization::Value>> &values) const override {
    ADD_FAILURE() << "multiGetLatest() should not be called by this test";
    values = {};
  }

  std::optional<concord::kvbc::categorization::TaggedVersion> getLatestVersion(const std::string &category_id,
                                                                               const std::string &key) const override {
    ADD_FAILURE() << "getLatestVersion() should not be called by this test";
    return {};
  }

  void multiGetLatestVersion(
      const std::string &category_id,
      const std::vector<std::string> &keys,
      std::vector<std::optional<concord::kvbc::categorization::TaggedVersion>> &versions) const override {
    ADD_FAILURE() << "multiGetLatestVersion() should not be called by this test";
  }

  std::optional<concord::kvbc::categorization::Updates> getBlockUpdates(BlockId block_id) const override {
    if (block_id >= 0 && block_id < blockId_) {
      auto data = data_.at(block_id).immutable_kv_pairs;
      concord::kvbc::categorization::Updates updates{};
      concord::kvbc::categorization::ImmutableUpdates immutable{};
      for (auto &[k, v] : data.kv) {
        std::string key(k);
        std::set<std::string> tags(v.tags.begin(), v.tags.end());
        concord::kvbc::categorization::ImmutableUpdates::ImmutableValue value{std::move(v.data), std::move(tags)};
        immutable.addUpdate(std::move(key), std::move(value));
      }
      updates.add(concord::kvbc::categorization::kExecutionEventsCategory, std::move(immutable));
      return {updates};
    }
    // The actual storage implementation (ReplicaImpl.cpp) expects us to
    // handle an invalid block range; let's simulate this here.
    ADD_FAILURE() << "Provide a valid block range " << block_id << " " << blockId_;
    return {};
  }

  BlockId getGenesisBlockId() const override {
    ADD_FAILURE() << "get() should not be called by this test";
    return 0;
  }

  BlockId getLastBlockId() const override { return blockId_; }

  // Dummy method to fill DB for testing, each client id can watch only the
  // block id that equals to their client id.
  void fillWithData(BlockId num_of_blocks) {
    blockId_ = num_of_blocks;
    std::string key{"Key"};
    for (BlockId i = 0; i <= num_of_blocks; i++) {
      concord::kvbc::categorization::ImmutableValueUpdate data;
      data.data = "TridVal" + std::to_string(i);
      data.tags.push_back(std::to_string(i));
      concord::kvbc::categorization::ImmutableInput input;
      input.kv.insert({concordUtils::toBigEndianStringBuffer(i) + key, data});
      data_.push_back({i, "cid", std::move(input)});
    }
  }

  // Dummy method to fill DB for testing, each client id can watch only the
  // event group id that corresponds to their client id.
  void fillWithEventGroupData(EventGroupId num_of_egs) {
    blockId_ = num_of_egs;
    for (EventGroupId i = 1; i <= num_of_egs; i++) {
      concord::kvbc::categorization::Event event;
      // trid ∈ {trid_1, trid_2}
      const std::string trid = "trid_" + std::to_string(i % 2 + 1);
      event.data = trid + "_val" + std::to_string(i);
      event.tags.push_back(trid);
      concord::kvbc::categorization::EventGroup event_group;
      event_group.events.emplace_back(event);
      if (latest_trid_eg_id[trid].empty()) {
        uint64_t id = 1;
        latest_trid_eg_id[trid] = concordUtils::toBigEndianStringBuffer(id);
      } else {
        auto latest_id = concordUtils::fromBigEndianBuffer<uint64_t>(latest_trid_eg_id[trid].data());
        latest_trid_eg_id[trid] = concordUtils::toBigEndianStringBuffer(++latest_id);
      }
      latest_trid_eg_id[kGlobalEgIdKey] = concordUtils::toBigEndianStringBuffer(i);

      trid_event_group_id[trid + "#" + latest_trid_eg_id[trid]] = concordUtils::toBigEndianStringBuffer(i);
      eg_data_.push_back({i, std::move(event_group)});
    }
  }

 private:
  BlockId blockId_{kLastBlockId};
};

// Helper function to test cases involving computation of expected hash
// value(s).
std::string blockIdToByteStringLittleEndian(const BlockId &block_id) {
#ifdef BOOST_LITTLE_ENDIAN
  return std::string(reinterpret_cast<const char *>(&block_id), sizeof(block_id));
#else  // BOOST_LITTLE_ENDIAN not defined in this case
#ifndef BOOST_BIG_ENDIAN
  static_assert(false,
                "Cannot determine endianness (needed for computing expected "
                "Thin Replica hash values).");
#endif  // BOOST_BIG_ENDIAN defined
  const char *block_id_as_bytes = reinterpret_cast<const char *>(block_id);
  string block_id_little_endian;
  for (size_t i = 1; i <= sizeof(block_id); ++i) {
    block_id_little_endian += *(block_id_as_bytes + sizeof(block_id) - i);
  }
  return block_id_little_endian;
#endif  // BOOST_LITTLE_ENDIAN defined/else
}

// Prefix the key with the immutable index to make it unique and ordered.
inline std::string prefixImmutableKey(const std::string &key, uint64_t immutable_index) {
  return concordUtils::toBigEndianStringBuffer(immutable_index) + key;
}

TEST(kvbc_filter_test, kvbfilter_update_success) {
  FakeStorage storage;
  int client_id = 1;
  const auto block_id = BlockId{1};
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  auto val_trid1 = CreateTridKvbValue("TridVal1", {"0", "1"});
  auto val_trid2 = CreateTridKvbValue("TridVal2", {"0"});

  std::string key1{"Key1"};
  std::string key2{"Key2"};
  concord::kvbc::categorization::ImmutableInput immutable{};
  concord::kvbc::categorization::ImmutableValueUpdate value1;
  value1.data = val_trid1;
  value1.tags = {"0", "1"};
  concord::kvbc::categorization::ImmutableValueUpdate value2;
  value2.data = val_trid2;
  value2.tags = {"0"};
  immutable.kv.insert({prefixImmutableKey(key1, block_id), value1});
  immutable.kv.insert({prefixImmutableKey(key2, block_id), value2});

  const auto &filtered = kvb_filter.filterUpdate({block_id, "cid", immutable});

  EXPECT_EQ(filtered.block_id, block_id);
  EXPECT_EQ(filtered.kv_pairs.size(), 1);
  for (auto &[k, v] : filtered.kv_pairs) {
    EXPECT_EQ(k, "Key1");
    EXPECT_EQ(v, "TridVal1");
  }
}

TEST(kvbc_filter_test, kvbfilter_update_success_eg) {
  FakeStorage storage;
  int client_id = 1;
  const auto eg_id = EventGroupId{1};
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  auto val_trid1 = CreateTridKvbValue("TridVal1", {"0", "1"});
  auto val_trid2 = CreateTridKvbValue("TridVal2", {"0"});

  concord::kvbc::categorization::EventGroup event_group{};
  concord::kvbc::categorization::Event event1;
  event1.data = val_trid1;
  event1.tags = {"0", "1"};
  concord::kvbc::categorization::Event event2;
  event2.data = val_trid2;
  event2.tags = {"0"};
  event_group.events.emplace_back(event1);
  event_group.events.emplace_back(event2);

  const auto &filtered = kvb_filter.filterEventGroupUpdate({eg_id, event_group});

  EXPECT_EQ(filtered.event_group_id, eg_id);
  EXPECT_EQ(filtered.event_group.events.size(), 1);
  EXPECT_EQ(filtered.event_group.events[0].data, "TridVal1");
}

TEST(kvbc_filter_test, kvbfilter_update_message_empty_prefix) {
  FakeStorage storage;
  int client_id = 0;
  const BlockId block_id = BlockId{1};
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  auto val_trid1 = CreateTridKvbValue("TridVal", {"0"});

  std::string key1{"Rey"};
  std::string key2{"Key"};
  concord::kvbc::categorization::ImmutableInput immutable{};
  concord::kvbc::categorization::ImmutableValueUpdate value1;
  value1.data = val_trid1;
  value1.tags = {"0"};
  concord::kvbc::categorization::ImmutableValueUpdate value2 = value1;
  immutable.kv.insert({prefixImmutableKey(key1, block_id), value1});
  immutable.kv.insert({prefixImmutableKey(key2, block_id), value2});

  const auto &filtered = kvb_filter.filterUpdate({block_id, "cid", immutable});

  EXPECT_EQ(filtered.block_id, block_id);
  const auto &filtered_kv = filtered.kv_pairs;
  EXPECT_EQ(filtered_kv.size(), 2);
  auto it1 = std::find(filtered_kv.begin(), filtered_kv.end(), std::pair<std::string, std::string>{"Rey", "TridVal"});
  EXPECT_NE(it1, filtered_kv.end());
  auto it2 = std::find(filtered_kv.begin(), filtered_kv.end(), std::pair<std::string, std::string>{"Key", "TridVal"});
  EXPECT_NE(it2, filtered_kv.end());
}

TEST(kvbc_filter_test, kvbfilter_update_client_has_no_trids) {
  FakeStorage storage;
  int client_id = 1;
  const auto block_id = BlockId{1};
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  std::string key1{"Rey"};
  std::string key2{"Key"};
  concord::kvbc::categorization::ImmutableInput immutable{};
  concord::kvbc::categorization::ImmutableValueUpdate value1;
  value1.data = "TridVal";
  value1.tags = {"0"};
  concord::kvbc::categorization::ImmutableValueUpdate value2 = value1;
  immutable.kv.insert({prefixImmutableKey(key1, block_id), value1});
  immutable.kv.insert({prefixImmutableKey(key2, block_id), value2});

  const auto &filtered = kvb_filter.filterUpdate({block_id, "cid", immutable});

  EXPECT_EQ(filtered.block_id, block_id);
  EXPECT_EQ(filtered.kv_pairs.size(), 0);
}

TEST(kvbc_filter_test, kvbfilter_update_client_has_no_trids_eg) {
  FakeStorage storage;
  int client_id = 1;
  const auto eg_id = EventGroupId{1};
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  concord::kvbc::categorization::EventGroup event_group{};
  concord::kvbc::categorization::Event event1;
  event1.data = "TridVal";
  event1.tags = {"0"};
  concord::kvbc::categorization::Event event2;
  event2.data = event1.data;
  event_group.events.emplace_back(event1);
  event_group.events.emplace_back(event2);

  const auto &filtered = kvb_filter.filterEventGroupUpdate({eg_id, event_group});

  EXPECT_EQ(filtered.event_group_id, eg_id);
  EXPECT_EQ(filtered.event_group.events.size(), 0);
}

TEST(kvbc_filter_test, kvbfilter_hash_update_success) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  std::string key{"Restringy"};
  std::string key1{"Key"};
  KvbFilteredUpdate::OrderedKVPairs kv{};
  ValueWithTrids proto;
  proto.add_trid("0");
  proto.set_value("TridVal");
  std::string buf = proto.SerializeAsString();
  kv.push_back({key, buf});
  kv.push_back({key1, buf});

  BlockId block_id = 0;
  auto hash_val = kvb_filter.hashUpdate({block_id, "cid", kv});

  // make hash value for check
  std::string concatenated_entry_hashes;
  concatenated_entry_hashes += blockIdToByteStringLittleEndian(block_id);
  std::map<std::string, std::string> entry_hashes;
  entry_hashes[computeSHA256Hash(key.data(), key.length())] = computeSHA256Hash(buf.data(), buf.length());
  entry_hashes[computeSHA256Hash(key1.data(), key1.length())] = computeSHA256Hash(buf.data(), buf.length());
  for (const auto &kvp_hashes : entry_hashes) {
    concatenated_entry_hashes += kvp_hashes.first;
    concatenated_entry_hashes += kvp_hashes.second;
  }

  EXPECT_EQ(hash_val, computeSHA256Hash(concatenated_entry_hashes));
}

TEST(kvbc_filter_test, kvbfilter_hash_update_success_eg) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  KvbFilteredEventGroupUpdate::EventGroup event_group{};
  concord::kvbc::categorization::Event event{};
  ValueWithTrids proto;
  proto.add_trid("0");
  proto.set_value("TridVal");
  std::string buf = proto.SerializeAsString();
  event.data.assign(buf);
  event_group.events.emplace_back(event);

  EventGroupId eg_id = 0;
  auto hash_val = kvb_filter.hashEventGroupUpdate({eg_id, event_group});

  // make hash value for check
  std::string concatenated_entry_hashes;
  concatenated_entry_hashes += blockIdToByteStringLittleEndian(eg_id);
  std::set<std::string> entry_hashes;
  entry_hashes.emplace(computeSHA256Hash(buf.data(), buf.length()));
  for (const auto &event_hash : entry_hashes) {
    concatenated_entry_hashes += event_hash;
  }

  EXPECT_EQ(hash_val, computeSHA256Hash(concatenated_entry_hashes));
}

TEST(kvbc_filter_test, kvbfilter_success_get_blocks_in_range) {
  FakeStorage storage;
  size_t client_id = 123;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));
  storage.fillWithData(kLastBlockId);

  BlockId block_id_start = 0;
  BlockId block_id_end = 10;
  KvbFilteredUpdate temporary;
  spsc_queue<KvbFilteredUpdate> queue_out{storage.getLastBlockId()};
  std::atomic_bool stop_exec = false;
  kvb_filter.readBlockRange(block_id_start, block_id_end, queue_out, stop_exec);

  std::vector<KvbFilteredUpdate> filtered_kv_pairs;
  while (queue_out.pop(temporary)) {
    filtered_kv_pairs.push_back(temporary);
  }

  EXPECT_EQ(filtered_kv_pairs.size(), block_id_end - block_id_start + 1);
  for (size_t i = 0; i < filtered_kv_pairs.size(); i++) {
    const auto &x = filtered_kv_pairs.at(i);
    if (i != client_id)
      EXPECT_EQ(x.kv_pairs.size(), 0);
    else
      EXPECT_EQ(x.kv_pairs.size(), 1);
  }
}

TEST(kvbc_filter_test, kvbfilter_success_get_blocks_in_range_eg) {
  FakeStorage storage;
  std::string client_id("trid_1");
  auto kvb_filter = KvbAppFilter(&storage, client_id);
  size_t num_event_groups_to_fill = 20;
  storage.fillWithEventGroupData(num_event_groups_to_fill);

  EventGroupId eg_id_start = 1;
  EventGroupId eg_id_end = 10;
  KvbFilteredEventGroupUpdate temporary;
  spsc_queue<KvbFilteredEventGroupUpdate> queue_out{num_event_groups_to_fill};
  std::atomic_bool stop_exec = false;
  kvb_filter.readEventGroupRange(eg_id_start, eg_id_end, queue_out, stop_exec);

  std::vector<KvbFilteredEventGroupUpdate> filtered_event_groups;
  while (queue_out.pop(temporary)) {
    filtered_event_groups.push_back(temporary);
  }

  EXPECT_EQ(filtered_event_groups.size(), eg_id_end - eg_id_start + 1);
  for (size_t i = 0; i < filtered_event_groups.size(); i++) {
    const auto &x = filtered_event_groups.at(i);
    for (auto event : x.event_group.events) {
      EXPECT_EQ(event.tags[0], client_id);
    }
  }
}

TEST(kvbc_filter_test, kvbfilter_stop_exec_in_the_middle) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));
  storage.fillWithData(1000);
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  KvbFilteredUpdate temporary;
  spsc_queue<KvbFilteredUpdate> queue_out{storage.getLastBlockId()};
  std::atomic_bool stop_exec = false;
  auto kvb_reader = std::async(std::launch::async,
                               &KvbAppFilter::readBlockRange,
                               kvb_filter,
                               0,
                               kLastBlockId,
                               std::ref(queue_out),
                               std::ref(stop_exec));
  while (queue_out.read_available() < 5) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (end < std::chrono::steady_clock::now()) ConcordAssert(false);
  }
  stop_exec = true;
  kvb_reader.get();
  int num_of_blocks = 0;
  while (queue_out.pop(temporary)) {
    num_of_blocks++;
  }
  EXPECT_GT(num_of_blocks, 0);
  EXPECT_LE(num_of_blocks, 1000);
}

TEST(kvbc_filter_test, kvbfilter_stop_exec_in_the_middle_eg) {
  FakeStorage storage;
  std::string client_id("trid_2");
  auto kvb_filter = KvbAppFilter(&storage, client_id);
  size_t num_event_groups_to_fill = 1000;
  storage.fillWithEventGroupData(num_event_groups_to_fill);
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  KvbFilteredEventGroupUpdate temporary;
  spsc_queue<KvbFilteredEventGroupUpdate> queue_out{num_event_groups_to_fill};
  std::atomic_bool stop_exec = false;
  EventGroupId eg_id_start = 1;
  EventGroupId eg_id_end = 100;
  auto kvb_reader = std::async(std::launch::async,
                               &KvbAppFilter::readEventGroupRange,
                               kvb_filter,
                               eg_id_start,
                               eg_id_end,
                               std::ref(queue_out),
                               std::ref(stop_exec));
  while (queue_out.read_available() < 5) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (end < std::chrono::steady_clock::now()) ConcordAssert(false);
  }
  stop_exec = true;
  kvb_reader.get();
  int num_of_egs = 0;
  while (queue_out.pop(temporary)) {
    num_of_egs++;
  }
  EXPECT_GT(num_of_egs, 0);
  EXPECT_LE(num_of_egs, 1000);
}

TEST(kvbc_filter_test, kvbfilter_block_out_of_range) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));
  BlockId block_id = kLastBlockId + 5;
  KvbFilteredUpdate temporary;
  spsc_queue<KvbFilteredUpdate> queue_out{storage.getLastBlockId()};
  std::atomic_bool stop_exec = false;
  EXPECT_THROW(kvb_filter.readBlockRange(block_id, block_id, queue_out, stop_exec);, InvalidBlockRange);
}

TEST(kvbc_filter_test, kvbfilter_event_group_out_of_range_eg) {
  FakeStorage storage;
  std::string client_id("trid_1");
  auto kvb_filter = KvbAppFilter(&storage, client_id);
  size_t num_event_groups_to_fill = 5;
  storage.fillWithEventGroupData(num_event_groups_to_fill);
  EventGroupId eg_id = 10;
  KvbFilteredEventGroupUpdate temporary;
  spsc_queue<KvbFilteredEventGroupUpdate> queue_out{10};
  std::atomic_bool stop_exec = false;
  EXPECT_THROW(kvb_filter.readEventGroupRange(eg_id, eg_id, queue_out, stop_exec);, InvalidEventGroupRange);
}

TEST(kvbc_filter_test, kvbfilter_start_block_greater_then_end_block) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));
  storage.fillWithData(kLastBlockId);
  BlockId block_id_end = 0;
  BlockId block_id_start = 10;
  KvbFilteredUpdate temporary;
  spsc_queue<KvbFilteredUpdate> queue_out{storage.getLastBlockId()};
  std::atomic_bool stop_exec = false;
  EXPECT_THROW(kvb_filter.readBlockRange(block_id_start, block_id_end, queue_out, stop_exec);, InvalidBlockRange);
}

TEST(kvbc_filter_test, kvbfilter_start_eg_greater_then_end_eg) {
  FakeStorage storage;
  std::string client_id("trid_1");
  auto kvb_filter = KvbAppFilter(&storage, client_id);
  size_t num_event_groups_to_fill = 10;
  storage.fillWithEventGroupData(num_event_groups_to_fill);
  EventGroupId eg_id_end = 1;
  EventGroupId eg_id_start = 10;
  KvbFilteredEventGroupUpdate temporary;
  spsc_queue<KvbFilteredEventGroupUpdate> queue_out{num_event_groups_to_fill};
  std::atomic_bool stop_exec = false;
  EXPECT_THROW(kvb_filter.readEventGroupRange(eg_id_start, eg_id_end, queue_out, stop_exec);, InvalidEventGroupRange);
}

TEST(kvbc_filter_test, kvbfilter_success_hash_of_blocks_in_range) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));
  storage.fillWithData(kLastBlockId);

  // add Trid 1 to watch on another value -> now Trid 1 watches on 2 values
  std::string key1 = {"Key"};
  concord::kvbc::categorization::ImmutableInput immutable{};
  concord::kvbc::categorization::ImmutableValueUpdate value1;
  value1.data = "TridVal2";
  value1.tags = {"1", "2"};
  immutable.kv.insert({prefixImmutableKey(key1, 2), value1});

  storage.data_.at(2) = {2, "cid", immutable};

  BlockId block_id_start = 0;
  BlockId block_id_end = 10;
  KvbFilteredUpdate temporary;
  spsc_queue<KvbFilteredUpdate> queue_out{storage.getLastBlockId()};
  std::atomic_bool stop_exec = false;
  kvb_filter.readBlockRange(block_id_start, block_id_end, queue_out, stop_exec);
  std::vector<KvbFilteredUpdate> filtered_kv_pairs;
  while (queue_out.pop(temporary)) {
    filtered_kv_pairs.push_back(temporary);
  }

  auto hash_value = kvb_filter.readBlockRangeHash(block_id_start, block_id_end);
  std::string concatenated_update_hashes;
  for (BlockId i = block_id_start; i <= block_id_end; ++i) {
    concatenated_update_hashes += kvb_filter.hashUpdate(filtered_kv_pairs.at(i));
  }

  EXPECT_EQ(hash_value, computeSHA256Hash(concatenated_update_hashes));
}

TEST(kvbc_filter_test, kvbfilter_success_hash_of_event_groups_in_range_eg) {
  FakeStorage storage;
  std::string client_id("trid_1");
  auto kvb_filter = KvbAppFilter(&storage, client_id);
  size_t num_event_groups_to_fill = 100;
  storage.fillWithEventGroupData(num_event_groups_to_fill);

  EventGroupId eg_id_start = 1;
  EventGroupId eg_id_end = 10;
  KvbFilteredEventGroupUpdate temporary;
  spsc_queue<KvbFilteredEventGroupUpdate> queue_out{storage.getLastBlockId()};
  std::atomic_bool stop_exec = false;
  kvb_filter.readEventGroupRange(eg_id_start, eg_id_end, queue_out, stop_exec);
  std::vector<KvbFilteredEventGroupUpdate> filtered_egs;
  while (queue_out.pop(temporary)) {
    filtered_egs.push_back(temporary);
  }

  auto hash_value = kvb_filter.readEventGroupRangeHash(eg_id_start, eg_id_end);
  EXPECT_EQ(filtered_egs.size(), 10);
  std::string concatenated_update_hashes;
  for (EventGroupId i = eg_id_start - 1; i < eg_id_end; ++i) {
    concatenated_update_hashes += kvb_filter.hashEventGroupUpdate(filtered_egs.at(i));
  }

  EXPECT_EQ(hash_value, computeSHA256Hash(concatenated_update_hashes));
}

TEST(kvbc_filter_test, kvbfilter_success_hash_of_block) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));
  storage.fillWithData(kLastBlockId);
  BlockId block_id_start = 1;

  auto hash_value = kvb_filter.readBlockHash(block_id_start);
  const auto &[block_id, _, filtered] = kvb_filter.filterUpdate(storage.data_[1]);

  std::string concatenated_entry_hashes;
  concatenated_entry_hashes += blockIdToByteStringLittleEndian(block_id);
  std::map<std::string, std::string> entry_hashes;
  for (const auto &[key, value] : filtered) {
    entry_hashes[computeSHA256Hash(key.data(), key.length())] = computeSHA256Hash(value.data(), value.length());
  }
  for (const auto &kvp_hashes : entry_hashes) {
    concatenated_entry_hashes += kvp_hashes.first;
    concatenated_entry_hashes += kvp_hashes.second;
  }

  EXPECT_EQ(hash_value, computeSHA256Hash(concatenated_entry_hashes));
}

TEST(kvbc_filter_test, kvbfilter_success_hash_of_event_group) {
  FakeStorage storage;
  std::string client_id("trid_1");
  auto kvb_filter = KvbAppFilter(&storage, client_id);
  size_t num_event_groups_to_fill = 100;
  storage.fillWithEventGroupData(num_event_groups_to_fill);
  EventGroupId eg_id_start = 1;

  auto hash_value = kvb_filter.readEventGroupHash(eg_id_start);
  const auto &[eg_id, filtered] = kvb_filter.filterEventGroupUpdate(storage.eg_data_[0]);

  EXPECT_EQ(eg_id, 1);
  std::string concatenated_entry_hashes;
  concatenated_entry_hashes += blockIdToByteStringLittleEndian(eg_id);
  std::set<std::string> entry_hashes;
  for (const auto &event : filtered.events) {
    entry_hashes.emplace(computeSHA256Hash(event.data));
  }
  for (const auto &event_hash : entry_hashes) {
    concatenated_entry_hashes += event_hash;
  }

  EXPECT_EQ(hash_value, computeSHA256Hash(concatenated_entry_hashes));
}

TEST(kvbc_filter_test, kvbfilter_hash_filter_block_out_of_range) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));
  storage.fillWithData(kLastBlockId);
  BlockId block_id_start = 1;
  BlockId block_id_end = kLastBlockId + 5;

  EXPECT_THROW(kvb_filter.readBlockRangeHash(block_id_start, block_id_end);, InvalidBlockRange);
}

TEST(kvbc_filter_test, kvbfilter_hash_filter_event_group_out_of_range) {
  FakeStorage storage;
  std::string client_id("trid_1");
  auto kvb_filter = KvbAppFilter(&storage, client_id);
  size_t num_event_groups_to_fill = 100;
  storage.fillWithEventGroupData(num_event_groups_to_fill);
  EventGroupId eg_id_start = 1;
  EventGroupId eg_id_end = 100;

  EXPECT_THROW(kvb_filter.readEventGroupRangeHash(eg_id_start, eg_id_end);, InvalidEventGroupRange);
}

TEST(kvbc_filter_test, kvbfilter_update_empty_kv_pair) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  concord::kvbc::categorization::ImmutableInput immutable{};
  BlockId block_id = 0;

  const auto &[bid, _, filtered] = kvb_filter.filterUpdate({block_id, "cid", immutable});

  EXPECT_EQ(bid, block_id);
  EXPECT_EQ(filtered.size(), 0);
}

TEST(kvbc_filter_test, kvbfilter_update_empty_event_group) {
  FakeStorage storage;
  std::string client_id("trid_1");
  auto kvb_filter = KvbAppFilter(&storage, client_id);

  concord::kvbc::categorization::EventGroup event_group{};
  EventGroupId eg_id = 1;

  const auto &[egid, filtered] = kvb_filter.filterEventGroupUpdate({eg_id, event_group});

  EXPECT_EQ(egid, eg_id);
  EXPECT_EQ(filtered.events.size(), 0);
}

TEST(kvbc_filter_test, updates_order) {
  FakeStorage storage;
  int client_id = 1;
  auto kvb_filter = KvbAppFilter(&storage, std::to_string(client_id));

  std::vector<std::string> order{"z", "o", "a", "c"};
  ValueWithTrids proto;
  proto.add_trid("1");
  proto.set_value("imm_value");
  std::string buf = proto.SerializeAsString();

  uint64_t immutable_index = 0;
  concord::kvbc::categorization::ImmutableUpdates immutables;
  immutable_index++;
  immutables.addUpdate(prefixImmutableKey("z", immutable_index),
                       concord::kvbc::categorization::ImmutableUpdates::ImmutableValue{std::string(buf), {"1", "2"}});
  immutable_index++;
  immutables.addUpdate(prefixImmutableKey("o", immutable_index),
                       concord::kvbc::categorization::ImmutableUpdates::ImmutableValue{std::string(buf), {"1", "2"}});
  immutable_index++;
  immutables.addUpdate(prefixImmutableKey("a", immutable_index),
                       concord::kvbc::categorization::ImmutableUpdates::ImmutableValue{std::string(buf), {"1", "2"}});
  immutable_index++;
  immutables.addUpdate(prefixImmutableKey("c", immutable_index),
                       concord::kvbc::categorization::ImmutableUpdates::ImmutableValue{std::string(buf), {"1", "2"}});

  concord::kvbc::categorization::Updates updates;
  concord::kvbc::categorization::VersionedUpdates internal;
  immutables.calculateRootHash(true);
  if (immutables.getData().kv.size() > 0)
    updates.add(concord::kvbc::categorization::kExecutionEventsCategory, std::move(immutables));
  auto imm_var_updates = updates.categoryUpdates(concord::kvbc::categorization::kExecutionEventsCategory);
  EXPECT_TRUE(imm_var_updates != std::nullopt);

  auto &imm_updates = std::get<concord::kvbc::categorization::ImmutableInput>((*imm_var_updates).get());

  auto ordered_kv_pairs = kvb_filter.filterKeyValuePairs(imm_updates);
  EXPECT_EQ(ordered_kv_pairs.size(), order.size());

  for (size_t i = 0; i < order.size(); ++i) {
    EXPECT_EQ(ordered_kv_pairs[i].first, order[i]);
  }
}

TEST(kvbc_filter_test, event_order_in_event_groups) {
  FakeStorage storage;
  std::string client_id("trid_1");
  auto kvb_filter = KvbAppFilter(&storage, client_id);

  std::vector<std::string> order{"z", "o", "a", "c"};

  concord::kvbc::categorization::EventGroup event_group;
  EventGroupId event_group_id = 0;
  for (auto val : order) {
    concord::kvbc::categorization::Event event = convertToEvent(std::move(val), {"1", "2"});
    addEventToEventGroup(std::move(event), event_group);
  }

  concord::kvbc::categorization::ImmutableUpdates event_group_immutables;
  // serialize event group
  std::vector<uint8_t> output;
  concord::kvbc::categorization::serialize(output, event_group);
  auto serialized_event_group = std::string(output.begin(), output.end());
  concord::kvbc::categorization::ImmutableUpdates::ImmutableValue imm_value{std::move(serialized_event_group),
                                                                            std::set<std::string>{}};
  event_group_id++;
  event_group_immutables.addUpdate(concordUtils::toBigEndianStringBuffer(event_group_id), std::move(imm_value));

  concord::kvbc::categorization::Updates updates;
  concord::kvbc::categorization::VersionedUpdates internal;
  event_group_immutables.calculateRootHash(true);
  if (event_group_immutables.getData().kv.size() > 0)
    updates.add(concord::kvbc::categorization::kExecutionGlobalEventGroupsCategory, std::move(event_group_immutables));
  auto imm_var_updates = updates.categoryUpdates(concord::kvbc::categorization::kExecutionGlobalEventGroupsCategory);
  EXPECT_TRUE(imm_var_updates != std::nullopt);

  auto &imm_updates = std::get<concord::kvbc::categorization::ImmutableInput>((*imm_var_updates).get());
  uint64_t global_eg_id = 1;
  ASSERT_TRUE(imm_updates.kv.find(concordUtils::toBigEndianStringBuffer(global_eg_id)) != imm_updates.kv.end());
  const std::string event_group_ser = imm_updates.kv.at(concordUtils::toBigEndianStringBuffer(global_eg_id)).data;
  const std::vector<uint8_t> input_vec(event_group_ser.begin(), event_group_ser.end());
  concord::kvbc::categorization::EventGroup event_group_out;
  concord::kvbc::categorization::deserialize(input_vec, event_group_out);
  ASSERT_TRUE(event_group_out.events.size() == 4);

  int i = 0;
  for (const auto &event : event_group_out.events) {
    EXPECT_EQ(event.data, order[i]);
    i++;
  }
}

}  // anonymous namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
