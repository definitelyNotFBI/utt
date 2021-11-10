#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <thread>
#include <vector>
#include "string.hpp"

#define CONFIG_PARAM_RO(param, type, default_val, description) \
  type param = default_val;                                    \
  type get##param() const { return param; }

#define CONFIG_PARAM(param, type, default_val, description) \
  CONFIG_PARAM_RO(param, type, default_val, description);   \
  void set##param(const type& val) { param = val; } /* NOLINT(bugprone-macro-parentheses) */

/**
 * The ReplicaConfig struct is a singleton that defines the `GetInstance` method that serves as an alternative to
 * constructor and lets clients access the same instance of this struct over and over.
 */
struct ReplicaConfig {
  CONFIG_PARAM(id,
               uint16_t,
               0,
               "unique identifier of the replica. "
               "The number of replicas in the system should be N = 3*fVal + 2*cVal + 1. "
               "In the current version, replicaId should be a number between 0 and  N-1. "
               "replicaId should also represent this replica in ICommunication.");
  CONFIG_PARAM(isReadOnly, bool, false, "Am I a read-only replica?");
  CONFIG_PARAM(numReplicas, uint16_t, 0, "number of regular replicas");
  CONFIG_PARAM(numRoReplicas, uint16_t, 0, "number of read-only replicas");
  CONFIG_PARAM(fVal, uint16_t, 0, "F value - max number of faulty/malicious replicas. fVal >= 1");
  CONFIG_PARAM(cVal, uint16_t, 0, "C value. cVal >=0");
  CONFIG_PARAM(numOfExternalClients, uint16_t, 0, "number of objects that represent external clients");

  CONFIG_PARAM(concurrencyLevel,
               uint16_t,
               0,
               "number of consensus operations that can be executed in parallel "
               "1 <= concurrencyLevel <= 30");
  CONFIG_PARAM(replicaPrivateKey, std::string, "", "RSA private key of the current replica");
  // Threshold crypto system
  CONFIG_PARAM(thresholdSystemType_, std::string, "", "type of threshold crypto system, [multisig-bls|threshold-bls]");
  CONFIG_PARAM(thresholdSystemSubType_, std::string, "", "sub-type of threshold crypto system [BN-254]");
  CONFIG_PARAM(thresholdPrivateKey_, std::string, "", "threshold crypto system bootstrap private key");
  CONFIG_PARAM(thresholdPublicKey_, std::string, "", "threshold crypto system bootstrap public key");
  std::vector<std::string> thresholdVerificationKeys_;

  // Crypto system
  // RSA public keys of all replicas. map from replica identifier to a public key
  std::set<std::pair<uint16_t, const std::string>> publicKeysOfReplicas;

 public:
  /**
   * Singletons should not be cloneable.
   */
  ReplicaConfig(ReplicaConfig& other) = delete;
  /**
   * Singletons should not be assignable.
   */
  void operator=(const ReplicaConfig&) = delete;
  /**
   * This is the static method that controls the access to the singleton
   * instance. On the first run, it creates a singleton object and places it
   * into the static field. On subsequent runs, it returns the client existing
   * object stored in the static field.
   */

  static ReplicaConfig* Get();

  template <typename T>
  void set(const std::string& param, const T& value) {
    opt_map[param] = std::to_string(value);
  }

  template <typename T>
  T get(const std::string& param, const T& defaultValue) const {
    if (auto it = opt_map.find(param); it != opt_map.end()) return concord::util::to<T>(it->second);
    return defaultValue;
  }

 protected:
  /**
   * The Singleton's constructor should always be private to prevent direct
   * construction calls with the `new` operator.
   */
  ReplicaConfig() = default;
  static ReplicaConfig* m_rconfig;

 private:
  std::unordered_map<std::string, std::string> opt_map;
};

template <>
inline void ReplicaConfig::set<std::string>(const std::string& param, const std::string& value) {
  opt_map[param] = value;
}
