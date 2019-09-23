#!/bin/bash

# Instructions:
#   a. generate key with 'ssh-keygen -f ./id_rsa' (on the Dockerfile folder)
#   b. add id_rsa.pub to 'SSH keys and GPG keys' in github profile settings
#   c. run this script
docker build . --build-arg SSH_PRIVATE_KEY="`cat id_rsa`" -t npereira/wamr-demo

