#!/bin/bash

ORIG_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$ORIG_DIR";

# Build client
docker rm -f client-build;
docker run -d -t --privileged --name client-build kamadan-trade-chat;
docker exec client-build /bin/sh -c "mkdir -p /app";
docker cp "$ORIG_DIR/client" client-build:/app/;
docker exec client-build /bin/sh -c "cd /app/client/hq_client; apk add --no-cache bash cmake ninja alpine-sdk && cmake -B linuxbuild -G Ninja && ninja -C linuxbuild";

RUNNING_CLIENT_CONTAINERS=$(docker ps --format "{{.ID}} {{.Image}} {{.Names}}" | grep "client" | awk '{print $1}')
docker stop $RUNNING_CLIENT_CONTAINERS

# Copy the linuxbuild directory from the temporary folder to the current folder
docker cp client-build:"/app/client/hq_client/linuxbuild/bin" "$ORIG_DIR/client/hq_client/linuxbuild";

docker start $RUNNING_CLIENT_CONTAINERS