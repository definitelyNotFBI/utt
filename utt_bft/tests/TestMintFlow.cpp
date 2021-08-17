#include <iostream>

#include "utt/Params.h"
#include "ThresholdParamGen.hpp"

int main() {
    using utt_bft::ThresholdParams;

    libutt::initialize(nullptr, 0);
    auto p = libutt::Params::Random();
    size_t n = 31, f = 10;
    size_t t = f+1;
    ThresholdParams tparams(p, n, f);
    auto replica_params = tparams.ReplicaParams();
    auto client_params = tparams.ClientParams();
    
     // The client creates a new coin
    auto esk = libutt::ESK::random();
    auto epk = esk.toEPK(client_params.p);
    long val = 1000;

    for(size_t i=0; i<n;i++) {
        std::stringstream ss;
        ss << epk;
        libutt::EPK epk2(ss);
        testAssertEqual(true, libutt::EpkProof::verify(replica_params[i].p, epk2));
        libutt::CoinComm cc(replica_params[i].p, epk, val);
        libutt::CoinSigShare coin = replica_params[i].my_sk.sign(replica_params[i].p, cc);

        libutt::CoinComm cc2(replica_params[i].p, esk.toEPK(replica_params[i].p), val);
        testAssertEqual(coin.verify(replica_params[i].p, cc2, client_params.bank_pks[i]), true);
    }

    // Now let the servers sign the message
    std::vector<libutt::CoinSigShare> sigShares;
    std::vector<size_t> ids;

    for(size_t i=0; i<t;i++) {
        auto id = (i+10)%n;
        libutt::CoinComm cc(replica_params[id].p, epk, val);
        libutt::CoinSigShare coin = replica_params[id].my_sk.sign(replica_params[id].p, cc);

        std::stringstream ss;
        ss << coin;

        libutt::CoinSigShare coin2(ss);

        testAssertEqual(coin.s1, coin2.s1);
        testAssertEqual(coin.s2, coin2.s2);
        testAssertEqual(coin.s3, coin2.s3);
        
        sigShares.push_back(coin);
        ids.push_back(id);
    }

    // Aggregate
    auto combined_coin = libutt::CoinSig::aggregate(client_params.n, sigShares, ids);
    libutt::CoinComm cc(client_params.p, epk, val);

    // Verify
    testAssertEqual(combined_coin.verify(client_params.p, cc, client_params.main_pk), true);

    std::cout << "All is well!" << std::endl;
    return 0;
}