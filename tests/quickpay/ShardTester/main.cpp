#include <getopt.h>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <regex>

#include "Crypto.hpp"
#include "Logging4cplus.hpp"
#include "assertUtils.hpp"
#include "msg/QuickPay.hpp"
#include "replica/Params.hpp"
#include "string.hpp"
#include "yaml_utils.hpp"

#include "utt/Tx.h"

typedef bftEngine::impl::RSASigner PrivateKey;
typedef bftEngine::impl::RSAVerifier PublicKey;

struct ReplicaConfig
{
    size_t iterations = 1000;
    size_t batch_size = 100;
    size_t num_threads = std::thread::hardware_concurrency();
    std::string folder_wallet;
    std::string folder_keys;
    std::string folder_utt_keys;
    std::string folder_tx;
    std::string prefix_wallets = "wallet_";
    std::string prefix_keys = "replica_keys_";
    std::string prefix_utt_keys = "utt_pvt_replica_";
    std::string prefix_tx = "tx_";
    uint16_t id;

    size_t num_replicas = 0;
    size_t num_faults = 0;

    size_t start_tx = 0;
    size_t end_tx = 0;
};

std::unique_ptr<ReplicaConfig> config = nullptr;

const struct option longOptions[] = {
    {"iterations",                  required_argument, 0, 'x'},
    {"batch_size",                  required_argument, 0, 'b'},
    {"num-threads",                 required_argument, 0, 'p'},
    {"wallets-folder",              required_argument, 0, 'w'},
    {"replica-keys-folder",         required_argument, 0, 'r'},
    {"utt-keys-folder",             required_argument, 0, 'u'},
    {"tx-folder",                   required_argument, 0, 't'},
    {"wallet-prefix",               required_argument, 0, 'W'},
    {"replica-key-prefix",          required_argument, 0, 'R'},
    {"utt-key-prefix",              required_argument, 0, 'U'},
    {"tx-prefix",                   required_argument, 0, 'T'},
    {"replica-id",                  required_argument, 0, 'i'},
    {"start-tx",                    required_argument, 0, 's'},
    {"end-tx",                      required_argument, 0, 'e'},
    {"num-replicas",                required_argument, 0, 'n'},
    {"num-faults",                  required_argument, 0, 'f'},
    {0, 0, 0, 0}
};

void parseArgs(int argc, char* argv[])
{
    config = std::make_unique<ReplicaConfig>();
    int o, longInd;
    while((o = getopt_long(argc, argv, "x:b:p:w:r:u:t:W:R:U:T:i:s:e:n:f:", 
                            longOptions, &longInd)) != EOF) 
    {
        switch(o) {
        case 'x': {
            config->iterations = std::stoul(std::string(optarg));
        } break;
        case 'b': {
            config->batch_size = std::stoul(std::string(optarg));
        } break;
        case 'p': {
            config->num_threads = std::stoul(std::string(optarg));
        } break;
        case 'w': {
            config->folder_wallet = optarg;
        } break;
        case 'W': {
            config->prefix_wallets = optarg;
        } break;
        case 'r': {
            config->folder_keys = optarg;
        } break;
        case 'R': {
            config->prefix_keys = optarg;
        } break;
        case 'u': {
            config->folder_utt_keys = optarg;
        } break;
        case 'U': {
            config->prefix_utt_keys = optarg;
        } break;
        case 't': {
            config->folder_tx = optarg;
        } break;
        case 'T': {
            config->prefix_tx = optarg;
        } break;
        case 'i': {
            config->id = concord::util::to<decltype(config->id)>
                            (std::string(optarg));
        } break;
        case 's': {
            config->start_tx = std::stoul(std::string(optarg));
        } break;
        case 'e': {
            config->end_tx = std::stoul(std::string(optarg));
        } break;
        case 'n': {
            config->num_replicas = std::stoul(std::string(optarg));
        } break;
        case 'f': {
            config->num_faults = std::stoul(std::string(optarg));
        } break;
        case '?': {
            throw std::runtime_error("Invalid arguments");
        } break;
        default:
            break;
        }
    }
    if(config->folder_keys.empty()) {
        throw std::runtime_error(
            "missing replica keys folder (--replica-keys-folder) or (-r)");
    }
    if(config->folder_wallet.empty()) {
        throw std::runtime_error(
            "missing wallets folder (--wallets-folder) or (-w)");
    }
    if(config->folder_utt_keys.empty()) {
        throw std::runtime_error(
            "missing utt keys folder (--utt-keys-folder) or (-u)");
    }
    if(config->folder_tx.empty()) {
        throw std::runtime_error(
            "missing tx folder (--tx-folder) or (-t)");
    }
    if(config->start_tx == 0) {
        throw std::runtime_error(
            "missing start tx (--start-tx) or (-s)"
        );
    }
    if(config->end_tx == 0) {
        throw std::runtime_error(
            "missing end tx (--end-tx) or (-e)"
        );
    }
    if(config->start_tx > config->end_tx) {
        throw std::runtime_error(
            "start tx > end tx"
        );
    }
    if(config->num_replicas == 0) {
        throw std::runtime_error(
            "missing num replicas (--num-replicas) or (-n)");
    }
    const auto& c = config;
    LOG_INFO(GL, "[Configuration] { "
                    << "iterations: " << c->iterations << ", "
                    << "batch size: " << c->batch_size << ", "
                    << "num threads: " << c->num_threads << ", "
                    << "id: " << c->id << ", "
                    << "replica keys folder: " << c->folder_keys << ", "
                    << "utt keys folder: " << c->folder_utt_keys << ", "
                    << "wallets folder: " << c->folder_wallet << ", "
                    << "tx folder: " << c->folder_tx << ", "
                    << "replica key prefix: " << c->prefix_keys << ", "
                    << "utt key prefix: " << c->prefix_utt_keys << ", "
                    << "wallet prefix: " << c->prefix_wallets << ", "
                    << "tx prefix: " << c->prefix_tx << ", "
                    << "tx range: [" << c->start_tx << "-" << c->end_tx << "]"
                    << " }"
    );
}

#include <sys/stat.h>

long GetFileSize(std::string& filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}


static void validateRSAPublicKey(const std::string& key) {
  const size_t rsaPublicKeyHexadecimalLength = 584;
  if (!(key.length() == rsaPublicKeyHexadecimalLength) && (std::regex_match(key, std::regex("[0-9A-Fa-f]+"))))
    throw std::runtime_error("Invalid RSA public key: " + key);
}

static void validateRSAPrivateKey(const std::string& key) {
  // Note we do not verify the length of RSA private keys because their length
  // actually seems to vary a little in the output; it hovers around 2430
  // characters but often does not exactly match that number.

  if (!std::regex_match(key, std::regex("[0-9A-Fa-f]+"))) 
    throw std::runtime_error("Invalid RSA private key: " + key);
}


std::unordered_map<uint16_t, std::string> getPublicKeysFromFile(
    std::uint16_t num_replicas,
    const std::string& filename, 
    std::uint16_t num_faults)
{
  using namespace concord::util;

  std::ifstream input(filename);
  if (!input.is_open()) throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": can't open ") + filename);

  auto numReplicas = yaml::readValue<std::uint16_t>(input, "num_replicas");
  ConcordAssertEQ(numReplicas, num_replicas);
  // We don't care what this value is
  auto numRoReplicas = yaml::readValue<std::uint16_t>(input, "num_ro_replicas");

  auto fVal = yaml::readValue<std::uint16_t>(input, "f_val");
  ConcordAssertEQ(num_faults, fVal);

  // We don't care what this value is
  auto cVal = yaml::readValue<std::uint16_t>(input, "c_val");

  auto id = yaml::readValue<std::uint16_t>(input, "replica_id");
  ConcordAssertEQ(num_replicas, id);

  // We don't care what this value is
  yaml::readValue<bool>(input, "read-only");

  // Note we validate the number of replicas using 32-bit integers in case
  // (3 * f + 2 * c + 1) overflows a 16-bit integer.
  uint32_t predictedNumReplicas = 3 * (uint32_t)fVal + 2 * (uint32_t)cVal + 1;
  if (predictedNumReplicas != (uint32_t)numReplicas) {
    throw std::runtime_error("num_replicas must be equal to (3 * f_val + 2 * c_val + 1)");
  }

  if (id >= numReplicas + numRoReplicas)
    throw std::runtime_error("replica IDs must be in the range [0, num_replicas + num_ro_replicas]");

  std::vector<std::string> rsaPublicKeys = yaml::readCollection<std::string>(input, "rsa_public_keys");

  if (rsaPublicKeys.size() != numReplicas + numRoReplicas)
    throw std::runtime_error("number of public RSA keys must match num_replicas");

  std::unordered_map<uint16_t, std::string> publicKeysOfReplicas;
  publicKeysOfReplicas.clear();
  for (size_t i = 0; i < numReplicas + numRoReplicas; ++i) {
    validateRSAPublicKey(rsaPublicKeys[i]);
    publicKeysOfReplicas.insert(std::pair<uint16_t, std::string>(i, rsaPublicKeys[i]));
  }

  std::string replicaPrivateKey = 
            yaml::readValue<std::string>(input, "rsa_private_key");
  validateRSAPrivateKey(replicaPrivateKey);
  return publicKeysOfReplicas;
}

int main(int argc, char* argv[])
{
    std::cout << "Namaskara!!" << std::endl;
    // parseArgs(argc, argv);
    config = std::make_unique<ReplicaConfig>();
    config->num_replicas = 4;
    config->end_tx = 5;
    config->folder_keys = "scripts";
    config->folder_utt_keys = "scripts/wallets";
    config->folder_tx = ".";
    config->num_faults = 1;
    config->start_tx = 5;
    config->id = 4;

    // DONE: Load tx
    // std::unordered_map<size_t, MintTx> mint_map;
    // for(size_t i=config->start_tx; i <= config->end_tx; i++) {
    //     std::string fname = config->folder_tx + "/" + config->prefix_tx + std::to_string(i);
    //     LOG_INFO(GL, "Opening: " << fname);
    //     std::ifstream mint_file(fname);
    //     ConcordAssert(mint_file.good()); // Check if we can open the file
    //     MintTx mtx(mint_file);
    // }
    // LOG_INFO(GL, "Loaded mint transactions");

    // // DONE: Load public keys
    // auto pub_key_file = config->folder_keys + "/" + 
    //                         config->prefix_keys + 
    //                         std::to_string(config->num_replicas);
    // LOG_INFO(GL, "Opening public key file: " << pub_key_file);
    // auto publicKeys = getPublicKeysFromFile(config->num_replicas, pub_key_file, config->num_faults);
    // ConcordAssert(publicKeys.size() >= config->num_replicas);
    // LOG_INFO(GL, "Successfully loaded public keys");

    // // TODO: UTT Keys
    auto utt_key_file = config->folder_utt_keys + "/" + 
                            config->prefix_utt_keys + 
                            std::to_string(0);
    LOG_INFO(GL, "Opening utt key file: " << utt_key_file);
    std::ifstream utt_file(utt_key_file);
    ConcordAssert(utt_file.good()); // Check if we can open the file
    auto params = std::make_unique<utt_bft::replica::Params>(utt_file);
    utt_bft::replica::Params p(utt_file);
    LOG_INFO(GL, "Successfully loaded UTT keys");

    // // TODO: Implement Mint Tx Verification
    // for(size_t i=config.start_tx; i<=config.end_tx; i++) {
    //     const MintTx* mint_tx = (const MintTx*)mint_map[i].data();
    //     std::cout << KVLOG(mint_tx->toString());
    //     // TODO: Check whether I am responsible for minting
    //     auto qp_tx = mint_tx->getQPTx();
    //     auto qp_msg = qp_tx->getQPMsg();
    //     ConcordAssertEQ(qp_msg->target_shard_id, 0);
    //     std::stringstream ss(std::string{});
    //     ss.write(reinterpret_cast<const char*>(qp_tx->getTxBuf()), qp_tx->tx_len);

    //     ConcordAssert(ss.good());
    //     libutt::Tx tx;
    //     ss >> tx;
    //     // TODO: Check tx
    // }
    // TODO: Add Threading
    // TODO: Add Iterations
    // TODO: Add Batching
    return 0;
}