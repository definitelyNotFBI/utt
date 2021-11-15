#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <asio/basic_waitable_timer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <asio.hpp>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "communication/CommDefs.hpp"
#include "common.hpp"
#include "quickpay/TesterClient/conn.hpp"
#include "client/Params.hpp"

namespace quickpay::client {

/*
 * The quickpay protocol for the replicas
 */
class protocol : public std::enable_shared_from_this<protocol> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::ip::tcp::endpoint endpoint_t;
    typedef asio::io_context io_ctx_t;
    typedef asio::ip::tcp::resolver resolver_t;

    static logging::Logger logger;

// ASIO related stuff
private:
    io_ctx_t& m_io_ctx_;
    asio::basic_waitable_timer<std::chrono::steady_clock> timer;

private:
    // Mutex to guard m_conn_
    std::mutex m_conn_mutex_;
    // WARNING: Access m_conn_ only after acquiring m_conn_mutex_
    std::vector<conn_handler_ptr> m_conn_;

// BFT data
private:
    std::unordered_map<uint16_t, asio::ip::tcp::endpoint> m_node_map_;
    std::unique_ptr<utt_bft::client::Params> m_params_ = nullptr;

// Functions
public:
    protocol(asio::io_context& io_ctx, bft::communication::NodeMap& node_map);

    // Start the protocol loop
    void start();

    // Add a connection
    void add_connection(conn_handler_ptr conn_ptr);

    // Start the experiment
    void start_experiments();
};

} // namespace quickpay::replica