#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/streambuf.hpp>
#include <functional>
#include <iostream>
#include <string>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "assertUtils.hpp"
#include "misc.hpp"
#include "msg/QuickPay.hpp"
#include "conn.hpp"
#include "config.hpp"

#include "sha_hash.hpp"
#include "threshsign/IThresholdSigner.h"
#include "utt/PolyCrypto.h"
#include "utt/Tx.h"
#include "utt/internal/PicoSha2.h"

namespace fullpay::ft::replica {

logging::Logger conn_handler::logger = logging::getLogger("quickpay.replica.conn");

bool conn_handler::check_tx(const QuickPayTx* qp_tx, const libutt::Tx& tx)
{
    auto* qp_msg = qp_tx->getQPMsg();
    LOG_DEBUG(logger, "QP Tx: " << std::endl 
                        << "target_shard_id: " << qp_msg->target_shard_id << std::endl 
                        << "hash len: " << qp_msg->hash_len << std::endl
                        << "qp msg len: " << qp_tx->qp_msg_len << std::endl
                        << "tx len:" << qp_tx->tx_len << std::endl
                        );

    if(!tx.validate(m_params_->p, m_params_->main_pk, m_params_->reg_pk)) {
        LOG_ERROR(logger, "Full pay validation failed");
        return false;
    }
    LOG_INFO(logger, "Got a new valid quick pay transaction");
    // Check DB
    std::string value;
    for(auto& null: tx.getNullifiers()) {
        // bool found;
        bool found;
        bool key_may_exist = m_db_->rawDB().KeyMayExist(
                                        rocksdb::ReadOptions{},
                                        m_db_->defaultColumnFamilyHandle(), 
                                        null,
                                        &value, 
                                        &found);
        if(!key_may_exist) {
            continue;
        }
        if (found) {
            return false;
        }
        // The key may exist, query the DB to check
        auto status = m_db_->rawDB().Get(rocksdb::ReadOptions{}, 
                                            m_db_->defaultColumnFamilyHandle(),
                                            null, &value);
        if (!status.IsNotFound()) {
            // The key exists, abort
            return false;
            break;
        }
    }
    return true;
}

void conn_handler::do_read(const asio::error_code& err, size_t bytes)
{
    if (err) {
        LOG_ERROR(logger, "Got error " << err.message());
        return;
    }
    LOG_INFO(logger, "Got " << bytes << " data from client " << id);
    // First reserve the space to copy the new messages
    internal_msg_buf.reserve(received_bytes + bytes);
    // Copy the received bytes
    internal_msg_buf.insert(internal_msg_buf.begin()+static_cast<long>(received_bytes), 
                                incoming_msg_buf.begin(),
                                incoming_msg_buf.begin()+bytes);
    // Update the # of received bytes
    received_bytes += bytes;
    LOG_INFO(logger, "Received bytes var: " << received_bytes);
    // Check if we have sufficient bytes to process
    if(received_bytes < sizeof(Msg)) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << received_bytes << "] for a Msg header[" 
                            << sizeof(Msg) << "], will try again");
        on_new_conn();
        return;
    }
    auto msg = (Msg*)internal_msg_buf.data();
    if(received_bytes < msg->msg_len) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << received_bytes << "] for a full Msg:" 
                            << msg->msg_len << ", will try again");
        on_new_conn();
        return;
    }
    // Save these as the underlying internal_msg_buf will change soon
    auto msg_len = msg->get_size();
    auto tp = msg->tp;
    received_bytes -= msg_len;
    // Remove one msg worth of data
    std::vector<uint8_t> msg_buf;
    msg_buf.resize(msg_len, 0);
    std::memcpy(
            msg_buf.data(), 
            internal_msg_buf.data(), 
            msg_len
    );
    // Reorder the internal_msg_buf
    std::rotate(internal_msg_buf.begin(), 
                    internal_msg_buf.begin()+static_cast<long>(msg_len), 
                    internal_msg_buf.end()
    );
    LOG_DEBUG(logger, "Got msg type: " << static_cast<uint8_t>(tp));
    // Handle msg
    if (tp == MsgType::BURN_REQUEST) {
        on_new_tx(msg_buf);
    } else if (tp == MsgType::ACK_MSG) {
        on_new_ack(msg_buf);
    }
    on_new_conn();
}

void conn_handler::on_new_ack(std::vector<uint8_t> msg_buf) {
    // DONE: Handle ack msg
    LOG_DEBUG(logger, "Got an Ack msg");
    auto msg = (Msg*)msg_buf.data();
    auto* ack_msg = msg->get_msg<const FullPayFTAck>();
    auto data = std::string{internal_msg_buf.begin()+sizeof(Msg), internal_msg_buf.begin()+static_cast<long>(msg->msg_len)};
    m_db_->rawDB().Put(rocksdb::WriteOptions{}, 
                            std::to_string(id)+"-" + std::to_string(ack_msg->seq_num),
                            data);
    
    std::vector<uint8_t> out_msg_buf;
    auto rsa_len = signer->signatureLength();
    auto ack_len = AckResponse::get_size(rsa_len);
    auto msg_len = Msg::get_size(ack_len);
    out_msg_buf.resize(msg_len, 0);
    auto out_msg = (Msg*)out_msg_buf.data();
    out_msg->msg_len = ack_len;
    out_msg->tp = MsgType::ACK_RESPONSE;
    out_msg->seq_num = msg->seq_num;
    send_ack_response(out_msg_buf);
}

void conn_handler::on_new_tx(std::vector<uint8_t> msg_buf) {
    LOG_DEBUG(logger, "Got a new tx " << msg_buf.size());

    auto msg = (Msg*)msg_buf.data();
    auto* qp_tx = msg->get_msg<const QuickPayTx>();
    std::stringstream ss;
    ss.write(reinterpret_cast<const char*>(qp_tx->getTxBuf()), static_cast<long>(qp_tx->tx_len));
        
    libutt::Tx tx(ss);
    if (!check_tx(qp_tx, tx)) {
        return;
    }

    // Burn the coin
    for(auto& nullif: tx.getNullifiers()) {
        m_db_->rawDB().Put(rocksdb::WriteOptions{}, 
                                nullif + std::to_string(nullif_ctr++), 
                                std::string());
    }

    // generate and send signature
    ss.str("");
    // DONE: Process the tx (The coins are still unspent)
    for(size_t txoIdx = 0; txoIdx < tx.outs.size(); txoIdx++) {
        auto sig = tx.shareSignCoin(txoIdx, m_params_->my_sk);
        ss << sig << std::endl;
    }
    auto ss_str = ss.str();
    auto rsa_sig_len = signer->signatureLength();
    auto resp_len = FullPayFTResponse::get_size(ss_str.size(), rsa_sig_len);
    auto msg_len = Msg::get_size(resp_len);
    LOG_DEBUG(logger, "Response Len: " << resp_len);
    LOG_DEBUG(logger, "Sig Len: " << ss_str.size());
    LOG_DEBUG(logger, "RSA Sig Len: " << rsa_sig_len);
    outgoing_msg_buf.resize(msg_len, 0);
    auto msg_out = (Msg*)outgoing_msg_buf.data();
    msg_out->msg_len = resp_len;
    msg_out->seq_num = msg->seq_num;
    msg_out->tp = MsgType::BURN_RESPONSE;
    auto resp = msg_out->get_msg<FullPayFTResponse>();
    resp->sig_len = ss_str.size();
    resp->rsa_len = rsa_sig_len;

    LOG_DEBUG(logger, "(B) Resp Sig Len: " << resp->sig_len);
    LOG_DEBUG(logger, "(B) Resp RSA Len: " << resp->rsa_len);

    auto tx_hash = tx.getHashHex();
    auto txhash = concord::util::SHA3_256().digest(
        (uint8_t*)tx_hash.data(), 
        tx_hash.size()
    );
    auto sig_status = signer->sign(
        reinterpret_cast<const char*>(txhash.data()), 
        txhash.size(), 
        reinterpret_cast<char*>(resp->getRSABuf()), 
        resp->rsa_len, 
        rsa_sig_len
    );
    // Not expecting any surprises here
    assert(sig_status);
    assert(rsa_sig_len <= resp->rsa_len);

    LOG_DEBUG(logger, "(A) Resp Sig Len: " << resp->sig_len);
    LOG_DEBUG(logger, "(A) Resp RSA Len: " << resp->rsa_len);
    std::memcpy(resp->getSigBuf(), ss_str.data(), resp->sig_len);
    send_response(msg_out->get_size());
    // auto qp_len = QuickPayMsg::get_size(txhash.size());
    // auto resp_size = QuickPayResponse::get_size(qp_len, sig_len);
    // outgoing_msg_buf.reserve(resp_size);
    // auto qp_resp = (QuickPayResponse*)outgoing_msg_buf.data();
    // qp_resp->qp_msg_len = qp_len;
    // qp_resp->sig_len = sig_len;
    // auto* qp = qp_resp->getQPMsg();
    // qp->target_shard_id = 0;
    // qp->hash_len = txhash.size();
    // std::memcpy(qp->getHashBuf(), txhash.data(), txhash.size());
    // signer->signData((const char*)txhash.data(), 
    //                     txhash.size(), 
    //                     (char*)qp_resp->getSigBuf(), 
    //                     sig_len);
    // send_response(qp_resp->get_size());
    metrics->fetch_add(1);
}

void conn_handler::send_response(size_t bytes) {
    this->mSock_.async_send(
        asio::buffer(outgoing_msg_buf.data(), bytes),
        [](const asio::error_code& err, size_t bytes) {
            if (err) {
                LOG_ERROR(logger, err.message());
                return;
            }
            LOG_INFO(logger, "Sent " << bytes << " data");
        }
    );
}

void conn_handler::send_ack_response(std::vector<uint8_t> msg_buf)
{
    this->mSock_.async_send(
        asio::buffer(msg_buf.data(), msg_buf.size()),
        [](const asio::error_code& err, size_t bytes) {
            if (err) {
                LOG_ERROR(logger, err.message());
                return;
            }
            LOG_INFO(logger, "Sent " << bytes << " data");
        }
    );
}

void conn_handler::on_new_conn() {
    this->mSock_.async_receive(
        asio::buffer(incoming_msg_buf.data(), incoming_msg_buf.size()),
        std::bind(&conn_handler::do_read, shared_from_this(), 
            std::placeholders::_1, std::placeholders::_2));
}

} // namespace fullpay::replica