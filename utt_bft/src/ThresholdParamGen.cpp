#include <iostream>
#include <iterator>
#include <vector>

#include "assertUtils.hpp"
#include "client/Params.hpp"
#include "replica/Params.hpp"
#include "ThresholdParamGen.hpp"

namespace utt_bft {

ThresholdParams::ThresholdParams(const libutt::Params& p, size_t n, size_t f)
    : p(p), kg{p, f+1, n}, 
    num_nodes_(n),
    initalized_(true)
{
    ConcordAssert(n > 3*f);
    for(auto i=0ul; i<n;i++) {
        bank_pks_.emplace_back(kg.getShareSK(i).toSharePK(p));
    }
}

std::vector<utt_bft::replica::Params> ThresholdParams::ReplicaParams() const {
    ConcordAssert(this->initalized_);
    std::vector<utt_bft::replica::Params> replica_params;

    for(size_t i=0; i<num_nodes_; i++) {
        replica_params.emplace_back(p, bank_pks_, kg.getPK(p), num_nodes_, kg.getShareSK(i));
    }

    return replica_params;
}

utt_bft::client::Params ThresholdParams::ClientParams() const {
    ConcordAssert(this->initalized_);
    return utt_bft::client::Params{p, bank_pks_, kg.getPK(p), num_nodes_};
}

}