#pragma once

#include <cstddef>
#include <vector>

#include "assertUtils.hpp"
#include "replica/Params.hpp"

namespace utt_bft {

class ThresholdParams {

typedef utt_bft::replica::Params ReplicaParam;
typedef utt_bft::client::Params ClientParam;

public:
    ThresholdParams(const libutt::Params& p, size_t n, size_t f);
    std::vector<ReplicaParam> ReplicaParams() const;
    ClientParam ClientParams() const;
    libutt::BankSK getSK() const {
        ConcordAssert(initalized_);
        return kg.sk;
    }
    libutt::Fr getU() const {
        ConcordAssert(initalized_);
        return kg.u;
    }
private:
    libutt::Params p;
    libutt::BankThresholdKeygen kg;
    std::vector<libutt::BankSharePK> bank_pks_;
    size_t num_nodes_;
    bool initalized_ = false;
};

}