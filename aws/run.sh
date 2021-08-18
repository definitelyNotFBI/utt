id=0
N=4

for ip in `cat ips.log`; do
    if [[ $id -lt $N ]]; then 
        echo "Connecting to Replica $id"
    else
        echo "Connecting to Client $id"
    fi
    id=$((id+1))
done