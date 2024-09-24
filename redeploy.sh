#!/bin/bash

ORIG_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$ORIG_DIR";

# Build basic image
source build_client.sh
docker rm -f server-build;
docker run -d -t --privileged --name server-build alpine:3.20;
docker exec server-build /bin/sh -c "apk update && apk add bash nodejs npm mysql mysql-client gdb python3 py3-pip py3-tqdm py3-pefile docker";
docker container commit server-build kamadan-trade-chat;

# Build client
source build_client.sh

docker rm -f gw_version_checker;
docker run -d -t --privileged --restart unless-stopped -w /app -v"$PWD":/app:Z -v //var/run/docker.sock:/var/run/docker.sock --name gw_version_checker kamadan-trade-chat /bin/sh -c "/app/gw_version_checker.sh; sleep 15m";

docker rm -f server;
docker run -d -t --name server -p80:80 -p3306:3306 -v"$PWD":/app:Z kamadan-trade-chat /bin/bash -c /app/run_server.sh;

docker rm -f client_ascalon;
docker run -d -t --name client_ascalon -v"$PWD":/app:Z kamadan-trade-chat /bin/bash -c /app/run_ascalon_client.sh;

docker rm -f client_kamadan;
docker run -d -t --name client_kamadan -v"$PWD":/app:Z kamadan-trade-chat /bin/bash -c /app/run_kamadan_client.sh;

docker image prune;