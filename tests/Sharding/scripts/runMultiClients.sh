#!/bin/bash

trap "exit" INT TERM
trap "kill 0" EXIT

# Script to start all the replicas across all the shards

NUM_SHARDS=${NUM_SHARDS:-"4"}
NUM_REPLICAS=${NUM_REPLICAS:-"4"}
NUM_CLIENTS=${NUM_CLIENTS:-"1"}
FAULTS=${FAULTS:-"$((NUM_REPLICAS/3))"}
IP_FILE=${IP_FILE:-"${NUM_REPLICAS}_comm_config"}

for((i=0;i<$NUM_CLIENTS;i++)); do
    id="$((NUM_REPLICAS+i))"
    echo "Running client $i with id: $id ..."
    ../Client/sharding_client \
        --network-config-file-prefix "${NUM_REPLICAS}_comm_config" \
        --num-faults "${FAULTS}" \
        --num-slow "0" \
        --numOps "400" \
        --utt-pub-prefix "${NUM_REPLICAS}_wallets/utt_pub_client.dat" \
        --keys-file-prefix "${NUM_REPLICAS}_replica_keys_" \
        --wallet-prefix "${NUM_REPLICAS}_wallets/wallet_" \
        --replica-id "$id" \
        --num-shards "${NUM_SHARDS}" \
        &> "logs-client-${id}.log"
done
