EMCC=${EMSDK}/fastcomp/emscripten/emcc # EMSDK must be defined to the root of emscriptien install

IWASM_ROOT=$(CURDIR)/../../wasm-micro-runtime/core/iwasm
APP_LIBS=${IWASM_ROOT}/lib/app-libs
NATIVE_LIBS=${IWASM_ROOT}/lib/native-interface

ARENA_LIBS=../runtime/arena-libs

INCLUDES=-I${APP_LIBS}/base -I${APP_LIBS}/extension/sensor -I${NATIVE_LIBS} -I${APP_LIBS}/extension/connection -I${ARENA_LIBS}/
FLAGS=-s WASM=1 -s SIDE_MODULE=1 -s ASSERTIONS=1 -s STACK_OVERFLOW_CHECK=2 -s TOTAL_MEMORY=65536 -s TOTAL_STACK=4096 -s "EXPORTED_FUNCTIONS=['_on_init', '_on_destroy', '_on_request', '_on_response','_on_sensor_event', '_on_timer_callback', '_on_connection_data']"
APP_LIB_SRC=$(wildcard ${APP_LIBS}/base/*.c) $(wildcard ${APP_LIBS}/extension/sensor/*.c) $(wildcard ${APP_LIBS}/extension/connection/*.c) $(wildcard ${NATIVE_LIBS}/*.c) $(wildcard ${ARENA_LIBS}/*.c)
OUT_DIR=../out/wasm-apps

SRCS=$(wildcard *.c)

TARGETS=$(patsubst %.c,%.wasm,$(SRCS))

%.wasm: %.c
	$(EMCC) -O3 $(INCLUDES) $(FLAGS) -o ${OUT_DIR}/$@ $< ${APP_LIB_SRC}

all: $(TARGETS)

clean:
	rm *wasm
