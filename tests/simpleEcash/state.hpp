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

enum class OpType : uint8_t { 
  Mint, 
  MintAck, 
  Pay 
};
enum class Flags : uint8_t {
  MINT = 0x0,
  PAY = 0x1,
  EMPTY = 0x2,
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

struct UTT_Msg {
  OpType type;

  public:
  static bft::client::Msg new_mint_msg(size_t value, libutt::CoinComm cc, size_t ctr);
};
#pragma pack(pop)