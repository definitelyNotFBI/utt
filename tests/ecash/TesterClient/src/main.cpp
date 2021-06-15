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

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include "communication/CommDefs.hpp"
#include "communication/CommFactory.hpp"
#include "client.h"
#include "communication/ICommunication.hpp"

const std::string default_ip_ = "127.0.0.1";
static const uint16_t base_port_ = 3710;
const std::string default_listen_ip_ = "0.0.0.0";
static const uint32_t buf_length_ = 128 * 1024;  // 128 kB
const uint16_t numOfReplicas = 4;
const uint16_t numOfClients = 1;

int main(int /*argc*/, char** /*argv*/) {
  // Overview of what the client needs to do
  // Setup networking
  uint16_t node_id = numOfReplicas + numOfClients;
  std::string ip = default_ip_;
  uint16_t port = static_cast<uint16_t>(base_port_ + node_id * 2);
  std::unordered_map<uint64_t, bft::communication::NodeInfo> nodes;
  // Create a map of where the port for each node is.
  for (int i = 0; i < (numOfReplicas + numOfClients); i++)
    nodes.insert({i, bft::communication::NodeInfo{ip, static_cast<uint16_t>(base_port_ + i * 2), i < numOfReplicas}});

  bft::communication::PlainTcpConfig conf(default_listen_ip_, port, buf_length_, nodes, numOfReplicas - 1, node_id);

  bft::communication::ICommunication* res = nullptr;
  res =
      bft::communication::PlainTCPCommunication::create(dynamic_cast<const bft::communication::PlainTcpConfig&>(conf));
  // The network is setup now
  using ecash::client::ClientConfig;
  ClientConfig config = ClientConfig::create(numOfReplicas);
  // Create a client now
  ecash::client::Client client{std::unique_ptr<bft::communication::ICommunication>(res), config};
  // Start communicating with the other nodes
  client.send_mint_tx(ecash::client::MsgMint::random());
}