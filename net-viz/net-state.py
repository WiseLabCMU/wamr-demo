import eventlet
import json
from flask import Flask, render_template
from flask_mqtt import Mqtt
from flask_socketio import SocketIO
import configparser

eventlet.monkey_patch()

app = Flask(__name__)

config = configparser.ConfigParser()
config.read('/conf/config.ini')
mqtt_server=config['mqtt']['server_address'].split(':', 1)

app.config['TEMPLATES_AUTO_RELOAD'] = True
app.config['MQTT_BROKER_URL'] = mqtt_server[0]
app.config['MQTT_BROKER_PORT'] = int(mqtt_server[1])
app.config['MQTT_USERNAME'] = ''
app.config['MQTT_PASSWORD'] = ''
app.config['MQTT_KEEPALIVE'] = 60
app.config['MQTT_TLS_ENABLED'] = False

mqtt = Mqtt(app)
socketio = SocketIO(app)
#bootstrap = Bootstrap(app)

node_dict = dict()

@app.route('/net-state')
def net_state():
    node_list = list()
    for node_data in node_dict.values():
        print (node_data)
        data = dict(data=node_data)
        node_list.append(data)

    return json.dumps(node_list), 200

@mqtt.on_message()
def handle_mqtt_message(client, userdata, message):

    obj = json.loads(message.payload.decode())

    # add object to dictionary
    if (obj.cmd == "module-inst" or obj.cmd == "pub-start" or obj.cmd == "sub-start" or obj.cmd == "rt-start"):
        if not obj.id in node_dict:
            node_dict[obj1['id']] = obj
 	
    # remove object from dictionary
    if (obj.cmd == "module-uninst" or obj.cmd == "pub-stop" or obj.cmd == "sub-stop" or obj.cmd == "rt-stop"):
        if obj.id in node_dict:
            thisdict.pop(obj.id)

    # new runtime; subscribe to its messages
    if (obj.cmd == "rt-start"):
        client.subscribe(msg.topic + "/" + obj.id)

@mqtt.on_log()
def handle_logging(client, userdata, level, buf):
    print(level, buf)

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000, use_reloader=False, debug=True)
