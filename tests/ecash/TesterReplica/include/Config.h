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
#include "ReplicaConfig.hpp"

namespace ecash::replica {

struct ReplicaConfig {
 public:
  uint16_t r_id;

  static const std::string default_ip_;
  // static const uint16_t base_port_ = 3710;
  static const std::string default_listen_ip_;
  // static const uint32_t buf_length_ = 128 * 1024;  // 128 kB
  static uint16_t numOfReplicas;
  static uint16_t numOfClients;

 private:
};
}  // namespace ecash::replica