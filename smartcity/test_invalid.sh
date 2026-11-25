#!/usr/bin/env bash
# Sends invalid location messages to the map. The marker must NOT move;
# the map's status box must show a warning for each message.
TOPIC="smartcity/objects/bin-rui-001/location"
send() {
    echo "Test: $1"
    mosquitto_pub -h test.mosquitto.org -t "$TOPIC" -m "$2"
    sleep 3
}

send "latitude out of range"  '{"id":"bin-rui-001","type":"waste_bin","name":"Rui Smart Bin 1","latitude":200,"longitude":11.9799}'
send "longitude missing"      '{"id":"bin-rui-001","type":"waste_bin","name":"Rui Smart Bin 1","latitude":57.6975}'
send "latitude is text"       '{"id":"bin-rui-001","type":"waste_bin","name":"Rui Smart Bin 1","latitude":"abc","longitude":11.9799}'
send "null island 0,0"        '{"id":"bin-rui-001","type":"waste_bin","name":"Rui Smart Bin 1","latitude":0,"longitude":0}'
send "wrong object id"        '{"id":"bin-other","type":"waste_bin","name":"Other Bin","latitude":57.70,"longitude":11.97}'
send "not JSON"               'hello'
echo "Done. The marker should still be at its last valid position."