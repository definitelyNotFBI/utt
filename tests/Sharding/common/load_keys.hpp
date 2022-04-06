#pragma once

#include <stdexcept>
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <regex>
#include <unordered_map>

#include "Crypto.hpp"
#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "bft.hpp"
#include "communication/CommDefs.hpp"
#include "config/test_comm_config.hpp"
#include "string.hpp"
#include "KeyfileIOUtils.hpp"
#include "yaml_utils.hpp"
#include "SigManager.hpp"

#include "threshsign/ThresholdSignaturesTypes.h"

namespace sharding {

static void validateRSAPublicKey(const std::string& key) {
  const size_t rsaPublicKeyHexadecimalLength = 584;
  if (!(key.length() == rsaPublicKeyHexadecimalLength) && (std::regex_match(key, std::regex("[0-9A-Fa-f]+"))))
    throw std::runtime_error("Invalid RSA public key: " + key);
}

inline std::unordered_map<uint16_t, std::string> getKeyFile(const std::string& filename) {
  using namespace concord::util;

  std::ifstream input(filename);
  if (!input.is_open()) throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": can't open ") + filename);

  auto numReplicas = yaml::readValue<std::uint16_t>(input, "num_replicas");
  auto numRoReplicas = yaml::readValue<std::uint16_t>(input, "num_ro_replicas");
  auto fVal = yaml::readValue<std::uint16_t>(input, "f_val");
  auto cVal = yaml::readValue<std::uint16_t>(input, "c_val");
  auto id = yaml::readValue<std::uint16_t>(input, "replica_id");
  auto isReadOnly = yaml::readValue<bool>(input, "read-only");
  (void)isReadOnly;

  // Note we validate the number of replicas using 32-bit integers in case
  // (3 * f + 2 * c + 1) overflows a 16-bit integer.
  uint32_t predictedNumReplicas = 3 * (uint32_t)fVal + 2 * (uint32_t)cVal + 1;
  if (predictedNumReplicas != (uint32_t)numReplicas)
    throw std::runtime_error("num_replicas must be equal to (3 * f_val + 2 * c_val + 1)");

  if (id >= numReplicas + numRoReplicas)
    throw std::runtime_error("replica IDs must be in the range [0, num_replicas + num_ro_replicas]");

  std::vector<std::string> rsaPublicKeys = yaml::readCollection<std::string>(input, "rsa_public_keys");

  if (rsaPublicKeys.size() != numReplicas + numRoReplicas)
    throw std::runtime_error("number of public RSA keys must match num_replicas");

  std::unordered_map<uint16_t, std::string> publicKeysOfReplicas;
  publicKeysOfReplicas.clear();
  for (size_t i = 0; i < numReplicas + numRoReplicas; ++i) {
    validateRSAPublicKey(rsaPublicKeys[i]);
    publicKeysOfReplicas.insert(std::pair<uint16_t, std::string>(i, rsaPublicKeys[i]));
  }

  return publicKeysOfReplicas;
}

}