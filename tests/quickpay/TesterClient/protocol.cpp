#include <asio/streambuf.hpp>
#include <cstring>
#include <functional>
#include <iostream>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
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
#include "histogram.hpp"
#include "misc.hpp"
#include "quickpay/TesterClient/config.hpp"
#include "quickpay/TesterClient/conn.hpp"
#include "quickpay/TesterClient/protocol.hpp"
#include "bft.hpp"
#include "utt/Utt.h"

namespace quickpay::client {

logging::Logger protocol::logger = logging::getLogger("quickpay.bft.client");

protocol::protocol(io_ctx_t& io_ctx, 
                   bft::communication::NodeMap& node_map, 
                   utt_bft::client::Params &&params)
    : m_io_ctx_(io_ctx), timer(io_ctx), tx_timer(io_ctx),
        my_ltsk(libutt::LTSK::random()), 
        my_ltpk(libutt::LTPK(params.p, my_ltsk))
{
    // Setup params
    m_params_ = std::make_unique<utt_bft::client::Params>(params);

    auto config = ClientConfig::Get();

    // Read genesis files
    auto genesis_filename = config->get(utt_genesis_file_key, std::string("genesis/genesis_")) 
                                + std::to_string(config->getid());
    LOG_INFO(logger, "Opening genesis file: " << genesis_filename);

    std::ifstream utt_genesis_file(genesis_filename);
    if (utt_genesis_file.fail()) {
        LOG_FATAL(logger, "Failed to open " << genesis_filename);
        throw std::runtime_error("Error opening utt genesis file");
    }

    for(auto j=0; j<2;j++) {
      libutt::CoinSecrets cs_temp(utt_genesis_file);
      libutt::CoinComm cc_temp(utt_genesis_file);
      libutt::CoinSig csign_temp(utt_genesis_file);

      my_initial_coins.push_back(std::make_tuple(cs_temp, cc_temp, csign_temp));
    }

    utt_genesis_file.close();

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

void protocol::on_timeout(const asio::error_code& err) {
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
    LOG_INFO(logger, "Got only " << num_connections 
                    << ". Attempting to proceed with partial connections");
    auto expected_conns = m_node_map_.size()-ClientConfig::Get()->getfVal();
    if (num_connections < expected_conns) {
        LOG_FATAL(logger, "Expected " << expected_conns << ", got " << num_connections);
        throw std::runtime_error("Insufficient connections");
    }
}

void protocol::on_tx_timeout(const asio::error_code err) {
    if (err == asio::error::operation_aborted) {
        LOG_DEBUG(logger, "Aborted tx timer");
        return;
    }
    LOG_INFO(logger, "tx Timed out");
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
    // Register handlers
    timer.expires_after(60s);
    timer.async_wait(std::bind(&protocol::on_timeout, 
            shared_from_this(), std::placeholders::_1));
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
    if (num_conns != m_node_map_.size()) {
        return;
    }
    LOG_INFO(logger, "Connected to all the replicas.");
    LOG_INFO(logger, "Cancelling the timer");
    timer.cancel();

    start_experiments();
}

// Run the quickpay client experiments
void protocol::start_experiments() {
    auto config = ClientConfig::Get();

    auto num_ops = config->getnumOfOperations();
    std::cout << "Starting " << num_ops
                << " iterations" << std::endl;

    experiment_idx = num_ops;
    throughput_begin = get_monotonic_time();
    send_tx();
}

// Sends the tx and waits for n-f responses or a timeout
// TODO:
void protocol::send_tx() {
    if (experiment_idx == 0) {
        auto throughput_end = get_monotonic_time();
        // Log histogram
        m_metric_mtx_.lock();
        std::cout << "Performance info from client " 
                            << ClientConfig::Get()->id
                            << std::endl
                            << hist.ToString() << std::endl;
        m_metric_mtx_.unlock();
        double throughput = (400*1000000)/double(throughput_end - throughput_begin);
        std::cout << "Throughput info from client "
                            << ClientConfig::Get()->id
                            << throughput
                            << std::endl;
        return;
    }
    experiment_idx--;
    LOG_INFO(logger, "Transaction #" << experiment_idx);

    std::vector<std::tuple<libutt::LTPK, libutt::Fr>> recv;
    recv.reserve(2);
    for(auto j=0; j<2;j++) {
        auto r = libutt::Fr(std::get<0>(my_initial_coins[j]).val);
        recv.push_back(std::make_tuple(my_ltpk, r));
    }

    libutt::Tx tx = libutt::Tx::create(m_params_->p, my_initial_coins, recv);

    for(auto& conn: m_conn_) {
        auto& sock = conn->socket();
        sock.async_receive(
            asio::buffer(conn->replica_msg_buf.data(), REPLICA_MAX_MSG_SIZE),
            std::bind(&conn_handler::on_tx_response, conn->shared_from_this(),
                std::placeholders::_1, std::placeholders::_2)
        );
    }

    {
        m_metric_mtx_.lock();
        begin = get_monotonic_time();
        m_metric_mtx_.unlock();
    }

    for(auto& conn: m_conn_) {
        // Reset the stream
        conn->out_ss.str("");
        conn->out_ss << tx;
        auto& sock = conn->socket();
        sock.async_send(
            asio::buffer(conn->out_ss.str(), conn->out_ss.str().size()),
            std::bind(&conn_handler::on_tx_send, 
                conn->shared_from_this(), std::placeholders::_1,
                std::placeholders::_2));
    }

    using namespace std::chrono_literals;
    // Started all the connections, lets wait for some time
    tx_timer.expires_after(60s);
    tx_timer.async_wait(std::bind(&protocol::on_tx_timeout, 
            shared_from_this(), std::placeholders::_1));

}

void protocol::add_response(uint8_t *ptr, size_t num_bytes, uint16_t id)
{
    auto end = get_monotonic_time();

    size_t num_responses = 0;
    {
        m_resp_mtx_.lock();
        matched_response.add(id, ptr, num_bytes);
        num_responses = matched_response.size();
        m_resp_mtx_.unlock();
    }
    if (num_responses != ClientConfig::Get()->getnumReplicas()) {
        return;
    }
    LOG_INFO(logger, "Got all the responses");
    LOG_INFO(logger, "Cancelling the tx timer");
    tx_timer.cancel();

    {
        m_resp_mtx_.lock();
        matched_response.clear();
        m_resp_mtx_.unlock();
    }
    auto elapsed = end - begin;

    {
        m_metric_mtx_.lock();
        hist.Add(double(elapsed));
        m_metric_mtx_.unlock();
    }
    LOG_INFO(logger, "Finished a transaction");
    // return double(elapsed);
    send_tx();
}

} // namespace quickpay::replica
