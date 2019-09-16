# Currently supports clang-8 compiler
# Using the "test.c" app from the README.md:
# clang-8 --target=wasm32 -O3 -Wl,--initial-memory=131072,--allow-undefined,--export=main,--no-threads,--strip-all,--no-entry -nostdlib -o test.wasm test.c
# Pay attention to spacing above! ^
# iwasm test.wasm

FROM ubuntu:xenial

RUN apt-get update; exit 0 # ignore errors ...
RUN apt-get install -y build-essential cmake g++-multilib git lib32gcc-5-dev python lbzip2 xz-utils

WORKDIR /
RUN git clone https://github.com/emscripten-core/emsdk.git
WORKDIR /emsdk
RUN ./emsdk install latest
RUN ./emsdk activate latest

ADD wamr-demo /wamr-demo

WORKDIR /wamr-demo
RUN ./build.sh

CMD /wamr-demo/docker/start-bridged-runtime.sh 
