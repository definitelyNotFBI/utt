#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <regex>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "bft.hpp"
#include "communication/CommDefs.hpp"
#include "config/test_comm_config.hpp"
#include "options.hpp"
#include "quickpay/TesterReplica/config.hpp"
#include "string.hpp"
#include "KeyfileIOUtils.hpp"
#include "yaml_utils.hpp"

namespace quickpay::replica {

// Forward declaration
Cryptosystem* getKeyFile(const std::string& filename);
static void validateRSAPublicKey(const std::string& key);
static void validateRSAPrivateKey(const std::string& key);

void todo(const std::string& msg) {
    throw std::runtime_error("Todo!()" + msg);
}

int o = 0;
int optionIndex = 0;

const auto KEY_FILE_ERROR = "invalid argument for --key-file-prefix";
const auto COMM_CONFIG_ERROR = "invalid argument for --network-config-file";

std::unique_ptr<TestSetup> TestSetup::ParseArgs(int argc, char *argv[])
{
    auto replica_config = ReplicaConfig::Get();

    std::string keysFilePrefix;
    std::string commConfigFile;
    std::string logPropsFile = "logging.properties";
    std::string certRootPath = "certs";
    std::string utt_params_file;
    replica_config->numOfExternalClients = 80;

    auto logger = logging::getLogger("quickpay.server");
    TestCommConfig testCommConfig(logger);

    while ((o = getopt_long(
                argc, argv, shortOptions, longOptions, &optionIndex)) != EOF) 
    {
        switch (o) {
        case 'i': {
            replica_config->id = concord::util::to<std::uint16_t>(std::string(optarg));
        } break;
        case 'k': {
            if (optarg[0] == '-') 
                throw std::runtime_error(KEY_FILE_ERROR);
            keysFilePrefix = optarg;
        } break;
        case 'n': {
            if (optarg[0] == '-') 
                throw std::runtime_error(COMM_CONFIG_ERROR);
            commConfigFile = optarg;
        } break;
        case 'c': {
          certRootPath = optarg;
          break;
        }
        case 'l': {
            logPropsFile = optarg;
        } break;
        case 'y': {
          const auto concurrencyLevel = concord::util::to<std::uint16_t>(std::string(optarg));
          if (concurrencyLevel < 1 || concurrencyLevel > 2*std::thread::hardware_concurrency())
            throw std::runtime_error{"invalid argument for --consensus-concurrency-level"};
          replica_config->concurrencyLevel = concurrencyLevel;
        } break;
        case 'U': {
            utt_params_file = std::string(optarg);
        } break;
        case '?': {
            throw std::runtime_error("invalid arguments");
        } break;

        default:
            break;
        }
    }

    if (keysFilePrefix.empty()) 
        throw std::runtime_error("missing --key-file-prefix");
    if (utt_params_file.empty()) 
        throw std::runtime_error("missing --utt-prefix");

    std::string key_file_name = keysFilePrefix + std::to_string(replica_config->id);
    auto sys = getKeyFile(key_file_name);
    assert(sys != nullptr);
    auto crypsys = std::shared_ptr<Cryptosystem>(sys);
    // if (sys) bftEngine::CryptoManager::instance(sys);
    // todo("Store cryptomanager instance"); // TODO

    uint16_t numOfReplicas =
        (uint16_t)(3 * replica_config->fVal + 2 * replica_config->cVal + 1 + replica_config->numRoReplicas);
    auto numOfClients =
        replica_config->numOfExternalClients;
    #ifdef USE_COMM_PLAIN_TCP
    bft::communication::PlainTcpConfig conf =
        testCommConfig.GetTCPConfig(true, replica_config->id, numOfClients, numOfReplicas, commConfigFile);
    #elif USE_COMM_TLS_TCP
    bft::communication::TlsTcpConfig conf = testCommConfig.GetTlsTCPConfig(
        true, replica_config->id, numOfClients, numOfReplicas, commConfigFile, certRootPath);
    #else
    bft::communication::PlainUdpConfig conf =
        testCommConfig.GetUDPConfig(true, replica_config->id, numOfClients, numOfReplicas, commConfigFile);
    #endif

    // auto my_info = conf.nodes[replica_config->getid()];
    // LOG_INFO(logger, "My info[" << 
    //                     "Host: " << my_info.host << 
    //                     ", Port: " << my_info.port << "]");

    return std::make_unique<TestSetup>(logger, 
                                        logPropsFile, 
                                        conf, utt_params_file, crypsys);
}

Cryptosystem* getKeyFile(const std::string& filename) {
  using namespace concord::util;

  std::ifstream input(filename);
  if (!input.is_open()) throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": can't open ") + filename);

  auto config = ReplicaConfig::Get();

  config->numReplicas = yaml::readValue<std::uint16_t>(input, "num_replicas");
  config->numRoReplicas = yaml::readValue<std::uint16_t>(input, "num_ro_replicas");
  config->fVal = yaml::readValue<std::uint16_t>(input, "f_val");
  config->cVal = yaml::readValue<std::uint16_t>(input, "c_val");
  config->id = yaml::readValue<std::uint16_t>(input, "replica_id");
  config->isReadOnly = yaml::readValue<bool>(input, "read-only");

  // Note we validate the number of replicas using 32-bit integers in case
  // (3 * f + 2 * c + 1) overflows a 16-bit integer.
  uint32_t predictedNumReplicas = 3 * (uint32_t)config->fVal + 2 * (uint32_t)config->cVal + 1;
  if (predictedNumReplicas != (uint32_t)config->numReplicas)
    throw std::runtime_error("num_replicas must be equal to (3 * f_val + 2 * c_val + 1)");

  if (config->id >= config->numReplicas + config->numRoReplicas)
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
                                         config->id + 1,
                                         config->thresholdSystemType_,
                                         config->thresholdSystemSubType_,
                                         config->thresholdPrivateKey_,
                                         config->thresholdPublicKey_,
                                         config->thresholdVerificationKeys_);
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

} // namespace quickpay::replica