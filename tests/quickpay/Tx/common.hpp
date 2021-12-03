#pragma once

#include <regex>
#include <string>
#include <fstream>
#include <unordered_map>

#include "Crypto.hpp"
#include "assertUtils.hpp"
#include "yaml_utils.hpp"

typedef bftEngine::impl::RSASigner PrivateKey;
typedef bftEngine::impl::RSAVerifier PublicKey;


std::string getKeyFile(
    const std::string& filename, 
    std::uint16_t num_replicas,
    std::uint16_t num_faults,
    std::uint16_t node_id,
    std::unordered_map<uint16_t, std::string> &publicKeysOfReplicas
    );

std::string getKeyFile(
    const std::string& filename, 
    std::uint16_t num_replicas,
    std::uint16_t num_faults,
    std::uint16_t node_id);