/**
* @file bme280.ino
* @author JayJayGas
* @package Adafruit_BME280_Library
* @package Adafruit-GFX-Library
* @package Adafruit-GFX-Library
* @package pubsubclient
* @brief sends output of bme280 via mqtt from ESP8266
* 
* BME280 collects data and puts it into a string of
* type tempurature,pressure,humidity. Units are
* *C,hPa,%. Data is sent via MQTT.
* 
* Will handle MQTT disconnects by retrying every 10
* seconds, and WIFI disconnects by retrying every
* 30 seconds
*/

#include <stdio.h>
#include <Wire.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
///////////////////Modules///////////////////////////
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <PubSubClient.h>
///////////////////Defs//////////////////////////////
#define MAX_VAL 10       //Averaged bme values
#define SCL_WIRE 5       //D1
#define SDA_WIRE 4       //D2
#define BME_ADDR 0x76    //may be 0x77
///////////////////Proto////////////////////////////
int setup_wifi(void);
int setup_mqtt(void);
int mqtt_reconnect(void);
int wifi_reconnect(void);
int mqtt_publish(const char* topic, char string);
int bme280_values(struct return_val* val);
int serial_print_all(struct return_val val);
int avg_data(struct return_val* val, struct return_val* avg);
///////////////////Glob Vars/////////////////////////
//BME sensors
const char* clientID = "";
const char* topic = "";
//WIFI/MQTT server
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const char* mqtt_user = "";
const char* mqtt_pass = "";
const int mqtt_port = 1883;
///////////////////Structs/////////////////////////
struct return_val {
  double t;  //temperature *C
  double p;  //pressure hPa
  double h;  //humidity %
};
///////////////////Init/////////////////////////////
Adafruit_BME280 bme;  // I2C;
WiFiClient espClient;
PubSubClient client(espClient);
///////////////////MAIN/////////////////////////////
void setup(void) {
  Serial.begin(115200);
  while (!Serial);  //wait for serial to start
  setup_wifi();
  setup_mqtt();
  bme.begin(BME_ADDR);
  Wire.begin(SDA_WIRE, SCL_WIRE);
}
void loop(void) {
  //reconnection wifi
  if (WiFi.status() != WL_CONNECTED) {
    wifi_reconnect();
  }
  //reconnection mqtt
  if (!client.connected()) {
    mqtt_reconnect();
    client.loop();
  }

  //collect 10 values
  struct return_val vals[MAX_VAL];  //struct for storing 10 values
  for (int i = 0; i < MAX_VAL; i++) {
    if (i % 2 == 0){
      client.loop();           //keep mqtt connection live every 2nd loop
    }
    bme280_values(&vals[i]);
    delay(5000);
  }

  //average 10 values
  struct return_val avg;
  avg_data(vals, &avg);

  //convert vals to str for mqtt + send
  char tph_csv[32];
  sprintf(tph_csv, "%0.2f,%0.2f,%0.2f",
          avg.t, avg.p, avg.h); //double to char with 2 dec. place
  mqtt_publish(topic, tph_csv);
}
////////////////////SETUP FUNCS//////////////////////////
/*****************************************************
* @brief wifi setup
* @note make sure you make the IP static when you get
        the printed local IP
*****************************************************/
int setup_wifi(void) {
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi failed");
    return 1;
  }
  else {
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());  //set static at router!
    return 0;
  }
}

/*****************************************************
* @brief MQTT setup
*****************************************************/
int setup_mqtt(void) {
  //mqtt check if client exists (bool client.connect = true)
  client.setServer(mqtt_server, mqtt_port);
  if (client.connect(clientID, mqtt_user, mqtt_pass)){
    Serial.println("MQTT broker connected");
    return 0;
  }
    else {
    Serial.println("MQTT broker failed to connect");
    return 1;
  }
}

/*****************************************************
* @brief MQTT publish string
* @param topic: The topic as a string
* @param string: The message as a string
* @return 0
*****************************************************/
int mqtt_publish(const char* topic, const char* string) {
  if (client.publish(topic, string)){
    Serial.print("Data sent: ");
    Serial.println(string);
  }
  return 0;
}

/*****************************************************
* @brief mqtt_reconnect: keep reconnecting on mqtt disconnect
* @note retries every 10 seconds
* @return 0
*****************************************************/
int mqtt_reconnect(void){
  //keep trying and wait 10 seconds between tries.
  while (!client.connected()){
    Serial.println(
      "lost connection to client, attempting to reconnect.");
    setup_mqtt();
    delay(10000);
  }
  return 0;
}

/*****************************************************
* @brief wifi_reconnect: keep reconnecting on mqtt disconnect
* @note retries every 30 seconds
* @return 0
*****************************************************/
int wifi_reconnect(void){
  //keep trying and wait 30 seconds between tries.
  while (WiFi.status() != WL_CONNECTED){
    Serial.println("WIFI lost Connection, attempting to reconnect.");
    setup_wifi();
    delay(30000);
  }
  return 0;
}

////////////////////Custom Funcs////////////////////////
/*****************************************************
* @brief Debug func: print return_val struct
* @param struct return_val val
* @return 0
*****************************************************/
int serial_print_all(struct return_val val) {
  Serial.println(val.t);
  Serial.println(val.p);
  Serial.println(val.h);
  return 0;
}

/*****************************************************
* @brief Get bme280 values
* @param struct return_val *val: pointer to struct return_val
* @return 0
*****************************************************/
int bme280_values(struct return_val* val) {
  val->t = bme.readTemperature();
  val->p = bme.readPressure() / 100.0F;
  val->h = bme.readHumidity();
  return 0;
}

/*****************************************************
* @brief Average values
* @param struct return_val* vals: Array struct of return_val.
* @param struct return_val* avg: Struct of return_val to return values.
* @return 0
*****************************************************/
int avg_data(struct return_val* vals, struct return_val* avg) {
  double tsum = 0;
  double psum = 0;
  double hsum = 0;
  for (int i = 0; i < MAX_VAL; i++) {
    tsum += vals[i].t;
    psum += vals[i].p;
    hsum += vals[i].h;
  }
  avg->t = tsum / MAX_VAL;
  avg->p = psum / MAX_VAL;
  avg->h = hsum / MAX_VAL;
  return 0;
}