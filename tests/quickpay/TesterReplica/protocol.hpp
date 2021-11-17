#include <asio/io_context.hpp>
#include <atomic>
#include <unordered_map>
#include "Logging4cplus.hpp"
#include "quickpay/TesterReplica/conn.hpp"

namespace quickpay::replica {

/*
 * The quickpay protocol for the replicas
 */
class protocol {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::io_context io_ctx_t;

    static logging::Logger logger;

private:
    io_ctx_t& m_io_ctx_;
    asio::ip::tcp::acceptor m_acceptor_;

private:
    std::atomic_llong id = 0;
    std::unordered_map<long, conn_handler_ptr> m_conn_;

public:
    protocol(asio::io_context& io_ctx, uint16_t port_num);

private:
    // Start handling new clients
    void start_accept();

    // Handle new clients
    void on_new_client(conn_handler_ptr connection, const asio::error_code& err);
};

} // namespace quickpay::replica