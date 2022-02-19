#!/bin/bash

NUM_REPLICAS=${NUM_REPLICAS:-10}
NUM_CLIENTS=${NUM_CLIENTS:-1000}
OUT_FILE=${1:-"${NUM_REPLICAS}_comm_config"}

echo "# Communication configuration" > ${OUT_FILE}
echo "replicas_config:" >> ${OUT_FILE}
for((i=0;i<$NUM_REPLICAS;i++)); do
    echo "  - 127.0.0.1:$((4000+i))" 
done >> ${OUT_FILE}

echo "clients_config:" >> ${OUT_FILE}
for((i=0;i<$NUM_CLIENTS;i++)); do
    echo "  - 127.0.0.1:$((6000+i))" 
done >> ${OUT_FILE}