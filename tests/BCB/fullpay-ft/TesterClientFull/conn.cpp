#include <asio/error_code.hpp>
#include <iostream>

#include "Logging4cplus.hpp"
#include "conn.hpp"
#include "msg/QuickPay.hpp"
#include "protocol.hpp"
#include "sha_hash.hpp"

namespace fullpay::ft::client {

logging::Logger conn_handler::logger = logging::getLogger("fullpay.ft.client.conn");

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
        LOG_ERROR(logger, "Failed to send tx to replica" << getid());
    } else {
        LOG_INFO(logger, "Successfully sent " << sent << " bytes of tx to replica" << getid());
    }
}

void conn_handler::on_ack_send(const asio::error_code& err, size_t sent) {
    if (err) {
        LOG_ERROR(logger, "Failed to send message to replica" << getid());
    } else {
        LOG_INFO(logger, "Successfully sent " << sent << "message to replica" << getid());
    }
    // Wait until all the things are sent
    // TODO: send tx
    // m_proto_->send_tx();
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
    received_bytes += got;
    // DONE: Check if sufficient bytes have been received
    if(received_bytes < sizeof(FullPayFTResponse)) {
        // DONE: Arm the handler
        mSock_.async_receive(
            asio::buffer(replica_msg_buf.data()+received_bytes, BCB::common::REPLICA_MAX_MSG_SIZE),
            std::bind(&conn_handler::on_tx_response, shared_from_this(),
                std::placeholders::_1, std::placeholders::_2, experiment_idx)
        );
        LOG_DEBUG(logger, "Got insufficient data");
        return;
    }
    auto resp = (FullPayFTResponse*)replica_msg_buf.data();
    if(received_bytes < resp->get_size()) {
        // DONE: Arm the handler
        mSock_.async_receive(
            asio::buffer(replica_msg_buf.data()+received_bytes, BCB::common::REPLICA_MAX_MSG_SIZE),
            std::bind(&conn_handler::on_tx_response, shared_from_this(),
                std::placeholders::_1, std::placeholders::_2, experiment_idx)
        );
        LOG_DEBUG(logger, "Got insufficient data");
        return;
    }
    received_bytes -= resp->get_size();
    // DONE: Check response
    auto sender_id = getid();
    // This is done here to check things in parallel
    // DONE: Check the response
    std::stringstream ss{std::string{}};
    ss.write(reinterpret_cast<const char*>(resp->getSigBuf()), 
                    static_cast<long>(resp->sig_len));
    auto tx = m_proto_->tx_map[experiment_idx];
    auto invalid_response = false;
    for(size_t i=0;i<3;i++) {
        libutt::RandSigShare sig;
        ss >> sig;
        libff::consume_OUTPUT_NEWLINE(ss);
        // DONE: Check the validity of the signature
        if (!tx.verifySigShare(i, sig, m_proto_->m_params_->bank_pks.at(sender_id))) {
            invalid_response = true;
            LOG_WARN(logger, "Signature check failed for Tx#" << experiment_idx 
                                << ", replica #" << sender_id
                                << ", idx#" << i);
            break;
        }
    }
    if (invalid_response) {
        return;
    }
    auto txhash = tx.getHashHex();
    auto tx_hash = concord::util::SHA3_256().digest((uint8_t*)txhash.data(), 
                                                    txhash.size());
    invalid_response = !(pk_map.at(sender_id)->verify(
        reinterpret_cast<const char*>(tx_hash.data()), 
        tx_hash.size(), 
        reinterpret_cast<const char*>(resp->getRSABuf()), 
        resp->rsa_len
    ));
    if(invalid_response) {
        LOG_WARN(logger, "Got invalid RSA signature");
        return;
    }
    m_proto_->add_response(
        replica_msg_buf.data(), 
        got, 
        getid(), 
        experiment_idx);
}

} // namespace fullpay::ft::client