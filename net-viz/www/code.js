var subs = [];
var pubs = [];
var selected_node;
var topic = 'arena/r';
var mqtt_client;
var state_uri='http://spatial.andrew.cmu.edu:5000/net-state'

document.addEventListener('DOMContentLoaded', function() {

    var cy = window.cy = cytoscape({
        container: document.getElementById('cy'),

        layout: {
            name: 'cose-bilkent',
            animationDuration: 1000
        },

        style: [{
            selector: 'node',
            css: {
                'content': 'data(label)',
                'text-valign': 'center',
                'text-halign': 'center'
            }
        }, {
            selector: 'node[type="runtime"]',
            style: {
                "font-size": 6,
                'shape': 'round-rectangle',
                'background-color': 'LightGray'
            }
        }, {
            selector: 'node[type="module"]',
            style: {
                "font-size": 4,
                'shape': 'ellipse',
                'background-color': 'LightBlue'
            }
        }, {
            selector: 'node[type="pub"]',
            style: {
                "font-size": 3,
                'shape': 'tag',
                'background-color': 'Chocolate'
            }
        }, {
            selector: 'node[type="sub"]',
            style: {
                "font-size": 3,
                'shape': 'barrel',
                'background-color': 'Coral'
            }
        }, {
            selector: ':parent',
            css: {
                'text-valign': 'top',
                'text-halign': 'center',
            }
        }, {
            selector: 'edge',
            css: {
                'curve-style': 'bezier',
                'target-arrow-shape': 'triangle'
            },
            style: {
                "font-size": 3,
                'label': 'data(label)' // maps to data.label
            }
        }],
/*
        'elements':  {  
        'nodes': [
              { 'data': { 'id': 'R1', 'label': 'Runtime 1', 'cmd': 'start'}},
              { 'data': { 'id': 'R2', 'label': 'Runtime 2' }},
              { 'data': { 'id': 'M1#R1', 'label': 'Module 1', 'parent': 'R1' }},
              { 'data': { 'id': 'M2#R2', 'label': 'Module 2', 'parent': 'R2'  }},
              { 'data': { 'id': 'M3#R2', 'label': 'Module 3', 'parent': 'R2'  }},
              { 'data': { 'id': 'M4#R2', 'label': 'Module 4', 'parent': 'R2'  }},
              { 'data': { 'id': 'SubA#M1#R1', 'parent': 'M1#R1' , 'label': 'SubA' }},
              { 'data': { 'id': 'SubB#M2#R2', 'parent': 'M2#R2' , 'label': 'SubB' }},
              { 'data': { 'id': 'PubA#M3#R2', 'parent': 'M3#R2' , 'label': 'PubA' }},
              { 'data': { 'id': 'PubB#M4#R2', 'parent': 'M3#R2' , 'label': 'PubB' }},
              { 'data': { 'id': 'SubA#M4#R2', 'parent': 'M4#R2' , 'label': 'SubA' }}
            ],
            'edges': [
              { 'data': { 'id': 'l1', 'source': 'PubA#M3#R2', 'target': 'SubA#M1#R1' } },
              { 'data': { 'id': 'l2', 'source': 'PubB#M4#R2', 'target': 'SubB#M2#R2' } },
              { 'data': { 'id': 'l3', 'source': 'PubA#M3#R2', 'target': 'SubA#M4#R2' } }
            ]
        },
        */
    });

    startConnect();

    cy.on('tap', 'node', function(evt) {
        var obj = evt.target;
        console.log('tapped ' + obj.id());
        selected_node = cy.$id(obj.id()).data();
        console.log(selected_node);
        if (selected_node.type == 'runtime') {
            console.log("runtime")
            document.getElementById('module_div').style.display = 'none';
            document.getElementById('runtime_div').style.display = 'block';
            document.getElementById('runtime_name').innerHTML = 'Runtime: <b>' + selected_node.label + '</b>'
        } else if (selected_node.type == 'module') {
            document.getElementById('runtime_div').style.display = 'none';
            document.getElementById('module_div').style.display = 'block';
            document.getElementById('module_name').innerHTML = 'Module: <b>' + selected_node.label + '</b>'
        } else {
            document.getElementById('runtime_div').style.display = 'none';
            document.getElementById('module_div').style.display = 'none';
        }
    });

    cy.on('tap', function(event) {
        // target holds a reference to the originator
        // of the event (core or element)
        var evtTarget = event.target;

        if (evtTarget === cy) {
            selected_node = null;
            document.getElementById('runtime_div').style.display = 'none';
        }
    });

});

/*
dataArray example:
[
    {
        "data": {
            "id": "runtime1",
            "label": "runtime1",
            "cmd": "rt-start",
            "address": "spatial.andrew.cmu.edu",
            "port": "8001"
        }
    },
    {
        "data": {
            "id": "runtime3",
            "label": "runtime3",
            "cmd": "rt-start",
            "address": "spatial.andrew.cmu.edu",
            "port": "8003"
        }
    },
    ...
]
*/
function loadNodesFromDataArray(dataArray) {
    //console.log(JSON.stringify(dataArray)); 
    dataArray.forEach(function(obj) {
        console.log(JSON.stringify(obj)); 
        loadNode(obj.data);
      });
  }
  
async function loadNodesFromStateService() {
  const response = await fetch(state_uri, {method: 'GET', mode: 'cors'})
  const myJson = await response.json();
  loadNodesFromDataArray(myJson);
}

async function sendRequest(mthd = 'POST', url = '', data = {}) {
    // Default options are marked with *
    const response = await fetch(url, {
        method: mthd, // *GET, POST, PUT, DELETE, etc.
        cache: 'no-cache', // *default, no-cache, reload, force-cache, only-if-cached
        credentials: 'omit', // include, *same-origin, omit
        headers: {
            'Content-Type': 'application/json'
        },
        referrer: 'no-referrer', // no-referrer, *client
        body: JSON.stringify(data) // body data type must match 'Content-Type' header
    });
    return await response.json(); // parses JSON response into native JavaScript objects
}

async function install(message) {
    if (typeof selected_node !== 'undefined') {
        secret = document.getElementById('token').value;
        target_url='http://'+selected_node.address+':'+selected_node.port+'/cwasm/v1/modules';
        
        module_file = document.getElementById('module_select').value;
        module_name = document.getElementById('inst_module_name').value;
        
        data = { name: module_name, wasm_file: module_file };

        request = { "token": secret, "url": target_url, "data":data };
        console.log(request);
        resp = await sendRequest('POST', 'http://' + window.location.host + ':5001/proxy', request); // post request to proxy
        console.log(resp);
    }
}

async function uninstall(message) {
    if (typeof selected_node !== 'undefined') {
        secret = document.getElementById('token').value;
        target_url='http://'+selected_node.address+':'+selected_node.port+'/cwasm/v1/modules';
        
        module_name = selected_node.id;
        
        data = { name: module_name };

        request = { "token": secret, "url": target_url, "data":data };
        console.log(request);
        resp = await sendRequest('DELETE', 'http://' + window.location.host + ':5001/proxy', request); // post request to proxy
        console.log(resp);
    }
}

function deleteRuntime(message) {
    if (typeof selected_node !== 'undefined') {
        mqtt_client.publish(topic+'/'+selected_node.id, '{"id": "'+selected_node.id+'", "cmd": "rt-stop"}', 1, false);
    }
}

async function deleteState(message) {
    const response = await fetch(state_uri, {
        method: 'DELETE', // *GET, POST, PUT, DELETE, etc.
        cache: 'no-cache', // *default, no-cache, reload, force-cache, only-if-cached
        credentials: 'omit', // include, *same-origin, omit
        referrer: 'no-referrer', // no-referrer, *client
    });
    return await response.json(); // parses JSON response into native JavaScript objects    
}

function runLayout(message) {
    cy.layout({
        name: 'cose-bilkent',
        animationDuration: 200
    }).run();
}

// Called after DOMContentLoaded
function startConnect() {
    // Generate a random client ID
    clientID = 'clientID-' + parseInt(Math.random() * 100);

    host = document.getElementById('mqtt_server').value;
    port = document.getElementById('mqtt_port').value;

    // Print output for the user in the messages div
    document.getElementById('status-box').value += 'Connecting to: ' + host + ' on port: ' + port + '\n';
    document.getElementById('status-box').value += 'Using the following client value: ' + clientID + '\n';

    // Initialize new Paho client connection
    client = new Paho.MQTT.Client(host, Number(port), clientID);

    mqtt_client = client;

    // Set callback handlers
    client.onConnectionLost = onConnectionLost;
    client.onMessageArrived = onMessageArrived;

    // Connect the client, if successful, call onConnect function
    client.connect({
        onSuccess: onConnect,
    });
}

// Called on connect button click
function reConnect() {
    try {
        client.disconnect();
    } catch (err) {
        console.log("Not connected..");
    }
    startConnect();
}

// Called when the client connects
function onConnect() {
    // Print output for the user in the messages div
    document.getElementById('status-box').value += 'Subscribing to: ' + topic + '\n';

    // Subscribe to the requested topic
    client.subscribe(topic);

    // load state after establishing connection to mqtt
    loadNodesFromStateService();
}

// Called when the client loses its connection
function onConnectionLost(responseObject) {
    console.log('Disconnected...');

    document.getElementById('status-box').value += 'Disconnected.\n';
    if (responseObject.errorCode !== 0) {
        document.getElementById('status-box').value += 'ERROR: ' + responseObject.errorMessage + '\n';
    }
}

function loadNode(nodeObj) {
    var conn_change = 0;

    if (nodeObj.cmd == 'module-inst' || nodeObj.cmd == 'pub-start' || nodeObj.cmd == 'sub-start' || nodeObj.cmd == 'rt-start') {

        if (nodeObj.cmd == 'rt-start') nodeObj.type = 'runtime';
        if (nodeObj.cmd == 'module-inst') nodeObj.type = 'module';
        if (nodeObj.cmd == 'pub-start') nodeObj.type = 'pub';
        if (nodeObj.cmd == 'sub-start') nodeObj.type = 'sub';
        try {
            cy.add({
                group: 'nodes',
                data: nodeObj
            });
        } catch (err) {
            console.log(err.message);
        }

        if (nodeObj.cmd == 'pub-start') {
            pubs.push(nodeObj);
            conn_change = 1;
        } else if (nodeObj.cmd == 'sub-start') {
            subs.push(nodeObj);
            conn_change = 1;
        }

        if (nodeObj.cmd == 'rt-start') {
            client.subscribe(topic + '/' + nodeObj.id);
        }

    }

    if (nodeObj.cmd == 'module-uninst' || nodeObj.cmd == 'pub-stop' || nodeObj.cmd == 'sub-stop' || nodeObj.cmd == 'rt-stop') {
        var d = cy.getElementById(nodeObj.id);
        cy.remove(d);

        if (nodeObj.cmd == 'pub-stop') {
            pubs = pubs.filter(x => x.id !== nodeObj.id);
            conn_change = 1;
        } else if (nodeObj.cmd == 'sub-stop') {
            subs = subs.filter(x => x.id !== nodeObj.id);
            conn_change = 1;
        }
    }

    if (conn_change == 1) {
        pubs.forEach(function(pub) {
            subs.forEach(function(sub) {
                try {
                    if (pub.topic == sub.topic) cy.add({
                        group: 'edges',
                        data: {
                            'id': pub.id + '-' + sub.id,
                            'source': pub.id,
                            'target': sub.id
                        }
                    });
                } catch (err) {
                    console.log(err.message);
                }
            });
        });
    }

    runLayout();

}

function testMessage()
{
    // double quote everything
    // add "type" property to indicate if is a sub or a pub
    // id of edges should be: <sourceID>-<targetID>
    // add "cmd" with sub-start/sub-stop, pub-start/pub-stop
    testData = { "elements":  {
        "nodes": [
        { "data": { "id": "MQTT", "label": 'MQTT', "cmd": "pub-start", "type": "pub"}},
        { "data": { "id": "mosqsub|38-Kevin-Laptop", "label": "mosqsub|38-Kevin-Laptop", "cmd": "sub-start", "type": "sub"}}
        ],
        "edges": [
        { "data": { "id": "MQTT-mosqsub|38-Kevin-Laptop", "source": "MQTT", "target": "mosqsub|38-Kevin-Laptop", "label": "0.113000" }}
        ]
        }};

        //loadNodesFromDataArray(testData.elements.nodes);
        cy.add(testData.elements);
        
        //loadEdgesFromDataArray(testData.elements.nodes);

}

// Called when a message arrives
function onMessageArrived(message) {
    document.getElementById('status-box').value += 'Received: ' + message.payloadString + '\n';
 
    var obj = JSON.parse(message.payloadString);
    loadNode(obj);
}

// Called when the disconnection button is pressed
function startDisconnect() {
    client.disconnect();
    document.getElementById('status-box').value += 'Disconnected\n';
}
