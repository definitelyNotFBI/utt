#!/bin/bash

REPLICA_PREFIX=replica_keys_
UTT_PREFIX=utt_pvt_replica_

NumReplicas=4
NumClients=${NUM_CLIENTS:-16}

for((i=1;i<=$NumClients;i++)); do
    id=$((i+NumReplicas-1))
    echo "Running client $i with id ..."
    ../quickpay_client  -n comm_config \
                        -f 1 \
                        -c 0 \
                        -p 400 \
                        -U genesis/utt_pub_client.dat \
                        -i $id \
                        &> clients$i.log &
done

wait

# Cleaning up
killall -9 quickpay_client
