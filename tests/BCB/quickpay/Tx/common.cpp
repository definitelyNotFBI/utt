#include "common.hpp"

static void validateRSAPublicKey(const std::string& key) {
  const size_t rsaPublicKeyHexadecimalLength = 584;
  if (!(key.length() == rsaPublicKeyHexadecimalLength) && (std::regex_match(key, std::regex("[0-9A-Fa-f]+"))))
    throw std::runtime_error("Invalid RSA public key: " + key);
}

static void validateRSAPrivateKey(const std::string& key) {
  // Note we do not verify the length of RSA private keys because their length
  // actually seems to vary a little in the output; it hovers around 2430
  // characters but often does not exactly match that number.

  if (!std::regex_match(key, std::regex("[0-9A-Fa-f]+"))) 
    throw std::runtime_error("Invalid RSA private key: " + key);
}

std::string getKeyFile(
    const std::string& filename, 
    std::uint16_t num_replicas,
    std::uint16_t num_faults,
    std::uint16_t node_id,
    std::unordered_map<uint16_t, std::string> &publicKeysOfReplicas
    )
{
  using namespace concord::util;

  std::ifstream input(filename);
  if (!input.is_open()) throw std::runtime_error(__PRETTY_FUNCTION__ + std::string(": can't open ") + filename);

  auto numReplicas = yaml::readValue<std::uint16_t>(input, "num_replicas");
  ConcordAssertEQ(numReplicas, num_replicas);
  // We don't care what this value is
  auto numRoReplicas = yaml::readValue<std::uint16_t>(input, "num_ro_replicas");

  auto fVal = yaml::readValue<std::uint16_t>(input, "f_val");
  ConcordAssertEQ(num_faults, fVal);

  // We don't care what this value is
  auto cVal = yaml::readValue<std::uint16_t>(input, "c_val");

  auto id = yaml::readValue<std::uint16_t>(input, "replica_id");
  ConcordAssertEQ(node_id, id);

  // We don't care what this value is
  yaml::readValue<bool>(input, "read-only");

  // Note we validate the number of replicas using 32-bit integers in case
  // (3 * f + 2 * c + 1) overflows a 16-bit integer.
  uint32_t predictedNumReplicas = 3 * (uint32_t)fVal + 2 * (uint32_t)cVal + 1;
  if (predictedNumReplicas != (uint32_t)numReplicas) {
    throw std::runtime_error("num_replicas must be equal to (3 * f_val + 2 * c_val + 1)");
  }

  if (id >= numReplicas + numRoReplicas)
    throw std::runtime_error("replica IDs must be in the range [0, num_replicas + num_ro_replicas]");

  std::vector<std::string> rsaPublicKeys = yaml::readCollection<std::string>(input, "rsa_public_keys");

  if (rsaPublicKeys.size() != numReplicas + numRoReplicas)
    throw std::runtime_error("number of public RSA keys must match num_replicas");

  publicKeysOfReplicas.clear();
  for (size_t i = 0; i < numReplicas + numRoReplicas; ++i) {
    validateRSAPublicKey(rsaPublicKeys[i]);
    publicKeysOfReplicas.insert(std::pair<uint16_t, std::string>(i, rsaPublicKeys[i]));
  }

  std::string replicaPrivateKey = 
            yaml::readValue<std::string>(input, "rsa_private_key");
  validateRSAPrivateKey(replicaPrivateKey);
  return replicaPrivateKey;
}



std::string getKeyFile(
    const std::string& filename, 
    std::uint16_t num_replicas,
    std::uint16_t num_faults,
    std::uint16_t node_id)
{
  std::unordered_map<uint16_t, std::string> publicKeysOfReplicas;
  return getKeyFile(filename, num_replicas, num_faults, node_id, publicKeysOfReplicas);
}

