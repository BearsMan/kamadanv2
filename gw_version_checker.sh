#!/bin/bash

ORIG_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
HEADQUARTER_DIR="$ORIG_DIR/client/Dependencies/Headquarter";

CURRENT_BUILD=$(cat "$HEADQUARTER_DIR/Gw.build")

python3 "$HEADQUARTER_DIR/tools/update_binary.py";
python3 "$HEADQUARTER_DIR/tools/convert_key_to_text.py";

NEW_BUILD=$(cat "$HEADQUARTER_DIR/Gw.build")

printf "Current = $CURRENT_BUILD, New = $NEW_BUILD\n";
if [[ "$CURRENT_BUILD" == "$NEW_BUILD" ]]; then
    printf "Gw.build is the same, no need to restart clients\n";
else
    printf "Gw.build Updated!!!\n";
    # Stop all running containers that use the current image (after building)
    RUNNING_BOT_CONTAINERS=$(docker ps --format "{{.ID}} {{.Image}} {{.Names}}" | grep "kamadan-trade-chat" | grep "client" | awk '{print $1}')
    docker restart $RUNNING_BOT_CONTAINERS
fi