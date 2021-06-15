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

#include "config.h"
#include <cstdint>
#include <set>
#include "base_types.h"

namespace ecash::client {

ClientConfig ClientConfig::create(uint16_t numOfReplicas) {
  // std::set<ReplicaId> all_replicas;
  // std::set<ClientId> ro_replicas;
  const uint16_t cid = numOfReplicas + 1;
  return ClientConfig{ClientId{cid}, 1, 0};
};
}  // namespace ecash::client