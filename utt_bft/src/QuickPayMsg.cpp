#include <ostream>
#include "msg/QuickPay.hpp"

std::ostream& operator<<(std::ostream& out, const MintTx& tx) {
    out << tx.tx << std::endl;
    out << tx.target_shard_id << std::endl;
    out << tx.sigs.size() << std::endl;

    for(auto& [id,val]: tx.sigs) {
        out << id << std::endl;
        out << val.size() << std::endl;
        std::string value(val.begin(), val.end());
        out << value << std::endl;
    }
    return out;
}

std::istream& operator>>(std::istream& in, MintTx& tx) {
    in >> tx.tx;
    libff::consume_OUTPUT_NEWLINE(in);

    in >> tx.target_shard_id;
    libff::consume_OUTPUT_NEWLINE(in);

    size_t num_keys;
    in >> num_keys;
    libff::consume_OUTPUT_NEWLINE(in);

    tx.sigs.reserve(num_keys);

    for(size_t i=0; i<num_keys;i++) {
        uint16_t id;
        in >> id;
        libff::consume_OUTPUT_NEWLINE(in);

        size_t sig_size;
        in >> sig_size;
        libff::consume_OUTPUT_NEWLINE(in);
        std::vector<uint8_t> sig(sig_size);
        in.read((char*)sig.data(), sig_size);
        libff::consume_OUTPUT_NEWLINE(in);
    }
    return in;
}
