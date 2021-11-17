#pragma once 

#include <asio.hpp>

namespace quickpay::client {

class protocol;

/*
 * On connecting to a new client, this is used to establish
 */
class conn_handler;
typedef std::shared_ptr<conn_handler> conn_handler_ptr;

const std::string utt_genesis_file_key = "utt.quickpay.genesis";

const size_t REPLICA_MAX_MSG_SIZE = 65999;

} // namespace quickpay::client