#pragma once 

#include <asio.hpp>

namespace quickpay::client {

class protocol;

/*
 * On connecting to a new client, this is used to establish
 */
class conn_handler;
typedef std::shared_ptr<conn_handler> conn_handler_ptr;

} // namespace quickpay::client