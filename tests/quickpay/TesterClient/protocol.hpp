#pragma once

#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "communication/CommDefs.hpp"
#include <asio.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <boost/enable_shared_from_this.hpp>

namespace quickpay::client {
namespace ba = asio;

/*
 * The quickpay protocol for the replicas
 */
class protocol : public boost::enable_shared_from_this<protocol> {
    typedef ba::ip::tcp::socket sock_t;
    typedef ba::ip::tcp::endpoint endpoint_t;
    typedef ba::io_context io_ctx_t;
    typedef ba::ip::tcp::resolver resolver_t;

    static logging::Logger logger;

private:
    io_ctx_t& m_io_ctx_;
    std::unordered_map<uint16_t, ba::ip::tcp::endpoint> m_node_map_;

public:
    protocol(ba::io_context& io_ctx, bft::communication::NodeMap& node_map);

    // Start the protocol loop
    void start();

public:
    std::mutex conn_lock;

private:
    // Connects to n-f nodes
    void wait_for_connections();
};

} // namespace quickpay::replica