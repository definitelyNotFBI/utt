#!/bin/bash
echo "Making sure no previous replicas are up..."
killall -9 skvbc_replica skvbc_client

# Delete all previous DB instances (if any)
rm -rf simpleKVBTests_DB_0 simpleKVBTests_DB_1 simpleKVBTests_DB_2 simpleKVBTests_DB_3

echo "Running replica 1..."
../TesterReplica/skvbc_replica -k setA_replica_ -i 0 -n comm_config -U utt_pvt_replica_ &
echo "Running replica 2..."
../TesterReplica/skvbc_replica -k setA_replica_ -i 1 -n comm_config -U utt_pvt_replica_ &
echo "Running replica 3..."
../TesterReplica/skvbc_replica -k setA_replica_ -i 2 -n comm_config -U utt_pvt_replica_ &
echo "Running replica 4..."
../TesterReplica/skvbc_replica -k setA_replica_ -i 3 -n comm_config -U utt_pvt_replica_ &

echo "Sleeping for 2 seconds"
sleep 2

echo "Running client!"
time ../TesterClient/skvbc_client -f 1 -c 0 -p 1800 -i 4 -n comm_config
# time ../../simpleEcash/client -id 4 -cf comm_config

echo "Finished!"
# Cleaning up
killall -9 skvbc_replica
