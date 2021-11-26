#include <asio/error_code.hpp>
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "bft.hpp"
#include "misc.hpp"
#include "quickpay/TesterReplica/config.hpp"
#include "quickpay/TesterReplica/conn.hpp"
#include "quickpay/TesterReplica/protocol.hpp"
#include "replica/Params.hpp"
#include "rocksdb/native_client.h"
#include <asio.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <fstream>

namespace quickpay::replica {

logging::Logger protocol::logger = logging::getLogger("quickpay.bft.replica");

protocol::protocol(asio::io_context& io_ctx, 
                    uint16_t port_num, 
                    std::shared_ptr<Cryptosystem> crypsys)
                : m_io_ctx_(io_ctx),
                    m_acceptor_(io_ctx, 
                                    asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 
                                    port_num)),
                    metrics_timer(io_ctx)
{
    auto replicaConfig = ReplicaConfig::Get();
    using concord::storage::rocksdb::NativeClient;
    auto db_file = "utt-db"+ std::to_string(replicaConfig->getid());
    LOG_INFO(logger, "Using DB File: " << db_file);
    db_ptr = NativeClient::newClient(db_file, 
                                        false, 
                                        NativeClient::DefaultOptions{});

    auto params_file_name = replicaConfig->get(utt_bft::UTT_PARAMS_REPLICA_KEY,std::string("")) + std::to_string(replicaConfig->getid());
    LOG_INFO(logger, "Using Params file: " << params_file_name);
    std::ifstream params_file(params_file_name); 
    if(!params_file.good()) {
        LOG_FATAL(logger, "Failed to open params file: "<< params_file_name);
        throw std::runtime_error("Error opening params file");
    }
    m_params_ = std::make_shared<utt_bft::replica::Params>(params_file);
    m_cryp_sys_ = crypsys;

    num_tx_processed = std::make_shared<std::atomic<uint64_t>>(0);
    last_logged_time = get_monotonic_time();

    LOG_INFO(logger, "Starting the connection at " << port_num);
}

void protocol::on_performance_timeout(const asio::error_code& err)
{
    auto time = get_monotonic_time();
    auto diff = time - last_logged_time;
    uint64_t num_tx = *num_tx_processed;
    auto tput = (double(num_tx)*1000000.0)/double(diff);
    std::cout << "Performance Info: " << std::endl <<
                        "Transactions: "    <<  num_tx      << std::endl << 
                        "Time: "            << double(diff) << std::endl << 
                        "Throughput: "      << tput         << std::endl
            ;
    last_logged_time = time;
    *num_tx_processed = 0;
    using namespace std::chrono_literals;
    // Register handlers
    metrics_timer.expires_after(10s);
    metrics_timer.async_wait(std::bind(&protocol::on_performance_timeout, 
            shared_from_this(), std::placeholders::_1));
}

void protocol::start() {
    using namespace std::chrono_literals;
    // Register handlers
    metrics_timer.expires_after(10s);
    metrics_timer.async_wait(std::bind(&protocol::on_performance_timeout, 
            shared_from_this(), std::placeholders::_1));

    start_accept();
}

void protocol::start_accept() 
{
    auto conn = conn_handler::create(m_io_ctx_, id++, m_params_, db_ptr, num_tx_processed, m_cryp_sys_);
    // asynchronous accept operation and wait for a new connection.
    m_acceptor_.async_accept(conn->socket(),
        std::bind(&protocol::on_new_client, this, conn,
        std::placeholders::_1));
}

void protocol::on_new_client(conn_handler_ptr conn, const asio::error_code& err) 
{
    LOG_INFO(logger, "Got a new client connection");
    if (!err) {
        m_conn_.emplace(conn->id, conn->shared_from_this());
        conn->on_new_conn();
    } else {
        LOG_ERROR(logger, err.message());
    }

    start_accept();
}

} // namespace quickpay::replica
