#pragma once

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/io_service.hpp>
#include <asio/ip/tcp.hpp>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <sstream>
#include <vector>
#include "Logging4cplus.hpp"
#include "quickpay/TesterClient/common.hpp"
#include "replica/Params.hpp"
#include "rocksdb/native_client.h"
#include "threshsign/IThresholdSigner.h"
#include "threshsign/ThresholdSignaturesTypes.h"

namespace quickpay::replica {

/*
 * On connecting to a new client, this is used to establish
 */
class conn_handler;
typedef std::shared_ptr<conn_handler> conn_handler_ptr;

class conn_handler : public std::enable_shared_from_this<conn_handler> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::io_context io_ctx_t;
    typedef std::shared_ptr<utt_bft::replica::Params> params_ptr_t;
    typedef std::shared_ptr<concord::storage::rocksdb::NativeClient> db_ptr_t;
    typedef std::shared_ptr<Cryptosystem> cryp_sys_ptr_t;
    typedef std::shared_ptr<IThresholdSigner> signer_t;

public:
    // constructor to create a connection
    conn_handler(io_ctx_t& io_ctx, 
                    long id, 
                    params_ptr_t params, 
                    db_ptr_t db,
                    std::shared_ptr<std::atomic<uint64_t>> metrics,
                    const cryp_sys_ptr_t& cryp_sys_ptr
                ): mSock_(io_ctx), 
                        incoming_msg_buf(client::REPLICA_MAX_MSG_SIZE), 
                        internal_msg_buf(0),
                        m_params_{std::move(params)}, 
                        m_db_{std::move(db)},
                        metrics{metrics},
                        signer{std::shared_ptr<IThresholdSigner>(cryp_sys_ptr->createThresholdSigner())},
                        id{id}
                        {}

    // creating a pointer
    static conn_handler_ptr create(io_ctx_t& io_ctx, 
                                    long id, 
                                    params_ptr_t params, 
                                    db_ptr_t db, 
                                    std::shared_ptr<std::atomic<uint64_t>> metrics,
                                    const cryp_sys_ptr_t& cryp_sys_ptr)
    {
        return conn_handler_ptr(new conn_handler(io_ctx, id, std::move(params), std::move(db), metrics, cryp_sys_ptr));
    }

    // things to do when we have a new connection
    void on_new_conn();

    // start the connection
    void start_conn();

    // Read data from clients
    void do_read(const asio::error_code& err, size_t bytes);

    // Send the replica of the response
    void send_response(size_t);

private:
    sock_t mSock_;
    std::vector<uint8_t> outgoing_msg_buf;
    std::vector<uint8_t> incoming_msg_buf;
    std::vector<uint8_t> internal_msg_buf;

private:
    std::shared_ptr<utt_bft::replica::Params> m_params_ = nullptr;
    std::shared_ptr<concord::storage::rocksdb::NativeClient> m_db_ = nullptr;
    uint64_t nullif_ctr = 0;
    std::shared_ptr<std::atomic<uint64_t>> metrics = nullptr;
    signer_t signer = nullptr;

private:
    static logging::Logger logger;

public:
    long id;
    // Get the socket
    asio::ip::tcp::socket& socket()
    {
        return mSock_;
    }

};


}