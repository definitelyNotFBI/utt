#include <vector>
#include "common.hpp"
#include "histogram.hpp"
#include "kvstream.h"
#include "misc.hpp"
#include "verifier.hpp"
#include "utt/PolyCrypto.h"

#include "setup.hpp"

#include "asio/thread_pool.hpp"

int main(int argc, char* argv[])
{
    libutt::initialize(nullptr);

    // Process the args
    auto setup = Setup::ParseArgs(argc, argv);

    // DONE: Generate quickpay Txs
    auto batch = setup->makeBatch();
    std::cout << KVLOG(batch.size()) << std::endl;

    auto [priv, publicKeysOfReplicas] = setup->getKeys();
    auto m_params_ptr_ = setup->getUTTParams();
    auto db = setup->getDb();

    LOG_INFO(GL, "Starting the experiment");

    concordUtils::Histogram hist;
    hist.Clear();
    VerifierReplica verifier(*setup, 
        std::move(publicKeysOfReplicas), 
        m_params_ptr_, db); 
    for(size_t iter = 0; iter < setup->iterations; iter++) {
        LOG_INFO(GL, "Iteration " << iter);
        auto start = get_monotonic_time();
        {
            verifier.verifyBatch(batch);
        }
        auto elapsed = double(get_monotonic_time() - start);
        hist.Add(elapsed);
    }
    LOG_INFO(GL, hist.ToString());
    return 0;
}

