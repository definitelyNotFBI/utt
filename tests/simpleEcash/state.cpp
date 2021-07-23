#include <cstddef>
#include <sstream>

#include "state.hpp"
#include "bftclient/bft_client.h"

bft::client::Msg UTT_Msg::new_mint_msg(size_t value, libutt::CoinComm cc, size_t ctr) {
  std::stringstream ss;
  ss << cc;
  auto cc_buf = ss.tellp();

  bft::client::Msg msg(sizeof(UTT_Msg)+sizeof(MintMsg)+cc_buf);

  // Create the header first
  auto utt_msg = reinterpret_cast<UTT_Msg*>(msg.data());
  utt_msg->type = OpType::Mint;

  // Create the mint message
  auto mint_msg = reinterpret_cast<MintMsg*>(msg.data()+sizeof(UTT_Msg));
  mint_msg->val = value;
  mint_msg->cc_buf_len = cc_buf;
  mint_msg->coin_id = ctr;

  std::memcpy(
    msg.data()+sizeof(UTT_Msg)+sizeof(MintMsg), 
    ss.str().data(), 
    cc_buf
  );

  // std::cout << "Sent" << cc_buf << std::endl;

  // export the message
  return msg;
}