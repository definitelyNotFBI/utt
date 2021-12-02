#include "verifier.hpp"
#include <rocksdb/options.h>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include "Logging4cplus.hpp"
#include "assertUtils.hpp"
#include "common.hpp"
#include "msg/QuickPay.hpp"
#include "replica/Params.hpp"

uint16_t VerifierReplica::id = 0;
VerifierReplica::VerifierReplica(Setup setup, 
    std::unordered_map<uint16_t, PublicKey> public_keys,
    std::shared_ptr<utt_bft::replica::Params> params,
    std::shared_ptr<Setup::db_t> db_ptr)
    : ctx{std::move(setup)}, 
        publicKeysOfReplicas{std::move(public_keys)},
        m_params_ptr_{std::move(params)},
        m_db_ptr_{std::move(db_ptr)}
{
    srand(time(NULL));
}

static const size_t num_inputs_coins = 3;

bool VerifierReplica::verifyBatch(std::vector<MintTx>& batch)
{
    std::string value;
    for(auto& mtx: batch) {
        // Check db if we already saw this tx before
        auto tx_hash = mtx.tx.getHashHex();
        bool found = false;
        auto key_may_exist = m_db_ptr_->rawDB().KeyMayExist(
            rocksdb::ReadOptions{},
            m_db_ptr_->defaultColumnFamilyHandle(),
            tx_hash,
            &value,
            &found
        );
        if (found && key_may_exist) {
            LOG_ERROR(GL, "Tx already seen 1. Replaying");
            return false;
        }
        if (key_may_exist) {
            auto status = m_db_ptr_->rawDB().Get(
                            rocksdb::ReadOptions{}, 
                            m_db_ptr_->defaultColumnFamilyHandle(), 
                            tx_hash, &value);
            if(!status.IsNotFound()) {
                LOG_ERROR(GL, "Tx already seen 2. Replaying");
                return false;
            }
        }
        if(mtx.sigs.size() <= ctx.num_faults) {
            LOG_ERROR(GL, "Insufficient signatures" << KVLOG(mtx.sigs.size(), ctx.num_faults));
            return false;
        }

        // Check if this tx is for my shard
        std::stringstream msg_to_verify(std::string{});
        msg_to_verify << mtx.tx << std::endl;
        msg_to_verify << target_shard_id << std::endl;
        // validate rsa sigs x3 for the three coins
        for(size_t i=0; i<num_inputs_coins; i++) {
            for(auto& [origin,sig]: mtx.sigs) {
                auto pubkey = publicKeysOfReplicas.find(origin);
                if(pubkey == publicKeysOfReplicas.end()) {
                    LOG_ERROR(GL, "Invalid origin for signature" << KVLOG(origin));
                    return false;
                }
                auto isSig = pubkey->second.verify(
                    msg_to_verify.str().data(),
                    msg_to_verify.str().size(),
                    (const char*)sig.data(), 
                    sig.size());
                if(!isSig) {
                    LOG_ERROR(GL, "Sig verification failed" << KVLOG(origin));
                    return false;
                }
            }
        }
        // DONE: validate tx
        auto is_tx_valid = mtx.tx.validate(m_params_ptr_->p, 
                            m_params_ptr_->main_pk, 
                            m_params_ptr_->reg_pk);
        if(!is_tx_valid) {
            LOG_ERROR(GL, "Tx is not valid");
            return false;
        }

        // Write out the tx to the DB to prevent replays
        m_db_ptr_->rawDB().Put(rocksdb::WriteOptions{}, 
                                tx_hash + std::to_string(rand()),
                                std::string());
    }

    return true;
}
