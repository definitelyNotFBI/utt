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
    std::vector<std::vector<uint8_t>> responses;
    std::vector<uint16_t> ids;

    std::set<uint16_t> responses_from;

    void add(uint16_t id, uint8_t* data_ptr, size_t bytes) {
        if (responses_from.count(id)) {
            LOG_WARN(GL, "Got duplicate repsonse from "<< id);
            return;
        }
        responses_from.insert(id);
        std::vector<uint8_t> data(bytes);
        memcpy(data.data(), data_ptr, bytes);
        responses.push_back(data);
        ids.push_back(id);
    }

    // Extract and reset the responses
    void clear(std::vector<uint16_t>& id_collection, std::vector<std::vector<uint8_t>>& data_collection) {
        responses_from.clear();
        ids.swap(id_collection);
        responses.swap(data_collection);
    }

    size_t size() const {
        return responses_from.size();
    }
};



} // namespace BCB::common