#include <iostream>

#include "utt/Params.h"
#include "ThresholdParamGen.hpp"

int main() {
    using utt_bft::ThresholdParams;

    libutt::initialize(nullptr, 0);
    libutt::Params p;
    size_t n = 31, f = 10;
    size_t t = f+1;
    ThresholdParams tparams(p, n, f);
    auto replica_params = tparams.ReplicaParams();
    auto client_params = tparams.ClientParams();
    
    // Generate genesis coins
    

    std::cout << "All is well!" << std::endl;
    return 0;
}