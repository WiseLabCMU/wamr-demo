var subs = [];
var pubs = [];
var selected_node;

document.addEventListener('DOMContentLoaded', function() {


    loadElements();

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
                'shape': 'round-rectangle',
                'background-color': 'LightGray'
            }
        }, {
            selector: 'node[type="module"]',
            style: {
                'shape': 'ellipse',
                'background-color': 'LightBlue'
            }
        }, {
            selector: 'node[type="pub"]',
            style: {
                'shape': 'tag',
                'background-color': 'Chocolate'
            }
        }, {
            selector: 'node[type="sub"]',
            style: {
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
            }
        }],

        /*
        'elements':  {  
        'nodes': [
              { 'data': { 'id': 'R1', 'label': 'Runtime 1', 'cmd': 'start'}},
              { 'data': { 'id': 'R2', 'label': 'Runtime 2' }},
              { 'data': { 'id': 'M1#R1', 'label': 'Module 1', 'parent': 'R1' }},
              { 'data': { 'id': 'M2#R2', 'label': 'Module 2', 'parent': 'R2'  }},
              { 'data': { 'id': 'PubA#M1#R1', 'parent': 'M1#R1' , 'label': 'PubA' }},
              { 'data': { 'id': 'SubA#M2#R2', 'parent': 'M2#R2' , 'label': 'SubA' }}
            ],
            'edges': [
              { 'data': { 'id': 'l1', 'source': 'PubA#M1#R1', 'target': 'SubA#M2#R2' } }

            ]
        },


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
            document.getElementById('runtime_div').style.display = 'block';
            document.getElementById('runtime_name').innerHTML = 'Runtime: <b>' + selected_node.label + '</b>'
        } else {
            document.getElementById('runtime_div').style.display = 'none';
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

async function loadElements() {
  const response = await fetch('http://localhost:5000/net-state', {method: 'GET', mode: 'cors'})
  const myJson = await response.json();
  console.log(JSON.stringify(myJson)); 
}

async function postInstallRequest(url = '', data = {}) {
    // Default options are marked with *
    const response = await fetch(url, {
        method: 'POST', // *GET, POST, PUT, DELETE, etc.
        mode: 'cors', // no-cors, *cors, same-origin
        cache: 'no-cache', // *default, no-cache, reload, force-cache, only-if-cached
        credentials: 'omit', // include, same-origin, *omit
        headers: {
            'Content-Type': 'application/json'
        },
        redirect: 'follow', // manual, *follow, error
        referrer: 'no-referrer', // no-referrer, *client
        body: JSON.stringify(data) // body data type must match 'Content-Type' header
    });
    return await response.json(); // parses JSON response into native JavaScript objects
}

function install(message) {
    if (typeof selected_node !== 'undefined') {
        var module_file = document.getElementById('module_select').value;

        alert('Install: ' + module_file + '; Addr:' + selected_node.address + '; Port:' + selected_node.port);
        postInstallRequest('http://' + selected_node.address + ':' + selected_node.port + '/cwasm/v1/modules', {
            name: 'conn',
            wasm_file: module_file
        })
    }
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
    topic = 'arena/r';

    // Print output for the user in the messages div
    document.getElementById('status-box').value += 'Subscribing to: ' + topic + '\n';

    // Subscribe to the requested topic
    client.subscribe(topic);
}

// Called when the client loses its connection
function onConnectionLost(responseObject) {
    console.log('Disconnected...');

    document.getElementById('status-box').value += 'Disconnected.\n';
    if (responseObject.errorCode !== 0) {
        document.getElementById('status-box').value += 'ERROR: ' + responseObject.errorMessage + '\n';
    }
}

// Called when a message arrives
function onMessageArrived(message) {
    document.getElementById('status-box').value += 'Received: ' + message.payloadString + '\n';
    var conn_change = 0;

    var obj = JSON.parse(message.payloadString);

    console.log(obj.cmd);
    if (obj.cmd == 'module-inst' || obj.cmd == 'pub-start' || obj.cmd == 'sub-start' || obj.cmd == 'rt-start') {

        if (obj.cmd == 'rt-start') obj.type = 'runtime';
        if (obj.cmd == 'module-inst') obj.type = 'module';
        if (obj.cmd == 'pub-start') obj.type = 'pub';
        if (obj.cmd == 'sub-start') obj.type = 'sub';
        try {
            cy.add({
                group: 'nodes',
                data: obj
            });
        } catch (err) {
            console.log(err.message);
        }

        if (obj.cmd == 'pub-start') {
            pubs.push(obj);
            conn_change = 1;
        } else if (obj.cmd == 'sub-start') {
            subs.push(obj);
            conn_change = 1;
        }

        if (obj.cmd == 'rt-start') {
            client.subscribe(topic + '/' + obj.id);
        }

    }

    if (obj.cmd == 'module-uninst' || obj.cmd == 'pub-stop' || obj.cmd == 'sub-stop' || obj.cmd == 'rt-stop') {
        var d = cy.getElementById(obj.id);
        cy.remove(d);

        if (obj.cmd == 'pub-stop') {
            pubs = pubs.filter(x => x.id !== obj.id);
            conn_change = 1;
        } else if (obj.cmd == 'sub-stop') {
            subs = subs.filter(x => x.id !== obj.id);
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

// Called when the disconnection button is pressed
function startDisconnect() {
    client.disconnect();
    document.getElementById('status-box').value += 'Disconnected\n';
}
