#include <asio/error_code.hpp>
#include <iostream>

#include "Logging4cplus.hpp"
#include "quickpay/TesterClient/conn.hpp"

namespace quickpay::client {

    logging::Logger conn_handler::logger = logging::getLogger("quickpay.client.conn");

void conn_handler::on_new_conn(const asio::error_code err) {
    LOG_INFO(logger, "Client is now connected to Replica " << m_replica_id_);
}

} // namespace quickpay::replica