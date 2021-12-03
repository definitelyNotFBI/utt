#!/bin/bash
set -e

chmod 0600 /repo.key
eval $(ssh-agent)
ssh-add /repo.key
ssh-keyscan -H github.com >> /etc/ssh/ssh_known_hosts
if [ ! -d libutt ]; then
    git clone git@github.com:alinush/libutt.git 
else 
    x=$(cd libutt; git pull origin master)
fi

cd libutt
./scripts/docker/install-deps.sh 
./scripts/docker/install.sh