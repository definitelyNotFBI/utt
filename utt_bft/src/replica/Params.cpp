#include <iostream>

#include <istream>
#include <libff/common/serialization.hpp>
#include <ostream>
#include "replica/Params.hpp"
#include "assertUtils.hpp"

#include "utt/Params.h"
#include "utt/RandSig.h"

#include "utt/Serialization.h"


namespace utt_bft::replica {

utt_bft::client::Params Params::ClientParams() const {
    return utt_bft::client::Params(
        this->p, 
        this->bank_pks, 
        this->main_pk, 
        this->n
    );
}

void Params::write(std::ostream& out) const {
    ConcordAssert(this->n == this->bank_pks.size());

    // this->p.write(out);
    out << p << std::endl;
    // this->main_pk.write(out);
    out << main_pk << std::endl;
    out << reg_pk << std::endl;
    out << this->n << std::endl;
    for(auto i=0ul; i<this->n; i++) {
        // this->bank_pks[i].write(out);
        out << bank_pks[i] << std::endl;
    }
    // this->my_sk.write(out);
    out << my_sk << std::endl;
}

void Params::read(std::istream &in) {
    // this->p.read(in);
    in >> p;
    libff::consume_OUTPUT_NEWLINE(in);
    // this->main_pk.read(in);
    in >> main_pk;
    libff::consume_OUTPUT_NEWLINE(in);

    in >> reg_pk;
    libff::consume_OUTPUT_NEWLINE(in);

    in >> this->n;
    libff::consume_OUTPUT_NEWLINE(in);

    this->bank_pks.reserve(this->n);
    for(auto i=0ul;i<this->n;i++) {
        libutt::RandSigSharePK bank_pk;
        in >> bank_pk;
        this->bank_pks.emplace_back(std::move(bank_pk));
        libff::consume_OUTPUT_NEWLINE(in);
    }
    // this->my_sk.read(in);
    in >> my_sk;
    libff::consume_OUTPUT_NEWLINE(in);
}

Params::Params(std::istream& in): Params() {
    this->read(in);
}

} // namespace utt_bft::client

std::ostream& operator<<(std::ostream& out, const utt_bft::replica::Params& params) {
    params.write(out);
    return out;
}

std::istream& operator>>(std::istream& in, utt_bft::replica::Params& params) {
    params.read(in);
    return in;
}
