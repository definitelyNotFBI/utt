#!/bin/sh
set -e
trap "trap - TERM && kill -- -$$" INT EXIT

scriptdir=$(cd $(dirname $0); pwd -P)

# parallel -j0 ::: \
$scriptdir/../server 0 > logs0.txt &
$scriptdir/../server 1 > logs1.txt &
$scriptdir/../server 2 > logs2.txt &
$scriptdir/../server 3 > logs3.txt &

wait