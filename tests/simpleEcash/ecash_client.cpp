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
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Logging4cplus.hpp"
#include "assertUtils.hpp"
#include "bftclient/base_types.h"
#include "ecash_client.hpp"
#include "SimpleClient.hpp"
#include "adapter_config.hpp"
#include "bftclient/bft_client.h"
#include "kvstream.h"
#include "state.hpp"
#include "utt/Bank.h"
#include "utt/Coin.h"
#include "utt/Keys.h"
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

EcashClient::EcashClient(logging::Logger logger_,
  ICommunication *comm, 
  bft::client::ClientConfig &conf, 
  const libutt::Params &p, 
  std::vector<libutt::BankSharePK>&& bank_pks, libutt::BankPK bpk,
  std::vector<std::tuple<libutt::CoinSecrets, libutt::CoinComm, libutt::CoinSig>> &&my_initial_coin_vec
  ) :
    bft::client::Client(std::unique_ptr<ICommunication>(comm),conf),
    p(p),
    my_ltsk(libutt::LTSK::random()),
    my_ltpk(libutt::LTPK(p, my_ltsk)),
    bpk(bpk),
    logger_(logger_),
    num_replicas_(conf.all_replicas.size()),
    my_coins(),
    bank_pks(bank_pks),
    my_initial_coins(my_initial_coin_vec)
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

bft::client::Msg EcashClient::NewMintTx(long value) {
  auto [ctr, empty_coin] = new_coin();
  std::stringstream ss;
  ss << empty_coin;
  auto cc_buf = ss.str().size();

  bft::client::Msg msg(sizeof(UTT_Msg)+sizeof(MintMsg)+cc_buf);

  // Create the header first
  auto utt_msg = UTT_MSG_PTR(msg.data());
  utt_msg->type = OpType::Mint;

  // Create the Mint Msg header
  auto mint_msg = MINT_MSG_PTR(msg.data());
  mint_msg->val = value;
  mint_msg->coin_id = ctr;
  mint_msg->cc_buf_len = cc_buf;

  // Copy the mint empty coin data
  std::memcpy(
    MINT_EMPTY_COIN_PTR(msg.data()), 
    ss.str().data(), 
    cc_buf
  );

  // export the message
  return msg;
}

bool EcashClient::verifyPayAckRSI(bft::client::Reply& reply) {
  // Check size for matched data
  if (reply.matched_data.size() < sizeof(UTT_Msg)) {
    LOG_ERROR(logger_, "Invalid matched data");
    return false;
  }
  // Check correct response format
  auto utt_msg = UTT_MSG_PTR(reply.matched_data.data());
  if(utt_msg->type != OpType::PayAck) {
    LOG_ERROR(logger_, "Invalid OpType");
    return false;
  }

  // Check RSI for every sender
  for(auto& [sender, rsi]: reply.rsi) {
    std::stringstream ss;
    ss.write(reinterpret_cast<const char*>(rsi.data()), rsi.size());
    libutt::Tx tx;
    ss >> tx;

    for(int i=0; i<2;i++) {
      libutt::CoinComm cc;
      libutt::CoinSig csign;

      ss >> cc;
      tx.outs[i].cc = std::make_unique<libutt::CoinComm>(cc);

      ss >> csign;
      tx.outs[i].sig = std::make_unique<libutt::CoinSig>(csign);

      // Check if it is mine
      auto mine = tx.outs[i].isMine(p, my_ltsk);
      if(!mine.has_value()) {
        printf("Expected a value :(");
        return false;
      }

      // Check signatures
      if (tx.outs[i].sig->verify(p, *tx.outs[i].cc.get(), bank_pks[sender.val])) {
        printf("Invalid signature from the sender");
        return false;
      }
    }
  }
  // Aggregate signatures

  // Make it mine
  
  return true;
}

std::optional<CoinSig> EcashClient::verifyMintAckRSI(bft::client::Reply& reply) {
  // Check size for matched data
  if (reply.matched_data.size() < sizeof(UTT_Msg)) {
    LOG_ERROR(logger_, "Invalid matched data");
    return nullopt;
  }

  // Check correct response format
  auto utt_msg = UTT_MSG_PTR(reply.matched_data.data());
  if(utt_msg->type != OpType::MintAck) {
    LOG_ERROR(logger_, "Invalid OpType");
    return nullopt;
  }

  auto ids = std::vector<size_t>();
  ids.reserve(reply.rsi.size());
  auto signed_shares = std::vector<CoinSigShare>();
  signed_shares.reserve(reply.rsi.size());
  libutt::CoinComm cc;
  bool first = true;
  
  // Check size of rsi for every message
  for(auto& [sender, rsi]: reply.rsi) {
    if(rsi.size() < sizeof(MintAckMsg)) {
      LOG_ERROR(logger_, "Invalid size for rsi\n");
      return nullopt;
    } 

    auto mint_ack = MINTACK_MSG_PTR(rsi.data());
    if(rsi.size() != sizeof(MintAckMsg)+mint_ack->coin_sig_share_size) {
      LOG_ERROR(logger_, 
        "Mismatch of size in RSI:" << rsi.size() << 
        ", and in MintAck: " << 
          sizeof(MintAckMsg)+mint_ack->coin_sig_share_size
      );
      return nullopt;
    }

    std::stringstream ss;

    ss.write(
      MINTACK_COINSHARE_PTR(rsi.data()),
      static_cast<long>(mint_ack->coin_sig_share_size)
    );
    
    libutt::CoinSigShare coinShare;
    ss >> coinShare;

    // For now, I know it is 1000
    // TODO(AB): Get it from the coin id
    if (my_coins.find(mint_ack->coin_id) == my_coins.end()) {
      LOG_ERROR(logger_, "I don't know the coin id");
      return nullopt;
    }
    auto& esk = my_coins[mint_ack->coin_id];

    libutt::CoinComm cc2(p, esk.toEPK(p), DEFAULT_COIN_VALUE);

    if(!first && cc != cc2) {
      LOG_ERROR(logger_, "Got invalid coin commitments");
      return nullopt;
    } else {
      first = false;
      cc = cc2;
    }
    // std::cout << "cc in loop" << cc << std::endl;

    if(!coinShare.verify(p, cc, bank_pks[sender.val])) {
      LOG_ERROR(logger_, "Coin share verification failed for " << sender.val);
      LOG_ERROR(logger_, "Params:" << p);
      LOG_ERROR(logger_, "Coin Commitment:" << cc);
      LOG_ERROR(logger_, "Coin Sig Share:" << coinShare);
      return nullopt;
    }

    ids.push_back(sender.val);
    // std::cout << "Adding id " << sender.val << std::endl;
    signed_shares.push_back(coinShare);
  }

  // Now that we have f+1 valid signatures on the coin commitment, we can construct the aggregate now
  // std::cout << "esk out loop" << esk.s << std::endl;
  // std::cout << "p out loop" << p << std::endl;
  // std::cout << "cc out loop" << cc << std::endl;

  // Combine the threshold coins
  LOG_DEBUG(logger_, "NumReplicas:" << num_replicas_);
  LOG_DEBUG(logger_, "Signed Shares Len:" << signed_shares.size());
  LOG_DEBUG(logger_, "Ids len:" << ids.size());
  auto combined_coin = CoinSig::aggregate(num_replicas_, signed_shares, ids);

  // TODO: No idea why this fails, for now move on. Already spent 2 days on this.
  if (!combined_coin.verify(p, cc, bpk)) {
    LOG_ERROR(logger_, "Combined verification failed before re-randomization");
    return nullopt;
  }

  // Re-randomize the coin
  // auto r_delta = libutt::Fr::random_element(), 
  //   u_delta = libutt::Fr::random_element();
  // combined_coin.rerandomize(r_delta, u_delta);

  // TODO: Check if the newly minted coin verifies against the aggregated public key of the servers, because if it doesn't then the servers won't accept this coin
  // TODO (Check with Alin on how to do this correctly)
  // if (!combined_coin.verify(p, cc, bpk)) {
  //   LOG_ERROR(logger_, "Combined verification failed after re-randomization");
  //   return nullopt;
  // }
  return std::optional<CoinSig>(combined_coin);
}

bft::client::Msg EcashClient::NewTestPaymentTx() {
  std::vector<std::tuple<libutt::LTPK, libutt::Fr>> recv;
  recv.reserve(2);
  for(auto j=0; j<2;j++) {
    auto r = libutt::Fr(std::get<0>(my_initial_coins[j]).val);
    recv.push_back(std::make_tuple(my_ltpk, r));
  }
  // printf("Tag %d\n", 3);

  libutt::Tx tx = libutt::Tx::create(p, my_initial_coins, recv);
  // printf("Tag %d\n", 4);
  // std::cout << tx << std::endl;

  printf("Self test %d\n", tx.verify(p, bpk));
  // std::cout << "Sending Tx: " << tx << std::endl;

  std::stringstream ss;
  ss << tx;

  bft::client::Msg msg(sizeof(UTT_Msg)+ss.str().size());
  UTT_Msg* m = reinterpret_cast<UTT_Msg*>(msg.data());
  m->type = OpType::Pay;

  std::memcpy(msg.data()+sizeof(UTT_Msg),ss.str().data(), ss.str().size());
  
  return msg;
}