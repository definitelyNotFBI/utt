#include <xutils/AutoBuf.h>
#include <asio/streambuf.hpp>
#include <cassert>
#include <cstdint>
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

#include "IKeyExchanger.hpp"
#include "Logging4cplus.hpp"
#include "client/Params.hpp"
#include "config/test_parameters.hpp"
#include "common.hpp"
#include "histogram.hpp"
#include "misc.hpp"
#include "msg/QuickPay.hpp"
#include "conn.hpp"
#include "protocol.hpp"
#include "bft.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"
#include "utt/Comm.h"
#include "utt/RandSig.h"
#include "utt/Serialization.h"
#include "utt/Tx.h"
#include "utt/Wallet.h"

#include "config.hpp"

namespace fullpay::ft::client {

logging::Logger protocol::logger = logging::getLogger("fullpay.bft.client");

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
    for(auto& [id, key]: ClientConfig::Get()->publicKeysOfReplicas) {
        auto pubkey = std::make_shared<BCB::common::PublicKey>(key.c_str(), KeyFormat::HexaDecimalStrippedFormat);
        pk_map.emplace(id, pubkey);
    }
}

void protocol::on_timeout(const asio::error_code& err, size_t idx) {
    if (err == asio::error::operation_aborted) {
        LOG_DEBUG(logger, "Aborted timer (This is good!)");
        return;
    }
    LOG_WARN(logger, "Connection Timed out (This is bad!) while waiting for " << idx);
    auto num_connections = 0ul;
    {
        m_conn_mutex_.lock();
        num_connections = m_conn_.size();
        m_conn_mutex_.unlock();
    }
    if (num_connections == m_node_map_.size()) {
        return;
    }
    LOG_WARN(logger, "Got only " << num_connections 
                    << ". Attempting to proceed with partial connections");
    auto expected_conns = m_node_map_.size()-ClientConfig::Get()->getfVal();
    if (num_connections < expected_conns) {
        LOG_FATAL(logger, "Expected " << expected_conns << ", got " << num_connections);
        throw std::runtime_error("Insufficient connections");
    }
}

void protocol::on_tx_timeout(const asio::error_code err, size_t idx) {
    if (err == asio::error::operation_aborted) {
        LOG_DEBUG(logger, "Aborted tx timer");
        return;
    }
    LOG_WARN(logger, "tx Timed out idx#" << idx);
}

void protocol::start() {
    // Check: Is the node id on the communication map the same as id in the bank pk ids?
    // Start connecting to all the nodes
    for(auto& [node_id, end_point]: m_node_map_) {
        auto conn = conn_handler::create(m_io_ctx_, node_id, pk_map, shared_from_this());
        auto& sock = conn->socket();
        sock.async_connect(end_point, std::bind(
            &conn_handler::on_new_conn, conn, std::placeholders::_1));
    }
    // Started all the connections, lets wait for some time
    using namespace std::chrono_literals;
    // Register handlers
    timer.expires_after(180s);
    timer.async_wait(std::bind(&protocol::on_timeout, 
            shared_from_this(), std::placeholders::_1, experiment_idx));
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

// Run the BCB client experiments
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
        std::exit(0);
    }

    experiment_idx--;
    LOG_INFO(logger, "Transaction #" << experiment_idx);

    {
        m_metric_mtx_.lock();
        begin = get_monotonic_time();
        m_metric_mtx_.unlock();
    }

    auto& current_tx = tx_map[experiment_idx];
    std::stringstream ss;
    ss << current_tx;
    auto ss_str = ss.str();

    std::vector<uint8_t> out_msg;

    auto txhash = current_tx.getHashHex();
    auto qp_len = QuickPayMsg::get_size(txhash.size());
    auto qp_tx_len = QuickPayTx::get_size(qp_len, ss_str.size());
    auto out_msg_len = Msg::get_size(qp_tx_len);
    out_msg.resize(out_msg_len, 0);
    Msg* msg = (Msg*)out_msg.data();
    msg->tp = MsgType::BURN_REQUEST;
    msg->seq_num = experiment_idx;
    msg->msg_len = qp_tx_len;
    auto qp_tx = msg->get_msg<QuickPayTx>();
    qp_tx->qp_msg_len = qp_len;
    qp_tx->tx_len = ss_str.size();
    auto qp = qp_tx->getQPMsg();
    qp->target_shard_id = 0;
    qp->hash_len = txhash.size();
    std::memcpy(qp->getHashBuf(), txhash.data(), txhash.size());
    std::memcpy(qp_tx->getTxBuf(), ss_str.data(), ss_str.size());

    LOG_DEBUG(logger, "Sending QP Tx:" << std::endl
                        << "target shard id: " << qp->target_shard_id << std::endl
                        << "hash len: " << qp->hash_len << std::endl 
    );

    std::vector<uint8_t> msg_buf(out_msg.begin(), out_msg.end());

    for(auto& conn: m_conn_) {
        conn->send_msg(msg_buf, Type::TX);
    }

    using namespace std::chrono_literals;
    // Started all the connections, lets wait for some time
    tx_timer.expires_after(180s);
    tx_timer.async_wait(std::bind(&protocol::on_tx_timeout, 
            shared_from_this(), std::placeholders::_1, experiment_idx));

}

void protocol::add_ack(uint16_t replica_id, size_t idx)
{
    auto end = get_monotonic_time();

    LOG_DEBUG(logger, "Got an ack for idx " << idx);
    auto config = ClientConfig::Get();
    size_t num_responses = 0, required_responses = config->getnumReplicas() - config->getfVal();

    {
        m_ack_mtx_.lock();
        if(exp_idx_num_acks_map.count(idx) == 0) {
            exp_idx_num_acks_map.emplace(idx, 1);
        } else {
            exp_idx_num_acks_map[idx] += 1;
        }
        num_responses = exp_idx_num_acks_map[experiment_idx];
        m_ack_mtx_.unlock();
    }
    LOG_DEBUG(logger, "Have " << num_responses << " for " << experiment_idx);

    // One of the connections will send this, the others will fail this
    // Only send on this exact condition
    if (num_responses != required_responses) {
        return;
    }
    // This part of the code is executed once, after receiving exactly n-f valid responses
    tx_timer.cancel();
    auto elapsed = end - begin;

    {
        m_metric_mtx_.lock();
        hist.Add(double(elapsed));
        m_metric_mtx_.unlock();
    }
    send_tx();
}

void protocol::send_ack(const std::vector<uint8_t>& msg) 
{
    // DONE: Send the ACK
    for(auto& conn: m_conn_) {
        // Reset the stream
        conn->send_ack_msg(msg, experiment_idx);
    }

    using namespace std::chrono_literals;
    // Started all the connections, lets wait for some time
    tx_timer.expires_after(120s);
    tx_timer.async_wait(std::bind(&protocol::on_tx_timeout, 
            shared_from_this(), std::placeholders::_1, experiment_idx));

}

void protocol::add_response(std::vector<uint8_t> response, uint16_t sender_id, size_t expIdx)
{
    if(experiment_idx < expIdx) {
        LOG_DEBUG(logger, "Ignoring stale response from " << sender_id 
                            << " for idx " << expIdx 
                            << " since we are already in in Tx#" 
                            << experiment_idx);
        return;
    }
    LOG_INFO(logger, "Adding " << response.size() << " of response from " << sender_id << " for idx " << expIdx);

    auto config = ClientConfig::Get();
    size_t num_responses = 0;
    auto required_responses = config->getnumReplicas() - config->getfVal();
    bool added = false, done = false, just_finished = false;
    std::vector<uint16_t> ids;
    std::vector<std::vector<uint8_t>> responses;
    auto resp = (FullPayFTResponse*)response.data();

    {
        m_resp_mtx_.lock();
        num_responses = matched_response.size();
        if (finished_indices.count(expIdx) != 0) {
            done = true;
        }
        else if (static_cast<int>(num_responses) < required_responses) {
            matched_response.add(sender_id, resp->getRSABuf(), resp->rsa_len);
            added = true;
        } else {
            finished_indices.insert(expIdx);
            // swap the responses with empty responses
            matched_response.clear(ids, responses);
            just_finished = true;
        }
        m_resp_mtx_.unlock();
    }

    // DONE: Update to n-f
    if (added || done) {
        // We don't have enough or we have already finished this tx
        LOG_DEBUG(logger, "Added: "<< added << " Done: " << done);
        return;
    }

    // Only one thread should enter when this condition is satisfied
    assert(just_finished);

    LOG_INFO(logger, "Got responses from enough servers");
    LOG_INFO(logger, "Cancelling the tx timer");
    // NOTE: Multiple threads will not call this
    // As only one thread will have just_finished = true
    tx_timer.cancel();

    LOG_INFO(logger, "Finished a transaction" << experiment_idx);
    // DONE: Construct an ack msg
    std::vector<uint8_t> msg_buf;
    auto sig_size = responses.at(0).size();
    auto ack_len = FullPayFTAck::get_size(ids.size(), sig_size, responses.size());
    auto msg_len = Msg::get_size(ack_len);
    msg_buf.resize(msg_len, 0);
    auto msg = (Msg*)msg_buf.data();
    msg->tp = MsgType::ACK_MSG;
    msg->msg_len = ack_len;
    msg->seq_num = experiment_idx;
    auto ack = msg->get_msg<FullPayFTAck>();
    ack->seq_num = experiment_idx;
    ack->num_ids = ids.size();
    ack->num_sigs = responses.size();
    ack->sig_len = sig_size;
    for(size_t i = 0; i < ids.size(); i++) {
        // DONE: Copy the IDS
        ack->getIdBuf()[i] = ids[i];
        // DONE: Copy the sigs
        std::memcpy(ack->getSigBuf(i), responses[i].data(), ack->sig_len);
    }

    send_ack(msg_buf);
}

} // namespace fullpay::replica
