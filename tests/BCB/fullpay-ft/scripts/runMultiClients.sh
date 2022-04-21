#!/bin/bash

REPLICA_PREFIX=replica_keys_
NUM_REPLICAS=${NUM_REPLICAS:-10}
NUM_CLIENTS=${NUM_CLIENTS:-16}
FAULTS=$((NUM_REPLICAS/3))

echo "Using #Replicas: ${NUM_REPLICAS}"
echo "Running $NUM_CLIENTS clients..."

for((i=1;i<=$NUM_CLIENTS;i++)); do
    id=$((i+NUM_REPLICAS-1))
    echo "Running client $i with id ..."
    ../TesterClientFull/fullpay_client \
                        --network-config-file "${NUM_REPLICAS}_comm_config" \
                        --num-faults "${FAULTS}" \
                        --num-slow 0 \
                        --numOps 400 \
                        --utt-pub-prefix "${NUM_REPLICAS}_wallets/utt_pub_client.dat" \
                        --keys-file-prefix "${NUM_REPLICAS}_replica_keys_" \
                        --wallet-prefix "${NUM_REPLICAS}_wallets/wallet_" \
                        --replica-id "${NUM_REPLICAS}" \
                        --log-props-file perf-logging.properties \
                        ${EXTRA_CLIENT_FLAGS} \
                        &> clients$i.log &
done

wait

# Cleaning up
killall -9 client
