#include <getopt.h>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "setup.hpp"
#include "Crypto.hpp"
#include "Logging4cplus.hpp"
#include "assertUtils.hpp"
#include "sha_hash.hpp"
#include "string.hpp"
#include "threshsign/ThresholdSignaturesTypes.h"
#include "utt/Params.h"
#include "utt/RegAuth.h"
#include "utt/Wallet.h"

#include "msg/QuickPay.hpp"
#include "yaml_utils.hpp"

typedef bftEngine::impl::RSASigner PrivateKey;
typedef bftEngine::impl::RSAVerifier PublicKey;

const struct option longOptions[] = {
    {"client-id",                   required_argument, 0, 'c'},
    {"num-replicas",                required_argument, 0, 'n'},
    {"num-faults",                  required_argument, 0, 'f'},
    {"output-folder",               required_argument, 0, 'o'},
    {"output-prefix",               required_argument, 0, 'O'},
    {"replica-keys-folder",         required_argument, 0, 'r'},
    {"replica-keys-prefix",         required_argument, 0, 'R'},
    {"wallets-folder",              required_argument, 0, 'w'},
    {"wallet-prefix",               required_argument, 0, 'W'},
    {0, 0, 0, 0}
};


std::unique_ptr<Setup> Setup::ParseArgs(int argc, char *argv[])
{
    int o = 0;
    int optionIndex = 0;
    Setup setup;
    while((o = getopt_long(argc, argv, "c:n:f:o:O:r:R:w:W:", 
                            longOptions, &optionIndex)) != EOF) 
    {
        switch(o) {
        case 'n': {
            setup.num_replicas = concord::util::to<std::size_t>
                                        (std::string(optarg));
        } break;
        case 'c': {
            setup.client_id = concord::util::to<std::uint16_t>(std::string(optarg));
        } break;
        case 'f': {
            setup.num_faults = concord::util::to<std::size_t>(std::string(optarg));
        } break;
        case 'o': {
            setup.output_folder = optarg;
        } break;
        case 'O': {
            setup.output_prefix = optarg;
        } break;
        case 'r': {
            setup.replica_folder = optarg;
        } break;
        case 'R': {
            setup.replica_prefix = optarg;
        } break;
        case 'w': {
            setup.wallets_folder = optarg;
        } break;
        case 'W': {
            setup.wallet_prefix = optarg;
        } break;
        case '?': {
            throw std::runtime_error("invalid arguments");
        } break;
        default:
            break;
        }
    }
    if (setup.num_replicas <= 3*setup.num_faults) {
        throw std::runtime_error("n <= 3f");
    }
    if (setup.replica_folder.empty()) {
        throw std::runtime_error("missing --replica-keys-folder (-r) parameter");
    }
    if (setup.wallets_folder.empty()) {
        throw std::runtime_error("missing --wallets-folder (-w) parameter");
    }
    if (setup.client_id == 0) {
        throw std::runtime_error("missing --client-id (-c) parameter");
    }
    return std::make_unique<Setup>(std::move(setup));
}

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
    std::uint16_t node_id) 
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

  std::unordered_map<uint16_t, std::string> publicKeysOfReplicas;
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

std::stringstream ss;
void Setup::makeTx()
{
    ConcordAssert(client_id >= num_replicas);
    auto wal_file = wallets_folder + "/" + 
                        wallet_prefix + std::to_string(client_id);
    std::ifstream walfile(wal_file);
    ConcordAssert(walfile.good());

    libutt::Wallet wal1,wal2;
    walfile >> wal1;
    walfile >> wal2;

    auto pid = wal2.getUserPid();
    auto tx = wal1.spendTwoRandomCoins(pid, true);
    ss.str("");
    ss << tx;
    auto tx_len = ss.str().size();
    auto tx_hash = tx.getHashHex();
    auto qp_msg_len = QuickPayMsg::get_size(tx_hash.size());
    auto qp_tx = QuickPayTx::alloc(qp_msg_len, tx_len);
    auto qp_msg = qp_tx->getQPMsg();
    qp_msg->target_shard_id = 0;
    qp_msg->hash_len = tx_hash.size();
    std::memcpy((uint8_t*)qp_msg->getHashBuf(), 
                (uint8_t*)tx_hash.data(), tx_hash.size());
    std::memcpy((uint8_t*)qp_tx->getTxBuf(), 
                (uint8_t*)ss.str().data(), ss.str().size());
    // tx will be valid
    auto qp_tx_hash = concord::util::SHA3_256().digest((uint8_t*)qp_tx, 
                                                    qp_tx->get_size());
    // DONE: Read private keys
    std::unordered_map<uint16_t, std::unique_ptr<PrivateKey>> priv_key_map;
    std::unordered_map<uint16_t, QuickPayResponse*> qp_resp_map;
    // DONE: Collect Responses
    for(size_t i = 0; i < num_faults+1 ; i++) {
        std::string filename = replica_folder + "/" + replica_prefix + std::to_string(i);
        LOG_DEBUG(m_logger_, "Processing file: " << filename << " for replica " << i);
        auto key_str = getKeyFile(filename, num_replicas, num_faults, i);
        auto key = std::make_unique<PrivateKey>(key_str.c_str());
        ConcordAssert(key != nullptr);
        auto sig_len = key->signatureLength();

        QuickPayResponse* resp = QuickPayResponse::alloc(qp_msg_len, 
                                                            sig_len);
        // DONE: Make a response
        std::memcpy((uint8_t*)resp->getQPMsg(), 
                    (uint8_t*)qp_msg, qp_msg->get_size());
        size_t len_sig_returned;
        char* sigBuf = new char[sig_len];
        key->sign((const char*)qp_tx_hash.data(), qp_tx_hash.size(), 
                    sigBuf, sig_len, len_sig_returned);
        std::memcpy(resp->getSigBuf(), sigBuf, sig_len);
        delete[] sigBuf;
        
        priv_key_map.emplace(i, std::move(key));

        // DONE: Store the responses
        qp_resp_map.emplace(i, resp);
        LOG_DEBUG(m_logger_, "Finished processing " << i);
    }
    // DONE: Create MintTx
    MintTx* mint_tx = MintTx::alloc(qp_msg_len, 
                                        priv_key_map[0]->signatureLength(), 
                                        num_faults+1);
    std::memcpy(mint_tx->getQPMsg(), (uint8_t*)qp_msg, qp_msg->get_size());
    for(size_t i=0; i<num_faults+1;i++) {
        std::memcpy(mint_tx->getSig(i), qp_resp_map[i]->getSigBuf(), 
                    qp_resp_map[i]->sig_len);
    }

    // DONE: Write MintTx
    std::ofstream mint_file(output_folder + "/" + output_prefix + 
                                std::to_string(client_id));
    ConcordAssert(mint_file.good());
    mint_file.write((const char*)mint_tx, mint_tx->get_size());
    mint_file.close();

    // Clean up
    QuickPayTx::free(qp_tx);
    for(auto& [i,resp]: qp_resp_map) {
        QuickPayResponse::free(resp);
    }
    MintTx::free(mint_tx);

    ConcordAssert(true);
}

