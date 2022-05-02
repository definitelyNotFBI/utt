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

enum class MsgType : uint8_t {
    BURN_REQUEST,
    BURN_RESPONSE,
    ACK_MSG,
    ACK_RESPONSE,
};

#pragma pack(push, 1)
struct Msg {
    MsgType tp;
    size_t seq_num;
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

struct QuickPayMsg {
    size_t target_shard_id;
    size_t hash_len;

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
struct AckResponse {
    size_t rsa_len;

    static size_t get_size(size_t rsa_len) {
        return rsa_len + sizeof(AckResponse);
    }

    size_t get_size() const {
        return rsa_len + sizeof(AckResponse);
    }

    uint8_t* getRSABuf() const {
        return ((uint8_t*)this)+sizeof(AckResponse);
    }

};


struct FullPayFTResponse {
    size_t sig_len;
    size_t rsa_len;

    static size_t get_size(size_t sig_len, size_t rsa_len) {
        return sig_len + rsa_len + sizeof(FullPayFTResponse);
    }

    size_t get_size() const {
        return sig_len + rsa_len + sizeof(FullPayFTResponse);
    }

    uint8_t* getSigBuf() const {
        return ((uint8_t*)this)+sizeof(FullPayFTResponse);
    }

    uint8_t* getRSABuf() const {
        return getSigBuf()+sig_len;
    }

};

struct FullPayFTAck {
    typedef size_t ID_TYPE;
    size_t seq_num;
    size_t num_ids;
    size_t num_sigs;
    size_t sig_len;

    static size_t get_size(size_t num_ids, size_t sig_len, size_t num_sigs) {
        return sizeof(FullPayFTAck) + (sizeof(num_ids)*num_ids) + (sig_len*num_sigs);
    }

    size_t get_size() const {
        return get_size(num_ids, sig_len, num_sigs);
    }

    size_t* getIdBuf() const {
        return (size_t*)(((uint8_t*)this)+sizeof(FullPayFTAck));
    }

    uint8_t* getSigBuf(size_t idx) const {
        auto id_buf_ptr = (uint8_t*)getIdBuf();
        auto end_of_id_buf_ptr = id_buf_ptr + (sizeof(size_t)*num_ids);
        return end_of_id_buf_ptr+(idx*sig_len);
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
