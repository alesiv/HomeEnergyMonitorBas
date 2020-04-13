#include <ESP8266WiFi.h>  //For ESP8266
#include <PubSubClient.h> //For MQTT
#include <ESP8266mDNS.h>  //For OTA
#include <WiFiUdp.h>      //For OTA
#include <ArduinoOTA.h>   //For OTA
#include <Wire.h>        
#include <Adafruit_ADS1015.h>

#define ledinbuilt 02
#define VERSION "1.1.0"

Adafruit_ADS1115 ads1(0x48);
Adafruit_ADS1115 ads2(0x49);
//const float multiplier = 0.1875F;
const float multiplier = 0.0625F;
const float FACTOR = 30; //30A/1V

//WIFI configuration
#define wifi_ssid "stantonamarlberg"
#define wifi_password "osterreich"

//MQTT configuration
#define mqtt_server "192.168.1.100"
#define mqtt_user "esp8266"
#define mqtt_password "esp8266password"
String mqtt_client_id="AEM-";
//MQTT Topic configuration
String mqtt_base_topic="/energymon/data";
#define voltageC1_topic "/voltageC1"
#define voltageC2_topic "/voltageC2"
#define voltageC3_topic "/voltageC3"
#define currentC1_topic "/currentC1"
#define currentC2_topic "/currentC2"
#define currentC3_topic "/currentC3"
#define powerC1_topic "/powerC1"
#define powerC2_topic "/powerC2"
#define powerC3_topic "/powerC3"
#define led_topic "/led"
#define online_topic "/online"
#define version_topic "/version"
#define debug_topic "/debug"
#define getVersion_topic "/getversion"

//MQTT client
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

//Necesary to make Arduino Software autodetect OTA device
WiFiServer TelnetServer(8266);

// Debug variables
int dbgCall;
String dbgMsg;

void setup_adc() 
{
  // Descomentar el que interese
  // ads.setGain(GAIN_TWOTHIRDS);  // +/- 6.144V  1 bit = 0.1875mV (default)
  // ads.setGain(GAIN_ONE);        +/- 4.096V  1 bit = 0.125mV
  ads1.setGain(GAIN_TWO);        // +/- 2.048V  1 bit = 0.0625mV
  ads2.setGain(GAIN_TWO);
  // ads.setGain(GAIN_FOUR);       +/- 1.024V  1 bit = 0.03125mV
  // ads.setGain(GAIN_EIGHT);      +/- 0.512V  1 bit = 0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    +/- 0.256V  1 bit = 0.0078125mV 
  ads1.begin();
  ads2.begin();
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.print(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("OK");
  Serial.print("   IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() { 

  
  Serial.begin(115200);
  Serial.println("Home Energy Monitor - by Alejandro Estrade");
  Serial.print("Versión ");
  Serial.println(VERSION);
  Serial.println("\r\n\nBooting...");

  pinMode(ledinbuilt, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  
  setup_wifi();

  Serial.print("Configuring OTA device...");
  TelnetServer.begin();   //Necesary to make Arduino Software autodetect OTA device  
  ArduinoOTA.onStart([]() {Serial.println("OTA starting...");});
  ArduinoOTA.onEnd([]() {Serial.println("OTA update finished!");Serial.println("Rebooting...");});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {Serial.printf("OTA in progress: %u%%\r\n", (progress / (total / 100)));});  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OK");

  Serial.println("Configuring MQTT server...");
  mqtt_client_id=mqtt_client_id+ESP.getChipId();
  mqtt_client.setServer(mqtt_server, 1883);
  Serial.printf("   Server IP: %s\r\n",mqtt_server);  
  Serial.printf("   Username:  %s\r\n",mqtt_user);
  Serial.println("   Cliend Id: "+mqtt_client_id);  
  Serial.println("   MQTT configured!");

  mqtt_client.setCallback(callback);
  Serial.println("Setup completed! Running app...");

  setup_adc();
}


void mqtt_reconnect() {
  
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {    
    if (mqtt_client.connect(mqtt_client_id.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      mqtt_client.subscribe((mqtt_base_topic + led_topic).c_str());
      mqtt_client.subscribe((mqtt_base_topic + getVersion_topic).c_str());
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  // Switch on the LED if an 1 was received as first character
  if (String(topic) ==  (mqtt_base_topic + led_topic).c_str()) {
    if ((char) payload[0] == '1') {
      digitalWrite(ledinbuilt, LOW);   // Turn the LED on (Note that LOW is the voltage level
      // but actually the LED is on; this is because
      // it is acive low on the ESP-01)
    } else {
      digitalWrite(ledinbuilt, HIGH);  // Turn the LED off by making the voltage HIGH
    }
  }
  
  // Return version topic
  if (String(topic) ==  (mqtt_base_topic + getVersion_topic).c_str()) {
    if ((char) payload[0] == 'V') {
      mqtt_client.publish((mqtt_base_topic + version_topic).c_str(), VERSION, true);
    }
  }
}

// Global variables

long now = 0; //in ms
long lastMsg = 0;
float voltagePeak[3];
float voltageRMS[3];
float currentRMS[3];
float power[3];
int min_timeout=2000; //in ms
long tiempo;
long rawAdc;
long minRaw;
long maxRaw;


void loop() {
  //int16_t adc0[1000]; //, adc1, adc2, adc3;
  //int16_t i;
  //unsigned long tiempo1, tiempo2, tiempo3;

  
  ArduinoOTA.handle();
  
  if (!mqtt_client.connected()) {
    mqtt_reconnect();
  }
  mqtt_client.loop();

  // ADC adquisition Channel 1
  tiempo = millis();
  rawAdc = ads1.readADC_Differential_0_1();
  minRaw = rawAdc;
  maxRaw = rawAdc;
  while (millis() - tiempo < 1000)
  {
    rawAdc = ads1.readADC_Differential_0_1();
    maxRaw = maxRaw > rawAdc ? maxRaw : rawAdc;
    minRaw = minRaw < rawAdc ? minRaw : rawAdc;
  }
 
  maxRaw = maxRaw > -minRaw ? maxRaw : -minRaw;
  voltagePeak[0] = maxRaw * multiplier;
  voltageRMS[0] = voltagePeak[0] * 0.70710678118;
  currentRMS[0] = voltageRMS[0] * FACTOR / 1000;
  power[0] = currentRMS[0] * 230.0;

  // ADC adquisition Channel 2
  tiempo = millis();
  rawAdc = ads1.readADC_Differential_2_3();
  minRaw = rawAdc;
  maxRaw = rawAdc;
  while (millis() - tiempo < 1000)
  {
    rawAdc = ads1.readADC_Differential_2_3();
    maxRaw = maxRaw > rawAdc ? maxRaw : rawAdc;
    minRaw = minRaw < rawAdc ? minRaw : rawAdc;
  }
 
  maxRaw = maxRaw > -minRaw ? maxRaw : -minRaw;
  voltagePeak[1] = maxRaw * multiplier;
  voltageRMS[1] = voltagePeak[1] * 0.70710678118;
  currentRMS[1] = voltageRMS[1] * FACTOR / 1000;
  power[1] = currentRMS[1] * 230.0;

  // ADC adquisition Channel 3
  tiempo = millis();
  rawAdc = ads2.readADC_Differential_0_1();
  minRaw = rawAdc;
  maxRaw = rawAdc;
  while (millis() - tiempo < 1000)
  {
    rawAdc = ads2.readADC_Differential_0_1();
    maxRaw = maxRaw > rawAdc ? maxRaw : rawAdc;
    minRaw = minRaw < rawAdc ? minRaw : rawAdc;
  }
 
  maxRaw = maxRaw > -minRaw ? maxRaw : -minRaw;
  voltagePeak[2] = maxRaw * multiplier;
  voltageRMS[2] = voltagePeak[2] * 0.70710678118;
  currentRMS[2] = voltageRMS[2] * FACTOR / 1000;
  power[2] = currentRMS[2] * 230.0;

  // public MQTT
  now = millis();
  if (now - lastMsg > min_timeout) {
    lastMsg = now;
    now = millis();
    
    //voltage = ads.readADC_Differential_0_1() * multiplier;
    //current = voltage * FACTOR / 1000.0;
    //power = currentRMS * 230.0;

    /*Serial.printf("maxRaw = %d    minRaw = %d", maxRaw, minRaw);
    Serial.print("\r\nSent ");
    Serial.print(String(power).c_str());
    Serial.println(" to "+mqtt_base_topic+power_topic);
    Serial.print("Sent ");
    Serial.print(String(currentRMS).c_str());
    Serial.println(" to "+mqtt_base_topic+current_topic);*/

    // Channel 1
    mqtt_client.publish((mqtt_base_topic+voltageC1_topic).c_str(), String(voltagePeak[0]).c_str(), true);
    mqtt_client.publish((mqtt_base_topic+currentC1_topic).c_str(), String(currentRMS[0]).c_str(), true);
    mqtt_client.publish((mqtt_base_topic+powerC1_topic).c_str(), String(power[0]).c_str(), true);
    // Channel 2
    mqtt_client.publish((mqtt_base_topic+voltageC2_topic).c_str(), String(voltagePeak[1]).c_str(), true);
    mqtt_client.publish((mqtt_base_topic+currentC2_topic).c_str(), String(currentRMS[1]).c_str(), true);
    mqtt_client.publish((mqtt_base_topic+powerC2_topic).c_str(), String(power[1]).c_str(), true);
    // Channel 3
    mqtt_client.publish((mqtt_base_topic+voltageC3_topic).c_str(), String(voltagePeak[2]).c_str(), true);
    mqtt_client.publish((mqtt_base_topic+currentC3_topic).c_str(), String(currentRMS[2]).c_str(), true);
    mqtt_client.publish((mqtt_base_topic+powerC3_topic).c_str(), String(power[2]).c_str(), true);
    
    mqtt_client.publish((mqtt_base_topic+online_topic).c_str(), "Testing", true);
    
    
  }

  if (dbgCall) {
    mqtt_client.publish((mqtt_base_topic + debug_topic).c_str(), dbgMsg.c_str(), true);
    dbgCall = 0;
  }
  
}
