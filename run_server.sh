#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

echo "!!!$SCRIPT_DIR!!!";

source "$SCRIPT_DIR/config.sh";

if [ "$DB_HOST" != "127.0.0.1" ]
then
  pkill mariadb;
  sleep 1;
  mysql_install_db --user=mysql --datadir=/var/lib/mysql;
  cp -ura "$SCRIPT_DIR/server/my.cnf" "/etc/my.cnf.d/zz_project.cnf";
  chmod 755 "/etc/my.cnf.d/zz_project.cnf";
  /usr/bin/mariadbd-safe --datadir='/var/lib/mysql' --nowatch;
  sleep 1;
  # User management bits
  mysql -u root -e "DROP USER IF EXISTS '${DB_USER}'@'%';";
  # Localhost
  mysql -u root -e "CREATE USER IF NOT EXISTS '${DB_USER}'@'localhost' IDENTIFIED BY '${DB_PASS}';";
  mysql -u root -e "SET PASSWORD FOR '${DB_USER}'@'localhost' = PASSWORD('${DB_PASS}');";
  mysql -u root -e "GRANT ALL PRIVILEGES ON * . * TO '${DB_USER}'@'localhost';";
  # Developer's IP address
  mysql -u root -e "CREATE USER IF NOT EXISTS '${DB_USER}'@'${SSH_IP}' IDENTIFIED BY '${DB_PASS}';";
  mysql -u root -e "SET PASSWORD FOR '${DB_USER}'@'${SSH_IP}' = PASSWORD('${DB_PASS}');";
  mysql -u root -e "GRANT ALL PRIVILEGES ON * . * TO '${DB_USER}'@'${SSH_IP}';";
  mysql -u root -e "FLUSH PRIVILEGES;";
  mysql -u ${DB_USER} -p${DB_PASS} -e "CREATE DATABASE IF NOT EXISTS ${DB_SCHEMA};";
fi

cd "$SCRIPT_DIR/server";

npm install;

node --expose-gc server.js;