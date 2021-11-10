#include <asio/io_context.hpp>
#include "Logging4cplus.hpp"
#include "quickpay/TesterReplica/conn.hpp"

namespace quickpay::replica {

/*
 * The quickpay protocol for the replicas
 */
class protocol {
    typedef ba::ip::tcp::socket sock_t;
    typedef ba::io_context io_ctx_t;

    static logging::Logger logger;

private:
    io_ctx_t& m_io_ctx_;
    ba::ip::tcp::acceptor m_acceptor_;

public:
    protocol(ba::io_context& io_ctx, uint16_t port_num);

private:
    // Start handling new clients
    void start_accept();

    // Handle new clients
    void on_new_client(conn_handler_ptr connection, const ba::error_code& err);
};

} // namespace quickpay::replica