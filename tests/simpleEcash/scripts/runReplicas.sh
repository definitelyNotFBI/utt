#!/bin/sh
set -e
trap "trap - TERM && kill -- -$$" INT EXIT

scriptdir=$(cd $(dirname $0); pwd -P)

# parallel -j0 ::: \
$scriptdir/../server -id 0 -cf comm_config > logs0.txt &
$scriptdir/../server -id 1 -cf comm_config > logs1.txt &
$scriptdir/../server -id 2 -cf comm_config > logs2.txt &
$scriptdir/../server -id 3 -cf comm_config > logs3.txt &

wait