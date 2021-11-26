#include "setup.hpp"
#include <getopt.h>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_map>
#include "assertUtils.hpp"
#include "string.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"
#include "utt/Params.h"
#include "utt/RegAuth.h"
#include "utt/Wallet.h"

#include "msg/QuickPay.hpp"
#include "yaml_utils.hpp"

const struct option longOptions[] = {
    {"num-replicas",                required_argument, 0, 'n'},
    {"num-faults",                  required_argument, 0, 'f'},
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
    while((o = getopt_long(argc, argv, "n:f:r:R:w:W:", 
                            longOptions, &optionIndex)) != EOF) 
    {
        switch(o) {
        case 'n': {
            setup.num_replicas = concord::util::to<std::size_t>
                                        (std::string(optarg));
        } break;
        case 'f': {
            setup.num_faults = concord::util::to<std::size_t>(std::string(optarg));
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


void getKeyFile(
    const std::string& filename, 
    std::unordered_map<uint16_t, std::string>& publicKeysOfReplicas,
    std::string& replicaPrivateKey) 
{
  using namespace concord::util;

  std::ifstream input(filename);
  if (!input.is_open()) throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": can't open ") + filename);

  auto numReplicas = yaml::readValue<std::uint16_t>(input, "num_replicas");
  auto numRoReplicas = yaml::readValue<std::uint16_t>(input, "num_ro_replicas");
  auto fVal = yaml::readValue<std::uint16_t>(input, "f_val");
  auto cVal = yaml::readValue<std::uint16_t>(input, "c_val");
  auto id = yaml::readValue<std::uint16_t>(input, "replica_id");
  auto isReadOnly = yaml::readValue<bool>(input, "read-only");
  (void)isReadOnly;

  // Note we validate the number of replicas using 32-bit integers in case
  // (3 * f + 2 * c + 1) overflows a 16-bit integer.
  uint32_t predictedNumReplicas = 3 * (uint32_t)fVal + 2 * (uint32_t)cVal + 1;
  if (predictedNumReplicas != (uint32_t)numReplicas)
    throw std::runtime_error("num_replicas must be equal to (3 * f_val + 2 * c_val + 1)");

  if (id >= numReplicas + numRoReplicas)
    throw std::runtime_error("replica IDs must be in the range [0, num_replicas + num_ro_replicas]");

  std::vector<std::string> rsaPublicKeys = yaml::readCollection<std::string>(input, "rsa_public_keys");

  if (rsaPublicKeys.size() != numReplicas + numRoReplicas)
    throw std::runtime_error("number of public RSA keys must match num_replicas");

  publicKeysOfReplicas.clear();
  for (size_t i = 0; i < numReplicas + numRoReplicas; ++i) {
    validateRSAPublicKey(rsaPublicKeys[i]);
    publicKeysOfReplicas.insert(std::pair<uint16_t, std::string>(i, rsaPublicKeys[i]));
  }

  replicaPrivateKey = yaml::readValue<std::string>(input, "rsa_private_key");
  validateRSAPrivateKey(replicaPrivateKey);
}

void Setup::makeTx(uint16_t clientId)
{
    ConcordAssert(clientId >= num_replicas);
    auto wal_file = wallets_folder + "/" + 
                        wallet_prefix + std::to_string(clientId);
    std::ifstream walfile(wal_file);
    ConcordAssert(walfile.good());

    libutt::Wallet wal1,wal2;
    walfile >> wal1;
    walfile >> wal2;

    std::cout << wal1 << std::endl;
    std::cout << wal2 << std::endl;

    // auto pid = wal2.getUserPid();
    // auto tx = wal1.spendTwoRandomCoins(pid, true);
    // // tx will be valid
    // auto hash = tx.getHashHex();
    // auto qp_msg_len = QuickPayMsg::get_size(hash.size());
    // // Get sig len
    // TODO: Read private keys
    // TODO: Create MintTx
    // TODO: Write MintTx

    ConcordAssert(true);
}
