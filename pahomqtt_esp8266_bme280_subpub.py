"""
Gets data from the output of mqtt_esp_bme280_sub as CSV
and publishes the last entry of each bme sensor to a screen.
Should only happen every ~1 hour

The data is in the format:
[BME#, Temp, Humidity, ..., Hours:Minutes, Days:Month:Year]

The data output will look like this in Python (list of strings)
['15.87 *C', '43.23%', '17.98 *C', '43.38%', '16.46 *C', '41.73%', '12:29', '26/06/25']

Which of course is converted to a string to be sent as:
15.87 *C,43.23%,17.98 *C,43.38%,16.46 *C,41.73%,12:29,26/06/25

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
#dicts of all topics
TOPICS_OUT = {
    1:""
}
TOPICS_IN = {
    1:""
}
#dict for CSV reading
TOPICS_TO_READ = [
    "",
    "",
    ""
]
DATA_DIR = Path("")
#for storing output
ret_message = ""

"""Functions"""
def read_csvdatafile() -> str:
    """This function will grab CSV data from the /mnt/usb folder and format it.
        1.  get last bytes of file.
        2.  find last value from the sensor in the file.
            TOPICS_TO_READ list used to match csv file for the topics that are needed.
        3.  format last data values for the topics into a list
        4.  convert list to a string
        4.  return string for transmission.
    
    Returns:
        mqtt_transmission_string:
            "Temp, Humidity, ..., Hours:Minutes, Days:Month:Year"
                is presented for example for 3 bme sensors:
            "15.87 *C,43.23%,17.98 *C,43.38%,16.46 *C,41.73%,12:29,26/06/25"
    """
    null_text = "No value found"
    #get today's date
    date_time = datetime.now()
    current_date = date_time.date()
    #save date and time as human readable strings for later
    current_time_str = date_time.strftime("%H:%M")
    current_date_str = date_time.strftime("%d/%m/%y")
    
    """open csv file and get last 500 bytes"""
    filename = str(current_date)                        #filename is date
    end_file = []                                       #to store [[csv_line1[csv_data]]; list of lists
    with open(DATA_DIR/filename, mode='rb') as file:
        file.seek(-500, 2)                              #last bytes from file, avoids reading ALL file lines from start to end.
        for line in file:
            line = line.decode()                        #convert bytes to string
            line = line.rstrip()
            line = line.split(',')
            end_file.append(line)
    end_file.pop(0)                                     #first line of file probably truncated so remove.
    end_file.reverse()                                  #reverse list
    
    """Find latest values in file with TOPICS_TO_READ dictionary,
    and store in list mqtt_transmission_vals"""
    mqtt_transmission_vals = []
    total_num_lines = len(end_file)                                             #number of lines
    line_number = 1                                                             #len starts at 1, so we start at first line
    for topic in TOPICS_TO_READ:                                                #get all topics
        for line in end_file:                                                   #get all lines in end of file
            if topic in line:                                                   #check if topic is in the line and if it is
                line[2] += " *C"                                                #add units to value
                line[4] += "%"                                                  #add units to value
                mqtt_transmission_vals.extend([line[2],line[4]])                #save line
                break                                                           #break to next topic
            elif line_number == total_num_lines:                                #no val
                mqtt_transmission_vals.extend([null_text,null_text])
            line_number += 1
    mqtt_transmission_vals.extend([current_time_str, current_date_str])         #add time/date to the data.
    """Note: At this point we have the following:
       [Temp, Humidity, ..., Hours:Minutes, Days:Month:Year]
       as:
       ['15.87 *C', '43.23%','17.98 *C', '43.38%', '16.46 *C', '41.73%', '12:29', '26/06/25']
       so to convert it to:
       15.87 *C,43.23%,17.98 *C,43.38%,16.46 *C,41.73%,12:29,26/06/25
       we will:
    """
    mqtt_transmission_string = ""
    for data in mqtt_transmission_vals:
        mqtt_transmission_string += str(data) + ","                         #convert list to string with "," between each value.
    mqtt_transmission_string = mqtt_transmission_string.rstrip(",")         #get rid of trailing ','.
    
    return str(mqtt_transmission_string)

def create_client_publish() -> mqtt.Client:
    """Create standard functions, see paho.mqtt documentation
    
    Creates a client that will publish out data.
    """
    client = mqtt.Client()
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

    def _on_connect(client, userdata, flags, rc):
        print ("Connected: ", str(rc))
    
    def _on_publish(client, userdata, result):
        date_time = datetime.now()
        for topic in TOPICS_OUT.values():
            print("data sent to: " + topic + " at " + str(date_time) + "\n")
    
    client.on_connect = _on_connect
    client.on_publish = _on_publish
    client.connect(MQTT_BROKER_IP, MQTT_BROKER_PORT)
    
    return client

def create_client_subscribe(publish_client: mqtt.Client) -> mqtt.Client:
    """Create standard functions, see paho.mqtt documentation.
    
    Creates a client that will subscribe to a topic.
    On getting a '0' from the client, it will initiate on_message()
    which will use the publishing client (see create_client_publish())
    to publish data from read_csvdatafile().
    
    Args:
        publish_client: Client that will publish.
        Use publish_client = create_client_publish() and provide this
        to the func.
    """
    client = mqtt.Client()
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    
    def _on_connect(client, userdata, flags, rc):
        """Standard paho function. See documentation.
        Runs when script connectes to client.

        Args:
            client
            userdata
            flags
            rc
        """
        print ("Connected: ", str(rc))
        for topic in TOPICS_IN.values():
            client.subscribe(topic)

    def _on_message(client, userdata, msg):
        """Standard paho function. See documentation.
        Runs when a message is recieved from the client.
        
        In this case when the ESP8266 sends a '0' and it is recieved by
        the client, this func will run read_csvdatafile(), which outputs
        a string that is sent by the script back to the ESP8266.
        
        Args:
            client
            userdata
            msg
        """
        print ("Topic: ", msg.topic + "\nMessage: " + str(msg.payload))
        print ("Sending data now")
        return_message = read_csvdatafile()
        print(return_message)
        for topic in TOPICS_OUT.values():
            publish_client.publish(topic, return_message)
    
    client.on_connect = _on_connect
    client.on_message = _on_message
    client.connect(MQTT_BROKER_IP, MQTT_BROKER_PORT)
    
    return client

def main() -> None:
    """Calls all other functions in the script.
    Uses the Paho loop (loop_forever()) to keep script running indefinetly.
    
    Only the subcribe_client does the work, the publish_client just publishes
    the data so that it does not interfer with the subscribe client.
    """
    #create clients.
    publish_client = create_client_publish()
    subscribe_client = create_client_subscribe(publish_client)
    #start loops.
    publish_client.loop_start()
    subscribe_client.loop_start()
    #keep loops active forever
    run = True
    while run:
        pass
    #disconnect if loop terminated.
    publish_client.disconnect()
    subscribe_client.disconnect()

if __name__ == "__main__":
    main()