#include <iostream>

#include "assertUtils.hpp"
#include "utt/Params.h"
#include "replica/Params.hpp"
#include "ThresholdParamGen.hpp"

using namespace utt_bft;

int main() {
    libutt::initialize(nullptr, 0);
    auto p = libutt::Params::random();
    size_t n = 31, f = 10;
    ThresholdParams tparams(n, f);

    // Test Replica Params
    std::stringstream ss;
    auto replica_params = tparams.ReplicaParams();
    auto client_params = tparams.ClientParams();
    for(size_t i=0;i<n;i++) {
        // Test serialization of replica params
        ss.str(std::string());
        ss << replica_params[i];
        auto rp2 = utt_bft::replica::Params(ss);
        ConcordAssertEQ(replica_params[i], rp2);

        // Test serialization of client params from replica params
        ss.str(std::string()); 
        auto client_params2 = rp2.ClientParams();
        ss << client_params2;
        auto cp2 = utt_bft::client::Params(ss);
        ConcordAssertEQ(client_params2, cp2);

        // Test if the client params from replica params is the same as that from the factory
        ConcordAssertEQ(client_params2, client_params);
    }

    std::cout << "All is well" << std::endl;
    return 0;
}