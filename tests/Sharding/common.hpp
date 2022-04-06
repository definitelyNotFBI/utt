#pragma once 

#include <asio.hpp>
#include <unordered_map>
#include "Crypto.hpp"

namespace sharding::common {

typedef std::shared_ptr<std::vector<std::unordered_map<uint16_t, std::shared_ptr<bftEngine::impl::RSAVerifier>>>> ShardMap;
typedef bftEngine::impl::RSAVerifier Verifier;
typedef std::vector<std::unordered_map<uint16_t, std::shared_ptr<Verifier>>> ShardedVerifMap;

inline uint16_t get_responsibility(std::string& nullif, size_t num_shards) 
{
  // Compute the number of bytes we need
  // AB: Assume that the number of shards does not exceed 2^16
  assert(nullif.size() > 6);
  std::stringstream ss;
  ss.str(std::string{});
  auto str = std::string{nullif.substr(2, 4)};
  ss << std::hex << str;
  uint32_t rand;
  ss >> rand;
  rand = (rand % num_shards);
  return rand;
}

typedef bftEngine::impl::RSAVerifier PublicKey;
typedef std::unordered_map<uint16_t, std::shared_ptr<PublicKey>> PublicKeyMap;

const std::string utt_wallet_file_key = "utt.quickpay.wallet";

const size_t REPLICA_MAX_MSG_SIZE = 20000;

typedef std::vector<uint8_t> byte_vec_t;

struct Response {
    uint16_t id;
    byte_vec_t data;
};

struct MatchedResponse {
private:
    std::unordered_map<size_t, std::vector<Response>> response_map;
    std::unordered_map<size_t, std::set<uint16_t>> id_map;
    size_t responses = 0;

public:
    void add(uint16_t id, uint8_t* data_ptr, size_t bytes, size_t shard_id) {
        if (id_map.count(shard_id) == 0) {
            id_map.emplace(shard_id, std::set<uint16_t>{});
        }
        if (id_map[shard_id].count(id) != 0) {
            LOG_WARN(GL, "Got duplicate repsonse from "<< id);
            return;
        }
        responses ++;
        if (response_map.count(shard_id) == 0) {
            response_map.emplace(shard_id, std::vector<Response>{});
        }
        std::vector<uint8_t> data(bytes);
        memcpy(data.data(), data_ptr, bytes);
        response_map[shard_id].push_back(Response{id, data});
    }

    // Extract and reset the responses
    std::unordered_map<size_t, std::vector<Response>> clear() 
    {        
        responses = 0;
        for(auto& [shard, set]: id_map) {
            set.clear();
        }
        std::unordered_map<size_t,std::vector<Response>> blank;
        response_map.swap(blank);
        return blank;
    }

    size_t size() const {
        return responses;
    }
};



} // namespace quickpay::client