#!/bin/bash

chmod +x docker/start-bridged-runtime.sh
#[[ $# -eq 1 ]] && docker run -d -v ${PWD}:/wamr-demo -p $1:8000 --rm --name wamr-demo-container npereira/wamr-demo /wamr-demo/docker/start-bridged-runtime.sh 
#[[ $# -eq 0 ]] && docker run -d -v ${PWD}:/wamr-demo -p 8000:8000 --rm --name wamr-demo-container npereira/wamr-demo /wamr-demo/docker/start-bridged-runtime.sh 

[[ $# -eq 1 ]] && docker run -d -p $1:8000 --rm --name wamr-demo-container npereira/wamr-demo 
[[ $# -eq 0 ]] && docker run -d -p 8000:8000 --rm --name wamr-demo-container npereira/wamr-demo 