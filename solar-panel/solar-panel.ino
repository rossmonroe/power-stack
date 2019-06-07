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
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"
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

Adafruit_MPL115A2 mpl115a2;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

//Battery Temperature Sensor
int sensePin = A0;
int sensorInput;
double tempMeasure;

typedef struct {
  float mplPres;
  float mplTemp;
  float lumi;
} solarPanel;

solarPanel sol;

void setup() {
  Serial.begin(115200);
  configureWifi();
  configureSensors();
  mqtt.setServer(mqtt_server, 1883);
  timer = millis();
}

void loop() {
  if (millis() - timer > 2000) { //a periodic report, every 10 seconds
      mqttConnectionCheck();
      tempPressure();
      advancedRead();
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

void configureSensors(){
  tsl.begin();
  mpl115a2.begin();
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

// ------------- PUBLISH DATA TO MQTT -------------
//This function saves the exit detected boolean and date
// and time data to char. The date and time is limited
// to only include day and hours without the time zone
// designation to make parsing and formatting easier 
// on the other side.
void publishSensors() {
  char buffer [250];
  char message[250];
  char sunLumi[7];
  char airTemp[8];
  char airPres[8];
  String(sol.lumi).toCharArray(sunLumi,7);
  String(sol.mplPres).toCharArray(airPres,8);
  String(sol.mplTemp).toCharArray(airTemp,8);
  
  sprintf(message, "{\"LightLevel\": \"%s\", \"AirTemp\": \"%s\", \"AirPres\": \"%s\"}", sunLumi, airTemp, airPres);
  mqtt.publish("PowerStack/SolarPanel", message);
  Serial.println("publishing");
  Serial.println(message);
  timer = millis();
}

void tempPressure(){
  sol.mplPres = mpl115a2.getPressure();
  sol.mplTemp = mpl115a2.getTemperature();
  sol.mplTemp = ((sol.mplTemp * 9)/5) + 32;
  Serial.print("Pressure (kPa): "); Serial.print(sol.mplPres, 4); Serial.print(" kPa  ");
  Serial.print("Temp (*C): "); Serial.print(sol.mplTemp, 1); Serial.println(" *C both measured together");
}

/**************************************************************************/
/*
    Configures the gain and integration time for the TSL2591
*/
/**************************************************************************/
void configureSensor(void)
{
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  //tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain (bright light)
  tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
  //tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain
  
  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time (dim light)

  /* Display the gain and integration time for reference sake */  
  Serial.println(F("------------------------------------"));
  Serial.print  (F("Gain:         "));
  tsl2591Gain_t gain = tsl.getGain();
  switch(gain)
  {
    case TSL2591_GAIN_LOW:
      Serial.println(F("1x (Low)"));
      break;
    case TSL2591_GAIN_MED:
      Serial.println(F("25x (Medium)"));
      break;
    case TSL2591_GAIN_HIGH:
      Serial.println(F("428x (High)"));
      break;
    case TSL2591_GAIN_MAX:
      Serial.println(F("9876x (Max)"));
      break;
  }
  Serial.print  (F("Timing:       "));
  Serial.print((tsl.getTiming() + 1) * 100, DEC); 
  Serial.println(F(" ms"));
  Serial.println(F("------------------------------------"));
  Serial.println(F(""));
}

void advancedRead(void)
{
  // More advanced data read example. Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum
  // That way you can do whatever math and comparisons you want!
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir, full;
  ir = lum >> 16;
  full = lum & 0xFFFF;
  sol.lumi = full;
  Serial.print(F("[ ")); Serial.print(millis()); Serial.print(F(" ms ] "));
  Serial.print(F("IR: ")); Serial.print(ir);  Serial.print(F("  "));
  Serial.print(F("Full: ")); Serial.print(full); Serial.print(F("  "));
  Serial.print(F("Visible: ")); Serial.print(full - ir); Serial.print(F("  "));
  Serial.print(F("Lux: ")); Serial.println(tsl.calculateLux(full, ir), 6);
}

