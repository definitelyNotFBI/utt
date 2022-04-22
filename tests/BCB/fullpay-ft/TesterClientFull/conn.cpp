#include <asio/error_code.hpp>
#include <iostream>

#include "Logging4cplus.hpp"
#include "conn.hpp"
#include "protocol.hpp"

namespace fullpay::ft::client {

logging::Logger conn_handler::logger = logging::getLogger("quickpay.client.conn");

void conn_handler::on_new_conn(const asio::error_code err) {
    if (err) {
        LOG_ERROR(logger, "Client connection error:" << err.message());
        return;
    }
    if (!mSock_.is_open()) {
        LOG_ERROR(logger, "Client connected but the socket is not open");
        return;
    }
    LOG_INFO(logger, "Client is now connected to Replica " << m_replica_id_);
    // Tell the protocol that we have a new connection
    m_proto_->add_connection(shared_from_this());
}

void conn_handler::on_tx_send(const asio::error_code& err, size_t sent) {
    if (err) {
        LOG_ERROR(logger, "Failed to send message to replica" << getid());
    } else {
        LOG_INFO(logger, "Successfully sent " << sent << "message to replica" << getid());
    }
}

void conn_handler::on_tx_response(const asio::error_code& err, size_t got, size_t experiment_idx) {
    if (err) {
        LOG_ERROR(logger, 
            "Failed to receive message from replica " << getid() << 
            " with error: " << err.message() << 
            " for tx#:" << experiment_idx
        );
        return;
    } else {
        LOG_INFO(logger, "Successfully received " << got 
            << " message from replica " << getid()
            << " for tx#:" << experiment_idx);
    }
    m_proto_->add_response(replica_msg_buf.data(), got, getid(), experiment_idx);
}

} // namespace quickpay::replica