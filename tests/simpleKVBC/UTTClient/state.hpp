// Concord
//
// Copyright (c) 2018-2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <cstdint>
#include <memory>

#include "utt/Coin.h"
#include "bftclient/bft_client.h"

// UTT_MSG_PTR of any base pointer is always the base pointer casted to UTT_MSG
// Since UTT Msg is always the first part of any message
#define UTT_MSG_PTR(baseptr) (reinterpret_cast<UTT_Msg*>(baseptr))
#define CONST_UTT_MSG_PTR(baseptr) (reinterpret_cast<const UTT_Msg*>(baseptr))

// MINT_MSG_PTR for a mint message is after the UTT_Msg
#define MINT_MSG_PTR(baseptr) (reinterpret_cast<MintMsg*>((baseptr)+sizeof(UTT_Msg)))
#define CONST_MINT_MSG_PTR(baseptr) (reinterpret_cast<const MintMsg*>((baseptr)+sizeof(UTT_Msg)))

// MINT_EMPTY_COIN_PTR of an empty coin is after the UTT Msg and the MintMsg Header
#define MINT_EMPTY_COIN_PTR(baseptr) (reinterpret_cast<unsigned char*>(baseptr)+sizeof(UTT_Msg)+sizeof(MintMsg))
#define CONST_MINT_EMPTY_COIN_PTR(baseptr) (reinterpret_cast<const char*>(baseptr)+sizeof(UTT_Msg)+sizeof(MintMsg))

// MINTACK_MSG_PTR for a mint ack message is the first part of the RSI
#define MINTACK_MSG_PTR(rsi_baseptr) (reinterpret_cast<MintAckMsg*>(rsi_baseptr))

// MINTACK_COIN_SHARE_PTR of an RSI is after the MintAckMsg header
#define MINTACK_COINSHARE_PTR(rsi_baseptr) (reinterpret_cast<const char*>((rsi_baseptr)+sizeof(MintAckMsg)))

enum class OpType : uint8_t { 
  Mint, 
  MintAck, 
  Pay,
  PayAck,
};

using namespace bft;

#pragma pack(push,1)

struct MintMsg {
  size_t val;
  size_t cc_buf_len;
  size_t coin_id;
};

struct MintAckMsg {
  size_t coin_id;
  size_t coin_sig_share_size;
};

struct PayAckMsg {
  size_t tx_len;
  size_t cc_len[2];
  size_t csign_len[2];
};

struct UTT_Msg {
  OpType type;

};
#pragma pack(pop)