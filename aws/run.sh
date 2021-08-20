id=0
N=4
InternalClients=16

for ip in `cat ips.log`; do
    ssh ubuntu@$ip "killall -9 skvbc_replica client" &
    ssh ubuntu@$ip "cd ~/concord-latest/build/tests/simpleKVBC/scripts; rm -rf simpleKVBTests_DB_0 simpleKVBTests_DB_1 simpleKVBTests_DB_2 simpleKVBTests_DB_3 utt-db0 utt-db1 utt-db2 utt-db3"
done
wait 

for ip in `cat ips.log`; do
    if [[ $id -lt $N ]]; then 
        echo "Connecting to Replica $id"
        ssh ubuntu@$ip "cd ~/concord-latest/build/tests/simpleKVBC/scripts/; ../TesterReplica/skvbc_replica -k replica_keys_ -i $id -n comm_config -U utt_pvt_replica_" &> logs$id.txt &
        id=$((id+1))
    else
        echo "Connecting to Client $id"
        for((j=0;j<$InternalClients;j++)); do 
            echo "Starting client ID: $id"
            ssh ubuntu@$ip "cd ~/concord-latest/build/tests/simpleKVBC/scripts/; ../UTTClient/client -n comm_config -f 1 -c 0 -p 400 -U utt_pub_client.dat -i $id" &> logs-clients$id-$j.txt &
            id=$((id+1))
        done
    fi
done

wait