#!/bin/bash
echo "Making sure no previous replicas are up..."
killall -9  skvbc_replica \
            client \
            quickpay_replica \
            fullpay_replica \
            sharding_replica \
            sharding_client \
            fullpay_replica2 \
            fullpay_client2

NUM_REPLICAS=${NUM_REPLICAS:-10}
NUM_CLIENTS=${NUM_CLIENTS:-1000}
REPLICA_PREFIX="${NUM_REPLICAS}_replica_keys_"
UTT_PREFIX="${NUM_REPLICAS}_wallets/utt_pvt_replica_"
FAULTS=$((NUM_REPLICAS/3))

echo "Using #Replicas: ${NUM_REPLICAS}"

# Delete all previous DB instances (if any)
rm -rf  simpleKVBTests_DB_* \
        utt-db*

# Create Concord configs if not already created
if [ ! -e "${NUM_REPLICAS}_replica_keys_0" ]; then 
    echo "Generating replica_keys"
    ../../../../tools/GenerateConcordKeys \
                        -n "${NUM_REPLICAS}" \
                        -f "${FAULTS}" \
                        -o "${NUM_REPLICAS}_replica_keys_" \
                        -r 1
fi

# Create UTT configs if not already created
if [ ! -e "certs/${NUM_REPLICAS}" ]; then 
    echo "Generating certificates"
    ../../quickpay/scripts/create_tls_certs.sh "${NUM_REPLICAS}"
fi

# Create wallet configs if not already created
if [ ! -e "${NUM_REPLICAS}_wallets/wallet_${NUM_CLIENTS}" ]; then
    echo "Generating UTT wallet files"
    mkdir -p "${NUM_REPLICAS}_wallets"
    ../../../../tools/GenerateUTTKeys \
                    --node-num ${NUM_REPLICAS} \
                    --fault-num "${FAULTS}" \
                    --num-wallets "${NUM_CLIENTS}" \
                    --wallet-prefix "wallet_" \
                    --output-dir "${NUM_REPLICAS}_wallets"
fi

# Create comm_config file
if [ ! -e "${NUM_REPLICAS}_comm_config" ]; then
    echo "Generating communication config files"
    NUM_REPLICAS="${NUM_REPLICAS}" \
        NUM_CLIENTS="${NUM_CLIENTS}" \
        bash ../../quickpay/scripts/gen-net-conf.sh || exit 1
    echo "Finished generating network configs"
fi


# Start the replicas
for((i=0;i<$NUM_REPLICAS;i++)); do
    echo "Running replica $i..."
    ../TesterReplicaFull/fullpay_replica2 \
        --key-file-prefix "${REPLICA_PREFIX}" \
        --replica-id "$i" \
        --network-config-file "${NUM_REPLICAS}_comm_config" \
        --utt-prefix ${UTT_PREFIX} \
        --log-props-file perf-logging.properties \
        ${EXTRA_REPLICA_FLAGS} \
            &> logs"$((i+1))".txt &
done

wait

# Cleaning up
killall -9 skvbc_replica \
            quickpay_replica \
            fullpay_replica \
            fullpay_replica2
