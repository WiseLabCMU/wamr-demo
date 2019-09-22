#!/bin/bash

CURR_DIR=$PWD
export WAMR_DIR=${PWD}/runtime/wasm-micro-runtime
OUT_DIR=${PWD}/out
BUILD_DIR=${PWD}/build

EMCC=~/emsdk/fastcomp/emscripten/emcc

IWASM_ROOT=${WAMR_DIR}/core/iwasm
APP_LIBS=${IWASM_ROOT}/lib/app-libs
NATIVE_LIBS=${IWASM_ROOT}/lib/native-interface
APP_LIB_SRC="${APP_LIBS}/base/*.c ${APP_LIBS}/extension/sensor/*.c ${APP_LIBS}/extension/connection/*.c ${NATIVE_LIBS}/*.c"
WASM_APPS=${PWD}/wasm-apps
HTTP_UPLOAD=${PWD}/http_upload
if [ "$1" == "clean" ]; then
 rm -rf ${OUT_DIR}
 exit 1
fi 

[[ -d ${OUT_DIR} ]] || mkdir ${OUT_DIR}

cd ${WAMR_DIR}/core/shared-lib/mem-alloc
if [ ! -d "tlsf" ];then
    git clone https://github.com/mattconte/tlsf
fi

echo "#####################build runtime project"
cd ${CURR_DIR}/runtime
mkdir -p out
cd out
cmake ..
make
if [ $? != 0 ];then
    echo "BUILD_FAIL runtime exit as $?\n"
    exit 2
fi
cp -a runtime ${OUT_DIR}
echo "#####################build runtime project success"

echo "#####################build bridge"
cd ${CURR_DIR}/bridge-tool
mkdir -p out
cp config.ini out
cd out
cmake ..
make
if [ $? != 0 ];then
        echo "BUILD_FAIL bridge tool exit as $?\n"
        exit 2
fi
cp bridge-tool ${OUT_DIR}
cp config.ini ${OUT_DIR}
echo "#####################build bridge-tool success"

[[ -d ${OUT_DIR}/wasm-apps ]] || mkdir ${OUT_DIR}/wasm-apps

echo "#####################build http upload"
cd ${HTTP_UPLOAD}
make
if [ $? != 0 ];then
        echo "BUILD_FAIL http upload exit as $?\n"
        exit 2
fi
cp out/http_upload ${OUT_DIR}
echo "#####################build http upload done"

[[ -d ${OUT_DIR}/wasm-apps ]] || mkdir ${OUT_DIR}/wasm-apps
if [ "$1" == "all" ];then

    echo "#####################build wasm apps"
    cd ${WASM_APPS}
    mkdir -p out
    make
    if [ $? != 0 ];then
            echo "BUILD_FAIL build exit as $?\n"
            exit 2
    fi
    cp out/* ${OUT_DIR}/wasm-apps
    echo "#####################build wasm apps done"
fi