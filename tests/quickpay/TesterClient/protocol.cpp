#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

#include <asio.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>

#include "Logging4cplus.hpp"
#include "client/Params.hpp"
#include "config/test_parameters.hpp"
#include "common.hpp"
#include "quickpay/TesterClient/config.hpp"
#include "quickpay/TesterClient/conn.hpp"
#include "quickpay/TesterClient/protocol.hpp"
#include "bft.hpp"

namespace quickpay::client {

logging::Logger protocol::logger = logging::getLogger("quickpay.bft.client");

protocol::protocol(io_ctx_t& io_ctx, 
                   bft::communication::NodeMap& node_map)
    : m_io_ctx_(io_ctx), timer(io_ctx)
{
    // Construct libutt params
    auto config = ClientConfig::Get();
    auto utt_params_filename = config->get(utt_bft::UTT_PARAMS_CLIENT_KEY, std::string(""));
    LOG_INFO(logger, "Opening " << utt_params_filename);

    std::ifstream utt_params_file(utt_params_filename);
    if (utt_params_file.fail()) {
        LOG_FATAL(logger, "Failed to open " << utt_params_filename);
        throw std::runtime_error("Error opening utt params file");
    }

    m_params_ = std::make_unique<utt_bft::client::Params>(utt_params_file);

    // Setup connections
    for(auto& [replica_id,node_info]: node_map) {
        // Skip clients
        if (!node_info.isReplica) {
            continue;
        }

        auto address = asio::ip::make_address(node_info.host);
        m_node_map_[replica_id] = endpoint_t(address, node_info.port);
        LOG_DEBUG(protocol::logger, "Adding " << m_node_map_[replica_id] 
                                            << " as replica " << replica_id);
    }
}

void protocol::start() {
    // Start connecting to all the nodes
    for(auto& [node_id, end_point]: m_node_map_) {
        auto conn = conn_handler::create(m_io_ctx_, node_id, shared_from_this());
        auto& sock = conn->socket();
        sock.async_connect(end_point, std::bind(
            &conn_handler::on_new_conn, conn, std::placeholders::_1));
    }
    // Started all the connections, lets wait for some time
    using namespace std::chrono_literals;
    timer.expires_from_now(60s);
    timer.async_wait([&](const asio::error_code& err) {
        if (err == asio::error::operation_aborted) {
            LOG_DEBUG(logger, "Aborted timer");
            return;
        }
        LOG_INFO(logger, "Connection Timed out");
        auto num_connections = 0ul;
        {
            m_conn_mutex_.lock();
            num_connections = m_conn_.size();
            m_conn_mutex_.unlock();
        }
        if (num_connections == m_node_map_.size()) {
            return;
        }
        LOG_INFO(logger, "Got only " << num_connections << ". Attempting to proceed with partial connections");
        auto expected_conns = m_node_map_.size()-ClientConfig::Get()->getfVal();
        if (num_connections < expected_conns) {
            LOG_FATAL(logger, "Expected " << expected_conns << ", got " << num_connections);
            throw std::runtime_error("Insufficient connections");
        }
    });
}

// Add a connection to the protocol
void protocol::add_connection(conn_handler_ptr conn_ptr) {
    size_t num_conns;
    {
        m_conn_mutex_.lock();
        m_conn_.push_back(conn_ptr);
        num_conns = m_conn_.size();
        m_conn_mutex_.unlock();
    }
    if (num_conns == m_node_map_.size()) {
        LOG_INFO(logger, "Connected to all the replicas. Cancelling the timer");
        timer.cancel();
    }
}

// Run the quickpay client experiments
void protocol::start_experiments() {
    auto config = ClientConfig::Get();

    auto num_ops = config->getnumOfOperations();
    std::cout << "Starting " << num_ops
                << " iterations" << std::endl;

    for(auto i=0; i<num_ops; i++) {
        // TODO:
    }

    std::cout << "Finished the experiments" << std::endl;
}

} // namespace quickpay::replica
