#!/bin/bash
set -e

chmod 0600 /repo.key
eval $(ssh-agent)
ssh-add /repo.key
ssh-keyscan -H github.com >> /etc/ssh/ssh_known_hosts
# x=$(cd libutt; git pull origin master)

cd libutt
./scripts/docker/install-deps.sh 
./scripts/docker/install.sh
