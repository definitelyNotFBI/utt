#!/bin/bash
echo "Making sure no previous replicas are up..."
killall -9  skvbc_replica \
            client \
            quickpay_client \
            quickpay_server

NUM_REPLICAS=${NUM_REPLICAS:-10}
REPLICA_PREFIX="$NUM_REPLICAS"_replica_keys_
UTT_PREFIX=wallets/utt_pvt_replica_
FAULTS=$((NUM_REPLICAS/3))

# Delete all previous DB instances (if any)
rm -rf  simpleKVBTests_DB_* \
        utt-db*

# Create Concord configs if not already created
if [ ! -e "${NUM_REPLICAS}_replica_keys_0" ]; then 
    echo "Generating replica_keys"
    ../../../tools/GenerateConcordKeys \
                        -n "${NUM_REPLICAS}" \
                        -f "${FAULTS}" \
                        -o "${NUM_REPLICAS}_replica_keys_"
fi

# Create UTT configs if not already created
if [ ! -e "certs/${NUM_REPLICAS}" ]; then 
    echo "Generating certificates"
    ../../quickpay/scripts/create_tls_certs.sh "${NUM_REPLICAS}"
fi

# Create wallet configs if not already created
if [ ! -e "wallets/wallet_${NUM_REPLICAS}" ]; then
    echo "Generating UTT wallet files"
    mkdir -p wallets
    ../../../tools/GenerateUTTKeys \
                    --node-num ${NUM_REPLICAS} \
                    --fault-num "${FAULTS}" \
                    --num-wallets "${NUM_REPLICAS}" \
                    --wallet-prefix wallet_ \
                    --output-dir wallets
fi

# Start the replicas
for((i=0;i<$NUM_REPLICAS;i++)); do
    echo "Running replica $i..."
    ../TesterReplica/skvbc_replica \
        --key-file-prefix "${REPLICA_PREFIX}" \
        --replica-id $i \
        --network-config-file "$NUM_REPLICAS_comm_config" \
        --log-props-file perf-logging.properties \
        --utt-prefix ${UTT_PREFIX} \
        ${EXTRA_REPLICA_FLAGS} \
            &> logs"$((i+1))".txt &
done

wait

# Cleaning up
killall -9 skvbc_replica
