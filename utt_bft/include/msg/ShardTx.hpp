#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <libff/common/serialization.hpp>
#include <ostream>
#include <unordered_map>
#include <vector>
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

struct MintMsg {
    size_t tx_len;
    // tx bytes here

    static size_t get_size(size_t tx_len) {
        return sizeof(BurnMsg)+tx_len;
    }

    size_t get_size() const {
        return get_size(tx_len);
    }

    uint8_t* get_tx_buf_ptr() const {
        return ((uint8_t*)this)+sizeof(MintMsg);
    }
};
#pragma pack(pop)