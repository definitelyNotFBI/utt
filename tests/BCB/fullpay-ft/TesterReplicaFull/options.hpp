#include <getopt.h>
#include <unistd.h>
#include <memory>

#include "Logging4cplus.hpp"
#include "communication/CommDefs.hpp"
#include "config.hpp"
#include "replica/Params.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"

namespace fullpay::ft::replica {

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

class TestSetup {
public:
    TestSetup(std::string& logFile, 
                const bft::communication::BaseCommConfig& conf, 
                std::shared_ptr<Cryptosystem> sys): 
        m_logPropsFile_(logFile), m_replicaId_(ReplicaConfig::Get()->getid()), m_conf_(conf), m_crypsys(sys) {}
public:
    static std::unique_ptr<TestSetup> ParseArgs(int argc, char* argv[]);
    std::string getLogProperties() { return m_logPropsFile_; }

    // For quickpay, all I need for communication is my info
    typedef bft::communication::NodeInfo NodeInfo;
    NodeInfo getMyInfo() { return m_conf_.nodes[m_replicaId_]; }
    std::shared_ptr<Cryptosystem> getCrypto() { return m_crypsys; }

private:
    std::string m_logPropsFile_;
    uint16_t m_replicaId_;
    bft::communication::BaseCommConfig m_conf_;
    std::shared_ptr<Cryptosystem> m_crypsys = nullptr;
};

} // namespace fullpay::replica