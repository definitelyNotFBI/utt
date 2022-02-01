#!/bin/bash

REPLICA_PREFIX=replica_keys_
NUM_REPLICAS=${NUM_REPLICAS:-10}
NUM_CLIENTS=${NUM_CLIENTS:-16}
FAULTS=$((NUM_REPLICAS/3))

echo "Running $NUM_CLIENTS clients..."

for((i=1;i<=$NUM_CLIENTS;i++)); do
    id=$((i+NUM_REPLICAS-1))
    echo "Running client $i with id ..."
    ../UTTClient/client -n "${NUM_REPLICAS}_comm_config" \
                        -f "${FAULTS}" \
                        -c 0 \
                        -p 400 \
                        -U wallets/utt_pub_client.dat \
                        -i $id \
                        -r "${NUM_REPLICAS}" \
                        -l perf-logging.properties \
                        ${EXTRA_CLIENT_FLAGS} \
                        &> clients$i.log &
done

wait

# Cleaning up
killall -9 client
