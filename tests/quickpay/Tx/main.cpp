#include <vector>
#include "histogram.hpp"
#include "kvstream.h"
#include "misc.hpp"
#include "utt/PolyCrypto.h"

#include "setup.hpp"

int main(int argc, char* argv[])
{
    libutt::initialize(nullptr);

    // Process the args
    auto setup = Setup::ParseArgs(argc, argv);

    // DONE: Read the wallets
    // DONE: Generate quickpay Txs
    
    std::vector<MintTx> batch;
    batch.reserve(setup->batch_size);
    for(size_t i=0; i<setup->batch_size;i++) {
        auto mtx = setup->makeTx(setup->num_replicas+i);
        batch.push_back(mtx);
    }
    std::cout << KVLOG(batch.size()) << std::endl;
    LOG_INFO(GL, "Starting the experiment");

    concordUtils::Histogram hist;
    for(size_t iter = 0; iter < setup->iterations; iter++) {
        auto start = get_monotonic_time();
        setup->verifyBatch(batch);
        auto elapsed = double(get_monotonic_time() - start);
        hist.Add(elapsed);
    }
    LOG_INFO(GL, hist.ToString());
    return 0;
}

