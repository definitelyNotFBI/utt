#include <getopt.h>
#include <unistd.h>
#include <memory>

#include "Logging4cplus.hpp"
#include "communication/CommDefs.hpp"
#include "config.hpp"
#include "replica/Params.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"

namespace quickpay::replica {

const struct option longOptions[] = {
    {"replica-id",                  required_argument, 0, 'i'},
    {"key-file-prefix",             required_argument, 0, 'k'},
    {"network-config-file",         required_argument, 0, 'n'},
    {"cert-root-path",              required_argument, 0, 'c'},
    {"log-props-file",              required_argument, 0, 'l'},
    {"consensus-concurrency-level", required_argument, 0, 'y'},
    {"utt-prefix",                  required_argument, 0, 'U'},
    {0, 0, 0, 0}
};

const auto shortOptions = "i:k:n:c:l:y:U:";

extern int o;// = 0;
extern int optionIndex;// = 0;
extern ReplicaConfig replica_config;

class TestSetup {
public:
    TestSetup(logging::Logger logger, 
                std::string& logFile, 
                const bft::communication::BaseCommConfig& conf, 
                const std::string& utt_params_file,
                std::shared_ptr<Cryptosystem> sys): 
        m_logPropsFile_(logFile), m_logger_(logger), m_replicaId_(ReplicaConfig::Get()->getid()), m_conf_(conf), m_utt_params_file_(utt_params_file), m_crypsys(sys) {}
public:
    static std::unique_ptr<TestSetup> ParseArgs(int argc, char* argv[]);
    logging::Logger getLogger() { return m_logger_; }
    std::string getLogProperties() { return m_logPropsFile_; }

    // For quickpay, all I need for communication is my info
    typedef bft::communication::NodeInfo NodeInfo;
    NodeInfo getMyInfo() { return m_conf_.nodes[m_replicaId_]; }

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