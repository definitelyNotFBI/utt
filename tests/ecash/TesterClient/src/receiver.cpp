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

#include "receiver.h"
#include <iostream>
#include "communication/ICommunication.hpp"

namespace ecash::client {
void MsgReceiver::onNewMessage(bft::communication::NodeNum sourceNode,
                               const char* const message,
                               size_t messageLength) {
  std::cout << "Got a new message" << message << "of length" << messageLength << std::endl;
}

// void MsgReceiver::onConnectionStatusChanged(bft::communication::NodeNum node,
//                                  bft::communication::ConnectionStatus newStatus)
// {
//     std::cout << "Connection Status Changed" << std::endl;
// }

}  // namespace ecash::client