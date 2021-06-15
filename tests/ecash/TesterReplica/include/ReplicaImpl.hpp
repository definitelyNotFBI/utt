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

#include "Logging4cplus.hpp"
#include "OpenTracing.hpp"
#include "Replica.hpp"
#include "CommandsHandler.hpp"
#include "Config.h"
#include "ReplicaConfig.hpp"
#include "communication/ICommunication.hpp"

namespace ecash::replica {

class ReplicaImpl {
 public:
  static logging::Logger replicaLogger;

  using IReplicaPtr = std::unique_ptr<bftEngine::IReplica>;
  using ReplicaImplPtr = std::unique_ptr<ReplicaImpl>;
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // IReplica implementation
  void start();
  void stop();
  bool isRunning() const;

  void set_command_handler(std::shared_ptr<ICommandsHandler> handler);

  static ReplicaImplPtr createNewReplica(ReplicaConfig);

  enum class Status : int {
    IDLE,
    RUNNING,
    STOPPING,
    ABORT,
  };

 private:
  IReplicaPtr m_replicaPtr = nullptr;
  bft::communication::ICommunication *m_ptrComm = nullptr;
  std::shared_ptr<ICommandsHandler> m_cmdHandler = nullptr;
  Status m_status = Status::IDLE;

 public:
  ReplicaImpl(ReplicaConfig &&);
  ReplicaImpl(ReplicaImpl &&) = default;
};

}  // namespace ecash::replica