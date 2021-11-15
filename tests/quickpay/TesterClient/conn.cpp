#include <asio/error_code.hpp>
#include <iostream>

#include "Logging4cplus.hpp"
#include "quickpay/TesterClient/conn.hpp"
#include "quickpay/TesterClient/protocol.hpp"

namespace quickpay::client {

    logging::Logger conn_handler::logger = logging::getLogger("quickpay.client.conn");

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
    // Tell the protocol that we have a new connection
    m_proto_->add_connection(shared_from_this());
}

} // namespace quickpay::replica