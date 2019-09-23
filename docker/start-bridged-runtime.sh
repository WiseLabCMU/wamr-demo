#!/bin/bash

cd out/

# generate uuid into config.ini
sed -i 's/uuid=/uuid='$(cat /proc/sys/kernel/random/uuid)'; /g' config.ini

./http_upload &
./runtime -s &
sleep 1
./bridge-tool
