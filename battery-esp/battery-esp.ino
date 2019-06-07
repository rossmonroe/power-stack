/* Battery ESP
 *  
 * This program evaluates the distance of 2 ultrasonic sensors
 * then when within range, will grab date and time from an api
 * and package it into a message that is sent over mqtt.
 */

// ------------- LIBRARY INCLUDES -------------
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <PubSubClient.h>
#include "Adafruit_Si7021.h"
#include <Adafruit_MPL115A2.h>

// ------------- WIFI SETTINGS -------------
#define ssid "Monroe"
#define pass "borstad1961"

// ------------- MQQT SETTINGS -------------
#define mqtt_server "broker.hivemq.com"
#define mqtt_name ""
#define mqtt_pass ""
WiFiClient espClient;
PubSubClient mqtt(espClient);
unsigned long timer;
char espUUID[12] = "ESP8602Ross";

Adafruit_Si7021 siSensor = Adafruit_Si7021();
Adafruit_MPL115A2 mpl115a2;

//Battery Temperature Sensor
int sensePin = A0;
int sensorInput;
double tempMeasure;

typedef struct {
  int battery; //battery temperature
  int box; //ambient box tempeerature
  float siHumi;
  float siTemp;
  float mplPres;
  float mplTemp;
  String tme;
} temperatureOfBox;

temperatureOfBox temp;

void setup() {
  Serial.begin(115200);
  configureWifi();
  siSensor.begin();
  mpl115a2.begin();
  mqtt.setServer(mqtt_server, 1883);
  timer = millis();
}

void loop() {
  if (millis() - timer > 2000) { //a periodic report, every 10 seconds
      mqttConnectionCheck();
      getDate();
      batteryAmbientCheck();
      batteryTempCheck();
      publishSensors();
  }
}

// ------------- WIFI SETUP -------------
void configureWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(); Serial.println("WiFi connected"); Serial.println();
  Serial.print("Your ESP has been assigned the internal IP address ");
  Serial.println(WiFi.localIP());
}

// ------------- MQTT CONNECTION CHECK -------------
void mqttConnectionCheck() {
  mqtt.loop();
  if (!mqtt.connected()) {
    reconnect();
  }
}


// ------------- RECONNECT TO MQTT -------------
void reconnect() {
  // Loop until we're reconnected
  while (!espClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    // Attempt to connect
    if (mqtt.connect(espUUID, mqtt_name, mqtt_pass)) { //the connction
      Serial.println("connected");
      // Once connected, publish an announcement...
      char announce[40];
      strcat(announce, espUUID);
      strcat(announce, "is connecting. <<<<<<<<<<<");
      mqtt.publish(espUUID, announce);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// ------------- DOWNLOAD DATE AND TIME -------------
//This API is a public one that requires no key so the url
//is very basic as well as the json parsing.
void getDate(){
  HTTPClient theClient;
  Serial.println();
  Serial.println("Making HTTP request to World Clock");
  theClient.begin("http://worldclockapi.com/api/json/pst/now");
  int httpCode = theClient.GET();
  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload.");
      DynamicJsonBuffer jsonBuffer;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return;
      }
      temp.tme = root["currentDateTime"].as<String>();
      Serial.println("==========");
      Serial.println(temp.tme);
      Serial.println("==========");
    }
    else {
      Serial.println(httpCode);
      Serial.println("Something went wrong with connecting to the endpoint.");
    }
  }
}

// ------------- PUBLISH DATA TO MQTT -------------
//This function saves the exit detected boolean and date
// and time data to char. The date and time is limited
// to only include day and hours without the time zone
// designation to make parsing and formatting easier 
// on the other side.
void publishSensors() {
  char buffer [250];
  char message[250];
  char dateTime[17];
  char batteryTemp[7];
  char boxTemp[7];
  char boxHumi[7];
  String(temp.tme).toCharArray(dateTime,17); //converts to char array
  String(temp.battery).toCharArray(batteryTemp,7); //converts to char array
  String(temp.siTemp).toCharArray(boxTemp,7); //converts to char array
  String(temp.siHumi).toCharArray(boxHumi,7);
  
  sprintf(message, "{\"DateTime\": \"%s\",\"BatteryTemp\": \"%s\", \"BoxTemp\": \"%s\", \"BoxHumi\": \"%s\"}", dateTime, batteryTemp, boxTemp, boxHumi);
  mqtt.publish("PowerStack/Battery", message);
  Serial.println("publishing");
  Serial.println(message);
  timer = millis();
}


void batteryTempCheck(){
  sensorInput = analogRead(A0);
  tempMeasure = (sensorInput - 500) / 10; //convert voltage to celcius
  tempMeasure = ((tempMeasure * 9)/5) + 32; // convert celsius to fahrenheit
  temp.battery = tempMeasure;
  Serial.print("Current Temperature: ");
  Serial.println(tempMeasure);
}
void batteryAmbientCheck(){
  temp.siHumi = siSensor.readHumidity();
  temp.siTemp = siSensor.readTemperature();
  temp.siTemp = ((temp.siTemp * 9)/5) + 32;
  Serial.print("Humidity: ");
  Serial.println(temp.siHumi, 2);
  Serial.print("Temperature: ");
  Serial.println(temp.siTemp, 2);
}

