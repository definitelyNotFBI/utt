#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/post.hpp>
#include <iostream>
#include <asio.hpp>
#include <vector>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "config/test_comm_config.hpp"
#include "quickpay/TesterClient/options.hpp"
#include "quickpay/TesterClient/protocol.hpp"
#include "utt/PolyCrypto.h"

int main(int argc, char* argv[])
{
    libutt::initialize(nullptr);

    using namespace quickpay::client;

    auto setup = TestSetup::ParseArgs(argc, argv);
    auto logger = setup->getLogger();

    LOG_INFO(logger, "Namaskara!");
    LOG_INFO(logger, "Starting quickpay server!");
    
    // auto config = ClientConfig::Get();
    auto map = setup->getReplicaInfo();

    // Make io_service
    asio::io_context io_ctx;
    auto client = protocol(io_ctx, map);
    client.start();
    io_ctx.run();
    
    return 0;
}
