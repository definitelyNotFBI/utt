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

// This program implements a client that sends requests to the simple register
// state machine defined in replica.cpp. It sends a preset number of operations
// to the replicas, and occasionally checks that the responses match
// expectations.
//
// Operations alternate:
//
//  1. `readMod-1` write operations, each with a unique value
//    a. Every second write checks that the returned sequence number is as
//       expected.
//  2. Every `readMod`-th operation is a read, which checks that the value
//     returned is the same as the last value written.
//
// The program expects no arguments. See the `scripts/` directory for
// information about how to run the client.

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "test_parameters.hpp"
#include "simple_test_client.hpp"
#include "Logger.hpp"

logging::Logger clientLogger = logging::getLogger("utt.bft.client");

ClientParams setupClientParams(int argc, char **argv) {
  ClientParams clientParams;
  clientParams.clientId = UINT16_MAX;
  clientParams.numOfFaulty = UINT16_MAX;
  clientParams.numOfSlow = UINT16_MAX;
  clientParams.numOfOperations = UINT16_MAX;
  char argTempBuffer[PATH_MAX + 10];
  bool is_debug = false;
  int o = 0;
  std::string logPropsFile = "logging.properties";
  while ((o = getopt(argc, argv, "i:f:c:p:n:U:l:d")) != EOF) {
    switch (o) {
      case 'i': {
        strncpy(argTempBuffer, optarg, sizeof(argTempBuffer) - 1);
        argTempBuffer[sizeof(argTempBuffer) - 1] = 0;
        string idStr = argTempBuffer;
        int tempId = std::stoi(idStr);
        if (tempId >= 0 && tempId < UINT16_MAX) clientParams.clientId = (uint16_t)tempId;
      } break;

      case 'd': {
        is_debug = true;
      } break;

      case 'f': {
        strncpy(argTempBuffer, optarg, sizeof(argTempBuffer) - 1);
        argTempBuffer[sizeof(argTempBuffer) - 1] = 0;
        string fStr = argTempBuffer;
        int tempfVal = std::stoi(fStr);
        if (tempfVal >= 1 && tempfVal < UINT16_MAX) clientParams.numOfFaulty = (uint16_t)tempfVal;
      } break;

      case 'c': {
        strncpy(argTempBuffer, optarg, sizeof(argTempBuffer) - 1);
        argTempBuffer[sizeof(argTempBuffer) - 1] = 0;
        string cStr = argTempBuffer;
        int tempcVal = std::stoi(cStr);
        if (tempcVal >= 0 && tempcVal < UINT16_MAX) clientParams.numOfSlow = (uint16_t)tempcVal;
      } break;

      case 'p': {
        strncpy(argTempBuffer, optarg, sizeof(argTempBuffer) - 1);
        argTempBuffer[sizeof(argTempBuffer) - 1] = 0;
        string numOfOpsStr = argTempBuffer;
        uint32_t tempPVal = std::stoul(numOfOpsStr);
        if (tempPVal >= 1 && tempPVal < UINT32_MAX) clientParams.numOfOperations = tempPVal;
      } break;

      case 'n': {
        strncpy(argTempBuffer, optarg, sizeof(argTempBuffer) - 1);
        argTempBuffer[sizeof(argTempBuffer) - 1] = 0;
        clientParams.configFileName = argTempBuffer;
      } break;

      case 'U': {
        strncpy(argTempBuffer, optarg, sizeof(argTempBuffer) - 1);
        argTempBuffer[sizeof(argTempBuffer) - 1] = 0;
        clientParams.utt_file_name = argTempBuffer;
      } break;

      case 'l': {
        strncpy(argTempBuffer, optarg, sizeof(argTempBuffer) - 1);
        argTempBuffer[sizeof(argTempBuffer) - 1] = 0;
        logPropsFile = argTempBuffer;
      } break;

      default:
        break;
    }
  }

  logging::initLogger(logPropsFile);
  if (is_debug) {
    // Enable all the logs (Use for debugging only)
    for (auto &logger: logging::Logger::getCurrentLoggers()) {
      logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
    }
    clientLogger.setLogLevel(log4cplus::ALL_LOG_LEVEL);
  }
  return clientParams;
}

// ClientConfig setupConsensusParams(ClientParams &clientParams) {
//   ClientConfig clientConfig;
//   clientConfig.clientId = clientParams.clientId;
//   clientConfig.fVal = clientParams.numOfFaulty;
//   clientConfig.cVal = clientParams.numOfSlow;
//   return clientConfig;
// }


int main(int argc, char **argv) {
  libutt::initialize(nullptr, 0);
  ReplicaConfig::instance().setpreExecutionFeatureEnabled(true);
  // TODO(IG:) configure Log4Cplus's output format, using default for now

  ClientParams clientParams = setupClientParams(argc, argv);
  if (clientParams.clientId == UINT16_MAX || clientParams.numOfFaulty == UINT16_MAX ||
      clientParams.numOfSlow == UINT16_MAX || clientParams.numOfOperations == UINT32_MAX) {
    LOG_FATAL(clientLogger, "Wrong usage! Required parameters: " << argv[0] << " -f F -c C -p NUM_OPS -i ID");
    exit(-1);
  }
  
  SimpleTestClient cl(clientParams, clientLogger);
  return cl.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
