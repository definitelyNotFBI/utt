for ip in `cat ips.log`; do
    ssh ubuntu@$ip "cd concord-latest && git pull origin && make" &
    # TODO: Send the local genesis config files to the machines
done
wait

for ip in `cat ips.log`; do
    scp comm_config.txt ubuntu@$ip:~/concord-latest/build/tests/simpleKVBC/scripts/comm_config
done