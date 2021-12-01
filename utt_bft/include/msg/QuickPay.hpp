#pragma once

#include <array>
#include <cstdint>
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
};
#pragma pack(pop)

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
};

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
};

// Add a toString() function
struct MintTx {
    size_t qp_tx_len;
    size_t sig_len;
    size_t num_sigs;
    // unsigned char[qp_tx_len]
    // unsigned char[sig_len][num_sigs]

    static size_t get_size(size_t sig_len, size_t num_sigs, size_t qp_tx_len) {
        return sizeof(MintTx) + 
                    qp_tx_len +
                    (sig_len*num_sigs);
    }

    size_t get_size() const {
        return get_size(sig_len, num_sigs, qp_tx_len);
    }

    static void free(MintTx* buf) {
        delete [] buf;
    }

    static MintTx* alloc(size_t sig_len, size_t num_sigs, size_t qp_tx_len) {
        auto size = get_size(sig_len, num_sigs, qp_tx_len);
        auto data_ptr = new uint8_t[size];
        memset(data_ptr, 0, size);
        auto mtx = (MintTx*)data_ptr;
        mtx->sig_len = sig_len;
        mtx->num_sigs = num_sigs;
        mtx->qp_tx_len = qp_tx_len;
        return mtx;
    }

    QuickPayTx* getQPTx() const {
        return (QuickPayTx*)((uint8_t*)this + sizeof(MintTx));
    }

    // TODO: Test this
    unsigned char* getSig(size_t idx) const {
        if(idx >= num_sigs) {
            return nullptr;
        }
        unsigned char* base_ptr = ((unsigned char*)getQPTx()) + qp_tx_len;
        return (base_ptr + (idx*sig_len));
    }
};