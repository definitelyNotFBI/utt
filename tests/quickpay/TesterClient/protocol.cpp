#include <asio/error_code.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <boost/bind.hpp>
#include "Logging4cplus.hpp"
#include "quickpay/TesterClient/conn.hpp"
#include "quickpay/TesterClient/protocol.hpp"
#include <boost/shared_ptr.hpp>
#include <asio.hpp>
#include <functional>
#include <iterator>
#include <vector>

namespace quickpay::client {

#define GET_IO_SERVICE(s) ((ba::io_context&)(s).get_executor().context())
logging::Logger protocol::logger = logging::getLogger("quickpay.bft.client");

protocol::protocol(ba::io_context& io_ctx, 
                   bft::communication::NodeMap& node_map)
    : m_io_ctx_(io_ctx)
{
    for(auto& [replica_id,node_info]: node_map) {
        if (!node_info.isReplica) {
            continue;
        }
        LOG_INFO(protocol::logger, "Attempting connection to node " 
                            << replica_id << " at " 
                            << node_info.host << ":" 
                            << node_info.port);
        auto address = ba::ip::make_address(node_info.host);
        m_node_map_[replica_id] = endpoint_t(address, node_info.port);
        LOG_INFO(protocol::logger, "Connected to "<< m_node_map_[replica_id]);
    }
}

void protocol::start() {
    for(auto& [node_id, end_point]: m_node_map_) {
        auto sock = sock_t(m_io_ctx_);
        auto conn = conn_handler::create(m_io_ctx_, node_id);
        sock.async_connect(end_point, std::bind(
            &conn_handler::on_new_conn, conn, std::placeholders::_1));
    }
}

void protocol::wait_for_connections() {}

} // namespace quickpay::replica
