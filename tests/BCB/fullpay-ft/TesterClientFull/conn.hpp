#pragma once

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include "Logger.hpp"
#include "Logging4cplus.hpp"

#include "TesterClientFull/config.hpp"
#include "common/defs.hpp"
#include "msg/QuickPay.hpp"
#include "protocol.hpp"
#include "common.hpp"

namespace fullpay::ft::client {

enum class Type : uint8_t {
    TX,
    ACK,
};

enum class SendState: uint8_t {
    UNKNOWN,
    READY,
    SENDING,
};

class conn_handler : public std::enable_shared_from_this<conn_handler> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::io_context io_ctx_t;
    typedef bftEngine::impl::RSAVerifier verifier_t;

friend class protocol;

public:
    // constructor to create a connection
    conn_handler(io_ctx_t& io_ctx, 
                    uint16_t id, 
                    BCB::common::PublicKeyMap pk_map,
                    std::shared_ptr<protocol> proto)
                : 
                    mSock_{io_ctx}, 
                    m_replica_id_{id}, 
                    m_proto_{proto}, 
                    replica_msg_buf(BCB::common::REPLICA_MAX_MSG_SIZE),
                    pk_map{std::move(pk_map)}
            {}

    // creating a pointer
    static conn_handler_ptr create(io_ctx_t& io_ctx, 
                    uint16_t id, 
                    BCB::common::PublicKeyMap pk_map,
                    std::shared_ptr<protocol> proto
                )
    {
        return conn_handler_ptr(new conn_handler(io_ctx, id, std::move(pk_map), proto));
    }

    // things to do when we have a new connection
    void on_new_conn(const asio::error_code err);

    // start the connection
    void start_conn();

    void send_msg(const std::vector<uint8_t>& msg_buf, Type tp);
    void send_ack_msg(const std::vector<uint8_t>& msg_buf, size_t experiment_idx);
    // to call after sending a transaction
    void on_msg_send(const asio::error_code& err, size_t sent, Type tp);

    // to call after receiving a response for a transaction
    void on_tx_response(std::vector<uint8_t>);
    void on_ack_response(std::vector<uint8_t>);

    // Perform reading
    void do_read(const asio::error_code& err, size_t sent);
private: 
    void try_send();

private:
    std::mutex m_ack_map_mtx_;
    std::unordered_map<size_t, std::vector<uint8_t>> ack_map;

private:
    sock_t mSock_;
    uint16_t m_replica_id_;
    std::shared_ptr<protocol> m_proto_;

private:
    // To send messages
    std::mutex m_msg_queue_mtx_;
    // WARNING Access this only using m_msg_queue_mtx_
    std::queue<std::pair<std::vector<uint8_t>, Type>> msg_queue;
    std::vector<uint8_t> msg;

private:
    // To receive messages
    std::vector<uint8_t> replica_msg_buf = std::vector<uint8_t>(BCB::common::REPLICA_MAX_MSG_SIZE);
    size_t received_bytes = 0;
    BCB::common::PublicKeyMap pk_map;

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