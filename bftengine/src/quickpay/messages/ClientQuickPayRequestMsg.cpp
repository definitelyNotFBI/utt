// Concord
//
// Copyright (c) 2019-2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the sub-component's license, as noted in the LICENSE
// file.

#include "ClientQuickPayRequestMsg.hpp"
#include "SimpleClient.hpp"

namespace quickpay {

using namespace std;
using namespace bftEngine;

ClientQuickPayRequestMsg::ClientQuickPayRequestMsg(NodeIdType sender,
                                                       uint64_t reqSeqNum,
                                                       uint32_t requestLength,
                                                       const char* request,
                                                       uint64_t reqTimeoutMilli,
                                                       const std::string& cid,
                                                       const concordUtils::SpanContext& spanContext,
                                                       const char* requestSignature,
                                                       uint32_t requestSignatureLen)
    : ClientRequestMsg(sender,
                       QUICK_PAY_REQ,
                       reqSeqNum,
                       requestLength,
                       request,
                       reqTimeoutMilli,
                       cid,
                       spanContext,
                       requestSignature,
                       requestSignatureLen) {
  msgBody_->msgType = MsgCode::QuickPayRequest;
}

unique_ptr<MessageBase> ClientQuickPayRequestMsg::convertToClientRequestMsg(bool emptyReq) {
  msgBody()->flags &= ~(1 << 1);
  unique_ptr<MessageBase> clientRequestMsg = make_unique<ClientRequestMsg>(clientProxyId(),
                                    flags(),
                                    requestSeqNum(),
                                    emptyReq ? 0 : requestLength(),
                                    requestBuf(),
                                    requestTimeoutMilli(),
                                    getCid(),
                                    spanContext<ClientRequestMsg>(),
                                    requestSignature(),
                                    requestSignatureLength());
  return clientRequestMsg;
}

}  // namespace quickpay
