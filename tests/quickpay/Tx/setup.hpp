#pragma once

#include <memory>
#include <string>
#include "Logger.hpp"
#include "Logging4cplus.hpp"
// #include "utt/Params.h"
// #include "utt/RegAuth.h"
// #include "utt/Wallet.h"

struct Setup {
    std::string replica_folder;
    std::string replica_prefix = "replica_keys_";
    std::string wallets_folder;
    std::string wallet_prefix = "wallet_";
    std::string output_folder = ".";
    std::string output_prefix = "tx_";
    uint16_t client_id = 0;
    size_t num_replicas, num_faults;

    // Parse the arguments
    static std::unique_ptr<Setup> ParseArgs(int argc, char* argv[]);
    
    // Create transactions
    void makeTx();

private:
    // std::vector<libutt::Wallet> getWallets();
    logging::Logger m_logger_ = logging::getLogger("quickpay.tx.gen");
};
