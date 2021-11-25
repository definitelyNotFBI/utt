#include <cstring>
#include <iostream>

#include <istream>
#include <libff/common/serialization.hpp>
#include <ostream>
#include "utt/Params.h"
#include "client/Params.hpp"
#include "assertUtils.hpp"
#include "utt/RandSig.h"

namespace utt_bft::client {

void Params::write(std::ostream& out) const {
    ConcordAssert(this->n == this->bank_pks.size());

    // this->p.write(out);
    out << p << std::endl;
    // this->main_pk.write(out);
    out << main_pk << std::endl;
    out << this->n << std::endl;

    for(auto i=0ul; i<this->n; i++) {
        // this->bank_pks[i].write(out);
        out << bank_pks[i] << std::endl;
    }
}

void Params::read(std::istream &in) {
    // this->p.read(in);
    in >> p;
    libff::consume_OUTPUT_NEWLINE(in);
    // this->main_pk.read(in);
    in >> main_pk;
    libff::consume_OUTPUT_NEWLINE(in);

    in >> this->n;
    libff::consume_OUTPUT_NEWLINE(in);

    this->bank_pks.reserve(this->n);

    for(auto i=0ul; i<this->n; i++) {
        libutt::RandSigSharePK bank_pk;
        in >> bank_pk;
        libff::consume_OUTPUT_NEWLINE(in);
        this->bank_pks.emplace_back(std::move(bank_pk));
    }
}

std::ostream& operator<<(std::ostream& out, const Params& params) {
    params.write(out);
    return out;
}

Params::Params(std::istream& in): Params() {
    this->read(in);
}

std::istream& operator>>(std::istream& in, Params& params) {
    params.read(in);
    return in;
}

} // namespace utt_bft::client