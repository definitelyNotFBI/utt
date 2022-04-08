#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/streambuf.hpp>
#include <functional>
#include <iostream>
#include <string>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "assertUtils.hpp"
#include "common.hpp"
#include "misc.hpp"
#include "msg/QuickPay.hpp"
#include "msg/ShardTx.hpp"
#include "conn.hpp"
#include "config.hpp"

#include "sha_hash.hpp"
#include "threshsign/IThresholdSigner.h"
#include "utt/PolyCrypto.h"
#include "utt/Tx.h"
#include "utt/internal/PicoSha2.h"

namespace sharding::replica {

logging::Logger conn_handler::logger = logging::getLogger("sharding.replica.conn");

bool conn_handler::check_burn_request(const BurnMsg* burn_msg, const libutt::Tx& tx) {
    LOG_INFO(logger, "Checking the burn request");

    // Is the target shard valid?
    auto config = ReplicaConfig::Get();
    if (burn_msg->target_shard_id >= config->num_shards) {
        LOG_ERROR(logger, "Got invalid shard id: " << burn_msg->target_shard_id);
        return false;
    }
    LOG_INFO(logger, "Tx has correct shard id");

    // Check whether it is my responsibility to burn this shard
    bool my_responsibility = false;
    for(auto& nullif: tx.getNullifiers()) {
        auto rand = common::get_responsibility(nullif, config->num_shards);
        if(config->shard_id == rand) {
            my_responsibility = true;
            break;
        }
    }

    if(!my_responsibility) {
        LOG_ERROR(logger, "Burning this tx is not my responsibility");
        return false;
    }

    //.Is it quickpay valid?
    // if(!tx.quickPayValidate(m_params_->p, m_params_->main_pk, m_params_->reg_pk)) 
    // {
    //     LOG_ERROR(logger, "tx validation failed");
    //     return false;
    // }

    // Is it spent already? Check DB
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
    LOG_INFO(logger, "Got a new valid sharded client burn transaction");
    return true;
}

bool conn_handler::check_mint_request(const MintMsg* mint_tx, 
    const libutt::Tx& tx, const MintProof& proof, const char* tx_data, size_t tx_len) 
{
    LOG_INFO(logger, "Checking the mint request");
    auto* config = ReplicaConfig::Get();

    // Get responsible shards
    std::set<size_t> responsible_shards{};
    for(auto& nullif: tx.getNullifiers()) {
        auto rand = common::get_responsibility(nullif, config->num_shards);
        responsible_shards.insert(rand);
    }

    auto txhash2 = concord::util::SHA3_256().digest((uint8_t*)tx_data, tx_len);
    ResponseData data{config->shard_id, txhash2};
    auto txhash = concord::util::SHA3_256().digest((uint8_t*)&data, 
                                                    sizeof(data));


    // DONE: Ensure all responsible shards are present
    for(const auto& shard: responsible_shards) {
        if(proof.map.count(shard)==0) {
            LOG_ERROR(logger, "Got a transaction with responses missing from responsible shard " << shard);
            return false;
        }
        auto shard_vec = proof.map.at(shard);
        if(shard_vec.size() != config->numReplicas) {
            LOG_ERROR(logger, "Got a transaction with insufficient responses " 
                << proof.map.at(shard).size() 
                <<" for responsible shard " << shard);
            return false;
        }
        // DONE: Check signatures
        for(const auto& response: shard_vec) {
            auto verif = config->pk_map.get()->at(shard).at(response.id);
            const BurnResponse* resp = (const BurnResponse*)response.data.data();

            if(verif->signatureLength() != resp->sig_len)
            {
                LOG_WARN(logger, "Got invalid signature length: Exp " << verif->signatureLength() << ", got " << resp->sig_len);
                return false;
            }
            if(!verif->verify((const char*)txhash.data(), txhash.size(), (const char*)resp->get_sig_buf_ptr(), resp->sig_len)) 
            {
                LOG_WARN(logger, "Signature check failed for shard " 
                                    << shard << " for replica " << response.id);
                return false;
            }
        }
    }
    //.Is the tx valid?
    if(!tx.validate(m_params_->p, m_params_->main_pk, m_params_->reg_pk)) 
    {
        LOG_ERROR(logger, "tx validation failed");
        return false;
    }


    // Is it spent already? Check DB
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
    LOG_INFO(logger, "Got a new valid sharded client mint transaction");
    return true;
}

void conn_handler::handle_burn_request()
{
    auto* msg = (Msg*)internal_msg_buf.data();
    auto* burn_msg = msg->get_msg<BurnMsg>();
    std::stringstream ss;
    ss.write(reinterpret_cast<const char*>(burn_msg->get_tx_buf_ptr()), static_cast<long>(burn_msg->tx_len));
        
    libutt::Tx tx(ss);
    if (!check_burn_request(burn_msg, tx)) {
        return;
    }

    // Burn the coin
    for(auto& nullif: tx.getNullifiers()) {
        m_db_->rawDB().Put(rocksdb::WriteOptions{}, 
                                nullif + std::to_string(nullif_ctr++), 
                                std::string());
    }

    auto txhash2 = concord::util::SHA3_256().digest(
                                (uint8_t*)burn_msg->get_tx_buf_ptr(), 
                                burn_msg->tx_len);
    ResponseData data{burn_msg->target_shard_id, txhash2};
    auto txhash = concord::util::SHA3_256().digest((uint8_t*)&data, 
                                                    sizeof(data));
    auto* config = ReplicaConfig::Get();
    assert(config->rsa_signer != nullptr);
    auto sig_len = config->rsa_signer->signatureLength();
    auto burn_resp_msg_len = BurnResponse::get_size(sig_len);
    auto resp_msg_size = Msg::get_size(burn_resp_msg_len);
    outgoing_msg_buf.resize(resp_msg_size);
    auto resp_msg = (Msg*)outgoing_msg_buf.data();
    resp_msg->tp = MsgType::BURN_RESPONSE;
    resp_msg->msg_len = burn_resp_msg_len;
    auto burn_resp = resp_msg->get_msg<BurnResponse>();
    burn_resp->sig_len = sig_len;

    size_t return_len;
    config->rsa_signer->sign((const char*)txhash.data(), 
                        txhash.size(), 
                        (char*)burn_resp->get_sig_buf_ptr(), 
                        sig_len, return_len);
    LOG_DEBUG(logger, "Return len: " << return_len);
    send_response(resp_msg->get_size());
    // metrics->fetch_add(1);
}

void conn_handler::handle_mint_request() {
    auto* msg = (Msg*)internal_msg_buf.data();
    auto* mint_msg = msg->get_msg<MintMsg>();
    std::stringstream ss, proof_ss;
    ss.write(reinterpret_cast<const char*>(mint_msg->get_tx_buf_ptr()), static_cast<long>(mint_msg->tx_len));
    proof_ss.write((const char*)mint_msg->get_proof_buf_ptr(), mint_msg->proof_len);
        
    libutt::Tx tx(ss);
    MintProof proof;
    proof_ss >> proof;
    auto tx_data = ss.str();
    if (!check_mint_request(mint_msg, tx, proof, tx_data.data(), tx_data.size())) {
        return;
    }

    // Burn the coin
    for(auto& nullif: tx.getNullifiers()) {
        m_db_->rawDB().Put(rocksdb::WriteOptions{}, 
                                nullif + std::to_string(nullif_ctr++), 
                                std::string());
    }

    // DONE: Mint Response
    // generate and send signature
    ss.str(std::string{});
    // DONE: Process the tx (The coins are still unspent)
    for(size_t txoIdx = 0; txoIdx < tx.outs.size(); txoIdx++) {
        auto sig = tx.shareSignCoin(txoIdx, m_params_->my_sk);
        ss << sig << std::endl;
    }
    auto ss_str = ss.str();
    size_t resp_msg_len = MintResponse::get_size(ss_str.size());
    size_t msg_len = Msg::get_size(resp_msg_len);
    outgoing_msg_buf.resize(msg_len, 0);
    auto* out_msg = (Msg*)outgoing_msg_buf.data();
    out_msg->msg_len = resp_msg_len;
    out_msg->tp = MsgType::MINT_RESPONSE;
    auto* mint_resp = out_msg->get_msg<MintResponse>();
    mint_resp->sig_len = ss_str.size();
    std::memcpy(mint_resp->get_sig_buf_ptr(), ss_str.data(), ss_str.size());
    send_response(msg_len);
    metrics->fetch_add(1);
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
    internal_msg_buf.insert(
            internal_msg_buf.begin()+static_cast<long>(received_bytes), 
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
        // arm the connection for the next time it is called
        on_new_conn();
        return;
    }
    // Handle tx
    auto* msg = (Msg*)internal_msg_buf.data();
    if (received_bytes < msg->get_size()) {
        LOG_WARN(logger, "Did not receive sufficient bytes [" 
                            << received_bytes << "] for a full msg"
                            << msg->get_size() <<", will try again");
        on_new_conn();
        return;
    }
    
    received_bytes -= msg->get_size();
    on_new_conn();

    // Ensure correct type
    if(msg->tp == MsgType::BURN_REQUEST) {
        LOG_INFO(logger, "Got a burn request");
        handle_burn_request();
        return;
    } else if (msg->tp == MsgType::MINT_REQUEST) {
        LOG_INFO(logger, "Got a mint request");
        handle_mint_request();
        return;
    }
    LOG_WARN(logger, "Got an invalid msg type: " << (uint8_t)msg->tp);
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

} // namespace sharding::replica