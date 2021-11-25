#include <iostream>
#include <libff/common/serialization.hpp>
#include <vector>

#include "assertUtils.hpp"
#include "client/Params.hpp"
#include "utt/Params.h"
#include "replica/Params.hpp"
#include "ThresholdParamGen.hpp"
#include "utt/RandSig.h"
#include "utt/Wallet.h"
#include "utt/Tx.h"

using namespace utt_bft;

int main() {
    libutt::initialize(nullptr, 0);
    auto p = libutt::Params::random();
    size_t n = 31, f = 10, num_clients = 1;
    ThresholdParams tparams(n, f);

    // Test Replica Params
    auto replica_params_tmp = tparams.ReplicaParams();
    std::vector<utt_bft::replica::Params> replica_params;
    auto client_params_tmp = tparams.ClientParams();
    std::vector<client::Params> client_params;
    auto wallets_tmp = tparams.randomWallets(2*num_clients, 2, 100, 10000000);
    std::vector<libutt::Wallet> wallets;
    for(size_t i=0;i<n;i++) {
        // First we store the replica params in a file during the setup
        std::stringstream ss;
        ss << replica_params_tmp[i];
        replica::Params rp(ss);

        // Check if we deserialize correctly
        ConcordAssertEQ(rp, replica_params_tmp[i]);
        replica_params.push_back(rp);
    }

    // Check if the client deserializes correctly
    for(size_t c = 0; c < num_clients; c++) {
        auto idx = 2*c;
        std::stringstream ss;
        ss << wallets_tmp[idx] << std::endl;
        ss << wallets_tmp[idx+1] << std::endl;

        auto wal1 = new libutt::Wallet;
        auto wal2 = new libutt::Wallet;
        ss >> *wal1;
        libff::consume_OUTPUT_NEWLINE(ss);
        ss >> *wal2;
        libff::consume_OUTPUT_NEWLINE(ss);
        ConcordAssertEQ(*wal1, wallets_tmp[idx]);
        ConcordAssertEQ(*wal2, wallets_tmp[idx+1]);

        wallets.push_back(*wal1);
        wallets.push_back(*wal2);

        ss.str("");
        // Check deserialization of public params
        ss << client_params_tmp;
        client::Params cp(ss);
        ConcordAssertEQ(cp, client_params_tmp);
        client_params.push_back(cp);
    }

    for(size_t c=0;c<num_clients;c++) {
        std::stringstream ss;
        auto wallet_idx = 2*c;
        auto recip_id = wallets[wallet_idx+1].getUserPid();
        libutt::Tx tx = wallets[wallet_idx].spendTwoRandomCoins(recip_id, false);
        for(size_t i=0; i<n;i++) {
            auto& rp = replica_params[i];
            // Every replica signs it and sends it
            ss.str("");
            ss << tx;
            libutt::Tx tx2(ss);
            ConcordAssertEQ(tx2, tx);
            ConcordAssertEQ(
                tx2.quickPayValidate(rp.p, rp.main_pk, rp.reg_pk), 
                true);
            ConcordAssertEQ(tx.outs.size(), tx2.outs.size());

            ss.str("");
            std::vector<libutt::RandSigShare> sigs;
            for(size_t j=0; j<tx2.outs.size();j++) {
                auto sig = tx2.shareSignCoin(j, rp.my_sk);
                ss << sig << std::endl;
                sigs.push_back(sig);
            }

            // Client side processing
            for(size_t j=0;j<tx.outs.size();j++) {
                libutt::RandSigShare sig;
                ss >> sig;
                libff::consume_OUTPUT_NEWLINE(ss);
                ConcordAssertEQ(sig, sigs[j]);
                // verify sig
                ConcordAssertEQ(tx.verifySigShare(j, sig, client_params[c].bank_pks[i]), true);
            }
            ConcordAssertEQ(rp.my_sk.toPK(), client_params[c].bank_pks[i]);
        }
    }

    std::cout << "All is well" << std::endl;
    return 0;
}
