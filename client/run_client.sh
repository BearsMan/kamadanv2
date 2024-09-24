#!/bin/bash
echo "Args:"
echo $@
echo "End Args."
GW_EMAIL=$1
GW_PASSWORD=$2
GW_CHARACTER=$3
GW_MAPID=$4
GW_MAPTYPE=$5
ISO_DATE=$(date --iso-8601)

sudo pkill -f "[-]email \"${GW_EMAIL}\""
cd ./hq_client/linuxbuild/bin/
sudo mkdir logs
sudo ./client -email "${GW_EMAIL}" \
-l "${ISO_DATE}_${GW_EMAIL}.txt" \
-password "${GW_PASSWORD}" -character "${GW_CHARACTER}" \
-mapid "${GW_MAPID}" -maptype "${GW_MAPTYPE}" "./libkamadan_trade_chat.so"