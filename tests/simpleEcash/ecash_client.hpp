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

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "Logger.hpp"
#include "bftclient/base_types.h"
#include "bftclient/bft_client.h"
#include "test_parameters.hpp"
#include "utt/Bank.h"
#include "utt/Coin.h"
#include "utt/Params.h"

using namespace std;
using namespace std::chrono;
using namespace bftEngine;
using namespace bft::communication;

class EcashClient : public bft::client::Client {
    public:
        EcashClient(ICommunication* comm, 
            bft::client::ClientConfig &cp, 
            libutt::Params &&p, 
            std::vector<libutt::BankSharePK> &&bank_pks,
            libutt::BankPK bpk
        );
        void wait_for_connections();
        ~EcashClient();
        static void Pool(size_t);
        std::tuple<size_t, libutt::EPK> new_coin();
        std::optional<libutt::CoinSig> verifyMintAckRSI(bft::client::Reply& reply);
    protected:
        uint16_t num_replicas_;
        bft::client::RequestConfig req_config_;
        std::unordered_map<size_t, libutt::ESK> my_coins;
        std::vector<libutt::BankSharePK> bank_pks;
    private:
    // TODO(Come from metadata storage)
        libutt::Params p;
        libutt::LTSK my_ltsk;
        libutt::LTPK my_ltpk;
        size_t coinCounter = 0;
        libutt::BankPK bpk;
};

