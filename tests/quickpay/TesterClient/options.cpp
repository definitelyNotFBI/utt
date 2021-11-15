#include <cassert>
#include <cstdint>
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
#include "quickpay/TesterClient/config.hpp"
#include "quickpay/TesterReplica/config.hpp"
#include "string.hpp"
#include "KeyfileIOUtils.hpp"
#include "yaml_utils.hpp"

namespace quickpay::client {

void todo(const std::string& msg) {
    throw std::runtime_error("Todo!()" + msg);
}

int o = 0;
int optionIndex = 0;

const auto COMM_CONFIG_ERROR = "invalid argument for --network-config-file (-U)";

std::unique_ptr<TestSetup> TestSetup::ParseArgs(int argc, char *argv[])
{
    auto client_config = ClientConfig::Get();

    std::string commConfigFile;
    std::string logPropsFile = "logging.properties";
    std::string utt_params_file;
    std::string genesis_file_prefix;

    auto logger = logging::getLogger("quickpay.server");
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
            commConfigFile = optarg;
        } break;
        case 'c': {
            auto val = concord::util::to<std::uint16_t>(std::string(optarg));
            client_config->setcVal(val);
            break;
        }
        case 'f': {
            auto val = concord::util::to<std::uint16_t>(std::string(optarg));
            client_config->setfVal(val);
            break;
        }
        case 'l': {
            logPropsFile = optarg;
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
        case 'g': {
            genesis_file_prefix = std::string(optarg);
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

    uint16_t numOfClients = 1;
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

    auto my_info = conf.nodes[client_config->getid()];
    LOG_INFO(logger, "My info[" << 
                        "Host: " << my_info.host << 
                        ", Port: " << my_info.port << "]");

    return std::make_unique<TestSetup>(logger, 
                                        logPropsFile, 
                                        conf, utt_params_file);
}

} // namespace quickpay::replica