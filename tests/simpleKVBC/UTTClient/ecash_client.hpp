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
#include <memory>
#include <optional>
#include <vector>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "bftclient/base_types.h"
#include "bftclient/bft_client.h"
#include "state.hpp"
#include "test_parameters.hpp"
#include "utt/Coin.h"
#include "utt/Params.h"
#include "client/Params.hpp"
#include "utt/Wallet.h"

#define DEFAULT_COIN_VALUE 1000

using namespace std;
using namespace std::chrono;
using namespace bftEngine;
using namespace bft::communication;

class EcashClient : public bft::client::Client {
    public:
        EcashClient(logging::Logger logger_,
            ICommunication* comm, 
            bft::client::ClientConfig &cp, 
            std::shared_ptr<utt_bft::client::Params> cParamsPtr_,
            std::shared_ptr<libutt::Wallet> wal_send,
            std::shared_ptr<libutt::Wallet> wal_recv
        );
        void wait_for_connections();
        ~EcashClient();
        bool verifyPayAckRSI(bft::client::Reply& reply);
        bft::client::Msg NewTestPaymentTx();

    public:
        std::shared_ptr<utt_bft::client::Params> cParamsPtr_ = nullptr;
        std::shared_ptr<libutt::Wallet> m_wallet_send_ptr_ = nullptr;
        std::shared_ptr<libutt::Wallet> m_wallet_recv_ptr_ = nullptr;

    private:
        logging::Logger logger_;

    protected:
        uint16_t num_replicas_;
};


