#!/bin/bash

docker rm netstate
docker run -d -it -v $PWD/../out:/conf -p 5000:5000 --name netstate npereira/netstate
