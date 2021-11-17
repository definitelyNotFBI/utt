#pragma once 

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct LengthPrefixMsg {
    size_t lengthPrefix = 0;
    std::vector<uint8_t> data;

    static const size_t getSize(size_t msg_size) { 
        return sizeof(LengthPrefixMsg) + msg_size; 
    }

    LengthPrefixMsg(std::vector<uint8_t>&& msg): 
        lengthPrefix{msg.size()}, data{msg} {}

    const uint8_t* data_ptr() const { return data.data(); }
};