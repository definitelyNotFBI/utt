#!/bin/bash

REPLICA_PREFIX=replica_keys_
UTT_PREFIX=utt_pvt_replica_

echo "Running client 1..."
../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i 4 &> clients1.log &

echo "Running client 2..."
../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i 5 &> clients2.log &

echo "Running client 3..."
../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i 6 &> clients3.log &

echo "Running client 4..."
../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i 7 &> clients4.log &

wait

# Cleaning up
killall -9 client
