# STEP 1. Create a temporary image to securely clone the repo
FROM ubuntu:18.04 as clone-repo-temp

RUN apt-get update

# Installing Git..
RUN apt-get install -y git

# Add ssh key as a build argument, passed with --build-arg or in the build block of your docker-compose.yml file
# Example:
#   a. generate with key with 'ssh-keygen -f ./id_rsa' (on the Dockerfile folder)
#   b. add id_rsa.pub to 'Access Keys' in bitbucket website repository settings
#   c. docker build --build-arg SSH_PRIVATE_KEY="`cat id_rsa`" .
#      Example using key file stored online: docker build --build-arg SSH_PRIVATE_KEY="`curl https://gist.githubusercontent.com/nampereira/9d32b24f167e58e1705dbb9106471dfe/raw/12ec0d21dbc293da915e23a1868f58c1a7bb4937/rsa_id`" .
ARG SSH_PRIVATE_KEY
RUN mkdir /root/.ssh/
RUN echo "${SSH_PRIVATE_KEY}" > /root/.ssh/id_rsa
RUN chmod 600 /root/.ssh/id_rsa

# Make sure bitbucket.org is a known host
RUN touch /root/.ssh/known_hosts
RUN ssh-keyscan github.com >> /root/.ssh/known_hosts

RUN git clone --recursive git@github.com:nampereira/wamr-demo.git

#STEP 2. Create final image and the application repo by copying it from the temporary image
# This way, the repository key is not stored in the final image
FROM ubuntu:18.04

RUN apt-get update #; exit 0 # ignore errors ...
RUN apt-get install -y build-essential cmake g++-multilib git lib32gcc-5-dev python lbzip2 xz-utils vim

WORKDIR /
RUN git clone https://github.com/emscripten-core/emsdk.git
WORKDIR /emsdk
RUN git reset --hard d33f7a2
RUN ./emsdk install latest
RUN ./emsdk activate latest
ENV PATH=$PATH:/emsdk:/emsdk/fastcomp/emscripten:/emsdk/node/8.9.1_64bit/bin
RUN /emsdk/emsdk_env.sh

# Copy repository from temporary image
RUN mkdir /wamr-demo
COPY --from=clone-repo-temp /wamr-demo /wamr-demo

WORKDIR /wamr-demo
#RUN ./build.sh all

#CMD /wamr-demo/docker/start-bridged-runtime.sh 
