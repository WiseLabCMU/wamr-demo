#!/bin/bash
# docker-runtime <port> <uuid>

chmod +x docker/start-bridged-runtime.sh

RUNTIME_PORT=${1:-8000}
RUNTIME_UUID=${2:-$(cat /proc/sys/kernel/random/uuid)}
RUNTIME_ADDR=${3:-spatial.andrew.cmu.edu}

printf "Runtime port=%s; uuid=%s; addr=%s\n" $RUNTIME_PORT $RUNTIME_UUID $RUNTIME_ADDR
docker run -d -it -p $RUNTIME_PORT:8000 -e RT_UUID=$RUNTIME_UUID -e ADDR=$RUNTIME_ADDR -e PORT=$RUNTIME_PORT --name wamr-demo-container-$RUNTIME_PORT npereira/wamr-demo
