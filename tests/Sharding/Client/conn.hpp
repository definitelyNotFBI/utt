#pragma once

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <memory>
#include "Crypto.hpp"
#include "Logger.hpp"
#include "Logging4cplus.hpp"

#include "common/defs.hpp"
#include "protocol.hpp"
#include "common.hpp"

namespace sharding::client {

class conn_handler : public std::enable_shared_from_this<conn_handler> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::io_context io_ctx_t;

friend class protocol;

public:
    // constructor to create a connection
    conn_handler(io_ctx_t& io_ctx, 
                    uint16_t id, 
                    std::shared_ptr<sharding::common::PublicKeyMap> pk_map,
                    size_t shard_id,
                    std::shared_ptr<sharding::client::protocol> proto)
                : 
                    mSock_{io_ctx}, 
                    m_replica_id_{id}, 
                    shard_id{shard_id},
                    m_proto_{proto}, 
                    out_ss(std::string{}), 
                    replica_msg_buf(sharding::common::REPLICA_MAX_MSG_SIZE),
                    pk_map{pk_map} 
            {}

    // creating a pointer
    static conn_handler_ptr create(io_ctx_t& io_ctx, 
                    uint16_t id, 
                    std::shared_ptr<sharding::common::PublicKeyMap> pk_map,
                    size_t shard_id,
                    std::shared_ptr<sharding::client::protocol> proto
                )
    {
        return conn_handler_ptr(new conn_handler(io_ctx, id, pk_map, shard_id, proto));
    }

    // things to do when we have a new connection
    void on_new_conn(const asio::error_code err);

    // start the connection
    void start_conn();

    // to call after sending a transaction
    void on_burn_send(const asio::error_code& err, size_t sen);
    void on_mint_send(const asio::error_code& err, size_t sen);
    // to call after receiving a response for a transaction
    void on_burn_response(const asio::error_code& err, size_t sen);
    void on_mint_response(const asio::error_code& err, size_t sen);

    // Perform reading
    void do_read();

private:
    sock_t mSock_;
    uint16_t m_replica_id_;
    size_t shard_id;
    std::shared_ptr<sharding::client::protocol> m_proto_;
    std::stringstream out_ss;
    // A buffer to collect messages from the connected replica
    std::vector<uint8_t> replica_msg_buf;
    size_t bytes_received_so_far = 0;
    std::shared_ptr<sharding::common::PublicKeyMap> pk_map;

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

    // Get shard id
    decltype(shard_id) get_shard_id() const { return shard_id; }

};


}