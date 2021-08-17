#include <iostream>

#include "assertUtils.hpp"
#include "utt/Params.h"
#include "replica/Params.hpp"
#include "ThresholdParamGen.hpp"

using namespace utt_bft;

int main() {
    libutt::initialize(nullptr, 0);
    auto p = libutt::Params::Random();
    size_t n = 31, t = 10;
    ThresholdParams tparams(p, n, t);

    // Test Replica Params
    std::stringstream ss;
    auto replica_params = tparams.ReplicaParams();
    auto client_params = tparams.ClientParams();
    for(size_t i=0;i<n;i++) {
        ss << replica_params[i];
        auto rp2 = utt_bft::replica::Params(ss);
        ConcordAssertEQ(replica_params[i], rp2);
        ss.str(std::string()); 

        auto client_params2 = rp2.ClientParams();
        ss << client_params2;
        auto cp2 = utt_bft::client::Params(ss);
        ConcordAssertEQ(client_params2, cp2);
        ConcordAssertEQ(client_params2, client_params);
        ss.str(std::string());
    }

    // Test Serialization of tx
    ss.str(std::string());
    

    std::cout << "All is well" << std::endl;

}