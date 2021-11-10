#include <getopt.h>
#include <unistd.h>
#include <memory>

#include "Logging4cplus.hpp"
#include "communication/CommDefs.hpp"
#include "config.hpp"
#include "client/Params.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"

namespace quickpay::client {

const struct option longOptions[] = {
    {"replica-id",                  required_argument, 0, 'i'},
    {"network-config-file",         required_argument, 0, 'n'},
    {"num-slow",                    required_argument, 0, 'c'},
    {"num-faults",                  required_argument, 0, 'f'},
    {"log-props-file",              required_argument, 0, 'l'},
    {"consensus-concurrency-level", required_argument, 0, 'y'},
    {"utt-pub-prefix",              required_argument, 0, 'U'},
    {"genesis-prefix",              required_argument, 0, 'g'},
    {"numOps",                      required_argument, 0, 'p'},
    {0, 0, 0, 0}
};

const auto shortOptions = "i:n:c:f:l:y:U:g:p:";

extern int o;
extern int optionIndex;
extern ClientConfig client_config;

class TestSetup {
public:
    TestSetup(logging::Logger logger, 
                std::string& logFile, 
                const bft::communication::BaseCommConfig& conf, 
                const std::string& utt_params_file): 
        m_logPropsFile_(logFile), m_logger_(logger), m_conf_(conf), m_utt_params_file_(utt_params_file) {}
public:
    static std::unique_ptr<TestSetup> ParseArgs(int argc, char* argv[]);
    logging::Logger getLogger() { return m_logger_; }
    std::string getLogProperties() { return m_logPropsFile_; }

    // For quickpay, all I need for communication is my info
    typedef bft::communication::NodeInfo NodeInfo;
    NodeInfo getMyInfo() { return m_conf_.nodes[m_replicaId_]; }
    bft::communication::NodeMap getReplicaInfo() { return m_conf_.nodes; } 

    // Get UTT Info
    std::string UttParamsFile() { return m_utt_params_file_; }
private:
    std::string m_logPropsFile_;
    logging::Logger m_logger_;
    uint16_t m_replicaId_;
    bft::communication::BaseCommConfig m_conf_;
    std::string m_utt_params_file_;
    std::shared_ptr<Cryptosystem> m_crypsys = nullptr;
};

} // namespace quickpay::replica