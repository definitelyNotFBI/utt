#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "common.hpp"
#include "msg/QuickPay.hpp"
#include "replica/Params.hpp"
#include "rocksdb/native_client.h"

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

    // Make batches
    std::vector<MintTx> makeBatch();
    std::vector<MintTx> makeBatch(size_t);

    // Get the UTT keys
    std::shared_ptr<utt_bft::replica::Params> getUTTParams(uint16_t rid = 0);

    // Get the keypairs
    std::pair<PrivateKey, std::unordered_map<uint16_t, PublicKey>> getKeys(uint16_t id = 0);

    // Get the database
    typedef concord::storage::rocksdb::NativeClient db_t;
    std::shared_ptr<db_t> getDb();

private:
    // std::vector<libutt::Wallet> getWallets();
    logging::Logger m_logger_ = logging::getLogger("quickpay.tx.gen");
};
