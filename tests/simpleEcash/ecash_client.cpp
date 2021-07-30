// Concord
//
// Copyright (c) 2018-2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include <xassert/XAssert.h>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "assertUtils.hpp"
#include "ecash_client.hpp"
#include "SimpleClient.hpp"
#include "adapter_config.hpp"
#include "bftclient/bft_client.h"
#include "state.hpp"
#include "utt/Bank.h"
#include "utt/Coin.h"
#include "utt/Params.h"
#include "utt/Utt.h"
#include "utt/Utils.h"

using namespace std;
using namespace std::chrono;
using namespace bftEngine;
using namespace bft::communication;
using namespace libutt;

std::set<bft::client::ReplicaId> generateSetOfReplicas_helpFunc(const int16_t numberOfReplicas) {
  std::set<bft::client::ReplicaId> retVal;
  for (uint16_t i = 0; i < numberOfReplicas; i++) {
      retVal.insert(bft::client::ReplicaId{i});
  }
  return retVal;
}

EcashClient::EcashClient(ICommunication *comm, 
  bft::client::ClientConfig &conf, 
  libutt::Params&& p, 
  std::vector<libutt::BankSharePK>&& bank_pks, libutt::BankPK bpk)
    :bft::client::Client(std::unique_ptr<ICommunication>(comm),conf),
    num_replicas_(conf.all_replicas.size()),
    my_coins(),
    bank_pks(bank_pks),
    p(p),
    my_ltsk(libutt::LTSK::random()),
    my_ltpk(libutt::LTPK(p, my_ltsk)),
    bpk(bpk)
{
}

void EcashClient::wait_for_connections() {
  communication_->Start();
  
  bool connectedToPrimary = false;
  while (true) {
      auto readyReplicas = 0;
      this_thread::sleep_for(1s);
      for (int i = 0; i < num_replicas_; ++i) {
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

EcashClient::~EcashClient() {
  // communication_->Stop();
  this->stop();
}

// Send an epk since it is a commitment to a coin with zero value
// Also return the coin ID
std::tuple<size_t, EPK> EcashClient::new_coin() {
  auto esk = libutt::ESK::random();
  auto epk = esk.toEPK(p);
  my_coins[coinCounter] = esk;
  return std::make_tuple<size_t, libutt::EPK>(coinCounter++, std::move(epk));
}

std::optional<CoinSig> EcashClient::verifyMintAckRSI(bft::client::Reply& reply) {
  // auto test_ctr = 0;
  // Check size for matched data
  if (reply.matched_data.size() != sizeof(UTT_Msg)) {
    printf("Invalid matched data\n");
    return nullopt;
  }

  // Check correct response format
  auto utt_msg = reinterpret_cast<UTT_Msg*>(reply.matched_data.data());
  if(utt_msg->type != OpType::MintAck) {
    printf("Invalid OpType\n");
    return nullopt;
  }

  auto ids = std::vector<size_t>(reply.rsi.size());
  auto signed_shares = std::vector<CoinSigShare>(reply.rsi.size());
  // printf("Reached %d\n", 0);
  size_t coin_id = 0;
  
  // Check size of rsi for every message
  for(auto& [sender, rsi]: reply.rsi) {
    if(rsi.size() <= sizeof(MintAckMsg)) {
      printf("Invalid size for rsi\n");
      return nullopt;
    } 

    auto mint_ack = reinterpret_cast<MintAckMsg*>(rsi.data());
    if(rsi.size() != sizeof(MintAckMsg)+mint_ack->coin_sig_share_size) {
      printf("Mismatch of size %lu in MintAck %lu\n", rsi.size(), sizeof(MintAckMsg)+mint_ack->coin_sig_share_size);
      return nullopt;
    }

    std::stringstream ss;
    ss.write(
      reinterpret_cast<char*>(rsi.data()+sizeof(MintAckMsg)),
      static_cast<long>(mint_ack->coin_sig_share_size)
    );
    
    libutt::CoinSigShare coinShare;
    ss >> coinShare;

    // printf("Attempting to verify\n");
    // For now, I know it is 1000
    // TODO(AB): Get it from the coin id
    auto& esk = my_coins[mint_ack->coin_id];
    coin_id = mint_ack->coin_id;
    libutt::CoinComm cc(p, esk.toEPK(p), 1000);

    if(!coinShare.verify(p, cc, bank_pks[sender.val])) {
      printf("Coin share verification failed for %u\n", sender.val);
      return nullopt;
    }

    ids.push_back(sender.val);
    signed_shares.push_back(coinShare);
  }

  // Now that we have f+1 valid signatures on the coin commitment, we can construct the aggregate now
  auto& esk = my_coins[coin_id];
  CoinComm cc(p, esk.toEPK(p), 1000);

  // Combine the threshold coins
  auto combined_coin = CoinSig::aggregate(num_replicas_, signed_shares, ids);
  if (!combined_coin.verify(p, cc, bpk)) {
    printf("Combined verification failed before re-randomization\n");
    return nullopt;
  }

  // Re-randomize the coin
  auto r_delta = libutt::Fr::random_element(), 
    u_delta = libutt::Fr::random_element();
  combined_coin.rerandomize(r_delta, u_delta);

  // TODO: Check if the newly minted coin verifies against the aggregated public key of the servers, because if it doesn't then the servers won't accept this coin
  if (!combined_coin.verify(p, cc, bpk)) {
    printf("Combined verification failed after re-randomization\n");
    return nullopt;
  }
  return std::optional<CoinSig>(combined_coin);
}