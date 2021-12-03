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
#include "ReplicaImpl.hpp"
#include "communication/CommDefs.hpp"
#include "communication/CommFactory.hpp"
#include "communication/ICommunication.hpp"
#include "bftengine/MetadataStorage.hpp"
#include "FileStorage.hpp"

using namespace ecash::replica;

int main(int /*argc*/, char** argv) {
  // Overview of what the client needs to do
  // Setup networking
  uint16_t node_id = atoi(argv[1]);

  bftEngine::MetadataStorage* metaDataStorage = nullptr;
  std::ostringstream dbFile;
  dbFile << "metadataStorageTest_" << node_id << ".txt";
  remove(dbFile.str().c_str());
  metaDataStorage = new bftEngine::FileStorage(ReplicaImpl::replicaLogger, dbFile.str());

  const ReplicaConfig r{node_id};
  auto replica = ReplicaImpl::createNewReplica(r);
  replica->start();
  // The replica is now running in its own thread. Block the main thread until
  // sigabort, sigkill or sigterm are not raised and then exit gracefully
  return 0;
}