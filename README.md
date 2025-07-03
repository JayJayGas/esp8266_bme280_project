This is a project takes 4 ESP8266 Node MCU boards and turns them into a temperature monitoring system.

In this project you have:
  - 3 Node MCU boards with BME280 temperature sensors.
  - 1 Node MCU board with a WeACT 2.9" or 4.2" e-Paper display to display the temperatures.
      There is a line you can comment/uncomment at the top of the script to change between a 2.9" and 4.9"
      e-Paper display. This script is setup for the 3-colour display only. To use the black and white you'll
      need to modify this line to use the black and white screens.
  - 1 Raspberry pi 5 hosting the MQTT server.
    Could really be any computer for this. It just needs to run the two python scripts.

The two .ino Arduino scripts are for the ESP8266:
  - esp8266_bme280_mqtt_pub.ino
    This is for the bme280 sensors to publish to the MQTT server.
  - esp8266_bme280_weact2.9inch_mqtt_subpub.ino
    This is for the WeAct e-Paper to print everything from the MQTT server.

The two .py python scripts use PahoMQTT to interact with the ESP8266
  - pahomqtt_esp8266_bme280_sub.py
    Listens to the ESP8266 with the BME280 sensors and writes all data to a local .csv file.
  - pahomqtt_esp8266_bme280_subpub.py
    Cuts the latest data from the .csv file collected by the subscriber script and sends it to the
    ESP8266 with the e-Paper display for display.

If you have any questions, feel free to ask. If you think the C++ is bad code, that's because it is.
Most of the C++ is just bad C that is adapted for C++.
