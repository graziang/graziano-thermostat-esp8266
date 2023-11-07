#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#define ONE_WIRE_BUS D3 //Pin to which is attached a temperature sensor
#define ONE_WIRE_MAX_DEV 15 //The maximum number of devices

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
long lastTemp; //The last measurement
const int durationTemp = 10 * 1000; //The frequency of temperature measurement
int numberOfDevices; //Number of temperature devices found
DeviceAddress devAddr[ONE_WIRE_MAX_DEV];  //An array device temperature sensors
float tempDev[ONE_WIRE_MAX_DEV]; //Saving the last measurement of temperature
float tempDevLast[ONE_WIRE_MAX_DEV]; //Previous temperature measurement
const char* username = "grazianotermostatopi";
const char*  userPassword = "grazianotermostato2018";
int thermostatPin = D6;

WiFiManager wm;

String GetAddressToString(DeviceAddress deviceAddress){
  String str = "";
  for (uint8_t i = 0; i < 8; i++){
    if( deviceAddress[i] < 16 ) str += String(0, HEX);
    str += String(deviceAddress[i], HEX);
  }
  return str;
}

//Setting the temperature sensor
void SetupDS18B20(){
  DS18B20.begin();
  numberOfDevices = DS18B20.getDeviceCount();
  Serial.print("Device count: ");
  Serial.println(numberOfDevices);

  lastTemp = millis();
  DS18B20.requestTemperatures();

  // Loop through each device, print out address
  for(int i=0; i<numberOfDevices; i++){
    // Search the wire for address
    if(DS18B20.getAddress(devAddr[i], i)){
      //devAddr[i] = tempDeviceAddress;
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: " + GetAddressToString(devAddr[i]));
      Serial.println();
    }
    else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }

    //Get resolution of DS18b20
    Serial.print("Resolution: ");
    Serial.print(DS18B20.getResolution( devAddr[i] ));
    Serial.println();

    //Read temperature from DS18b20
    float tempC = DS18B20.getTempC( devAddr[i] );
    Serial.print("Temp C: ");
    Serial.println(tempC);
  }
}

void TemperatureLoop(long now) {
    if(now-lastTemp>durationTemp) { //Take a measurement at a fixed time (durationTemp)

        std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
        client->setInsecure();

        Serial.println("Getting thermostat...");
        HTTPClient http;  //Declare an object of class HTTPClient
        http.setAuthorization(username, userPassword);
        String payload = "";
        if(http.begin(*client, "https://globally-crucial-beetle.ngrok-free.app/thermostat/thermostat/map")){
            int httpCode = http.GET();
            Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

            if (httpCode == HTTP_CODE_OK) { //Check the returning code
                payload = http.getString();
                //Get the request response payload
                Serial.println(httpCode); //Print the response payload
                Serial.println(payload);
            }
            else {
                Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            }
            http.end();
        }
        else {
            Serial.println("[HTTP] Unable to connect");
            return;
        }

        if(payload == ""){
            return;
        }
        
        StaticJsonDocument<200> jsonBuffer;
        StaticJsonDocument<200> responseObject;
        deserializeJson(responseObject, payload);
        String thermostatId = responseObject["thermostat"];
        JsonArray sensors = responseObject["sensors"];
        bool thermostatState = responseObject["state"].as<bool>();
        //tour on thermostat
        if(thermostatState) {
            Serial.println("THERMOSTAT ON");
            digitalWrite(thermostatPin, LOW);
        }
        else {
            Serial.println("THERMOSTAT OFF");
            digitalWrite(thermostatPin, HIGH);
        }

        if(sensors.size() == numberOfDevices) {
            String messageBody = "{";
            for(int i=0; i<numberOfDevices; i++) {
                float tempC = DS18B20.getTempC(devAddr[i]); //Measuring temperature in Celsius
                Serial.println("address: " + GetAddressToString(devAddr[i]) + ", temp: " + tempC);
                char ts[6];
                String temperature = dtostrf(tempC, 2, 2, ts);
                if (i > 0) {
                    messageBody += ", ";
                }
                messageBody += "\"" + GetAddressToString(devAddr[i]) + "\" : \"" + tempC + "\"";
            }
            messageBody += "}";

            Serial.println("body to send: " + messageBody);
            Serial.println("Posting measurement...");
            http.setAuthorization(username, userPassword);
            if(http.begin(*client, "https://globally-crucial-beetle.ngrok-free.app/thermostat/measurements?thermostat_id=" + thermostatId)){
                http.addHeader("Content-Type", "application/json");
                int httpCode = http.POST(messageBody);
                if (httpCode == HTTP_CODE_OK) { //Check the returning code
                    String payload = http.getString();   //Get the request response payload
                    Serial.println(httpCode); //Print the response payload
                    Serial.println(payload);
                }
                else {
                    Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
                }
                http.end();
            }
            else {
                Serial.println("[HTTP] Unable to connect");
            }
        }
        DS18B20.setWaitForConversion(false); //No waiting for measurement
        DS18B20.requestTemperatures(); //Initiate the temperature measurement
        lastTemp = millis();  //Remember the last time measurement
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP    
    // put your setup code here, to run once:
    Serial.begin(115200);
    pinMode(thermostatPin, OUTPUT);
    
    //reset settings - wipe credentials for testing
    //wm.resetSettings();

    wm.setConfigPortalBlocking(true);
    wm.setConfigPortalTimeout(60);
    //automatically connect using saved credentials if they exist
    //If connection fails it starts an access point with the specified name
    if(wm.autoConnect("Graziano Termostato")){
        Serial.println("connected...yeey :)");
    }
    else {
        Serial.println("Configportal running");
    }

    //Setup DS18b20 temperature sensor
    SetupDS18B20();
    digitalWrite(thermostatPin, HIGH);
}

void loop() {
    wm.process();
    Serial.println("Start loop");
    if ((WiFi.status() == WL_CONNECTED)) {
     long t = millis();
     TemperatureLoop(t);
    }
    else {
      Serial.println("Disconnesso, riavvia");
      digitalWrite(thermostatPin, HIGH);
      ESP.reset();
    }
}