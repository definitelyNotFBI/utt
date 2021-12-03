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

#pragma pack(push, 1)
struct QuickPayMsg {
    size_t target_shard_id;
    size_t hash_len;
    // unsigned char[hash_len]

    static QuickPayMsg* alloc(size_t hash_len) {
        // TODO: Fix
        return (QuickPayMsg*)malloc(get_size(hash_len));
    }

    static void free(QuickPayMsg* p) {
        delete[] p;
    }

    size_t get_size() const {
        return get_size(hash_len);
    }

    unsigned char* getHashBuf() {
        return (unsigned char*)this + sizeof(QuickPayMsg);
    }

    static size_t get_size(size_t hash_size) {
        return sizeof(QuickPayMsg) + hash_size;
    }

    std::string toString() const {
        std::stringstream ss;
        ss << KVLOG(get_size()) << std::endl ;
        ss << "0x";
        for(size_t i=0; i<get_size(); i++) {
            ss << *((uint8_t*)this)+i;
        }
        ss << std::endl;
        return ss.str();
    }

};
#pragma pack(pop)

#pragma pack(push, 1)
struct QuickPayTx {
    size_t qp_msg_len;
    size_t tx_len;
    // unsigned char* qp_msg_data;
    // unsigned char* tx_data;

    static QuickPayTx* alloc(size_t msg_len, size_t tx_len) {
        auto size = get_size(msg_len, tx_len);
        auto* buf = new uint8_t[size];
        memset(buf, 0, size);
        auto qp = (QuickPayTx*)buf;
        qp->qp_msg_len = msg_len;
        qp->tx_len = tx_len;
        return qp;
    }

    static void free(QuickPayTx* buf) { delete [] buf; }

    QuickPayMsg* getQPMsg() const {
        return (QuickPayMsg*)((uint8_t*)this + sizeof(QuickPayTx));
    }

    unsigned char* getTxBuf() const {
        return (unsigned char*)((uint8_t*)this + sizeof(QuickPayTx) 
                                    + qp_msg_len);
    }

    size_t get_size() const {
        return get_size(qp_msg_len, tx_len);
    }

    static size_t get_size(size_t msg_size, size_t tx_len) {
        return sizeof(QuickPayTx) + msg_size + tx_len;
    }

    std::string toString() const {
        std::stringstream ss;
        ss << KVLOG(get_size()) << std::endl ;
        ss << KVLOG(qp_msg_len) << std::endl ;
        ss << KVLOG(tx_len) << std::endl ;
        ss << KVLOG(getQPMsg()->toString()) << std::endl;
        return ss.str();
    }
};
#pragma pack(pop)

#pragma pack(push, 1)
// A quickpayresponse is a signature on hash of the quickpay tx, along with the origin id
struct QuickPayResponse {
    size_t sig_len;
    size_t qp_msg_len;
    // QuickPayMsg msg;
    // unsigned char* sig;

    static QuickPayResponse* alloc(size_t msg_len, size_t sig_len) {
        auto size = get_size(msg_len, sig_len);
        auto data_ptr = new uint8_t[size];
        memset(data_ptr, 0, size);
        auto qp = (QuickPayResponse*)data_ptr;
        qp->qp_msg_len = msg_len;
        qp->sig_len = sig_len;
        return qp;
    }

    QuickPayMsg* getQPMsg() const {
        return (QuickPayMsg*)((uint8_t*)this + sizeof(QuickPayResponse));
    }

    unsigned char* getSigBuf() const {
        return (unsigned char*)((uint8_t*)this+sizeof(QuickPayResponse)+qp_msg_len);
    }

    size_t get_size() const {
        return get_size(qp_msg_len, sig_len);
    }

    static size_t get_size(size_t msg_size, size_t sig_len) {
        return msg_size + sig_len + sizeof(QuickPayResponse);
    }

    static void free(QuickPayResponse* buf) {
        delete[] buf;
    }

    std::string toString() const {
        std::stringstream ss;
        ss << KVLOG(get_size()) << std::endl;
        ss << "0x";
        for(size_t i=0; i<get_size(); i++) {
            ss << *((uint8_t*)this)+i;
        }
        ss << std::endl;
        return ss.str();
    }
    
};
#pragma pack(pop)

class MintTx;

std::ostream& operator<<(std::ostream& in, const MintTx& tx);
std::istream& operator>>(std::istream& out, MintTx& tx);

// Add a toString() function
class MintTx {
public:
    libutt::Tx tx;
    // Who signed the transaction
    size_t target_shard_id;
    std::unordered_map<uint16_t, std::vector<uint8_t>> sigs;

public:
    MintTx() {}

    MintTx(std::istream& in)    
    {
        in >> *this;
    }
};
