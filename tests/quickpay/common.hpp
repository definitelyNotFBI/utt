#pragma once 

#include <asio.hpp>
#include <unordered_map>
#include "Crypto.hpp"

namespace quickpay::client {

class protocol;

/*
 * On connecting to a new client, this is used to establish
 */
class conn_handler;
typedef std::shared_ptr<conn_handler> conn_handler_ptr;
typedef bftEngine::impl::RSAVerifier PublicKey;
typedef std::unordered_map<uint16_t, std::shared_ptr<PublicKey>> PublicKeyMap;

const std::string utt_wallet_file_key = "utt.quickpay.wallet";

const size_t REPLICA_MAX_MSG_SIZE = 20000;

} // namespace quickpay::client