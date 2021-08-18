#!/bin/bash
echo "Making sure no previous replicas are up..."
killall -9 skvbc_replica client

# Delete all previous DB instances (if any)
rm -rf simpleKVBTests_DB_0 simpleKVBTests_DB_1 simpleKVBTests_DB_2 simpleKVBTests_DB_3 utt-db0 utt-db1 utt-db2 utt-db3

# Create UTT configs if not already created
if [ ! -e replica_keys_3 ]; then 
    echo "Generating replica_keys"
    ../../../tools/GenerateConcordKeys -n 4 -f 1 -o replica_keys_
fi
if [ ! -e certs/999 ]; then 
    echo "Generating certificates"
    ../../simpleEcash/scripts/create_tls_certs.sh 1000
fi
if [ ! -e genesis_999 ]; then
    echo "Generating UTT genesis files"
    ../../../tools/GenerateUTTKeys -n 4 -f 1 -g 1000
fi

REPLICA_PREFIX=replica_keys_
UTT_PREFIX=utt_pvt_replica_

echo "Running replica 1..."
../TesterReplica/skvbc_replica -k ${REPLICA_PREFIX} -i 0 -n comm_config -U ${UTT_PREFIX} &> logs1.txt &
echo "Running replica 2..."
../TesterReplica/skvbc_replica -k ${REPLICA_PREFIX} -i 1 -n comm_config -U ${UTT_PREFIX} &> logs2.txt &
echo "Running replica 3..."
../TesterReplica/skvbc_replica -k ${REPLICA_PREFIX} -i 2 -n comm_config -U ${UTT_PREFIX} &> logs3.txt &
echo "Running replica 4..."
../TesterReplica/skvbc_replica -k ${REPLICA_PREFIX} -i 3 -n comm_config -U ${UTT_PREFIX} &> logs4.txt &

wait

# Cleaning up
killall -9 skvbc_replica
