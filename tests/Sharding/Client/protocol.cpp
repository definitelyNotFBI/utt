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
#include "conn.hpp"
#include "msg/ShardTx.hpp"
#include "options.hpp"
#include "protocol.hpp"
#include "bft.hpp"
#include "sha_hash.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"

#include "utt/Comm.h"
#include "utt/RandSig.h"
#include "utt/Serialization.h"
#include "utt/Tx.h"
#include "utt/Wallet.h"

#include "config.hpp"

namespace sharding::client {

logging::Logger protocol::logger = logging::getLogger("sharding.bcb.client");

protocol::protocol(io_ctx_t& io_ctx, 
                   std::vector<bft::communication::NodeMap>& node_map, 
                   utt_bft::client::Params &&params)
    : m_io_ctx_(io_ctx), timer(io_ctx), tx_timer(io_ctx)
{
    // Setup params
    m_params_ = std::make_unique<utt_bft::client::Params>(params);

    auto config = ClientConfig::Get();

    // Read genesis files
    auto wallet_filename = config->get(sharding::common::utt_wallet_file_key, 
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
    for(size_t shard_id=0;shard_id<config->num_shards;shard_id++) {
        std::unordered_map<uint16_t, conn_handler_ptr> map;
        m_conn_.emplace(shard_id, map);
    }

    // Setup node maps
    for(size_t shard_id=0;shard_id<config->num_shards;shard_id++) {
        std::unordered_map<uint16_t, asio::ip::tcp::endpoint> map;
        for(auto& [replica_id,node_info]: node_map.at(shard_id)) {
        // Skip clients
        if (!node_info.isReplica) {
            continue;
        }

        auto address = asio::ip::make_address(node_info.host);
        map[replica_id] = endpoint_t(address, node_info.port);
        LOG_DEBUG(protocol::logger, "Adding " << map[replica_id] 
                                            << " as replica " << replica_id);
        }
        m_node_map_.push_back(map);
    }

    // Setup BFT keys
    auto* map = new sharding::common::PublicKeyMap();
    pk_map = std::shared_ptr<sharding::common::PublicKeyMap>(map);
    for(auto& [id, key]: ClientConfig::Get()->publicKeysOfReplicas) {
        auto pubkey = std::shared_ptr<sharding::common::PublicKey>(
                            new sharding::common::PublicKey(key.c_str()));
        pk_map->emplace(id, pubkey);
    }
}

void protocol::on_timeout(const asio::error_code& err) {
    if (err == asio::error::operation_aborted) {
        LOG_DEBUG(logger, "Aborted timer");
        return;
    }
    LOG_INFO(logger, "Connection Timed out");
    // TODO: Fix Partial connections handling on timeout
    auto config = ClientConfig::Get();
    auto num_connections = 0ul;
    {
        m_conn_mutex_.lock();
        num_connections = num_conns;
        m_conn_mutex_.unlock();
    }
    if (num_connections == config->numReplicas*config->num_shards) {
        return;
    }
    LOG_INFO(logger, "Got only " << num_connections 
                    << ". Attempting to proceed with partial connections");
    // TODO: Fix number of expected connections
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
    auto config = ClientConfig::Get();
    // Start connecting to all the nodes
    for(size_t shard=0; shard<config->num_shards;shard++) {
        for(auto& [node_id, end_point]: m_node_map_.at(shard)) {
            auto conn = conn_handler::create(m_io_ctx_, node_id, pk_map, shard, shared_from_this());
            auto& sock = conn->socket();
            sock.async_connect(end_point, std::bind(
                &conn_handler::on_new_conn, conn, std::placeholders::_1));
        }
    }
    // Started all the connections, lets wait for some time
    using namespace std::chrono_literals;
    // Register handlers
    timer.expires_after(120s);
    timer.async_wait(std::bind(&protocol::on_timeout, 
            shared_from_this(), std::placeholders::_1));
}

// Add a connection to the protocol
void protocol::add_connection(sharding::client::conn_handler_ptr conn_ptr) {
    auto config = ClientConfig::Get();
    size_t shard_id = conn_ptr->get_shard_id();
    uint16_t replica_id = conn_ptr->getid();
    size_t conns;
    {
        m_conn_mutex_.lock();
        // m_conn_.emplace_back(std::move(conn_ptr));
        m_conn_[shard_id].emplace(replica_id, conn_ptr);
        num_conns += 1;
        conns = num_conns;
        m_conn_mutex_.unlock();
    }
    if (conns != (m_node_map_.size()*config->numReplicas)) {
        return;
    }
    LOG_INFO(logger, "Connected to all the replicas.");
    LOG_INFO(logger, "Cancelling the timer");
    timer.cancel();

    start_experiments();
}

// Run the sharded client experiments
void protocol::start_experiments() {
    matched_response.clear();

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
    send_burn_tx();
}

void protocol::send_mint_tx(std::unordered_map<size_t, std::vector<sharding::common::Response>>&& responses) {
    auto* config = ClientConfig::Get();
    LOG_INFO(logger, "Transaction (Mint) #" << experiment_idx);

    auto& current_tx = tx_map[experiment_idx];
    // Get responsible shards
    // DONE: target_shard_id
    size_t target_shard_id = 
        (config->id - config->numReplicas)% config->num_shards;
    // Send only to the shards responsible
    for(auto& [replica_id, conn]: m_conn_[target_shard_id]) {
        auto& sock = conn->socket();
        sock.async_receive(asio::buffer(conn->replica_msg_buf.data(), sharding::common::REPLICA_MAX_MSG_SIZE),
                            std::bind(&conn_handler::on_mint_response,
                                    conn->shared_from_this(),
                                    std::placeholders::_1,
                                    std::placeholders::_2));
    }

    std::stringstream ss, resp_ss;
    ss << current_tx;
    auto ss_str = ss.str();
    MintProof proof{responses};
    resp_ss << proof;
    std::stringstream test_ss(resp_ss.str());
    MintProof proof2;
    test_ss >> proof2;
    assert(deep_compare(proof, proof2));
    auto resp_str = resp_ss.str();

    auto mint_req_len = MintMsg::get_size(ss_str.size(), resp_str.size());
    auto msg_len = Msg::get_size(mint_req_len);
    out_msg_buf.resize(msg_len);
    Msg* msg = (Msg*)out_msg_buf.data();
    msg->tp = MsgType::MINT_REQUEST;
    msg->msg_len = mint_req_len;
    MintMsg* mint_msg = msg->get_msg<MintMsg>();
    // DONE: Randomize shard id based on client id
    mint_msg->tx_len = ss_str.size();
    mint_msg->proof_len = resp_str.size();
    std::memcpy(mint_msg->get_tx_buf_ptr(), ss_str.data(), ss_str.size());
    std::memcpy(mint_msg->get_proof_buf_ptr(), resp_str.data(), resp_str.size());

    LOG_DEBUG(logger, "Sending Sharding mint request:" << std::endl
                        << "target shard id: " << target_shard_id << std::endl
    );

    responses_waiting = config->numReplicas;
    LOG_INFO(logger, "Waiting for " << responses_waiting << " mint replies");

    // DONE: Send to only the shards responsible
    for(auto& [replica_id, conn]: m_conn_[target_shard_id]) {
        // Reset the stream
        conn->out_ss.str("");
        conn->out_ss.write((const char*)msg, msg_len);
        auto& sock = conn->socket();
        sock.async_send(
            asio::buffer(conn->out_ss.str(), msg->get_size()),
            std::bind(
                &conn_handler::on_mint_send, conn->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    }

    using namespace std::chrono_literals;
    // Started all the connections, lets wait for some time
    tx_timer.expires_after(60s);
    tx_timer.async_wait(std::bind(&protocol::on_tx_timeout, 
            shared_from_this(), std::placeholders::_1));

}

// Sends the tx and waits for n-f responses or a timeout
// TODO:
void protocol::send_burn_tx() {
    auto* config = ClientConfig::Get();
    if (experiment_idx == 0) {
        // Log histogram
        m_metric_mtx_.lock();
        std::cout << "Performance info from client " 
                            << ClientConfig::Get()->id
                            << std::endl
                            << hist.ToString() << std::endl;
        m_metric_mtx_.unlock();
        return;
    }
    experiment_idx--;
    LOG_INFO(logger, "Transaction #" << experiment_idx);

    auto& current_tx = tx_map[experiment_idx];
    // Get responsible shards
    // DONE:
    std::set<uint16_t> responsible_shards;
    for(auto& nullif: current_tx.getNullifiers()) {
        LOG_INFO(logger, "Nullifier: " << nullif);
        responsible_shards.insert(
            common::get_responsibility(nullif, config->num_shards)
        );
    }
    for(auto shard: responsible_shards) {
        LOG_INFO(logger, "Responsible shards:"<< shard);
    }
    
    // Send only to the shards responsible
    // DONE:
    for(auto& [shard_id, conn_map]: m_conn_) {
        if(responsible_shards.count(shard_id) == 0) {
            continue;
        }
        for(auto& [replica_id, conn]: conn_map) {
          auto& sock = conn->socket();
          sock.async_receive(asio::buffer(conn->replica_msg_buf.data(), sharding::common::REPLICA_MAX_MSG_SIZE),
                             std::bind(&conn_handler::on_burn_response,
                                       conn->shared_from_this(),
                                       std::placeholders::_1,
                                       std::placeholders::_2));
        }
    }

    {
        m_metric_mtx_.lock();
        begin = get_monotonic_time();
        m_metric_mtx_.unlock();
    }

    std::stringstream ss;
    ss << current_tx;
    auto ss_str = ss.str();

    auto burn_req_len = BurnMsg::get_size(ss_str.size());
    auto msg_len = Msg::get_size(burn_req_len);
    out_msg_buf.resize(msg_len);
    Msg* msg = (Msg*)out_msg_buf.data();
    msg->tp = MsgType::BURN_REQUEST;
    msg->msg_len = burn_req_len;
    BurnMsg* burn_msg = msg->get_msg<BurnMsg>();
    // DONE: Randomize shard id based on client id
    burn_msg->target_shard_id = 
        (config->id - config->numReplicas)% config->num_shards;
    burn_msg->tx_len = ss_str.size();
    std::memcpy(burn_msg->get_tx_buf_ptr(), ss_str.data(), ss_str.size());

    LOG_DEBUG(logger, "Sending Sharding Burn request:" << std::endl
                        << "target shard id: " << burn_msg->target_shard_id << std::endl
    );

    responses_waiting = responsible_shards.size()*config->numReplicas;
    LOG_INFO(logger, "Waiting for " << responses_waiting << " replies");

    // DONE: Send to only the shards responsible
    for(auto& [shard_id, conn_map]: m_conn_) {
        if(responsible_shards.count(shard_id) == 0) {
            continue;
        }
        for(auto& [replica_id, conn]: conn_map) {
          // Reset the stream
          conn->out_ss.str("");
          conn->out_ss.write((const char*)msg, msg_len);
          auto& sock = conn->socket();
          sock.async_send(
              asio::buffer(conn->out_ss.str(), msg->get_size()),
              std::bind(
                  &conn_handler::on_burn_send, conn->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
        }
    }

    using namespace std::chrono_literals;
    // Started all the connections, lets wait for some time
    tx_timer.expires_after(60s);
    tx_timer.async_wait(std::bind(&protocol::on_tx_timeout, 
            shared_from_this(), std::placeholders::_1));

}

void protocol::add_mint_response(uint8_t *ptr, size_t num_bytes, uint16_t id, size_t shard_id)
{
    auto end = get_monotonic_time();
    Msg* msg = (Msg*)ptr;
    if(msg->tp != MsgType::MINT_RESPONSE) {
        LOG_ERROR(logger, "Got an invalid mint response type message as response");
    }

    // DONE: Add response to the matcher
    size_t num_responses = 0;
    {
        m_resp_mtx_.lock();
        matched_response.add(id, ptr, num_bytes, shard_id);
        num_responses = matched_response.size();
        m_resp_mtx_.unlock();
    }

    if (num_responses != responses_waiting) {
        return;
    }

    LOG_INFO(logger, "Got all " << responses_waiting << " responses");
    LOG_INFO(logger, "Cancelling the tx timer");
    tx_timer.cancel();

    std::unordered_map<size_t, std::vector<sharding::common::Response>> responses;

    {
        m_resp_mtx_.lock();
        // swap the responses with empty responses
        responses = matched_response.clear();
        m_resp_mtx_.unlock();
    }
    // If part 2, do this
    auto elapsed = end - begin;

    {
        m_metric_mtx_.lock();
        hist.Add(double(elapsed));
        m_metric_mtx_.unlock();
    }
    LOG_INFO(logger, "Finished a transaction");
    send_burn_tx();

}

void protocol::add_burn_response(uint8_t *ptr, size_t num_bytes, uint16_t id, size_t shard_id)
{
    LOG_INFO(logger, "Adding " << num_bytes << " of response from " << id);
    // Verify transactions
    auto config = ClientConfig::Get();
    auto verif = config->pk_map->at(shard_id)[id];
    assert(verif != nullptr);
    Msg* original_msg = (Msg*)out_msg_buf.data();
    BurnMsg* burn_msg = original_msg->get_msg<BurnMsg>();
    auto txhash2 = concord::util::SHA3_256().digest(
                                (uint8_t*)burn_msg->get_tx_buf_ptr(), 
                                burn_msg->tx_len);
    ResponseData data{burn_msg->target_shard_id, txhash2};
    auto txhash = concord::util::SHA3_256().digest((uint8_t*)&data, 
                                                    sizeof(data));
    Msg* msg = (Msg*)ptr;
    if(msg->tp != MsgType::BURN_RESPONSE) {
        LOG_ERROR(logger, "Got an invalid type message as response");
    }
    BurnResponse* burn_resp = msg->get_msg<BurnResponse>();
    if(verif->signatureLength() != burn_resp->sig_len) {
        LOG_WARN(logger, "Got invalid signature length: Exp " << verif->signatureLength() << ", got " << burn_resp->sig_len);
    }

    if(!verif->verify((const char*)txhash.data(), txhash.size(), (const char*)burn_resp->get_sig_buf_ptr(), burn_resp->sig_len)) {
        LOG_WARN(logger, "Signature check failed");
    }

    size_t num_responses = 0;
    {
        m_resp_mtx_.lock();
        matched_response.add(id, (uint8_t*)burn_resp, msg->msg_len, shard_id);
        num_responses = matched_response.size();
        m_resp_mtx_.unlock();
    }

    if (num_responses != responses_waiting) {
        return;
    }

    LOG_INFO(logger, "Got all " << responses_waiting << " responses");
    LOG_INFO(logger, "Cancelling the tx timer");
    tx_timer.cancel();

    std::unordered_map<size_t, std::vector<sharding::common::Response>> responses;

    {
        m_resp_mtx_.lock();
        // swap the responses with empty responses
        responses = matched_response.clear();
        m_resp_mtx_.unlock();
    }

    send_mint_tx(std::move(responses));
}

} // namespace sharding::client
