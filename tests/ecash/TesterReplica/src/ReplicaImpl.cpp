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

#include <memory>
#include "OpenTracing.hpp"
#include "Replica.hpp"
#include "CommandsHandler.hpp"
#include "Config.h"
#include "ReplicaConfig.hpp"
#include "ReplicaImpl.hpp"
#include "communication/CommDefs.hpp"
#include "test_comm_config.hpp"
#include "communication/CommFactory.hpp"

namespace ecash::replica {

const std::string ReplicaConfig::default_ip_ = "127.0.0.1";
// static const uint16_t base_port_ = 3710;
const std::string ReplicaConfig::default_listen_ip_ = "0.0.0.0";
// static const uint32_t buf_length_ = 128 * 1024;  // 128 kB
uint16_t ReplicaConfig::numOfReplicas = 4;
uint16_t ReplicaConfig::numOfClients = 1;

// NOLINTNEXTLINE(misc-definitions-in-headers)
logging::Logger ReplicaImpl::replicaLogger = logging::getLogger("ecash.replica");

using ReplicaImplPtr = std::unique_ptr<ReplicaImpl>;

void ReplicaImpl::start() {
  m_status = Status::RUNNING;
  m_ptrComm->Start();
}

void ReplicaImpl::stop() {
  m_status = Status::STOPPING;
  // Wait for stop tasks to finish
  m_replicaPtr->stop();
  // Now stop
  m_status = Status::IDLE;
}

bool ReplicaImpl::isRunning() const { return m_status == Status::RUNNING; }

void ReplicaImpl::set_command_handler(std::shared_ptr<ICommandsHandler> handler) { m_cmdHandler = handler; }

ReplicaImplPtr ReplicaImpl::createNewReplica(ReplicaConfig r_config) {
  ReplicaImpl r{std::move(r_config)};
  return std::make_unique<ReplicaImpl>(std::move(r));
}

ReplicaImpl::ReplicaImpl(ReplicaConfig&& rc) {
  TestCommConfig testCommConfig(replicaLogger);
  bftEngine::ReplicaConfig& replicaConfig = bftEngine::ReplicaConfig::instance();
  replicaConfig.numReplicas = 4;
  testCommConfig.GetReplicaConfig(rc.r_id, "private_replica_", &replicaConfig);
  bft::communication::PlainTcpConfig conf = testCommConfig.GetTCPConfig(
      true, rc.r_id, ReplicaConfig::numOfClients, ReplicaConfig::numOfReplicas, "sameple_config.txt");
  m_ptrComm = bft::communication::CommFactory::create(conf);
  this->set_command_handler(std::make_shared<ICommandsHandler>(ICommandsHandler()));
}

}  // namespace ecash::replica