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
struct ClientConfig {
  CONFIG_PARAM(id,
               uint16_t,
               0,
               "unique identifier of the replica. "
               "The number of replicas in the system should be N = 3*fVal + 2*cVal + 1. "
               "In the current version, replicaId should be a number between 0 and  N-1. "
               "replicaId should also represent this replica in ICommunication.");
  CONFIG_PARAM(numReplicas, uint16_t, 4, "number of regular replicas");
  CONFIG_PARAM(fVal, uint16_t, 1, "F value - max number of faulty/malicious replicas. fVal >= 1");
  CONFIG_PARAM(cVal, uint16_t, 0, "C value. cVal >=0");
  CONFIG_PARAM(concurrencyLevel,
               uint16_t,
               0,
               "number of consensus operations that can be executed in parallel "
               "1 <= concurrencyLevel <= 30");
  CONFIG_PARAM(numOfOperations, uint16_t, 0, 
               "the number of transactions to be sent by the client");
  CONFIG_PARAM(measurePerformance, bool, true, 
               "whether or not to measure performance metrics");
  // Crypto system
  // RSA public keys of all replicas. map from replica identifier to a public key
  std::set<std::pair<uint16_t, const std::string>> publicKeysOfReplicas;

 public:
  /**
   * Singletons should not be cloneable.
   */
  ClientConfig(ClientConfig& other) = delete;
  /**
   * Singletons should not be assignable.
   */
  void operator=(const ClientConfig&) = delete;
  /**
   * This is the static method that controls the access to the singleton
   * instance. On the first run, it creates a singleton object and places it
   * into the static field. On subsequent runs, it returns the client existing
   * object stored in the static field.
   */

  static ClientConfig* Get();

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
  ClientConfig() = default;
  static ClientConfig* m_rconfig;

 private:
  std::unordered_map<std::string, std::string> opt_map;
};

template <>
inline void ClientConfig::set<std::string>(const std::string& param, const std::string& value) {
  opt_map[param] = value;
}
