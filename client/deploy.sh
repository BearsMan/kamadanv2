#!/bin/bash
echo "Args:"
echo $@
echo "End Args."
RED='\033[36m'
NC='\033[0m' # No Color
DB_USER=$1
DB_PASS=$2
DB_SCHEMA=$3
#PROJECT_CODE_FOLDER=$2
PROJECT_CODE_FOLDER="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
printf "${RED}*** Project code folder is ${PROJECT_CODE_FOLDER} ***${NC}\n";
SERVER_TIMEZONE="UTC"
if [ -f /sys/hypervisor/uuid ] && [ `head -c 3 /sys/hypervisor/uuid` == ec2 ]; then
  # EC2 server, use dig to find our public IP.
  SERVER_IP=$(dig +short myip.opendns.com @resolver1.opendns.com)
else
  # Local server, get last IP from hostname array
  SERVER_IP=$(hostname -I | awk '{ print $NF}')
fi
printf "${RED}*** Server IP is ${SERVER_IP} ***${NC}\n";

if [ "${SERVER_IP}" == "10.10.10.51" ]; then
  SSH_IP="10.10.10.1"
else
  SSH_IP=$(echo $SSH_CLIENT | awk '{ print $1}')
fi
printf "${RED}*** Connected ssh client is ${SSH_IP} ***${NC}\n";

REQUIRED_PACKAGES='apt-transport-https build-essential curl libcurl4-openssl-dev gcc-multilib clang ninja-build cmake pkg-config libssl-dev'

sudo ln -sf /usr/share/zoneinfo/${SERVER_TIMEZONE} /etc/localtime; 
export NODE_ENV=production; 

# Required Packages process:
# 1. Check to see if we're missing any of the packages listed in REQUIRED_PACKAGES string using dpkg -s command
# 2. If we're missing some packages, run apt-get update and apt-get install to grab them.

sudo dpkg -s ${REQUIRED_PACKAGES} 2>/dev/null >/dev/null || (
  printf "${RED}*** Installing missing packages via apt-get ***${NC}\n";
  sudo apt-get update;
  sudo apt-get install -y ${REQUIRED_PACKAGES});

printf "${RED}*** Running cmake...***${NC}\n";
cd ${PROJECT_CODE_FOLDER}/hq_client;
sudo cmake -B linuxbuild -G "Ninja" -DCMAKE_C_COMPILER=clang && sudo cmake --build linuxbuild;
sudo pkill -f [-]character;
sudo cmake --build linuxbuild
sudo forever restartall;