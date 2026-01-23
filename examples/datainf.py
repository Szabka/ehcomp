#!/usr/bin/env python
##demo code provided by Steve Cope at www.steves-internet-guide.com
##email steve@steves-internet-guide.com
##Free to use for any purpose
##If you like and use this code you can
##buy me a drink here https://www.paypal.me/StepenCope
"""
Passwords demo code
"""

import paho.mqtt.client as mqtt  #import the client1
import time
import json
import yaml

QOS1=1
QOS2=1
CLEAN_SESSION=True
broker="gha.hock.hu"
SOURCE_SENSOR_ID="84f703eb92ba"  # Only process messages from this sensor ID
PUBLISH_TOPIC="ghanode/sensor"  # Topic to republish the modified message

def on_disconnect(client, userdata, flags, rc=0):
    m="DisConnected flags"+"result code "+str(rc)
    print(m)

def on_connect(client, userdata, flags, rc):
    print("Connected flags ",str(flags),"result code ",str(rc))

def on_message(client, userdata, message):
    payload_str = message.payload.decode("utf-8")
    print("message received  "  , payload_str)
    
    try:
        # Parse the JSON message
        data = json.loads(payload_str)
        
        # Only process messages from the specified sensor ID
        if data.get("Sensor ID") == SOURCE_SENSOR_ID:
            data["Sensor ID"] = "84f703eb9210"
            data["light"] = data["light"] * 2
            client.publish(PUBLISH_TOPIC, json.dumps(data), QOS1)
            data1 = json.loads(payload_str)
            data1["Sensor ID"] = "84f703eb9220"
            data1["temperature"] = data["temperature"] + 8
            client.publish(PUBLISH_TOPIC, json.dumps(data1), QOS1)
            data1 = json.loads(payload_str)
            data1["Sensor ID"] = "84f703eb9230"
            data1["temperature"] = data["temperature"] + 11
            data1["humidity"] = data["humidity"] + 22
            client.publish(PUBLISH_TOPIC, json.dumps(data1), QOS1)
            data1 = json.loads(payload_str)
            data1["Sensor ID"] = "84f703eb9240"
            data1["light"] = data["light"] * 2
            data1["humidity"] = data["humidity"] + 22
            client.publish(PUBLISH_TOPIC, json.dumps(data1), QOS1)
            print("message republished with new sensor ID and increased temperature value")
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON: {e}")
    except Exception as e:
        print(f"Error processing message: {e}")
def on_publish(client, userdata, mid):
    print("message published "  ,str(mid))

# Load secrets from secrets.yaml
with open("secrets.yaml", "r") as f:
    secrets = yaml.safe_load(f)
mqtt_username = secrets.get("mqtt_user", "anonymous")
mqtt_password = secrets.get("mqtt_password", "")

print("creating client 1 with clean session set to",CLEAN_SESSION)
client1 = mqtt.Client("datainf",clean_session=CLEAN_SESSION)    #create new instance
client1.username_pw_set(username=mqtt_username, password=mqtt_password)
client1.tls_set()

client1.on_message=on_message        #attach function to callback
client1.on_connect=on_connect
print("connecting to ",broker)
client1.connect(broker,8883,keepalive=37)
#client1.loop_start()
client1.subscribe("ghanode/sensor")
#client1.on_disconnect=on_disconnect
#time.sleep(300)
client1.loop_forever()
client1.disconnect()
#client1.loop_stop()
