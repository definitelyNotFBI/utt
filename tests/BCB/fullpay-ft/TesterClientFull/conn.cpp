#include <algorithm>
#include <asio/error_code.hpp>
#include <iostream>
#include <optional>

#include "Logging4cplus.hpp"
#include "TesterClientFull/config.hpp"
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
    start_conn();
    // Tell the protocol that we have a new connection
    m_proto_->add_connection(shared_from_this());
}

void conn_handler::start_conn()
{
    // Setup receive handler for every connected server
    auto& sock = mSock_;
    sock.async_receive(
        asio::buffer(replica_msg_buf.data()+received_bytes, BCB::common::REPLICA_MAX_MSG_SIZE),
        std::bind(&conn_handler::do_read, 
            shared_from_this(),
            std::placeholders::_1, std::placeholders::_2)
    );
}

void conn_handler::do_read(const asio::error_code& err, size_t got)
{
    if (err) {
        LOG_ERROR(logger, 
            "Failed to receive message from replica " << getid() << 
            " with error: " << err.message()
        );
        return;
    } else {
        LOG_INFO(logger, "Successfully received " << got 
            << " message from replica " << getid());
    }
    received_bytes += got;
    // DONE: Check if sufficient bytes have been received
    if(received_bytes < sizeof(Msg)) {
        // DONE: Arm the handler
        mSock_.async_receive(
            asio::buffer(replica_msg_buf.data()+received_bytes, BCB::common::REPLICA_MAX_MSG_SIZE),
            std::bind(&conn_handler::do_read, shared_from_this(),
                std::placeholders::_1, std::placeholders::_2)
        );
        LOG_DEBUG(logger, "Got insufficient data");
        return;
    }
    auto msg = (Msg*)replica_msg_buf.data();
    if(received_bytes < msg->get_size()) {
        // DONE: Arm the handler
        mSock_.async_receive(
            asio::buffer(replica_msg_buf.data()+received_bytes, BCB::common::REPLICA_MAX_MSG_SIZE),
            std::bind(&conn_handler::do_read, shared_from_this(),
                std::placeholders::_1, std::placeholders::_2)
        );
        LOG_DEBUG(logger, "Got insufficient data");
        return;
    }
    size_t msg_len = msg->get_size();
    std::vector<uint8_t> msg_buf(replica_msg_buf.begin(), replica_msg_buf.begin()+(long)msg_len);
    std::rotate(replica_msg_buf.begin(), 
                    replica_msg_buf.begin()+static_cast<long>(msg_len), 
                    replica_msg_buf.begin()+static_cast<long>(received_bytes));
    received_bytes = received_bytes - msg_len;
    msg = (Msg*)msg_buf.data();
    LOG_DEBUG(logger, "Msg size " << msg->get_size() << " for replica " << getid());
    LOG_DEBUG(logger, "Checking received bytes " << received_bytes << " for replica " << getid());
    auto tp = msg->tp;
    auto seq_num = msg->seq_num;
    if(tp == MsgType::BURN_RESPONSE) {
        on_tx_response(msg_buf);
    } else if (tp == MsgType::ACK_RESPONSE) {
        m_proto_->add_ack(getid(), seq_num);
    }
    // Recursively process messages
    if(received_bytes > 0) {
        do_read(err, 0);
    } else {
        // Otherwise, the above part will add start_conn
        start_conn();
    }
}

void conn_handler::send_msg(const std::vector<uint8_t>& msg_buf, Type tp)
{
    LOG_DEBUG(logger, "Adding to the queue");
    {
        m_msg_queue_mtx_.lock();
        msg_queue.push(std::make_pair(msg_buf, tp));
        m_msg_queue_mtx_.unlock();
    }
    try_send();
}

void conn_handler::try_send() 
{
    std::optional<std::pair<std::vector<uint8_t>, Type>> msg_buf = std::nullopt;
    {
        m_msg_queue_mtx_.lock();
        if (!msg_queue.empty()) {
            msg_buf = msg_queue.front();
            msg_queue.pop();
        }
        m_msg_queue_mtx_.unlock();
    }

    if(msg_buf == std::nullopt) {
        LOG_DEBUG(logger, "Nothing to send to " << getid());
        return;
    }
    LOG_DEBUG(logger, "Something to send to " << getid());
    msg = msg_buf.value().first;
    auto tp = msg_buf->second;

    auto& sock = mSock_;
    sock.async_send(
        asio::buffer(msg, msg.size()),
        std::bind(&conn_handler::on_msg_send, 
            shared_from_this(), std::placeholders::_1,
            std::placeholders::_2, tp));
}

void conn_handler::on_msg_send(const asio::error_code& err, size_t sent, Type tp) {
    if (err) {
        LOG_ERROR(logger, "Failed to send message to replica" << getid());
        return;
    }
    LOG_INFO(logger, "Successfully sent " << sent << "message to replica" << getid());
    try_send();
    // if (tp == Type::TX) {
    //     return;
    // } else if (tp == Type::ACK) {
    //     currentExpIdx -= 1;
    //     m_proto_->add_ack(getid(), currentExpIdx+1);
    // }
    // Wait until all the acks are sent
}

void conn_handler::on_tx_response(std::vector<uint8_t> msg_buf) {
    auto msg = (Msg*)msg_buf.data();
    auto resp = msg->get_msg<FullPayFTResponse>();
    // DONE: Check response
    auto sender_id = getid();
    // This is done here to check things in parallel
    // DONE: Check the response
    std::stringstream ss{std::string{}};
    LOG_DEBUG(logger, "Checking sigs for " << msg->seq_num);
    ss.write(reinterpret_cast<const char*>(resp->getSigBuf()), 
                    static_cast<long>(resp->sig_len));
    auto tx = m_proto_->tx_map[msg->seq_num];
    auto invalid_response = false;
    for(size_t i=0;i<tx.outs.size(); i++) {
        libutt::RandSigShare sig;
        ss >> sig;
        libff::consume_OUTPUT_NEWLINE(ss);
        // DONE: Check the validity of the signature
        if (!tx.verifySigShare(i, sig, m_proto_->m_params_->bank_pks.at(sender_id))) {
            // invalid_response = true;
            LOG_WARN(logger, "Signature check failed for Tx#" << msg->seq_num 
                                << ", replica #" << sender_id
                                << ", idx#" << i);
            // break;
        }
    }
    // if (invalid_response) {
    //     return;
    // }
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
    LOG_DEBUG(logger, "Adding a response from " << getid() << " for tx#" << msg->seq_num);
    std::vector<uint8_t> response(
        msg_buf.begin()+sizeof(Msg),
        msg_buf.end()
    );
    m_proto_->add_response(
        response,
        getid(), 
        msg->seq_num);
}

} // namespace fullpay::ft::client