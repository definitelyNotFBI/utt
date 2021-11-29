#!/bin/bash
echo "Making sure no previous replicas are up..."
killall -9 quickpay_replica quickpay_client

NUM_REPLICAS=4

# Delete all previous DB instances (if any)
rm -rf simpleKVBTests_DB_{0,1,2,3} utt-db{0,1,2,3}

# Create UTT configs if not already created
if [ ! -e replica_keys_4 ]; then 
    echo "Generating replica_keys"
    ../../../tools/GenerateConcordKeys \
                        -n 4 \
                        -f 1 \
                        -r 1 \
                        -o replica_keys_
fi
if [ ! -e certs/999 ]; then 
    echo "Generating certificates"
    ./create_tls_certs.sh 1000
fi
if [ ! -e wallets/wallet_999 ]; then
    echo "Generating UTT wallet files"
    mkdir -p wallets
    ../../../tools/GenerateUTTKeys \
                    --node-num 4 \
                    --fault-num 1 \
                    --num-wallets 1000 \
                    --wallet-prefix wallet_ \
                    --output-dir wallets
fi

REPLICA_PREFIX=replica_keys_
UTT_PREFIX=wallets/utt_pvt_replica_

for((i=0;i<$NUM_REPLICAS;i++)); do
    echo "Running replica $((i+1))..."
    ../TesterReplica/quickpay_replica \
                        --key-file-prefix ${REPLICA_PREFIX} \
                        --replica-id "$i" \
                        --network-config-file comm_config \
                        --utt-prefix ${UTT_PREFIX} \
                        ${EXTRA_REPLICA_FLAGS} \
                        &> logs"$((i+1))".txt &
done

wait

# Cleaning up
killall -9 quickpay_replica

