#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/post.hpp>
#include <iostream>
#include <asio.hpp>
#include <vector>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "quickpay/TesterClient/options.hpp"
#include "quickpay/TesterClient/protocol.hpp"

int main(int argc, char* argv[])
{
    libutt::initialize(nullptr);

    using namespace quickpay::client;

    auto setup = TestSetup::ParseArgs(argc, argv);
    logging::initLogger(setup->getLogProperties());

    auto logger = logging::getLogger("quickpay.client");

    LOG_INFO(logger, "Namaskara!");
    LOG_INFO(logger, "Starting quickpay server!");

    // Make io_service
    asio::io_context io_ctx;

    auto map = setup->getReplicaInfo();
    auto client = std::make_shared<protocol>(io_ctx, map);
    client->start();
    io_ctx.run();
    
    return 0;
}
