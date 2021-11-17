#include <asio/error_code.hpp>
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "quickpay/TesterReplica/conn.hpp"
#include "quickpay/TesterReplica/protocol.hpp"
#include <asio.hpp>
#include <functional>

namespace quickpay::replica {

logging::Logger protocol::logger = logging::getLogger("quickpay.bft.replica");

    protocol::protocol(asio::io_context& io_ctx, uint16_t port_num): m_io_ctx_(io_ctx),
        m_acceptor_(io_ctx, 
                    asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 
                                            port_num)
                    )
    {
        LOG_INFO(logger, "Starting the connection at " << port_num);
        start_accept();
    }


void protocol::start_accept() 
{
    auto conn = conn_handler::create(m_io_ctx_, id++);
    // asynchronous accept operation and wait for a new connection.
    m_acceptor_.async_accept(conn->socket(),
        std::bind(&protocol::on_new_client, this, conn,
        std::placeholders::_1));
}

void protocol::on_new_client(conn_handler_ptr conn, const asio::error_code& err) 
{
    LOG_INFO(logger, "Got a new client connection");
    if (!err) {
        m_conn_.emplace(conn->id, conn->shared_from_this());
        conn->on_new_conn();
    } else {
        LOG_ERROR(logger, err.message());
    }

    start_accept();
}

} // namespace quickpay::replica
