// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <cstdint>
#include <vector>
#include "threshsign/bls/relic/BlsNumTypes.h"
namespace ecash::client {

typedef std::vector<uint8_t> Msg;

// The Mint message
struct MsgMint {
  // c = H(sn)g^r
  BLS::Relic::G2T commitment;

  void to_msg(uint8_t *buf, int size) { commitment.toBytes(buf, size); }

  static MsgMint random();
};
}  // namespace ecash::client