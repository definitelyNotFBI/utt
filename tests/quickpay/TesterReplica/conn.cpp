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
#include "quickpay/TesterReplica/conn.hpp"
#include "config.hpp"

#include "threshsign/IThresholdSigner.h"
#include "utt/Tx.h"

namespace quickpay::replica {

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

    if(!tx.quickPayValidate(m_params_->p, m_params_->main_pk, m_params_->reg_pk)) {
        LOG_ERROR(logger, "Quick pay validation failed");
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
    auto perf_start = get_monotonic_time();
    // First reserve the space to copy the new messages
    internal_msg_buf.reserve(received_bytes + bytes);
    // Copy the received bytes
    internal_msg_buf.insert(internal_msg_buf.begin()+received_bytes, 
                                incoming_msg_buf.begin(),
                                incoming_msg_buf.begin()+bytes);
    // Update the # of received bytes
    received_bytes += bytes;
    LOG_INFO(logger, "Received bytes var: " << received_bytes);
    // Check if we have sufficient bytes to process
    if(received_bytes < sizeof(QuickPayTx)) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << received_bytes << "] for a QP TX header[" 
                            << sizeof(QuickPayTx) << "], will try again");
        on_new_conn();
        return;
    }
    // Handle tx
    auto* qp_tx = (const QuickPayTx*)internal_msg_buf.data();
    if (qp_tx->get_size() < received_bytes) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << received_bytes << "] for a full tx"
                            << qp_tx->get_size() <<", will try again");
        on_new_conn();
        return;
    }
    
    received_bytes = 0;
    on_new_conn();
    std::stringstream ss;
    ss.write(reinterpret_cast<const char*>(qp_tx->getTxBuf()), qp_tx->tx_len);
        
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
    auto txhash = tx.getHashHex();
    auto qp_len = QuickPayMsg::get_size(txhash.size());
    auto sig_len = signer->requiredLengthForSignedData();
    auto resp_size = QuickPayResponse::get_size(qp_len, sig_len);
    outgoing_msg_buf.reserve(resp_size);
    auto qp_resp = (QuickPayResponse*)outgoing_msg_buf.data();
    qp_resp->qp_msg_len = qp_len;
    qp_resp->sig_len = sig_len;
    auto* qp = qp_resp->getQPMsg();
    qp->target_shard_id = 0;
    qp->hash_len = txhash.size();
    std::memcpy(qp->getHashBuf(), txhash.data(), txhash.size());
    signer->signData(txhash.c_str(), txhash.size(), (char*)qp_resp->getSigBuf(), sig_len);
    send_response(qp_resp->get_size());
    metrics->fetch_add(1);
    // Move the remaining buffers again
    auto perf_end = get_monotonic_time();
    LOG_INFO(logger, "Tx processing time: " << double(perf_end-perf_start));
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

void conn_handler::on_new_conn() {
    this->mSock_.async_receive(
        asio::buffer(incoming_msg_buf.data(), incoming_msg_buf.size()),
        std::bind(&conn_handler::do_read, shared_from_this(), 
            std::placeholders::_1, std::placeholders::_2));
}

} // namespace quickpay::replica