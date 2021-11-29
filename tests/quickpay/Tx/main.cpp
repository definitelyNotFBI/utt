#include "utt/PolyCrypto.h"

#include "setup.hpp"

int main(int argc, char* argv[])
{
    libutt::initialize(nullptr);

    // Process the args
    auto setup = Setup::ParseArgs(argc, argv);

    // DONE: Read the wallets
    // DONE: Generate quickpay Txs
    setup->makeTx();

    return 0;
}

