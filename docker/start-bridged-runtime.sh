#!/bin/bash
# uses RT_UUID, IP_ADDR, PORT variables that can be passed when starting the container with the -e flag

cd out/

if [ -z "$RT_UUID" ]
then
    RT_UUID=$(cat /proc/sys/kernel/random/uuid)
fi

# get mqtt server and topic prefix from config.ini
MQTT_SRV=$(cat config.ini  | grep server_address | cut -d'=' -f2 | cut -d ':' -f1)
TOPIC_PREFIX=$(cat config.ini | grep topic-prefix | cut -d'=' -f2)

# sends a runtime start message on behaf of the runtime (which will still send its own, but the second message to the same runtime uuid is ignored)
mosquitto_pub -h $MQTT_SRV -t $TOPIC_PREFIX -m '{ "id":"'$RT_UUID'", "label": "'$RT_UUID'", "cmd": "rt-start", "address": "'$ADDR'", "port":"'$PORT'"}'

# insert uuid into config.iniexc
sed -i 's/uuid=/uuid='$RT_UUID' ; /g' config.ini

./http_upload &
./runtime -s &
sleep 1
./bridge-tool > log.txt