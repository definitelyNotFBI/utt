#pragma once

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/system/system_error.hpp>
#include <cstdint>
#include <memory>
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "quickpay/TesterClient/protocol.hpp"
#include "common.hpp"

namespace quickpay::client {

class conn_handler : public std::enable_shared_from_this<conn_handler> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::io_context io_ctx_t;

public:
    // constructor to create a connection
    conn_handler(io_ctx_t& io_ctx, uint16_t id, std::shared_ptr<protocol> proto): mSock_{io_ctx}, m_replica_id_{id}, m_proto_{proto} {}

    // creating a pointer
    static conn_handler_ptr create(io_ctx_t& io_ctx, uint16_t id, std::shared_ptr<protocol> proto)
    {
        return conn_handler_ptr(new conn_handler(io_ctx, id, proto));
    }

    // things to do when we have a new connection
    void on_new_conn(const asio::error_code err);

    // start the connection
    void start_conn();

private:
    sock_t mSock_;
    uint16_t m_replica_id_;
    std::shared_ptr<protocol> m_proto_;
private:
    static logging::Logger logger;

public:
    // Get the socket
    asio::ip::tcp::socket& socket()
    {
        return mSock_;
    }

};


}