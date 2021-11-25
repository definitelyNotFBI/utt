#include <log4cplus/logger.h>
#include <memory>
#include <thread>
#include "assertUtils.hpp"
#include "bftclient/bft_client.h"
#include "bftclient/config.h"
#include "communication/ICommunication.hpp"
#include "client/IClient.hpp"

#include "client/Params.hpp"

using namespace bft::communication;
using namespace bft::client;
typedef utt_bft::client::Params ClientParams;

using bft::client::ClientConfig;

namespace utt_bft::client {

IClient::IClient(ICommunication* comm, ClientParams cp, const ClientConfig& config)
    :Client(std::unique_ptr<ICommunication>(comm), config),
    cp_(std::move(cp)),
    num_replicas_(cp.n)
{
    ConcordAssert(cp.n == config.all_replicas.size());
}

void IClient::start() {
    wait_for_connections();
}

void IClient::wait_for_connections() {
    communication_->Start();
  
  bool connectedToPrimary = false;
  while (true) {
      auto readyReplicas = 0ul;
      std::this_thread::sleep_for(1s);
      for (size_t i = 0; i < num_replicas_; ++i) {
        readyReplicas += (communication_->getCurrentConnectionStatus(i) == ConnectionStatus::Connected);
        if (i==0) {
          connectedToPrimary = true;
        }
      }
      if (readyReplicas >= num_replicas_ - config_.f_val && connectedToPrimary) {
        break;
      }
  }
}

}