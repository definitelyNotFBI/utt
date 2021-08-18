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

echo "Running client 5..."
../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i 8 &> clients5.log &

echo "Running client 6..."
../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i 9 &> clients6.log &

echo "Running client 7..."
../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i 10 &> clients7.log &

echo "Running client 8..."
../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i 11 &> clients8.log &

wait

# Cleaning up
killall -9 client
