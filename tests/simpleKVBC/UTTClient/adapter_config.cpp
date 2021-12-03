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

#include "bftclient/config.h"
#include <chrono>
#include <iostream>
#include "bftengine/SimpleClient.hpp"
#include "test_parameters.hpp"

bft::client::ClientConfig make_adapter_config(ClientParams& cp, const bftEngine::SimpleClientParams& scp) {
    bft::client::ClientConfig c;
    c.retry_timeout_config.initial_retry_timeout = std::chrono::milliseconds(scp.clientInitialRetryTimeoutMilli);
    c.retry_timeout_config.max_retry_timeout = std::chrono::milliseconds(scp.clientMaxRetryTimeoutMilli);
    c.retry_timeout_config.min_retry_timeout = std::chrono::milliseconds(scp.clientMinRetryTimeoutMilli);
    c.retry_timeout_config.number_of_standard_deviations_to_tolerate = scp.numberOfStandardDeviationsToTolerate;
    c.retry_timeout_config.samples_per_evaluation = scp.samplesPerEvaluation;
    c.retry_timeout_config.samples_until_reset = scp.samplesUntilReset;
    
    c.c_val = cp.numOfSlow;
    c.f_val = cp.numOfFaulty;
    c.id = bft::client::ClientId{cp.clientId};
    for(uint16_t i = 0;i<cp.get_numOfReplicas(); i++) 
        c.all_replicas.insert(bft::client::ReplicaId{i});
    
    std::cout << "Initial retry timeout" 
        << scp.clientInitialRetryTimeoutMilli 
        << std::endl
        << "Max retry timeout"
        << scp.clientMaxRetryTimeoutMilli
        << std::endl
        ;

    return c;
}