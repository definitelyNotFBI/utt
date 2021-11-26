#pragma once

#include <memory>
#include <string>
// #include "utt/Params.h"
// #include "utt/RegAuth.h"
// #include "utt/Wallet.h"

struct Setup {
    std::string replica_folder;
    std::string replica_prefix = "replica_keys_";
    std::string wallets_folder;
    std::string wallet_prefix = "wallet_";
    size_t num_replicas, num_faults;

    static std::unique_ptr<Setup> ParseArgs(int argc, char* argv[]);
    void makeTx(uint16_t client_id);

private:
    // std::vector<libutt::Wallet> getWallets();
};
