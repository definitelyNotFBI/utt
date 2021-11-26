#include "utt/PolyCrypto.h"

#include "setup.hpp"

int main(int argc, char* argv[])
{
    libutt::initialize(nullptr);

    // Process the args
    auto setup = Setup::ParseArgs(argc, argv);

    // TODO: Read the wallets
    setup->makeTx(4);

    // TODO: Generate quickpay Txs
    return 0;
}

