#!/bin/bash
echo "Args:"
echo $@
echo "End Args."
GW_EMAIL=$1
GW_PASSWORD=$2
GW_CHARACTER=$3
GW_MAPID=$4
GW_MAPTYPE=$5
sudo pkill -f "[-]email \"${GW_EMAIL}\""
../kamadan-trade-client/Dependencies/Headquarter/bin/bin/client \
-email "${GW_EMAIL}" -password "${GW_PASSWORD}" -character "${GW_CHARACTER}" \
-mapid "${GW_MAPID}" -maptype "${GW_MAPTYPE}" "../../../../hq_client/bin/chat-client.so"