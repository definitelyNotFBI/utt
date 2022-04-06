#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <regex>
#include <unordered_map>

#include "Crypto.hpp"
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "bft.hpp"
#include "communication/CommDefs.hpp"
#include "config/test_comm_config.hpp"
#include "string.hpp"
#include "KeyfileIOUtils.hpp"
#include "yaml_utils.hpp"
#include "SigManager.hpp"
#include "common/load_keys.hpp"

#include "options.hpp"
#include "protocol.hpp"
#include "common.hpp"
#include "config.hpp"

namespace sharding::client {

void todo(const std::string& msg) {
    throw std::runtime_error("Todo!()" + msg);
}

int o = 0;
int optionIndex = 0;

const auto COMM_CONFIG_ERROR = "invalid argument for --network-config-file (-U)";

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


Cryptosystem* getKeyFile(const std::string& filename) {
  using namespace concord::util;

  std::ifstream input(filename);
  if (!input.is_open()) throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": can't open ") + filename);

  auto config = ClientConfig::Get();

  config->numReplicas = yaml::readValue<std::uint16_t>(input, "num_replicas");
  config->numRoReplicas = yaml::readValue<std::uint16_t>(input, "num_ro_replicas");
  config->fVal = yaml::readValue<std::uint16_t>(input, "f_val");
  config->cVal = yaml::readValue<std::uint16_t>(input, "c_val");
  auto id = yaml::readValue<std::uint16_t>(input, "replica_id");
  config->isReadOnly = yaml::readValue<bool>(input, "read-only");

  // Note we validate the number of replicas using 32-bit integers in case
  // (3 * f + 2 * c + 1) overflows a 16-bit integer.
  uint32_t predictedNumReplicas = 3 * (uint32_t)config->fVal + 2 * (uint32_t)config->cVal + 1;
  if (predictedNumReplicas != (uint32_t)config->numReplicas)
    throw std::runtime_error("num_replicas must be equal to (3 * f_val + 2 * c_val + 1)");

  if (id >= config->numReplicas + config->numRoReplicas)
    throw std::runtime_error("replica IDs must be in the range [0, num_replicas + num_ro_replicas]");

  std::vector<std::string> rsaPublicKeys = yaml::readCollection<std::string>(input, "rsa_public_keys");

  if (rsaPublicKeys.size() != config->numReplicas + config->numRoReplicas)
    throw std::runtime_error("number of public RSA keys must match num_replicas");

  config->publicKeysOfReplicas.clear();
  for (size_t i = 0; i < config->numReplicas + config->numRoReplicas; ++i) {
    validateRSAPublicKey(rsaPublicKeys[i]);
    config->publicKeysOfReplicas.insert(std::pair<uint16_t, std::string>(i, rsaPublicKeys[i]));
  }

  config->replicaPrivateKey = yaml::readValue<std::string>(input, "rsa_private_key");
  validateRSAPrivateKey(config->replicaPrivateKey);

  if (config->isReadOnly) return nullptr;

  return Cryptosystem::fromConfiguration(input,
                                         "common",
                                         id + 1,
                                         config->thresholdSystemType_,
                                         config->thresholdSystemSubType_,
                                         config->thresholdPrivateKey_,
                                         config->thresholdPublicKey_,
                                         config->thresholdVerificationKeys_);
}


std::unique_ptr<TestSetup> TestSetup::ParseArgs(int argc, char *argv[])
{
    auto client_config = ClientConfig::Get();

    std::string commConfigFilePrefix;
    std::string logPropsFile = "logging.properties";
    std::string utt_params_file;
    std::string wallet_file_prefix;
    std::string cert_root_path = "certs";
    std::string keys_file_prefix;

    std::optional<size_t> num_shard_field = std::nullopt;

    auto logger = logging::getLogger("sharding.bcb.client");
    TestCommConfig testCommConfig(logger);

    while ((o = getopt_long(
                argc, argv, shortOptions, longOptions, &optionIndex)) != EOF) 
    {
        switch (o) {
        case 'i': {
            client_config->id = concord::util::to<std::uint16_t>(std::string(optarg));
        } break;
        case 'n': {
            if (optarg[0] == '-') 
                throw std::runtime_error(COMM_CONFIG_ERROR);
            commConfigFilePrefix = optarg;
        } break;
        case 'c': {
            auto val = concord::util::to<std::uint16_t>(std::string(optarg));
            client_config->setcVal(val);
        } break;
        case 'C': {
            cert_root_path = optarg;
        } break;
        case 'f': {
            auto val = concord::util::to<std::uint16_t>(std::string(optarg));
            client_config->setfVal(val);
        } break;
        case 'S': {
            auto val = concord::util::to<size_t>(std::string(optarg));
            num_shard_field = val;
        } break;
        case 'l': {
            logPropsFile = optarg;
        } break;
        case 'k': {
            keys_file_prefix = optarg;
        } break;
        case 'y': {
          const auto concurrencyLevel = concord::util::to<std::uint16_t>(std::string(optarg));
          if (concurrencyLevel < 1 || concurrencyLevel > 2*std::thread::hardware_concurrency())
            throw std::runtime_error{"invalid argument for --consensus-concurrency-level"};
          client_config->concurrencyLevel = concurrencyLevel;
        } break;
        case 'U': {
            utt_params_file = std::string(optarg);
        } break;
        case 'w': {
            wallet_file_prefix = std::string(optarg);
        } break;
        case 'p': {
            const auto numOps = concord::util::to<std::uint16_t>(std::string(optarg));
            client_config->setnumOfOperations(numOps);
        } break;
        case '?': {
            throw std::runtime_error("invalid arguments");
        } break;
        default:
            break;
        }
    }

    if (utt_params_file.empty()) 
        throw std::runtime_error("missing --utt-pub-prefix (-U)");
    client_config->set(utt_bft::UTT_PARAMS_CLIENT_KEY, utt_params_file);
    
    if (keys_file_prefix.empty())
        throw std::runtime_error("missing --keys-file-prefix (-k)");

    if (!wallet_file_prefix.empty())
        client_config->set(common::utt_wallet_file_key, wallet_file_prefix);

    if(!num_shard_field.has_value()) {
        throw std::runtime_error("missing --num-shards (-S)");
    }
    client_config->num_shards = num_shard_field.value();
    if (commConfigFilePrefix.empty()) {
        throw std::runtime_error("missing --comm-config-prefix (-n)");
    }

    uint16_t numOfClients;
    std::vector<bft::communication::BaseCommConfig> comm_configs;
    comm_configs.reserve(client_config->num_shards);

    for(size_t i=0; i<client_config->num_shards;i++) {
        std::string commConfigFile = commConfigFilePrefix + std::to_string(i);
        std::string certRootPath = cert_root_path + std::to_string(i);

        #ifdef USE_COMM_PLAIN_TCP
        bft::communication::PlainTcpConfig conf =
            testCommConfig.GetTCPConfig(true, replica_config->id, numOfClients, numOfReplicas, commConfigFile);
        #elif USE_COMM_TLS_TCP
        bft::communication::TlsTcpConfig conf = testCommConfig.GetTlsTCPConfig(
            false, client_config->id, numOfClients, client_config->numReplicas, commConfigFile);
        #else
        bft::communication::PlainUdpConfig conf =
            testCommConfig.GetUDPConfig(true, replica_config->id, numOfClients, numOfReplicas, commConfigFile);
        #endif
        comm_configs.push_back(conf);
    }

    auto my_info = comm_configs.at(0).nodes[client_config->getid()];
    LOG_INFO(logger, "My info[" << 
                        "Host: " << my_info.host << 
                        ", Port: " << my_info.port << "]");

    // Get Cryptosystem

    std::string key_file_name = keys_file_prefix + 
        std::to_string(client_config->numReplicas) + "-s" + std::to_string(0);
    auto sys = getKeyFile(key_file_name);
    assert(sys == nullptr);
    typedef bftEngine::impl::RSAVerifier Verifier;
    std::vector<std::unordered_map<uint16_t, std::shared_ptr<Verifier>>> cryps_vec;
    // cryps_vec.reserve(client_config->num_shards);
    for(size_t shard=0;shard<client_config->num_shards;shard++) {
      std::string key_file_name =
          keys_file_prefix + std::to_string(client_config->numReplicas) + "-s" + std::to_string(shard);
      auto cryp = sharding::getKeyFile(key_file_name);
      std::unordered_map<uint16_t, std::shared_ptr<Verifier>> map;
      for(auto& [id,pk]: cryp) {
          std::shared_ptr<Verifier> verif = std::shared_ptr<Verifier>(new Verifier{pk.c_str(), KeyFormat::HexaDecimalStrippedFormat});
          map.emplace(id, verif);
      }
      cryps_vec.push_back(map);
    }
    common::ShardMap map = std::make_shared<common::ShardedVerifMap>(cryps_vec);
    client_config->setpk_map(map);

    return std::make_unique<TestSetup>(logger, 
                                        logPropsFile, 
                                        comm_configs, 
                                        utt_params_file);
}

} // namespace quickpay::replica