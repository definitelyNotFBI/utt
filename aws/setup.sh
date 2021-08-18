for ip in `cat ips.log`; do
    scp comm_config.txt ubuntu@$ip:~/concord-latest/build/tests/simpleKVBC/scripts/comm_config
    ssh ubuntu@$ip "cd concord-latest && git pull origin && make -j 16"
    # TODO: Send the local genesis config files to the machines
done