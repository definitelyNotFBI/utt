#!/bin/bash
echo "Making sure no previous replicas are up..."
killall -9 skvbc_replica client

NUM_REPLICAS=${NUM_REPLICAS:-4}

# Delete all previous DB instances (if any)
rm -rf simpleKVBTests_DB_{0,1,2,3} utt-db{0,1,2,3}

# Create UTT configs if not already created
if [ ! -e replica_keys_3 ]; then 
    echo "Generating replica_keys"
    ../../../tools/GenerateConcordKeys -n 4 -f 1 -o replica_keys_
fi
if [ ! -e certs/999 ]; then 
    echo "Generating certificates"
    ../../simpleEcash/scripts/create_tls_certs.sh 1000
fi
if [ ! -e genesis/genesis_999 ]; then
    echo "Generating UTT genesis files"
    mkdir -p genesis
    ../../../tools/GenerateUTTKeys -n 4 -f 1 -g 1000 -o genesis
fi

REPLICA_PREFIX=replica_keys_
UTT_PREFIX=genesis/utt_pvt_replica_

for((i=0;i<$NUM_REPLICAS;i++)); do
    echo "Running replica $i..."
    ../TesterReplica/skvbc_replica \
        --key-file-prefix ${REPLICA_PREFIX} \
        --replica-id $i \
        --network-config-file comm_config \
        --log-props-file perf-logging.properties \
        --utt-prefix ${UTT_PREFIX} \
        ${EXTRA_REPLICA_FLAGS} \
            &> logs"$((i+1))".txt &
done

wait

# Cleaning up
killall -9 skvbc_replica
