#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/streambuf.hpp>
#include <functional>
#include <iostream>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "quickpay/TesterReplica/conn.hpp"
#include "utt/Utt.h"

namespace quickpay::replica {

logging::Logger conn_handler::logger = logging::getLogger("quickpay.replica.conn");

void conn_handler::do_read(const asio::error_code& err, size_t bytes)
{
    if (err) {
        LOG_ERROR(logger, "Got error " << err.message());
        return;
    }
    std::cout << "got " << bytes << std::endl;
    // Handle tx
    std::stringstream ss;
    ss.write(reinterpret_cast<const char*>(incoming_msg_buf.data()), bytes);
    
    libutt::Tx tx(ss);
    // tx.verify(mPrams)
    LOG_INFO(logger, "Got a new valid transaction");
    // TODO: Check validity, and egenerate and send signature
    out_ss.str("");
    out_ss << "Hello world";
    send_response();
    on_new_conn();
}

void conn_handler::send_response() {
    this->mSock_.async_send(
        asio::buffer(out_ss.str(), out_ss.str().size()),
        [](const asio::error_code& err, size_t bytes) {
            if (err) {
                LOG_ERROR(logger, err.message());
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

} // namespace quickpay::replica