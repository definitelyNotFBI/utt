#pragma once

#include <cstddef>
#include <vector>

#include "assertUtils.hpp"
#include "replica/Params.hpp"
#include "utt/Factory.h"
#include "utt/Coin.h"
#include "utt/RandSig.h"
#include "utt/Wallet.h"

namespace utt_bft {

class ThresholdParams {

typedef utt_bft::replica::Params ReplicaParam;
typedef utt_bft::client::Params ClientParam;

public:
    ThresholdParams(size_t n, size_t f);
    std::vector<ReplicaParam> ReplicaParams() const;
    ClientParam ClientParams() const;

    const libutt::RandSigSK& getSK() const {
        ConcordAssert(initalized_);
        return factory.getBankSK();
    }

    std::vector<libutt::Wallet> randomWallets(size_t numWallets, size_t numCoins, size_t maxDenom, size_t budget) {
        return factory.randomWallets(numWallets, numCoins, maxDenom, budget);
    }
private:
    libutt::Factory factory;
    std::vector<libutt::RandSigSharePK> bank_pks_;
    std::vector<libutt::RandSigShareSK> bank_sks_;
    size_t num_nodes_;
    bool initalized_ = false;
};

}