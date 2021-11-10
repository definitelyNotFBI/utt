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
#include "Logger.hpp"
#include "Logging4cplus.hpp"

namespace quickpay::client {

namespace ba = asio;

/*
 * On connecting to a new client, this is used to establish
 */
class conn_handler;
typedef boost::shared_ptr<conn_handler> conn_handler_ptr;

class conn_handler : public boost::enable_shared_from_this<conn_handler> {
    typedef ba::ip::tcp::socket sock_t;
    typedef ba::io_context io_ctx_t;

public:
    // constructor to create a connection
    conn_handler(io_ctx_t& io_ctx, uint16_t id): mSock_(io_ctx), m_replica_id_(id) {}

    // creating a pointer
    static conn_handler_ptr create(io_ctx_t& io_ctx, uint16_t id)
    {
        return conn_handler_ptr(new conn_handler(io_ctx, id));
    }

    // things to do when we have a new connection
    void on_new_conn(const asio::error_code err);

    // start the connection
    void start_conn();

private:
    sock_t mSock_;
    uint16_t m_replica_id_;
private:
    static logging::Logger logger;

public:
    // Get the socket
    ba::ip::tcp::socket& socket()
    {
        return mSock_;
    }

};


}