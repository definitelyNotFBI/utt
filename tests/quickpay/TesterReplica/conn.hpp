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

namespace quickpay::replica {

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

public:
    // constructor to create a connection
    conn_handler(io_ctx_t& io_ctx): mSock_(io_ctx) {}

    // creating a pointer
    static conn_handler_ptr create(io_ctx_t& io_ctx)
    {
        return conn_handler_ptr(new conn_handler(io_ctx));
    }

    // things to do when we have a new connection
    void on_new_conn();

    // start the connection
    void start_conn();

private:
    sock_t mSock_;

public:
    // Get the socket
    ba::ip::tcp::socket& socket()
    {
        return mSock_;
    }

};


}