#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>

#include <asio.hpp>
#include <asio/buffer.hpp>
#include <asio/basic_waitable_timer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <vector>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "communication/CommDefs.hpp"
#include "common.hpp"
#include "histogram.hpp"
#include "common/defs.hpp"
#include "conn.hpp"
// #include "quickpay/TesterClient/protocol.hpp"
#include "client/Params.hpp"
#include "utt/Wallet.h"
#include "utt/Tx.h"

namespace sharding::client {

using sharding::common::MatchedResponse;

/*
 * The fullpay protocol for the clients
 */
class protocol : public std::enable_shared_from_this<protocol> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::ip::tcp::endpoint endpoint_t;
    typedef asio::io_context io_ctx_t;
    typedef asio::ip::tcp::resolver resolver_t;

    static logging::Logger logger;

friend class conn_handler;

// ASIO related stuff
private:
    io_ctx_t& m_io_ctx_;
    asio::basic_waitable_timer<std::chrono::steady_clock> timer;
    asio::basic_waitable_timer<std::chrono::steady_clock> tx_timer;

private:
    // Mutex to guard m_conn_ and num_conns
    std::mutex m_conn_mutex_;
    size_t num_conns = 0;
    // WARNING: Access m_conn_ or num_conns only after acquiring m_conn_mutex_
    std::unordered_map<size_t, 
        std::unordered_map<uint16_t, 
            sharding::client::conn_handler_ptr>> m_conn_;
    // Mutex to guard responses
    std::mutex m_resp_mtx_;
    // WARNING: Access m_conn_ only after acquiring m_conn_mutex_
    MatchedResponse matched_response;

// BFT data
private:
    std::vector<std::unordered_map<uint16_t, asio::ip::tcp::endpoint>>
                                                    m_node_map_;
    std::unique_ptr<utt_bft::client::Params> m_params_ = nullptr;
    std::unique_ptr<libutt::Wallet> m_wallet_send_ = nullptr;
    std::unique_ptr<libutt::Wallet> m_wallet_recv_ = nullptr;
    std::shared_ptr<sharding::common::PublicKeyMap> pk_map = nullptr;
    std::vector<uint8_t> out_msg_buf;

// Other data
private:
    uint16_t experiment_idx;
    uint16_t responses_waiting = 0;
    std::unordered_map<decltype(experiment_idx), libutt::Tx> tx_map;

// Metrics
private:
    std::mutex m_metric_mtx_;
    concordUtils::Histogram hist;
    uint64_t begin, throughput_begin;


// Functions
public:
    protocol(asio::io_context& io_ctx, 
                std::vector<bft::communication::NodeMap>& node_map,
                utt_bft::client::Params &&m_params_);

    // Start the protocol loop
    void start();

    // Add a connection
    void add_connection(sharding::client::conn_handler_ptr conn_ptr);

    // Adds a response
    void add_burn_response(uint8_t* ptr, size_t data, uint16_t id,size_t shard_id);
    void add_mint_response(uint8_t* ptr, size_t data, uint16_t id,size_t shard_id);

    // Start the experiment
    void start_experiments();

    // Send the transaction
    void send_burn_tx();
    void send_mint_tx(std::unordered_map<size_t, std::vector<sharding::common::Response>>&& responses);

    void on_timeout(const asio::error_code& err);
    void on_tx_timeout(const asio::error_code err);
};

} // namespace fullpay::client