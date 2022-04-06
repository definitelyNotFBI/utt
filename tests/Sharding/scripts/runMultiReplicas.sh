#!/bin/bash

trap "exit" INT TERM
trap "kill 0" EXIT

# Script to start all the replicas across all the shards

NUM_SHARDS=${NUM_SHARDS:-"4"}
NUM_REPLICAS=${NUM_REPLICAS:-"4"}
NUM_CLIENTS=${NUM_CLIENTS:-"1"}
FAULTS=${FAULTS:-"$((NUM_REPLICAS/3))"}
IP_FILE=${IP_FILE:-"${NUM_REPLICAS}_comm_config"}

# Setup certificates (every time)
rm -rf certs*

for((shard_id=0;shard_id<$NUM_SHARDS;shard_id++)); do
    echo "Shard#: $shard_id"
    # DONE: Create certs for the first shard for all the replicas
    CERT_FOLDER="certs${shard_id}"
    if [ "$shard_id" -eq "0" ]; then
        mkdir -p "${CERT_FOLDER}"
        ./create_tls_certs.sh "$((NUM_REPLICAS+NUM_CLIENTS))" "${CERT_FOLDER}"
    else
    # DONE: Then create certs for the other shards, but copy the remaining certs from the first one
        cp "certs0" "${CERT_FOLDER}" -r
        ./create_tls_certs.sh "${NUM_REPLICAS}" "${CERT_FOLDER}" "0"
    fi
done


# Setup communication config files
rm -rf *_comm_config_*

# The convention for comm config files is: {num replicas}_comm_config_{num shards}
BASE_IP="127.0.0.1"
BASE_PORT="4000"
for((shard_id=0;shard_id<$NUM_SHARDS;shard_id++)); do
    echo "Shard#: $shard_id"
    FILE="${IP_FILE}${shard_id}"
    for((id=0;id<NUM_REPLICAS+NUM_CLIENTS;id++)); do
        if [ "$id" -eq "0" ]; then
            echo "replicas_config: " > "${FILE}"
        elif [ "$id" -eq "${NUM_REPLICAS}" ]; then
            echo "clients_config: " >> "${FILE}"
        fi
        if [ "$id" -lt "${NUM_REPLICAS}" ]; then
            echo "    - ${BASE_IP}:$((BASE_PORT+(NUM_REPLICAS*shard_id)+id))" >> "${FILE}"
        else
            echo "    - ${BASE_IP}:$((BASE_PORT+(NUM_REPLICAS*NUM_SHARDS)+id))" >> "${FILE}"
        fi
    done
done

# DONE: Create replica keys
# The convention for creating replica keys is
# {num replicas}_replica_keys_{0,1,..,num replicas}-s{0,1,...,num shards}

if [ ! -e "${NUM_REPLICAS}_replica_keys_$((NUM_REPLICAS-1))-s$((NUM_SHARDS-1))" ]; then 
for((shard_id=0;shard_id<$NUM_SHARDS;shard_id++)); do
    echo "Generating replica_keys"
    ../../../tools/GenerateConcordKeys \
                        -n "${NUM_REPLICAS}" \
                        -f "${FAULTS}" \
                        -o "${NUM_REPLICAS}_test_" \
                        -r 1
    # <= because the nth file contains config for the entire shard
    for((id=0;id<=NUM_REPLICAS;id++)); do
        mv "${NUM_REPLICAS}_test_${id}" "${NUM_REPLICAS}_replica_keys_${id}-s${shard_id}"
    done
done
fi

# Create UTT configs

if [ ! -e "${NUM_REPLICAS}_wallets/wallet_${NUM_CLIENTS}" ]; then
    echo "Generating UTT wallet files"
    mkdir -p "${NUM_REPLICAS}_wallets"
    ../../../tools/GenerateUTTKeys \
                    --node-num ${NUM_REPLICAS} \
                    --fault-num "${FAULTS}" \
                    --num-wallets "${NUM_CLIENTS}" \
                    --wallet-prefix "wallet_" \
                    --output-dir "${NUM_REPLICAS}_wallets"
fi

# Run replicas
for((i=0;i<NUM_SHARDS;i++)); do
    for((j=0;j<NUM_REPLICAS;j++)); do
        echo "Shard #${i} - Replica #${j}"
        ../Replica/sharding_replica \
            --key-file-prefix "${NUM_REPLICAS}_replica_keys_" \
            --replica-id "$j" \
            --network-config-file-prefix "${NUM_REPLICAS}_comm_config" \
            --shard-id "$i" \
            --num-shards "${NUM_SHARDS}" \
            --utt-prefix "${NUM_REPLICAS}_wallets/utt_pvt_replica_" \
            &> "logs-shard${i}-r$j.log" &
    done
done

wait
