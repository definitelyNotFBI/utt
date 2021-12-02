#include <getopt.h>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "kvstream.h"
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

#include "msg/QuickPay.hpp"
#include "yaml_utils.hpp"

typedef bftEngine::impl::RSASigner PrivateKey;
typedef bftEngine::impl::RSAVerifier PublicKey;

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


std::string getKeyFile(
    const std::string& filename, 
    std::uint16_t num_replicas,
    std::uint16_t num_faults,
    std::uint16_t node_id) 
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
  ConcordAssertEQ(node_id, id);

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
  return replicaPrivateKey;
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

bool Setup::verifyBatch(const std::vector<MintTx>& batch)
{
    return true;
}
