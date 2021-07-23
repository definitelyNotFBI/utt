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
#include <sstream>
#include <tuple>
#include <unordered_map>

#include "ecash_client.hpp"
#include "SimpleClient.hpp"
#include "adapter_config.hpp"
#include "bftclient/bft_client.h"
#include "state.hpp"
#include "utt/Coin.h"
#include "utt/Utt.h"

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

EcashClient::EcashClient(ICommunication *comm, bft::client::ClientConfig &conf)
  :bft::client::Client(std::unique_ptr<ICommunication>(comm),conf),
  num_replicas_(conf.all_replicas.size()),
  my_coins(),
  bank_pks()
{

  libutt::initialize(nullptr);

  // Load the params file
  p = new libutt::Params;
  std::ifstream ifile("../utt_pub_client.dat");
  ifile >> *p;
  size_t n =num_replicas_;
  for(size_t i=0; i<n;i++) {
    BankSharePK bspk;
    ifile >> bspk;
    bank_pks.push_back(bspk);
  }
  assertEqual(bank_pks.size(), num_replicas_);
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

CoinComm EcashClient::new_coin() {
  // CoinSecrets cs(*p, 0);
  auto x = CoinComm::NewCoin(*p, 100);
  my_coins[coinCounter++] = x;
  auto cc = std::get<0>(x);
  return cc;
}

bool EcashClient::verifyMintAckRSI(bft::client::Reply& reply) {
  // auto test_ctr = 0;
  // Check size for matched data
  if (reply.matched_data.size() != sizeof(UTT_Msg)) {
    printf("Invalid matched data\n");
    return false;
  }

  // Check correct response format
  auto utt_msg = reinterpret_cast<UTT_Msg*>(reply.matched_data.data());
  if(utt_msg->type != OpType::MintAck) {
    printf("Invalid OpType\n");
    return false;
  }

  auto coin_shares = std::unordered_map<uint16_t, CoinSig>(reply.rsi.size());
  printf("Reached %d\n", 0);
  // Check size of rsi for every message
  for(auto& [sender, rsi]: reply.rsi) {
    if(rsi.size() <= sizeof(MintAckMsg)) {
      printf("Invalid size for rsi\n");
      return false;
    } 
    auto mint_ack = reinterpret_cast<MintAckMsg*>(rsi.data());
    if(rsi.size() != sizeof(MintAckMsg)+mint_ack->coin_sig_share_size) {
      printf("Mismatch of size %lu in MintAck %lu\n", rsi.size(), sizeof(MintAckMsg)+mint_ack->coin_sig_share_size);
      return false;
    }
    std::stringstream ss;
    ss.read(
     reinterpret_cast<char*>(rsi.data()+sizeof(MintAckMsg)), 
     static_cast<long>(mint_ack->coin_sig_share_size)
    );
    printf("Reached %d; Sender is %u\n", 1, sender.val);
    
    // libutt::CoinSig coinShare;
    // ss >> coinShare;

    printf("Reached %d\n", 4);

    // printf("Attempting to verify\n");
    auto& [cc, cs] = my_coins[mint_ack->coin_id];
    (void)cs;
    // if(coinShare.verify(*p, cc, bank_pks[sender.val])) {
    //   printf("Coin share verification failed for %u", sender.val);
    // }

    // coin_shares.emplace(sender.val, CoinSig{coinShare});
    printf("Reached %d\n", 2);
  }
  printf("Reached %d\n", 3);
  return true;
}