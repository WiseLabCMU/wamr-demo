# STEP 1. Create a temporary image to securely clone the repo
FROM ubuntu:18.04 as clone-repo-temp

USER root

RUN apt-get update

# Installing Git..
RUN apt-get install -y git

# Add ssh key as a build argument, passed with --build-arg or in the build block of your docker-compose.yml file
# Instrutions:
#   a. generate key with 'ssh-keygen -f ./id_rsa' (on the Dockerfile folder)
#   b. add id_rsa.pub to 'SSH keys and GPG keys' in github profile settings
#   c. docker build --build-arg SSH_PRIVATE_KEY="`cat id_rsa`" .
#      Example using key file stored online: docker build --build-arg SSH_PRIVATE_KEY="`curl https://gist.githubusercontent.com/nampereira/9d32b24f167e58e1705dbb9106471dfe/raw/12ec0d21dbc293da915e23a1868f58c1a7bb4937/rsa_id`" .
ARG SSH_PRIVATE_KEY
RUN mkdir /root/.ssh/
RUN echo "${SSH_PRIVATE_KEY}" > /root/.ssh/id_rsa
RUN chmod 600 /root/.ssh/id_rsa

# Make sure bitbucket.org is a known host
RUN touch /root/.ssh/known_hosts
RUN ssh-keyscan github.com >> /root/.ssh/known_hosts

WORKDIR /
RUN git clone --recursive git@github.com:WiseLabCMU/wamr-demo.git
# make sure we get the lastest wasm-micro-runtime
WORKDIR /wamr-demo/runtime/wasm-micro-runtime/ 
RUN git checkout master && git pull

#STEP 2. Create final image and the application repo by copying it from the temporary image
# This way, the repository key is not stored in the final image
FROM ubuntu:18.04

USER root

RUN apt-get update 
RUN apt-get install -y build-essential cmake g++-multilib git lib32gcc-5-dev python lbzip2 xz-utils vim mosquitto-clients sed

WORKDIR /
RUN git clone https://github.com/emscripten-core/emsdk.git
WORKDIR /emsdk
RUN ./emsdk install sdk-releases-fastcomp-98f49919f25e06fa557cbcb1321d4c10e60c87ca-64bit # tested version
RUN ./emsdk activate sdk-releases-fastcomp-98f49919f25e06fa557cbcb1321d4c10e60c87ca-64bit 
ENV PATH=$PATH:/emsdk:/emsdk/fastcomp/emscripten:/emsdk/node/8.9.1_64bit/bin

# Copy repository from temporary image
RUN mkdir /wamr-demo
COPY --from=clone-repo-temp /wamr-demo /wamr-demo  

WORKDIR /wamr-demo
ENV EMSDK=/emsdk
ENV EM_CONFIG=/root/.emscripten
ENV EMSDK_NODE=/emsdk/node/8.9.1_64bit/bin/node
RUN chmod +x ./build.sh
RUN ./build.sh all
RUN chmod +x /wamr-demo/docker/start-bridged-runtime.sh 

CMD /wamr-demo/docker/start-bridged-runtime.sh 