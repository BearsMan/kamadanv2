#!/bin/bash

ORIG_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$ORIG_DIR";

source config.sh;

GW_EMAIL=$GW_KAMADAN_EMAIL
GW_PASSWORD=$GW_KAMADAN_PASSWORD
GW_CHARACTER=$GW_KAMADAN_CHARACTER

sudo pkill -f "[-]email \"${GW_EMAIL}\""
cd "client/Dependencies/Headquarter";
DIRNAME="$(pwd)"
bin/bin/client -email "${GW_EMAIL}" -password "${GW_PASSWORD}" -character "${GW_CHARACTER}" "${DIRNAME}/../../hq_client/bin/kamadan-trade-client.so";