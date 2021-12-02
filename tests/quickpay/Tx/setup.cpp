#include <getopt.h>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "kvstream.h"
#include "rocksdb/native_client.h"
#include "setup.hpp"
#include "Crypto.hpp"
#include "Logging4cplus.hpp"
#include "assertUtils.hpp"
#include "sha_hash.hpp"
#include "string.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"
#include "utt/Params.h"
#include "utt/RegAuth.h"
#include "utt/Wallet.h"

#include "common.hpp"

#include "msg/QuickPay.hpp"
#include "yaml_utils.hpp"

const struct option longOptions[] = {
    {"batch-size",                  required_argument, 0, 'b'},
    {"num-faults",                  required_argument, 0, 'f'},
    {"iterations",                  required_argument, 0, 'i'},
    {"num-replicas",                required_argument, 0, 'n'},
    {"replica-keys-folder",         required_argument, 0, 'r'},
    {"replica-keys-prefix",         required_argument, 0, 'R'},
    {"wallets-folder",              required_argument, 0, 'w'},
    {"wallet-prefix",               required_argument, 0, 'W'},
    {0, 0, 0, 0}
};


std::unique_ptr<Setup> Setup::ParseArgs(int argc, char *argv[])
{
    int o = 0;
    int optionIndex = 0;
    Setup setup;
    while((o = getopt_long(argc, argv, "b:f:i:n:r:R:w:W:", 
                            longOptions, &optionIndex)) != EOF) 
    {
        switch(o) {
        case 'b': {
            setup.batch_size = 
                concord::util::to<std::size_t>(std::string(optarg));
        } break;
        case 'f': {
            setup.num_faults = concord::util::to<std::size_t>(std::string(optarg));
        } break;
        case 'i': {
            setup.iterations = 
                concord::util::to<std::size_t>(std::string(optarg));
        } break;
        case 'n': {
            setup.num_replicas = concord::util::to<std::size_t>
                                        (std::string(optarg));
        } break;
        case 'r': {
            setup.replica_folder = optarg;
        } break;
        case 'R': {
            setup.replica_prefix = optarg;
        } break;
        case 'w': {
            setup.wallets_folder = optarg;
        } break;
        case 'W': {
            setup.wallet_prefix = optarg;
        } break;
        case '?': {
            throw std::runtime_error("invalid arguments");
        } break;
        default:
            break;
        }
    }
    if (setup.num_replicas <= 3*setup.num_faults) {
        throw std::runtime_error("n <= 3f");
    }
    if (setup.replica_folder.empty()) {
        throw std::runtime_error("missing --replica-keys-folder (-r) parameter");
    }
    if (setup.wallets_folder.empty()) {
        throw std::runtime_error("missing --wallets-folder (-w) parameter");
    }
    return std::make_unique<Setup>(std::move(setup));
}



MintTx Setup::makeTx(uint16_t client_id)
{
    ConcordAssert(client_id >= num_replicas);
    auto wal_file = wallets_folder + "/" + 
                        wallet_prefix + std::to_string(client_id);
    std::ifstream walfile(wal_file);
    ConcordAssert(walfile.good());

    libutt::Wallet wal1,wal2;
    walfile >> wal1;
    walfile >> wal2;

    MintTx mtx;

    auto pid = wal2.getUserPid();
    mtx.tx = wal1.spendTwoRandomCoins(pid, true);
    mtx.target_shard_id = 0;

    std::unordered_map<uint16_t, std::unique_ptr<PrivateKey>> priv_key_map;

    std::stringstream msg_to_sign;
    msg_to_sign << mtx.tx << std::endl;
    msg_to_sign << mtx.target_shard_id << std::endl;

    // DONE: Collect Responses
    for(size_t i = 0; i < num_faults+1 ; i++) {
        std::string filename = replica_folder + "/" + replica_prefix + std::to_string(i);
        LOG_DEBUG(m_logger_, "Processing file: " << filename << " for replica " << i);
        auto key_str = getKeyFile(filename, num_replicas, num_faults, i);
        auto key = std::make_unique<PrivateKey>(key_str.c_str());
        ConcordAssert(key != nullptr);
        auto sig_len = key->signatureLength();
        size_t actual_sig_len;

        // DONE: Make a response
        mtx.sigs.emplace(i, std::vector<uint8_t>());
        mtx.sigs[i].resize(sig_len);
        key->sign((const char*)msg_to_sign.str().data(), 
                    msg_to_sign.str().size(), 
                    (char*)mtx.sigs[i].data(), 
                    sig_len, 
                    actual_sig_len);
        
        priv_key_map.emplace(i, std::move(key));

        // DONE: Store the responses
        LOG_DEBUG(m_logger_, "Finished processing " << i);
    }
    // DONE: Create MintTx
    return mtx;
}

std::vector<MintTx> Setup::makeBatch()
{
    return makeBatch(batch_size);
}

std::vector<MintTx> Setup::makeBatch(size_t bsize)
{
    std::vector<MintTx> batch;
    batch.reserve(bsize);
    for(size_t i=0; i<bsize;i++) {
        auto mtx = makeTx(num_replicas+i);
        batch.push_back(mtx);
    }
    return batch;
}

std::shared_ptr<utt_bft::replica::Params> Setup::getUTTParams(uint16_t rid)
{
    std::ifstream utt_key_file(wallets_folder + "/utt_pvt_replica_" + 
                                    std::to_string(rid));
    ConcordAssert(utt_key_file.good());
    return std::make_shared<utt_bft::replica::Params>(utt_key_file);
}

std::pair<PrivateKey, std::unordered_map<uint16_t, PublicKey>> 
Setup::getKeys(uint16_t id)
{
    std::string filename = replica_folder + "/" + 
                            replica_prefix + std::to_string(id);
    std::unordered_map<uint16_t, std::string> pub_key_strings;
    auto priv_key = getKeyFile(filename, num_replicas, 
                num_faults, id, pub_key_strings);
    std::unordered_map<uint16_t, PublicKey> publicKeysOfReplicas(num_replicas);
    for(auto&[origin, pkey_str]:pub_key_strings) {
        publicKeysOfReplicas.emplace(origin, pkey_str.c_str());
    }
    return std::make_pair<PrivateKey, std::unordered_map<uint16_t, PublicKey>>(PrivateKey(priv_key.c_str()), std::move(publicKeysOfReplicas));
}

std::shared_ptr<Setup::db_t> Setup::getDb()
{
    auto db_file = "utt-db-shard-bench-test";
    using concord::storage::rocksdb::NativeClient;
    return NativeClient::newClient(db_file, 
                                        false, 
                                        NativeClient::DefaultOptions{});
}

// bool Setup::verifyBatch(const std::vector<MintTx>& batch)
// {
//     return true;
// }
