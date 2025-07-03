"""Subscribes to BME sensors from esp8266 using esp8266_bme280_mqtt_pub.ino

Author: JayJayGas
"""

import paho.mqtt.client as mqtt
from datetime import datetime
from pathlib import Path

"""Global Vars/Structs"""
MQTT_USERNAME = ""
MQTT_PASSWORD = ""
MQTT_BROKER_IP = "localhost"
MQTT_BROKER_PORT = 1883
#dict of all esps with bme on local network
TOPICS = {
    1:"",
    2:"",
    3:""
}
DATA_DIR = Path("")
#for storing output
ret_message = ""

"""Setup and open client"""
client = mqtt.Client()
client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
date = [datetime.now()] #use a list because it's mutable (accessible outside scopes)

"""Functions"""
def write_data(topic:str, ret_message:bytes) -> None:
    current_date = datetime.now()
    if current_date.date() > date[0].date():
        date.clear()
        date.append(current_date)
        file_name = str(current_date.date())
        with open((DATA_DIR/file_name), "x") as file:
            pass
    else:
        file_name = str(date[0].date())
    with open(DATA_DIR/file_name, mode='a', newline='') as file:
        file.write(str(current_date)+","+topic+","+ret_message.decode()+"\n")
    
def _on_connect(client, userdata, flags, rc) -> None:
    print ("Connected: ", str(rc))
    for topic in TOPICS.values():
        client.subscribe(topic)
    
def _on_message(client, userdata, msg) -> None:
    print ("Topic: ", msg.topic + "\nMessage: " + str(msg.payload))
    ret_message = msg.payload
    write_data(msg.topic, ret_message)

def main() -> None:
    """Calls all other functions in the script.
    
    Contains one looping Paho client using all functions
    in the script.
    """
    client.on_connect = _on_connect
    client.on_message = _on_message
    client.connect(MQTT_BROKER_IP, MQTT_BROKER_PORT)
    client.loop_forever()
    client.disconnect()

if __name__ == "__main__":
    main()