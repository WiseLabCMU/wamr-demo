## WARM-based tools to interact/support the ARENA

This repository includes:

* WASM [**runtime**](runtime/): [WARM](https://github.com/intel/wasm-micro-runtime) with modifications to support MQTT pubsub (based on WARM/samples/simple/, using our [modified WAMR runtime](https://github.com/WiseLabCMU/wasm-micro-runtime))
* [**bridge-tool**](bridge-tool/): Bridge MQTT/REST to the runtime (based on WARM/test-tools/host-tool)
* WASM [**file upload utility**](http_upload/): Tool to upload WASM files so that the runtime can execute them
* [**Dockerfile**](docker/) for a container that executes the runtime, bridge and file upload utility
* WASM [**demo applications**](wasm-apps/)

See a depiction of the relation between the runtime and the bridge [here](https://user-images.githubusercontent.com/3504501/64827014-b7f7fb80-d590-11e9-9fdb-f9fb3e683853.png).

## Building

### Linux 

```
1. Install tools/dependencies: build-essential cmake g++-multilib git lib32gcc-5-dev
2. Clone the repo
3. cd wamr-demo
4. ./build.sh
```

**The [Dockerfile](https://github.com/WiseLabCMU/wamr-demo/blob/master/docker/Dockerfile) is a good documentation of the setup needed.**

### Other

Dependencies for other systems are similar to WAMR:
https://github.com/intel/wasm-micro-runtime/blob/master/doc/building.md

### Docker

You can find a dockerized build in [Docker Hub](https://hub.docker.com/r/npereira/wamr-demo)

Running the dockerized build (runtime + bridge) is a good way to test it. Run:
```
docker run -it -p 8000:8000 npereira/wamr-demo
```

## REST Interface

You can send commands to install/uninstall and query runtimes by sending requests to the ```/cwasm/v1/modules``` endpoint.

**Install** module named 'pub' from the wasm file named 'mqtt_publisher.wasm':
```
curl -v -H "Content-Type: application/json" -d '{"name":"pub", "wasm_file":"mqtt_publisher.wasm"}' http://<runtime-ip>:<port>/cwasm/v1/modules
```

> The default port is ```8000```. This is configured in the file ```config.ini```.

> The file indicated (*mqtt_publisher.wasm* in the example) **must** exist in the runtime local folder ```wasm-apps/``` (configurable in ```config.ini```). To upload a file, you can use the upload utility (see [bellow](https://github.com/WiseLabCMU/wamr-demo/blob/master/README.md#wasm-file-upload-utility)).

**Uninstall** module named 'pub':
```
curl -v -X DELETE -H "Content-Type: application/json" -d '{"name":"pub"}' http://<runtime-ip>:<port>/cwasm/v1/modules
```
**Query** installed modules:
```
curl -v http://<runtime-ip>:<port>/cwasm/v1/modules
```

## MQTT Interface

The runtime uses a UUID as defined in the file ```config.ini``` (default is ```runtime1```). When launching from the docker image, the [container start script](https://github.com/WiseLabCMU/wamr-demo/blob/master/docker/start-bridged-runtime.sh) assigns a new UUID to the runtime.

Each runtime will send events to ```<realm>/r/<UUID>```, where ```realm``` is also configured in ```config.ini```.

### MQTT Runtime Events

#### Runtime start/stop events

When the runtime starts, it sends a ```start``` message to the MQTT topic ```<realm>/r/<UUID>```. It also sets a *last will* message on this topic, so that the MQTT server notifies that it detected the end of the connection to the runtime. These are the json messages for start/stop:

```
{ "id":"<runtime uuid>", "label": "<runtime uuid>", "cmd": "rt-start" }
{ "id":"<runtime uuid>", "label": "<runtime uuid>", "cmd": "rt-stop" }
```

#### Module install/uninstall events

Module install/uninstall events are also sent to the MQTT topic ```<realm>/r/<UUID>```, with the format:
```
{ "id":"<module uuid>", "label": "<module-name>", "parent":"<runtime uuid>", "cmd": "module-inst", "module-name": "<module name>"}"
{ "id":"<module uuid>", "label": "<module-name>", "parent":"<runtime uuid>", "cmd": "module-uninst", "module-name": "<module name>"}"
```

#### Publisher/Subscriber start/stop events

When a module publishes for the first time, stops, subscribes or unsubscribes to a topic, events are sent in the format:
```
{ "id":"<pub/sub uuid>", "label": "<mqtt topic>", "parent":"<module uuid>", "cmd": "pub-start", "topic": "<mqtt topic>"}"
{ "id":"<pub/sub uuid>", "label": "<mqtt topic>", "parent":"<module uuid>", "cmd": "pub-stop", "topic": "<mqtt topic>"}"
{ "id":"<pub/sub uuid>", "label": "<mqtt topic>", "parent":"<module uuid>", "cmd": "sub-start", "topic": "<mqtt topic>"}"
{ "id":"<pub/sub uuid>", "label": "<mqtt topic>", "parent":"<module uuid>", "cmd": "sub-stop", "topic": "<mqtt topic>"}"
```

### Module Install/Uninstall with MQTT

You can also install/uninstall WASM modules with MQTT.

**Install** module named 'pub' from the wasm file named 'mqtt_publisher.wasm':
```
mosquitto_pub -t arena/r/<runtime uuid> -h oz.andrew.cmu.edu -m '{ "id":"runtime uuid", "cmd": "module-inst", "name": "pub", "wasm_file": "mqtt_publisher.wasm" }’
```
> The file indicated (*mqtt_publisher.wasm* in the example) **must** exist in the runtime local folder ```wasm-apps/``` (configurable in ```config.ini```). To upload a file, you can use the upload utility (see [bellow](https://github.com/WiseLabCMU/wamr-demo/blob/master/README.md#wasm-file-upload-utility)).

## WASM File Upload Utility

To upload WASM files to the runtime, send them to the ```/upload``` endpoint of the *http upload utility* (port 8021 by default; also defined in ```config.ini```) :
curl --data "name=mqtt_publisher" --data-binary @./mqtt_publisher.wasm http://localhost:8080/cwasm/v1/modules

> The argument ```name``` indicates the WASM file name, to which the *.wasm* extension is added. The file is saved in the ```wasm-apps/``` (configurable in ```config.ini```).

## WASM Applications

The WASM applications have the [WAMR APIs available](https://github.com/intel/wasm-micro-runtime/blob/master/doc/wamr_api.md).

### Libc API

We extended slightly the [Libc API's available in WAMR](https://github.com/intel/wasm-micro-runtime/blob/master/doc/wamr_api.md). These are the added calls:
```c
char *gcvt(float value, int ndigits, char * buf);
int rand();
void srand(unsigned seed);
time_t time(time_t *second);
```

### MQTT API

We also added the following calls to Publish/Subscribe on MQTT, defined in [mqtt_pubsub.h](https://github.com/WiseLabCMU/wamr-demo/blob/master/runtime/demo-libs/mqtt_pubsub.h).

```c
bool mqtt_publish(const char *url, int fmt, void *payload, int payload_len);
bool mqtt_subscribe(const char *topic, request_handler_f handler);
```

### WASM Aplication Examples

See the examples at [wasm-apps](https://github.com/WiseLabCMU/wamr-demo/tree/master/wasm-apps). To build these examples, type ```make``` in this folder (uses **emscripten** to compile; see [WAMR instructions on how to install](https://github.com/intel/wasm-micro-runtime/blob/master/doc/building.md#use-emscripten-tool), or have a look at the [Dockerfile](https://github.com/WiseLabCMU/wamr-demo/blob/master/docker/Dockerfile)).

