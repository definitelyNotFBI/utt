#include <iostream>
#include <iterator>
#include <vector>

#include "assertUtils.hpp"
#include "client/Params.hpp"
#include "replica/Params.hpp"
#include "ThresholdParamGen.hpp"
#include "utt/Factory.h"

namespace utt_bft {

ThresholdParams::ThresholdParams(size_t n, size_t f)
    : factory{f+1, n}, 
    num_nodes_(n),
    initalized_(true)
{
    ConcordAssert(n > 3*f);
    bank_sks_ = factory.getBankShareSKs();
    for(auto i=0ul; i<n;i++) {
        bank_pks_.emplace_back(bank_sks_[i].toPK());
    }
}

std::vector<utt_bft::replica::Params> ThresholdParams::ReplicaParams() const {
    ConcordAssert(this->initalized_);
    std::vector<utt_bft::replica::Params> replica_params;

    for(size_t i=0; i<num_nodes_; i++) {
        replica_params.emplace_back(factory.getParams(), 
                                        bank_pks_, 
                                        factory.getBankPK(),
                                        factory.getRegAuthPK(),
                                        num_nodes_, 
                                        bank_sks_[i]);
    }

    return replica_params;
}

utt_bft::client::Params ThresholdParams::ClientParams() const {
    ConcordAssert(this->initalized_);
    return utt_bft::client::Params{factory.getParams(), 
                                    bank_pks_, factory.getBankPK(), num_nodes_};
}

}