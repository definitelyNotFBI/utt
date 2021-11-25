#include <asio/io_context.hpp>
#include <atomic>
#include <memory>
#include <unordered_map>
#include "Logging4cplus.hpp"
#include "quickpay/TesterReplica/conn.hpp"
#include "replica/Params.hpp"
#include "rocksdb/native_client.h"
#include "threshsign/ThresholdSignaturesTypes.h"

namespace quickpay::replica {

/*
 * The quickpay protocol for the replicas
 */
class protocol {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::io_context io_ctx_t;

    static logging::Logger logger;

// ASIO Stuff
private:
    io_ctx_t& m_io_ctx_;
    asio::ip::tcp::acceptor m_acceptor_;
    std::unordered_map<long, conn_handler_ptr> m_conn_;
    std::shared_ptr<utt_bft::replica::Params> m_params_ = nullptr;

// Protocol-related stuff
private:
    std::atomic_llong id = 0;
    std::shared_ptr<concord::storage::rocksdb::NativeClient> db_ptr = nullptr;
    std::shared_ptr<Cryptosystem> m_cryp_sys_ = nullptr;

public:
    protocol(asio::io_context& io_ctx, uint16_t port_num, std::shared_ptr<Cryptosystem> crypsys);

private:
    // Start handling new clients
    void start_accept();

    // Handle new clients
    void on_new_client(conn_handler_ptr connection, const asio::error_code& err);
};

} // namespace quickpay::replica