#include <cstring>
#include <iostream>

#include <istream>
#include <libff/common/serialization.hpp>
#include <ostream>
#include "utt/Bank.h"
#include "utt/Params.h"
#include "client/Params.hpp"
#include "assertUtils.hpp"

namespace utt_bft::client {

void Params::write(std::ostream& out) const {
    ConcordAssert(this->n == this->bank_pks.size());

    this->p.write(out);
    this->main_pk.write(out);
    out << this->n << std::endl;

    for(auto i=0ul; i<this->n; i++) {
        this->bank_pks[i].write(out);
    }
}

void Params::read(std::istream &in) {
    this->p.read(in);
    this->main_pk.read(in);
    in >> this->n;
    libff::consume_OUTPUT_NEWLINE(in);

    this->bank_pks.reserve(this->n);

    for(auto i=0ul; i<this->n; i++) {
        this->bank_pks.emplace_back(in);
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