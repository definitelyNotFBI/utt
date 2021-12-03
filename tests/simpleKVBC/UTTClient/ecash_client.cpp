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
#include <memory>
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
#include "utt/Tx.h"

#include "simpleKVBTestsBuilder.hpp"
#include "utt/Wallet.h"

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
                          std::shared_ptr<utt_bft::client::Params> cParamsPtr_,
                          std::shared_ptr<libutt::Wallet> wal_send,
                          std::shared_ptr<libutt::Wallet> wal_recv
                        ) 
      : bft::client::Client(std::unique_ptr<ICommunication>(comm),conf),
        cParamsPtr_{std::move(cParamsPtr_)},
        m_wallet_send_ptr_{std::move(wal_send)},
        m_wallet_recv_ptr_{std::move(wal_recv)},
        logger_(logger_),
        num_replicas_(conf.all_replicas.size())
      {}

void EcashClient::wait_for_connections() {
  // communication_->Start();
  
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

// // Send an epk since it is a commitment to a coin with zero value
// // Also return the coin ID
// std::tuple<size_t, EPK> EcashClient::new_coin() {
//   auto [res, found] = my_coins.emplace(coinCounter, ESK::random());
//   auto& esk = res->second;
//   auto epk = esk.toEPK(cParamsPtr_->p);
//   return std::make_tuple<size_t, libutt::EPK>(coinCounter++, std::move(epk));
// }

// bft::client::Msg EcashClient::NewMintTx(long value) {
//   auto [ctr, empty_coin] = new_coin();
//   std::stringstream ss;
//   ss << empty_coin;
//   auto cc_buf = ss.str().size();

//   bft::client::Msg msg;
//   msg.resize((BasicRandomTests::SimpleMintRequest::getSize(cc_buf)));
//   // Build the message
//   auto* req = (BasicRandomTests::SimpleMintRequest*)msg.data();
//   req->header.type = BasicRandomTests::MINT;
//   req->val = value;
//   req->coinId = ctr;
//   req->cc_buf_len = cc_buf;
//   std::memcpy(req->getMintBuffer(), ss.str().data(), cc_buf);
//   // export the message
//   return msg;
// }

bool EcashClient::verifyPayAckRSI(bft::client::Reply& reply) {
  // Check size for matched data
  if (reply.matched_data.size() < sizeof(BasicRandomTests::SimpleReply)) {
    LOG_ERROR(logger_, "Invalid matched data");
    return false;
  }
  // Check correct response format
  auto utt_msg = (BasicRandomTests::SimpleReply*)reply.matched_data.data();
  if(utt_msg->type != BasicRandomTests::PAY) {
    LOG_ERROR(logger_, "Invalid OpType");
    return false;
  }

  // Check RSI for every sender
//   for(auto& [sender, rsi]: reply.rsi) {
//     std::stringstream ss;
//     ss.write(reinterpret_cast<const char*>(rsi.data()), rsi.size());
//     libutt::Tx tx(ss);

//     for(int i=0; i<2;i++) {
//       libutt::CoinComm cc(ss);
//       tx.outs[i].cc = std::make_unique<libutt::CoinComm>(cc);


//       libutt::CoinSig csign(ss);
//       tx.outs[i].sig = std::make_unique<libutt::CoinSig>(csign);

//       // Check if it is mine
//       auto mine = tx.outs[i].isMine(cParamsPtr_->p, my_ltsk);
//       if(!mine.has_value()) {
//         printf("Expected a value :(");
//         return false;
//       }

//       // Check signatures
//       if (tx.outs[i].sig->verify(cParamsPtr_->p, *tx.outs[i].cc.get(), cParamsPtr_->bank_pks[sender.val])) {
//         printf("Invalid signature from the sender");
//         return false;
//       }
//     }
//   }
  // Aggregate signatures

  // Make it mine
  
  return true;
}

// std::optional<CoinSig> EcashClient::verifyMintAckRSI(bft::client::Reply& reply) {
//   // Check size for matched data
//   if (reply.matched_data.size() != 
//           BasicRandomTests::SimpleReply_Mint::getCommonSize()) 
//   {
//     LOG_ERROR(logger_, "Invalid size for matched data");
//     return nullopt;
//   }

//   // Check correct response format
//   auto utt_msg = (BasicRandomTests::SimpleReply_Mint*)reply.matched_data.data();
//   if(utt_msg->header.type != BasicRandomTests::MINT) {
//     LOG_ERROR(logger_, "Invalid OpType");
//     return nullopt;
//   }

//   auto ids = std::vector<size_t>();
//   ids.reserve(reply.rsi.size());
//   auto signed_shares = std::vector<CoinSigShare>();
//   signed_shares.reserve(reply.rsi.size());
//   std::optional<libutt::CoinComm> cc;
  
//   // Check size of rsi for every message
//   for(auto& [sender, rsi]: reply.rsi) {
//     auto cc_buf = BasicRandomTests::SimpleReply_Mint::RSI_getCoinSigShareLen(rsi.data());
//     if(rsi.size() != cc_buf + sizeof(BasicRandomTests::SimpleReply_Mint::coin_sig_share_len)) {
//       LOG_ERROR(logger_, "Invalid size for rsi\n");
//       return nullopt;
//     }

//     std::stringstream ss;
//     auto coin_buf = BasicRandomTests::SimpleReply_Mint::RSI_getCoinSigShareBuffer(rsi.data());
//     ss.write(
//       reinterpret_cast<const char*>(coin_buf),
//       static_cast<long>(cc_buf)
//     );
    
//     libutt::CoinSigShare coinShare(ss);

//     // For now, I know it is 1000
//     // TODO(AB): Get it from the coin id
//     auto item = my_coins.find(utt_msg->coinId); 
//     if (item == my_coins.end()) {
//       LOG_ERROR(logger_, "I don't know the coin id");
//       return nullopt;
//     }
//     auto& esk = item->second;

//     libutt::CoinComm cc2(cParamsPtr_->p, esk.toEPK(cParamsPtr_->p), DEFAULT_COIN_VALUE);

//     if(cc.has_value() && cc != cc2) {
//       LOG_ERROR(logger_, "Got invalid coin commitments");
//       return nullopt;
//     } else {
//       cc = cc2;
//     }
//     // std::cout << "cc in loop" << cc << std::endl;

//     if(!coinShare.verify(cParamsPtr_->p, cc.value(), cParamsPtr_->bank_pks[sender.val])) {
//       LOG_ERROR(logger_, "Coin share verification failed for " << sender.val);
//       LOG_ERROR(logger_, "Params:" << cParamsPtr_->p);
//       LOG_ERROR(logger_, "Coin Commitment:" << cc.value());
//       LOG_ERROR(logger_, "Coin Sig Share:" << coinShare);
//       return nullopt;
//     }

//     ids.push_back(sender.val);
//     // std::cout << "Adding id " << sender.val << std::endl;
//     signed_shares.push_back(coinShare);
//   }

//   // Now that we have f+1 valid signatures on the coin commitment, we can construct the aggregate now
//   // std::cout << "esk out loop" << esk.s << std::endl;
//   // std::cout << "p out loop" << p << std::endl;
//   // std::cout << "cc out loop" << cc << std::endl;

//   // Combine the threshold coins
//   LOG_DEBUG(logger_, "NumReplicas:" << num_replicas_);
//   LOG_DEBUG(logger_, "Signed Shares Len:" << signed_shares.size());
//   LOG_DEBUG(logger_, "Ids len:" << ids.size());
//   auto combined_coin = CoinSig::aggregate(num_replicas_, signed_shares, ids);

//   // TODO: No idea why this fails, for now move on. Already spent 2 days on this.
//   if (!combined_coin.verify(cParamsPtr_->p, cc.value(), cParamsPtr_->main_pk)) {
//     LOG_ERROR(logger_, "Combined verification failed before re-randomization");
//     return nullopt;
//   }

//   // Re-randomize the coin
//   auto r_delta = libutt::Fr::random_element(), 
//     u_delta = libutt::Fr::random_element();
//   combined_coin.rerandomize(r_delta, u_delta);
  
//   // Rerandomize the coin commitments
//   cc.value().rerandomize(cParamsPtr_->p, r_delta);

//   // TODO: Check if the newly minted coin verifies against the aggregated public key of the servers, because if it doesn't then the servers won't accept this coin
//   // TODO (Check with XXX on how to do this correctly)
//   if (!combined_coin.verify(cParamsPtr_->p, cc.value(), cParamsPtr_->main_pk)) {
//     LOG_ERROR(logger_, "Combined verification failed after re-randomization");
//     return nullopt;
//   }
//   return std::optional<CoinSig>(combined_coin);
// }

bft::client::Msg EcashClient::NewTestPaymentTx() {
  // printf("Tag %d\n", 3);

  auto recip_id = m_wallet_recv_ptr_->getUserPid();
  libutt::Tx tx = m_wallet_send_ptr_->spendTwoRandomCoins(recip_id, false);

  std::stringstream ss;
  ss << tx;
  auto tx_buf_len = ss.str().size();

  bft::client::Msg msg(BasicRandomTests::SimplePayRequest::getSize(tx_buf_len));
  auto pay_req = (BasicRandomTests::SimplePayRequest*)msg.data();
  pay_req->header.type = BasicRandomTests::PAY;
  pay_req->tx_buf_len = tx_buf_len;

  std::memcpy(pay_req->getTxBuf(),ss.str().data(), tx_buf_len);
  
  return msg;
}
