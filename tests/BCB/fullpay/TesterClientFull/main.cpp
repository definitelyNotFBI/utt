#include <iostream>
#include <fstream>
#include <vector>

#include <asio.hpp>
#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/post.hpp>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "bft.hpp"
#include "client/Params.hpp"
#include "quickpay/TesterClient/options.hpp"
#include "protocol.hpp"
// #include "protocol.hpp"
#include "quickpay/TesterClient/config.hpp"

int main(int argc, char* argv[])
{
    libutt::initialize(nullptr);

    // using namespace fullpay::client;

    auto logger = logging::getLogger("fullpay.client");

    auto setup = quickpay::client::TestSetup::ParseArgs(argc, argv, logger);
    logging::initLogger(setup->getLogProperties());

    LOG_INFO(logger, "Namaskara!");
    LOG_INFO(logger, "Starting fullpay client!");

    // Construct libutt params
    auto config = ClientConfig::Get();
    auto utt_params_filename = config->get(utt_bft::UTT_PARAMS_CLIENT_KEY, std::string(""));
    LOG_INFO(logger, "Opening " << utt_params_filename);

    std::ifstream utt_params_file(utt_params_filename);
    if (utt_params_file.fail()) {
        LOG_FATAL(logger, "Failed to open " << utt_params_filename);
        throw std::runtime_error("Error opening utt params file");
    }
    utt_bft::client::Params params(utt_params_file);
    utt_params_file.close();

    // Make io_service
    asio::io_context io_ctx;

    auto map = setup->getReplicaInfo();
    auto client = std::make_shared<fullpay::client::protocol>(io_ctx, map, std::move(params));
    client->start();
    io_ctx.run();
    
    return 0;
}
