#pragma once 

#include <asio.hpp>
#include <unordered_map>
#include "Crypto.hpp"

namespace BCB::common {

typedef bftEngine::impl::RSAVerifier PublicKey;
typedef std::unordered_map<uint16_t, std::shared_ptr<PublicKey>> PublicKeyMap;

const std::string utt_wallet_file_key = "utt.quickpay.wallet";

const size_t REPLICA_MAX_MSG_SIZE = 20000;

struct MatchedResponse {
    std::unordered_map<uint16_t, std::vector<uint8_t>> responses;
    // std::set<uint16_t> responses_from;

    void add(uint16_t id, uint8_t* data_ptr, size_t bytes) {
        if (responses.count(id)) {
            LOG_WARN(GL, "Got duplicate repsonse from "<< id);
            return;
        }
        std::vector<uint8_t> data;
        data.reserve(bytes);
        memcpy(data.data(), data_ptr, bytes);
        responses.emplace(id, data);
    }

    void remove(uint16_t id) {
        responses.erase(id);
    }

    // Extract and reset the responses
    void clear(std::vector<uint16_t>& id_collection, std::vector<std::vector<uint8_t>>& data_collection) 
    {
        for(auto [id, data]: this->responses) {
            id_collection.push_back(id);
            data_collection.push_back(data);
        }
        responses.clear();
    }

    size_t size() const {
        return responses.size();
    }
};



} // namespace BCB::common