#pragma once

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <memory>
#include "Logger.hpp"
#include "Logging4cplus.hpp"

#include "common/defs.hpp"
#include "protocol.hpp"
#include "common.hpp"

namespace fullpay::ft::client {

class conn_handler : public std::enable_shared_from_this<conn_handler> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::io_context io_ctx_t;

friend class protocol;

public:
    // constructor to create a connection
    conn_handler(io_ctx_t& io_ctx, 
                    uint16_t id, 
                    std::shared_ptr<BCB::common::PublicKeyMap> pk_map,
                    std::shared_ptr<protocol> proto)
                : 
                    mSock_{io_ctx}, 
                    m_replica_id_{id}, 
                    m_proto_{proto}, 
                    out_ss(""), 
                    replica_msg_buf(BCB::common::REPLICA_MAX_MSG_SIZE),
                    pk_map{pk_map} 
            {}

    // creating a pointer
    static conn_handler_ptr create(io_ctx_t& io_ctx, 
                    uint16_t id, 
                    std::shared_ptr<BCB::common::PublicKeyMap> pk_map,
                    std::shared_ptr<protocol> proto
                )
    {
        return conn_handler_ptr(new conn_handler(io_ctx, id, pk_map, proto));
    }

    // things to do when we have a new connection
    void on_new_conn(const asio::error_code err);

    // start the connection
    void start_conn();

    // to call after sending a transaction
    void on_tx_send(const asio::error_code& err, size_t sen);
    // to call after receiving a response for a transaction
    void on_tx_response(const asio::error_code& err, size_t sen);

    // Perform reading
    void do_read();

private:
    sock_t mSock_;
    uint16_t m_replica_id_;
    std::shared_ptr<protocol> m_proto_;
    std::stringstream out_ss;
    std::vector<uint8_t> replica_msg_buf;
    std::shared_ptr<BCB::common::PublicKeyMap> pk_map;

private:
    static logging::Logger logger;

public:
    // Get the socket
    asio::ip::tcp::socket& socket()
    {
        return mSock_;
    }

    // Get the id
    decltype(m_replica_id_) getid() const { return m_replica_id_; }

};


}