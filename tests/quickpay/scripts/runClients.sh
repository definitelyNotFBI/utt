#!/bin/bash

REPLICA_PREFIX=replica_keys_
UTT_PREFIX=wallets/utt_pvt_replica_

NumReplicas=4
NumClients=${NUM_CLIENTS:-16}

for((i=1;i<=$NumClients;i++)); do
    id=$((i+NumReplicas-1))
    echo "Running client $i with id ..."
    ../TesterClient/quickpay_client \
                        --network-config-file comm_config \
                        --num-faults 1 \
                        --num-slow 0 \
                        --numOps 400 \
                        --utt-pub-prefix wallets/utt_pub_client.dat \
                        --keys-file-prefix replica_keys_ \
                        --replica-id $id \
                        &> clients$i.log &
done

wait

# Cleaning up
killall -9 quickpay_client
