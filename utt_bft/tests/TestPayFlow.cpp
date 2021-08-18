#include <xassert/XAssert.h>
#include <iostream>
#include <sstream>

#include "utt/Params.h"
#include "ThresholdParamGen.hpp"

int main() {
    using utt_bft::ThresholdParams;

    libutt::initialize(nullptr, 0);
    auto p = libutt::Params::Random();
    size_t n = 31, f = 10;
    // size_t t = f+1;
    ThresholdParams tparams(p, n, f);
    auto replica_params = tparams.ReplicaParams();
    auto client_params = tparams.ClientParams();
    
    size_t number_of_genesis_files = 10;
    auto genfile = new std::stringstream[number_of_genesis_files];
    // What the generateUTTKeys outputs to the file?
    // Later we will check whether the client i reads the same values from gen_file[i]
    std::vector<libutt::CoinSecrets> coin_secrets[2];
    std::vector<libutt::CoinSig> coin_sigs[2];
    std::vector<libutt::CoinComm> coin_comms[2];
    for (auto i=0ul; i< number_of_genesis_files; i++) {
        libutt::CoinSecrets cs[2] = {
            libutt::CoinSecrets{p},
            libutt::CoinSecrets{p}
        };
        libutt::CoinComm cc[2] = {
            libutt::CoinComm(p, cs[0]),
            libutt::CoinComm(p, cs[1]),
        };
        libutt::CoinSig csign[2] = {
            tparams.getSK().thresholdSignCoin(p, cc[0], tparams.getU()),
            tparams.getSK().thresholdSignCoin(p, cc[1], tparams.getU()),
        };

        for(auto j=0ul; j<2;j++) {
            std::stringstream test_ss;

            test_ss << cs[j];
            libutt::CoinSecrets test(test_ss);
            testAssertEqual(cs[j], test);

            test_ss.str(std::string());
            test_ss << cc[j];
            libutt::CoinComm test_cc(test_ss);
            testAssertEqual(cc[j], test_cc);
            
            test_ss.str(std::string());
            test_ss << csign[j];
            libutt::CoinSig test_csign(test_ss);
            testAssertEqual(csign[j], test_csign);

            genfile[i] << cs[j];
            genfile[i] << cc[j];
            genfile[i] << csign[j];

            coin_secrets[j].push_back(cs[j]);
            coin_comms[j].push_back(cc[j]);
            coin_sigs[j].push_back(csign[j]);
        }
    }

    // Check if the client reads the files correctly
    for (auto i=0ul; i< number_of_genesis_files; i++) {
        for(auto j=0ul; j<2;j++) {
            libutt::CoinSecrets cs(genfile[i]);
            testAssertEqual(cs, coin_secrets[j][i]);

            libutt::CoinComm cc(genfile[i]);
            testAssertEqual(cc, coin_comms[j][i]);

            libutt::CoinSig csign(genfile[i]);
            testAssertEqual(csign, coin_sigs[j][i]);
        }
    }

    // Client creates the transactions
    // TODO

    // Server processes the transactions
    // TODO
    
    // Server responds with the transactions
    // TODO
    
    std::cout << "All is well!" << std::endl;
    return 0;
}