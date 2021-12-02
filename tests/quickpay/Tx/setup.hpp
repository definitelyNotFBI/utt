#pragma once

#include <memory>
#include <string>
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "msg/QuickPay.hpp"

struct Setup {
    std::string replica_folder;
    std::string replica_prefix = "replica_keys_";
    std::string wallets_folder;
    std::string wallet_prefix = "wallet_";
    std::string output_folder = ".";
    std::string output_prefix = "tx_";
    size_t num_replicas, num_faults;
    size_t batch_size = 100, iterations = 100;

    // Parse the arguments
    static std::unique_ptr<Setup> ParseArgs(int argc, char* argv[]);
    
    // Create transactions
    MintTx makeTx(uint16_t);

    bool verifyBatch(const std::vector<MintTx>& batch);

private:
    // std::vector<libutt::Wallet> getWallets();
    logging::Logger m_logger_ = logging::getLogger("quickpay.tx.gen");
};
