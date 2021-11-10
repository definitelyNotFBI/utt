#pragma once

#include <c++/7/optional>
#include <istream>
#include <ostream>
#include <vector>
#include <cstdlib>
#include <iostream>

#include "utt/Params.h"
#include "utt/Bank.h"

namespace utt_bft::client {

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

    // TODO: Add/Replace with Wallet
    // TODO: Add genesis coins

    // The number of nodes in the system
    size_t n;

public:
    Params(
        libutt::Params p, 
        std::vector<libutt::BankSharePK> bank_pks, 
        libutt::BankPK main_pk,
        size_t n)
        : p(std::move(p)), bank_pks(std::move(bank_pks)), main_pk(main_pk), n(n) 
    {}

public:
    void read(std::istream& in);
    void write(std::ostream& out) const;
    Params(std::istream& in);
    friend std::ostream& operator<<(std::ostream& out, const Params& params);
    friend std::istream& operator>>(std::istream& in, Params& params);

    int operator==(const Params& params) {
        if((this->p != params.p || this->n != params.n || this->main_pk.X != params.main_pk.X || this->bank_pks.size() != params.bank_pks.size())) {
            return 0;
        }
        for(size_t i=0; i<this->bank_pks.size();i++) {
            if(this->bank_pks[i].X != params.bank_pks[i].X) {
                return 0;
            }
        }
        return 1;
    }

    int operator!=(const Params& params) {
        return !operator==(params);
    }

private:
    // Disable the implicit constructor for the public or inheritors, so that the only way we can build this object is via copy/move.
    Params(): p(libutt::Params::FOR_DESERIALIZATION_ONLY()), main_pk(libutt::BankPK::FOR_DESERIALIZATION_ONLY()) {};
};


} // namespace utt_bft::client