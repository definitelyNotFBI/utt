#pragma once

#include <istream>
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>

#include "utt/Params.h"
#include "utt/RandSig.h"

#include "client/Params.hpp"
#include "utt/RegAuth.h"


namespace utt_bft::replica {

struct Params {
// The data structure parts
public:
    // The general parameters: g,g_1,g_2, ...
    // SEE libutt::Params
    libutt::Params p;

    // A vector of all the replica's bank public keys
    std::vector<libutt::RandSigSharePK> bank_pks;

    // The main public key. This is the key that is threshold shared in bank_pks
    libutt::RandSigPK main_pk;

    // TODO: Add RPK, IBE MPK
    libutt::RegAuthPK reg_pk;

    // The number of nodes in the system
    size_t n;

    // My secret Key
    libutt::RandSigShareSK my_sk;

public:
    Params(libutt::Params p, 
            std::vector<libutt::RandSigSharePK> bank_pks, 
            libutt::RandSigPK main_pk, 
            libutt::RegAuthPK reg_pk,
            size_t n, 
            libutt::RandSigShareSK my_sk
        ): p(std::move(p)), 
            bank_pks(std::move(bank_pks)), 
            main_pk(std::move(main_pk)), 
            reg_pk{std::move(reg_pk)}, 
            n{n}, 
            my_sk{std::move(my_sk)} 
        {}

public:
    int operator==(const Params& p) {
        if(!(p.p == this->p &&
            p.bank_pks.size() == this->bank_pks.size() &&
            p.n == this->n &&
            p.main_pk == this->main_pk &&
            p.reg_pk.mpk == this->reg_pk.mpk &&
            p.reg_pk.vk == this->reg_pk.vk &&
            p.my_sk == this->my_sk)) {
                return 0;
            }
        for(size_t i=0; i<n;i++) {
            if (this->bank_pks[i] != p.bank_pks[i]) {
                return 0;
            }
        }
        return 1;
    }

    int operator!=(const Params& p) {
        return !(*this == p);
    }

// The public methods
public:
    // Returns the corresponding instance of client params object
    utt_bft::client::Params ClientParams() const;

public:
    void write(std::ostream& out) const;
    void read(std::istream& in);
    Params(std::istream& in);

private:
    // Disable the implicit constructor for the public or inheritors, so that the only way we can build this object is via copy/move.
    Params() {};
};

} // namespace utt_bft::client

std::ostream& operator<<(std::ostream& out, const utt_bft::replica::Params& params);