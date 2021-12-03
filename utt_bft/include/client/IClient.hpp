#pragma once

#include <log4cplus/logger.h>
#include "bftclient/bft_client.h"
#include "bftclient/config.h"
#include "communication/ICommunication.hpp"

#include "client/Params.hpp"

using namespace bft::communication;

namespace utt_bft::client {
  typedef utt_bft::client::Params ClientParams;
  using bft::client::ClientConfig;
// This class implements the behaviours for a UTT BFT Client
class IClient : bft::client::Client {
public:
    IClient(ICommunication* comm, ClientParams cp, const ClientConfig& config);
    void start();
protected:
    void wait_for_connections();

private:
  ClientParams cp_;
  size_t num_replicas_;
  logging::Logger logger_ = logging::getLogger("utt.bft.client");
};
} // namespacce utt_bft
