#!/bin/bash

set -e
trap "trap - TERM && kill -- -$$" INT EXIT

scriptdir=$(cd $(dirname $0); pwd -P)

$scriptdir/../client -cf comm_config -id 4 &> logs4.txt &

wait
