#include <asio/error_code.hpp>
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "quickpay/TesterReplica/conn.hpp"
#include "quickpay/TesterReplica/protocol.hpp"
#include <boost/shared_ptr.hpp>
#include <asio.hpp>
#include <functional>

namespace quickpay::replica {

#define GET_IO_SERVICE(s) ((ba::io_context&)(s).get_executor().context())

logging::Logger protocol::logger = logging::getLogger("quickpay.bft.replica");

    protocol::protocol(ba::io_context& io_ctx, uint16_t port_num): m_io_ctx_(io_ctx),
        m_acceptor_(io_ctx, 
                    ba::ip::tcp::endpoint(ba::ip::tcp::v4(), 
                                            port_num)
                    )
    {
        LOG_INFO(logger, "Starting the connection at " << port_num);
        start_accept();
    }


void protocol::start_accept() 
{
    auto conn = conn_handler::create(m_io_ctx_);
    // asynchronous accept operation and wait for a new connection.
     m_acceptor_.async_accept(conn->socket(),
        std::bind(&protocol::on_new_client, this, conn,
        std::placeholders::_1));
}

void protocol::on_new_client(conn_handler_ptr conn, const ba::error_code& err) 
{
    LOG_INFO(logger, "Got a new client connection");
    if (!err) {
      conn->on_new_conn();
    } else {
        LOG_ERROR(logger, err.message());
    }

    start_accept();
}

} // namespace quickpay::replica
