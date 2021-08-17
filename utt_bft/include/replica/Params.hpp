#pragma once

#include <istream>
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>

#include "utt/Params.h"
#include "utt/Bank.h"

#include "client/Params.hpp"

namespace utt_bft::replica {

struct Params {
// The data structure parts
public:
    // The general parameters: g,g_1,g_2, ...
    // SEE libutt::Params
    libutt::Params p;

    // A vector of all the replica's bank public keys
    std::vector<libutt::BankSharePK> bank_pks;

    // The main public key. This is the key that is threshold shared in bank_pks
    libutt::BankPK main_pk;

    // The number of nodes in the system
    size_t n;

    // My secret Key
    libutt::BankShareSK my_sk;

public:
    Params(libutt::Params p, std::vector<libutt::BankSharePK> bank_pks, libutt::BankPK main_pk, size_t n, libutt::BankShareSK my_sk)
        : p(std::move(p)), bank_pks(std::move(bank_pks)), 
            main_pk(main_pk), n{n}, 
            my_sk{my_sk} 
    {}

public:
    int operator==(const Params& p) {
        if(!(p.p == this->p &&
            p.bank_pks.size() == this->bank_pks.size() &&
            p.n == this->n &&
            p.main_pk.X == this->main_pk.X &&
            p.my_sk == this->my_sk)) {
                return 0;
            }
        for(size_t i=0; i<n;i++) {
            if (this->bank_pks[i].X != p.bank_pks[i].X) {
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
    friend std::ostream& operator<<(std::ostream& out, const Params& params);
    friend std::istream& operator>>(std::istream& in, Params& params);

private:
    // Disable the implicit constructor for the public or inheritors, so that the only way we can build this object is via copy/move.
    Params(): p(libutt::Params::FOR_DESERIALIZATION_ONLY()), main_pk(libutt::BankPK::FOR_DESERIALIZATION_ONLY()), my_sk(libutt::BankShareSK::FOR_DESERIALIZATION_ONLY()) 
    {};
};


} // namespace utt_bft::client