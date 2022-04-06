#pragma once

#include <getopt.h>
#include <unistd.h>
#include <cstddef>
#include <memory>

#include "Crypto.hpp"
#include "Logging4cplus.hpp"
#include "communication/CommDefs.hpp"
#include "config.hpp"
#include "client/Params.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"

namespace sharding::client {

const struct option longOptions[] = {
    {"num-slow",                    required_argument, 0, 'c'},
    {"cert-root",                   required_argument, 0, 'C'},
    {"num-faults",                  required_argument, 0, 'f'},
    {"replica-id",                  required_argument, 0, 'i'},
    {"keys-file-prefix",            required_argument, 0, 'k'},
    {"log-props-file",              required_argument, 0, 'l'},
    {"network-config-file-prefix",  required_argument, 0, 'n'},
    {"numOps",                      required_argument, 0, 'p'},
    {"num-shards",                  required_argument, 0, 'S'},
    {"utt-pub-prefix",              required_argument, 0, 'U'},
    {"wallet-prefix",               required_argument, 0, 'w'},
    {"consensus-concurrency-level", required_argument, 0, 'y'},
    {0, 0, 0, 0}
};

const auto shortOptions = "c:C:f:i:k:l:n:p:S:U:w:y:";

extern int o;
extern int optionIndex;
extern ClientConfig client_config;

class TestSetup {
public:
    TestSetup(logging::Logger logger, 
                std::string& logFile, 
                const std::vector<bft::communication::BaseCommConfig>& conf, 
                const std::string& utt_params_file): 
        m_logPropsFile_{logFile}, m_logger_{logger}, m_sharded_conf_{conf}, m_utt_params_file_{utt_params_file} {}
public:
    static std::unique_ptr<TestSetup> ParseArgs(int argc, char* argv[]);
    std::string getLogProperties() { return m_logPropsFile_; }

    // For sharded setting, all I need for communication is my info at any of the configs, since they are generated in a synchronized fashion
    typedef bft::communication::NodeInfo NodeInfo;
    NodeInfo getMyInfo() { 
        return m_sharded_conf_.at(0).nodes[m_replicaId_]; 
    }

    bft::communication::NodeMap getReplicaInfo(size_t shard_id) { 
        return m_sharded_conf_.at(shard_id).nodes; 
    } 
    std::vector<bft::communication::NodeMap> getReplicaInfo() { 
        std::vector<bft::communication::NodeMap> maps;
        for(size_t i=0; i<m_sharded_conf_.size();i++) {
          auto map = m_sharded_conf_.at(i).nodes; 
          maps.push_back(map);
        }
        return maps;
    } 

    // Get UTT Info
    std::string UttParamsFile() { return m_utt_params_file_; }
private:
    std::string m_logPropsFile_;
    logging::Logger m_logger_;
    uint16_t m_replicaId_;
    std::vector<bft::communication::BaseCommConfig> m_sharded_conf_;
    std::string m_utt_params_file_;
    std::shared_ptr<Cryptosystem> m_crypsys = nullptr;
};

} // namespace quickpay::replica