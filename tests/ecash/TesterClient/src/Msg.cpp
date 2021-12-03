// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "Msg.h"
#include "threshsign/bls/relic/BlsNumTypes.h"
#include "threshsign/bls/relic/Library.h"
#include "threshsign/bls/relic/PublicParametersFactory.h"

namespace ecash::client {

MsgMint MsgMint::random() {
  BLS::Relic::G2T h_sn;
  BLS::Relic::BNT r;
  r.Random(128);
  h_sn.Random();
  MsgMint m;
  m.commitment = h_sn;
  auto params = BLS::Relic::PublicParametersFactory::getWhatever();
  auto g2 = params.getGenerator2();
  g2 = g2.Times(r);
  m.commitment = h_sn.Add(g2);
  return m;
}

}  // namespace ecash::client