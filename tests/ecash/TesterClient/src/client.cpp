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

#include "client.h"
#include <memory>
#include <utility>

namespace ecash::client {

Client::Client(std::unique_ptr<bft::communication::ICommunication> comm, const ClientConfig& config)
    : communication_(std::move(comm)), config_(config) {
  for (auto i = 1; i <= config_.numOfReplicas; i++) {
    all_replicas.insert(i);
  }
  communication_->setReceiver(config_.id.id, &receiver_);
  communication_->Start();
}

void Client::send_mint_tx(MsgMint&& msg_mint) {
  int len = msg_mint.commitment.getByteCount();
  Msg m(len);
  msg_mint.to_msg(m.data(), len);
  communication_->send(all_replicas, std::move(m));
}

}  // namespace ecash::client