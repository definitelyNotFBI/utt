#pragma once

#include <string>

namespace utt_bft {

// Use this key in replica config to store utt-pvt-replica.dat
const std::string UTT_PARAMS_REPLICA_KEY = "utt.bft.replica.params_prefix";

// Use this key in the client config to store utt-pub-client.dat
const std::string UTT_PARAMS_CLIENT_KEY = "utt.bft.client.params";

} // namespace utt_bft