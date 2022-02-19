#include <xutils/AutoBuf.h>
#include <asio/streambuf.hpp>
#include <cstring>
#include <functional>
#include <iostream>
#include <fstream>
#include <iterator>
#include <libff/common/serialization.hpp>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
#include "msg/QuickPay.hpp"
#include "config.hpp"
#include "conn.hpp"
#include "protocol.hpp"
#include "bft.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"
#include "utt/Comm.h"
#include "utt/RandSig.h"
#include "utt/Serialization.h"
#include "utt/Tx.h"
#include "utt/Wallet.h"

namespace quickpay::client {

logging::Logger protocol::logger = logging::getLogger("quickpay.bft.client");

protocol::protocol(io_ctx_t& io_ctx, 
                   bft::communication::NodeMap& node_map, 
                   utt_bft::client::Params &&params)
    : m_io_ctx_(io_ctx), timer(io_ctx), tx_timer(io_ctx)
{
    // Setup params
    m_params_ = std::make_unique<utt_bft::client::Params>(params);

    auto config = ClientConfig::Get();

    // Read genesis files
    auto wallet_filename = config->get(BCB::common::utt_wallet_file_key, 
                                            std::string()) 
                                + std::to_string(config->getid() - config->numReplicas);
    LOG_INFO(logger, "Opening wallet file: " << wallet_filename);

    std::ifstream utt_wallet_file(wallet_filename);
    if (utt_wallet_file.fail()) {
        LOG_FATAL(logger, "Failed to open " << wallet_filename);
        throw std::runtime_error("Error opening utt wallet file");
    }

    libutt::Wallet wal1, wal2;
    utt_wallet_file >> wal1;
    libff::consume_OUTPUT_NEWLINE(utt_wallet_file);
    utt_wallet_file >> wal2;
    libff::consume_OUTPUT_NEWLINE(utt_wallet_file);
    m_wallet_send_ = std::make_unique<libutt::Wallet>(std::move(wal1));
    m_wallet_recv_ = std::make_unique<libutt::Wallet>(std::move(wal2));
    
    utt_wallet_file.close();

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

    // Setup BFT keys
    using BCB::common::PublicKeyMap;
    using BCB::common::PublicKey;
    auto* map = new BCB::common::PublicKeyMap();
    pk_map = std::shared_ptr<BCB::common::PublicKeyMap>(map);
    for(auto& [id, key]: ClientConfig::Get()->publicKeysOfReplicas) {
        auto pubkey = std::shared_ptr<PublicKey>(
                            new PublicKey(key.c_str()));
        pk_map->emplace(id, pubkey);
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

std::string end_point_to_string(const asio::ip::tcp::endpoint& ep)
{
    return ep.address().to_string() + std::to_string(ep.port());
}

void protocol::start() {
    // Start connecting to all the nodes
    for(auto& [node_id, end_point]: m_node_map_) {
        auto conn = conn_handler::create(m_io_ctx_, node_id, pk_map, shared_from_this());
        auto& sock = conn->socket();
        sock.async_connect(end_point, std::bind(
            &conn_handler::on_new_conn, conn, end_point_to_string(end_point),  std::placeholders::_1));
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
        m_conn_.emplace_back(std::move(conn_ptr));
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
    std::vector<uint16_t> ids(0);
    std::vector<std::vector<uint8_t>> data(0);
    matched_response.clear(ids, data);

    auto config = ClientConfig::Get();

    auto num_ops = config->getnumOfOperations();

    auto& recip = m_wallet_recv_->getUserPid();
    for(size_t i=0; i< num_ops; i++) {
        auto current_tx = m_wallet_send_->spendTwoRandomCoins(recip, false);
        tx_map.emplace(i, current_tx);
    }

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
        auto num_ops = ClientConfig::Get()->getnumOfOperations();
        double throughput = (num_ops*1000000)/double(throughput_end - throughput_begin);
        std::cout << "Throughput info from client "
                            << ClientConfig::Get()->id
                            << throughput
                            << std::endl;
        return;
    }
    experiment_idx--;
    LOG_INFO(logger, "Transaction #" << experiment_idx);

    for(auto& conn: m_conn_) {
        auto& sock = conn->socket();
        sock.async_receive(
            asio::buffer(conn->replica_msg_buf.data(), BCB::common::REPLICA_MAX_MSG_SIZE),
            std::bind(&conn_handler::on_tx_response, conn->shared_from_this(),
                std::placeholders::_1, std::placeholders::_2)
        );
    }

    {
        m_metric_mtx_.lock();
        begin = get_monotonic_time();
        m_metric_mtx_.unlock();
    }

    auto& current_tx = tx_map[experiment_idx];
    std::stringstream ss;
    ss << current_tx;
    auto ss_str = ss.str();

    auto txhash = current_tx.getHashHex();
    auto qp_len = QuickPayMsg::get_size(txhash.size());
    auto qp_tx = QuickPayTx::alloc(qp_len, ss_str.size());
    auto qp = qp_tx->getQPMsg();
    qp->target_shard_id = 0;
    qp->hash_len = txhash.size();
    std::memcpy(qp->getHashBuf(), txhash.data(), txhash.size());
    std::memcpy(qp_tx->getTxBuf(), ss_str.data(), ss_str.size());

    LOG_DEBUG(logger, "Sending QP Tx:" << std::endl
                        << "target shard id: " << qp->target_shard_id << std::endl
                        << "hash len: " << qp->hash_len << std::endl 
    );

    for(auto& conn: m_conn_) {
        // Reset the stream
        conn->out_ss.str("");
        conn->out_ss.write((const char*)qp_tx, qp_tx->get_size());
        auto& sock = conn->socket();
        sock.async_send(
            asio::buffer(conn->out_ss.str(), qp_tx->get_size()),
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
    LOG_INFO(logger, "Adding " << num_bytes << " of response from " << id);
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

    std::vector<uint16_t> ids;
    std::vector<std::vector<uint8_t>> responses;
    {
        m_resp_mtx_.lock();
        // swap the responses with empty responses
        matched_response.clear(ids, responses);
        m_resp_mtx_.unlock();
    }

    auto elapsed = end - begin;

    {
        m_metric_mtx_.lock();
        hist.Add(double(elapsed));
        m_metric_mtx_.unlock();
    }
    LOG_INFO(logger, "Finished a transaction");
    send_tx();
}

} // namespace quickpay::client
