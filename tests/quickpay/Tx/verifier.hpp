#pragma once

#include <memory>
#include <unordered_map>
#include "common.hpp"
#include "replica/Params.hpp"
#include "setup.hpp"
#include "thread_pool.hpp"

class VerifierReplica {
public:
    Setup ctx;
    std::unordered_map<uint16_t, PublicKey> publicKeysOfReplicas;
    static uint16_t id;
    size_t target_shard_id = 0;
    std::shared_ptr<utt_bft::replica::Params> m_params_ptr_ = nullptr;
    std::shared_ptr<Setup::db_t> m_db_ptr_ = nullptr;
    std::unique_ptr<thread_pool> m_pool_ptr_ = nullptr;

    VerifierReplica(Setup setup, 
        std::unordered_map<uint16_t, PublicKey>, 
        std::shared_ptr<utt_bft::replica::Params> params, 
        std::shared_ptr<Setup::db_t> db_ptr);

    bool verifyBatch(std::vector<MintTx>& batch);
};