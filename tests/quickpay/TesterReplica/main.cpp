#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/post.hpp>
#include <iostream>
#include <asio.hpp>
#include <vector>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "config/test_comm_config.hpp"
#include "options.hpp"
#include "quickpay/TesterClient/config.hpp"
#include "quickpay/TesterReplica/config.hpp"
#include "quickpay/TesterReplica/protocol.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"

int main(int argc, char* argv[])
{
    libutt::initialize(nullptr);

    using namespace quickpay::replica;

    auto setup = TestSetup::ParseArgs(argc, argv);
    logging::initLogger(setup->getLogProperties());
    auto logger = logging::getLogger("quickpay.replica");

    LOG_INFO(logger, "Namaskara!");
    LOG_INFO(logger, "Starting quickpay server!");

    auto my_info = setup->getMyInfo();
    LOG_INFO(logger, "My info[" << 
                        "Host: " << my_info.host << 
                        ", Port: " << my_info.port << "]");
    
    auto config = ReplicaConfig::Get();
    auto port_num = setup->getMyInfo().port;

    // Make io_service
    asio::io_context io_ctx;
    auto server = std::make_shared<protocol>(io_ctx, port_num, 
                                                setup->getCrypto());
    
    // Start and make threads available to the io_service
    config->concurrencyLevel = std::thread::hardware_concurrency();
    int num_threads = config->getconcurrencyLevel();
    
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    for (auto i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&io_ctx]() {
            io_ctx.run();
        });
    }
    

    // Wait for completion
    for (auto & thread : threads)
    {
        thread.join();
    }

    return 0;
}
