#pragma once

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <sstream>
#include <vector>
#include "Logging4cplus.hpp"
#include "quickpay/TesterClient/common.hpp"

namespace quickpay::replica {

/*
 * On connecting to a new client, this is used to establish
 */
class conn_handler;
typedef std::shared_ptr<conn_handler> conn_handler_ptr;

class conn_handler : public std::enable_shared_from_this<conn_handler> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::io_context io_ctx_t;

public:
    // constructor to create a connection
    conn_handler(io_ctx_t& io_ctx, long id): 
        mSock_(io_ctx), incoming_msg_buf(client::REPLICA_MAX_MSG_SIZE), 
        id{id} {}

    // creating a pointer
    static conn_handler_ptr create(io_ctx_t& io_ctx, long id)
    {
        return conn_handler_ptr(new conn_handler(io_ctx, id));
    }

    // things to do when we have a new connection
    void on_new_conn();

    // start the connection
    void start_conn();

    // Read data from clients
    void do_read(const asio::error_code& err, size_t bytes);

    // Send the replica of the response
    void send_response();

private:
    sock_t mSock_;
    std::stringstream out_ss;
    std::vector<uint8_t> incoming_msg_buf;

private:
    static logging::Logger logger;

public:
    long id;
    // Get the socket
    asio::ip::tcp::socket& socket()
    {
        return mSock_;
    }

};


}