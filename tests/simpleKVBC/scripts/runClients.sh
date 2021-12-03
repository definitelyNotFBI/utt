#!/bin/bash

REPLICA_PREFIX=replica_keys_

NumReplicas=4
NumClients=${NUM_CLIENTS:-16}

echo "Running $NumClients clients..."

for((i=1;i<=$NumClients;i++)); do
    id=$((i+NumReplicas-1))
    echo "Running client $i with id ..."
    ../UTTClient/client -n comm_config \
                        -f 1 \
                        -c 0 \
                        -p 400 \
                        -U wallets/utt_pub_client.dat \
                        -i $id \
                        -l perf-logging.properties \
                        ${EXTRA_CLIENT_FLAGS} \
                        &> clients$i.log &
done

wait

# Cleaning up
killall -9 client
