#!/usr/bin/env bash

set -ex


SCRIPT_DIR="$(realpath $(dirname "$0"))"
echo "Script dir: ${SCRIPT_DIR}"

echo "Clone Zabbix UI"
cd "${SCRIPT_DIR}/www"
svn co svn://svn.zabbix.com/branches/4.0/frontends/php zabbix
cd -

echo "Clone Zabbix sender"
cd "${SCRIPT_DIR}/www/feed"
git clone https://github.com/disc/zabbix-sender.git --branch 1.3.1
cd -
