#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <libff/common/serialization.hpp>
#include <ostream>
#include <unordered_map>
#include <vector>
#include "common.hpp"
#include "kvstream.h"

#include "utt/RegAuth.h"
#include "utt/Params.h"
#include "utt/Tx.h"

// Can be of two kinds for now
enum class MsgType : uint8_t {
    BURN_REQUEST,
    BURN_RESPONSE,
    MINT_REQUEST,
    MINT_RESPONSE,
};

#pragma pack(push,1)
struct Msg {
    MsgType tp;
    size_t msg_len;

    static size_t get_size(size_t msg_len) {
        return sizeof(Msg)+msg_len;
    }

    size_t get_size() const {
        return get_size(msg_len);
    }

    template<class T>
    T* get_msg() const {
        return (T*)(((uint8_t*)this)+sizeof(Msg));
    }
};

struct ResponseData {
    size_t target_shard_id;
    std::array<uint8_t, 32> txhash;
};

struct BurnMsg {
    size_t target_shard_id;
    size_t tx_len;
    // tx bytes here

    static size_t get_size(size_t tx_len) {
        return sizeof(BurnMsg)+tx_len;
    }

    size_t get_size() const {
        return get_size(tx_len);
    }

    uint8_t* get_tx_buf_ptr() const {
        return ((uint8_t*)this)+sizeof(BurnMsg);
    }
};

struct BurnResponse {
    size_t sig_len;
    
    static size_t get_size(size_t sig_len) {
        return sizeof(BurnResponse) + sig_len;
    }

    size_t get_size() const {
        return get_size(sig_len);
    }

    uint8_t* get_sig_buf_ptr() const {
        return ((uint8_t*)this)+sizeof(BurnResponse);
    }
};

struct MintResponse {
    size_t sig_len;
    static size_t get_size(size_t sig_len) {
        return sizeof(MintResponse) + sig_len;
    }

    size_t get_size() const {
        return get_size(sig_len);
    }

    uint8_t* get_sig_buf_ptr() const {
        return ((uint8_t*)this)+sizeof(MintResponse);
    }
};

struct MintMsg {
    size_t tx_len;
    size_t proof_len;
    // tx bytes here
    // proof bytes here

    static size_t get_size(size_t tx_len, size_t proof_len) {
        return sizeof(MintMsg)+proof_len+tx_len;
    }

    size_t get_size() const {
        return get_size(tx_len, proof_len);
    }

    uint8_t* get_tx_buf_ptr() const {
        return ((uint8_t*)this)+sizeof(MintMsg);
    }

    uint8_t* get_proof_buf_ptr() const {
        return ((uint8_t*)this)+sizeof(MintMsg)+tx_len;
    }
};
#pragma pack(pop)

struct MintProof {
    std::unordered_map<size_t, std::vector<sharding::common::Response>> map;
};

inline bool deep_compare(const MintProof& left, const MintProof& right)
{
    if(left.map.size() != right.map.size()) {
        std::cout << "Map size mismatch" << std::endl;
        return false;
    }
    for(const auto&[shard_id, vec]: left.map) {
        if(right.map.count(shard_id) == 0) {
            std::cout << "Missing map shards" << std::endl;
            return false;
        }
        const auto& rvec = right.map.at(shard_id);
        if(vec.size() != rvec.size()) {
            std::cout << "Vec map size mismatch" << std::endl;
            return false;
        }

        for(size_t i=0;i<vec.size();i++) {
            if(vec[i].id != rvec[i].id) {
                std::cout << "Vec map id mismatch" << vec[i].id 
                    << ", " << rvec[i].id 
                    << std::endl;
                return false;
            }
            if(vec[i].data.size() != rvec[i].data.size()) {
                std::cout << "Vec map data size mismatch" << vec[i].data.size() 
                    << ", " << rvec[i].data.size() 
                    << std::endl;
                return false;
            }
            // memcmp returns 0 if everything is equal
            // So we check if it is non-zero
            if(std::memcmp(vec[i].data.data(), rvec[i].data.data(), vec[i].data.size()) != 0) {
                std::cout << "Vec map data mismatch" << i
                    << std::endl;
                return false;
            }
        }
    }
    return true;
}

inline std::ostream& operator<< (std::ostream& os, const MintProof& proof) {
    os << proof.map.size() << std::endl;
    for(const auto& [shard_id, resp_vec]: proof.map) {
        os << shard_id << std::endl;
        os << resp_vec.size() << std::endl;

        for(const auto& response:resp_vec) {
            os << response.id << std::endl;
            os << response.data.size() << std::endl;
            os.write((const char*)response.data.data(), response.data.size());
        }
    }
    return os;

}

inline std::istream& operator>> (std::istream& in, MintProof& proof) {
    size_t num_shards;
    in >> num_shards;
    libff::consume_OUTPUT_NEWLINE(in);

    for(size_t i=0; i<num_shards;i++) {
        size_t shard_id;
        in >> shard_id;
        libff::consume_OUTPUT_NEWLINE(in);

        if(proof.map.count(shard_id)==0){
            proof.map.emplace(shard_id, std::vector<sharding::common::Response>());
        }

        size_t num_responses;
        in >> num_responses;
        libff::consume_OUTPUT_NEWLINE(in);

        for(size_t j=0;j<num_responses;j++) {
            uint16_t id;
            in >> id;
            libff::consume_OUTPUT_NEWLINE(in);

            size_t resp_len;
            in >> resp_len;
            libff::consume_OUTPUT_NEWLINE(in);

            sharding::common::byte_vec_t data;
            data.resize(resp_len, 0);
            in.read((char*)data.data(), resp_len);
            proof.map[shard_id].push_back(sharding::common::Response{id, data});
        }
    }
    return in;
}