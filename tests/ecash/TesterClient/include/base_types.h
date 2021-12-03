// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use
// this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license
// terms. Your use of these subcomponents is subject to the terms and conditions of the
// subcomponent's license, as noted in the LICENSE file.

#pragma once

#include <cstdint>
#include <vector>

namespace ecash::client {

// The ID of the replica
struct ReplicaId {
  uint16_t id;

  bool operator==(const ReplicaId& other) const { return id == other.id; }
  bool operator!=(const ReplicaId& other) const { return id != other.id; }
  bool operator<(const ReplicaId& other) const { return id < other.id; }
};

// The ID of the clients
struct ClientId {
  uint16_t id;

  bool operator==(const ReplicaId& other) const { return id == other.id; }
  bool operator!=(const ReplicaId& other) const { return id != other.id; }
  bool operator<(const ReplicaId& other) const { return id < other.id; }
};

// Types of messages sent by the client
enum MsgFlags : uint8_t {
  Mint = 0x0,  // For the minting transaction
  Pay = 0x1,   // For the payment transaction
};

}  // namespace ecash::client