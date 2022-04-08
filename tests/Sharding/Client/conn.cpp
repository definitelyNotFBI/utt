#include <asio/error_code.hpp>
#include <iostream>

#include "Logging4cplus.hpp"
#include "conn.hpp"
#include "msg/ShardTx.hpp"
#include "protocol.hpp"

namespace sharding::client {

logging::Logger conn_handler::logger = logging::getLogger("sharding.bcb.client.conn");

void conn_handler::on_new_conn(const asio::error_code err) {
    if (err) {
        LOG_ERROR(logger, "Client connection error:" << err.message());
        return;
    }
    if (!mSock_.is_open()) {
        LOG_ERROR(logger, "Client connected but the socket is not open");
        return;
    }
    LOG_INFO(logger, "Client is now connected to Replica " << m_replica_id_ << " in shard#" << shard_id);
    // Tell the protocol that we have a new connection
    m_proto_->add_connection(shared_from_this());
}

void conn_handler::on_burn_send(const asio::error_code& err, size_t sent) {
    if (err) {
        LOG_ERROR(logger, "Failed to send burn message to replica" << getid() 
                                << " in shard " << shard_id);
    } else {
        LOG_INFO(logger, "Successfully sent " << sent 
                            << " burn message to replica" << getid() 
                            << " in shard" << shard_id);
    }
}

void conn_handler::on_mint_send(const asio::error_code& err, size_t sent) {
    if (err) {
        LOG_ERROR(logger, "Failed to send mint message to replica" << getid() 
                                << " in shard " << shard_id);
    } else {
        LOG_INFO(logger, "Successfully sent " << sent 
                            << " mint message to replica" << getid() 
                            << " in shard" << shard_id);
    }
}

void conn_handler::on_burn_response(const asio::error_code& err, size_t got) {
    if (err) {
        LOG_ERROR(logger, "Failed to receive burn message from replica " << getid() << " with error: " << err.message());
        return;
    } else {
        LOG_INFO(logger, "Successfully received " << got << " burn message from replica " << getid());
    }
    // Ensure that the full message has arrived
    // TODO:

    LOG_INFO(logger, "Got " << got << 
                        " from replica " << getid());
    bytes_received_so_far += got;
    if(bytes_received_so_far < sizeof(Msg)) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << bytes_received_so_far << "] for a Msg header[" 
                            << sizeof(Msg) << "], will try again");
        // arm the connection for the next time it is called
        mSock_.async_receive(asio::buffer(replica_msg_buf.data(), sharding::common::REPLICA_MAX_MSG_SIZE),
                            std::bind(&conn_handler::on_burn_response,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    std::placeholders::_2));

        return;
    }

    auto* msg = (const Msg*)replica_msg_buf.data();
    if (bytes_received_so_far < msg->get_size()) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << bytes_received_so_far << "] for a full msg"
                            << msg->get_size() <<", will try again");
        // rearm the connection
        mSock_.async_receive(asio::buffer(replica_msg_buf.data(), sharding::common::REPLICA_MAX_MSG_SIZE),
                            std::bind(&conn_handler::on_burn_response,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    std::placeholders::_2));
        return;
    }
    // We have the full message
    bytes_received_so_far -= msg->get_size();

    m_proto_->add_burn_response(replica_msg_buf.data(), got, getid(), shard_id);
}

void conn_handler::on_mint_response(const asio::error_code& err, size_t got) {
    if (err) {
        LOG_ERROR(logger, "Failed to receive mint message from replica " << getid() << " with error: " << err.message());
        return;
    } else {
        LOG_INFO(logger, "Successfully received " << got << " mint message from replica " << getid());
    }
    LOG_INFO(logger, "Got " << got << 
                        " from replica " << getid());


    bytes_received_so_far += got;
    if(bytes_received_so_far < sizeof(Msg)) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << bytes_received_so_far << "] for a Msg header[" 
                            << sizeof(Msg) << "], will try again");
        // arm the connection for the next time it is called
        mSock_.async_receive(asio::buffer(replica_msg_buf.data(), sharding::common::REPLICA_MAX_MSG_SIZE),
                            std::bind(&conn_handler::on_mint_response,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    std::placeholders::_2));

        return;
    }

    auto* msg = (const Msg*)replica_msg_buf.data();
    if (bytes_received_so_far < msg->get_size()) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << bytes_received_so_far << "] for a full msg"
                            << msg->get_size() <<", will try again");
        // rearm the connection
        mSock_.async_receive(asio::buffer(replica_msg_buf.data(), sharding::common::REPLICA_MAX_MSG_SIZE),
                            std::bind(&conn_handler::on_mint_response,
                                    shared_from_this(),
                                    std::placeholders::_1,
                                    std::placeholders::_2));
        return;
    }
    // We have the full message
    bytes_received_so_far -= msg->get_size();
    m_proto_->add_mint_response(replica_msg_buf.data(), got, getid(), shard_id);
}


} // namespace sharding::client