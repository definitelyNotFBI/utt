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

#include <memory>
#include "communication/ICommunication.hpp"
#include "config.h"
#include "receiver.h"
#include "Msg.h"

namespace ecash::client {

class Client : SimpleClientImp {
 public:
  // How to create a new client
  Client(std::unique_ptr<bft::communication::ICommunication> comm, const ClientConfig& config);

  // Stop the client
  void stop() { communication_->Stop(); }

  // Send messages to the replicas
  void send_mint_tx(MsgMint&& msg_mint);
  // void send_pay_tx();

 private:
  // Handle on the communication interface
  std::unique_ptr<bft::communication::ICommunication> communication_;
  // Handle on Msg Reactor
  MsgReceiver receiver_;
  ClientConfig config_;
  std::set<bft::communication::NodeNum> all_replicas;
};

}  // namespace ecash::client