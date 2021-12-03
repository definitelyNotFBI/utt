#include <xassert/XAssert.h>
#include <iostream>
#include <sstream>

#include "assertUtils.hpp"
#include "utt/Params.h"
#include "ThresholdParamGen.hpp"
#include "utt/Wallet.h"
#include "utt/Serialization.h"

int main() {
    using utt_bft::ThresholdParams;

    libutt::initialize(nullptr, 0);
    auto p = libutt::Params::random();
    size_t n = 31, f = 10;

    ThresholdParams tparams(n, f);
    auto replica_params = tparams.ReplicaParams();
    auto client_params = tparams.ClientParams();
    
    size_t number_of_wallet_files = 10;
    auto walfiles = new std::stringstream[number_of_wallet_files];
    auto wallets = tparams.randomWallets(number_of_wallet_files, 2, 1, 10000000);
    // What the generateUTTKeys outputs to the file?
    // Later we will check whether the client i reads the same values from gen_file[i]
    // Wallets are already checked for serialization and deserialization by libutt in TestTxn.cpp

    // TODO: Check if the client reads the files correctly
    for (auto i=0ul; i< number_of_wallet_files; i++) {
        walfiles[i] << wallets[i];

        libutt::Wallet wal;
        walfiles[i] >> wal;
        std::stringstream ss;
        ss << wal;

        testAssertEqual(walfiles[i].str(), ss.str());
    }

    // TODO: Client creates the transactions

    // TODO: Server processes the transactions
    
    // TODO: Server responds with the transactions
    
    std::cout << "All is well!" << std::endl;
    return 0;
}