#!/usr/bin/env sh

DEVICE_IP=192.168.3.90
DEVICE_PORT=80
DEVICE_URL="/settings/"
DEVICE_USER="config"
DEVICE_PASSW="config"
DEVICE_FULL_URL="${DEVICE_USER}:${DEVICE_PASSW}@${DEVICE_IP}:${DEVICE_PORT}${DEVICE_URL}"

set -x -e

curl -v -X POST -H 'Content-Type: application/json' --data '{  "bmePar3": "0"  }' "${DEVICE_FULL_URL}"
