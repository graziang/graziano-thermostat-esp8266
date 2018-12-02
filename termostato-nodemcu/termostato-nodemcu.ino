#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <ESP8266mDNS.h>  
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>


//------------------------------------------
//DS18B20
#define ONE_WIRE_BUS D3 //Pin to which is attached a temperature sensor
#define ONE_WIRE_MAX_DEV 15 //The maximum number of devices

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
int numberOfDevices; //Number of temperature devices found
DeviceAddress devAddr[ONE_WIRE_MAX_DEV];  //An array device temperature sensors
float tempDev[ONE_WIRE_MAX_DEV]; //Saving the last measurement of temperature
float tempDevLast[ONE_WIRE_MAX_DEV]; //Previous temperature measurement
long lastTemp; //The last measurement
const int durationTemp = 1 * 10 * 1000; //The frequency of temperature measurement
const char* username = "*";
const char*  userPassword = "*";
int thermostatPin = D6;




//------------------------------------------
//WIFI
const char* ssid = "BadMotherFucker";
const char* wifiPassword = "kitemmuort";

//------------------------------------------
//HTTP
ESP8266WebServer server(80);

//------------------------------------------
//Convert device id to String
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

  Serial.print("Parasite power is: "); 
  if( DS18B20.isParasitePowerMode() ){ 
    Serial.println("ON");
  }else{
    Serial.println("OFF");
  }
  
  numberOfDevices = DS18B20.getDeviceCount();
  Serial.print( "Device count: " );
  Serial.println( numberOfDevices );

  lastTemp = millis();
  DS18B20.requestTemperatures();

  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++){
    // Search the wire for address
    if( DS18B20.getAddress(devAddr[i], i) ){
      //devAddr[i] = tempDeviceAddress;
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: " + GetAddressToString(devAddr[i]));
      Serial.println();
    }else{
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

//Loop measuring the temperature
void TempLoop(long now){
  if( now - lastTemp > durationTemp ){ //Take a measurement at a fixed time (durationTemp = 5000ms, 5s)
 
    if (WiFi.status() != WL_CONNECTED) {
      ESP.restart();
    }
    StaticJsonBuffer<200> jsonBuffer;

     Serial.println("Getting thermostat..."); 
    HTTPClient http;  //Declare an object of class HTTPClient
    http.setAuthorization(username, userPassword);
    http.begin("http://graziano-thermostat.herokuapp.com/thermostat/thermostat/map");
    int httpCode = http.GET(); 
    String payload = "";
                                                 
    if (httpCode == 200) { //Check the returning code
      payload = http.getString();     
         //Get the request response payload
      Serial.println(httpCode); //Print the response payload
      Serial.println(payload);
    }
    else {
      Serial.println("Connection error" + httpCode); 
    }
    http.end(); 

    if(payload == ""){
      return;
    }

    JsonObject& responseObject = jsonBuffer.parseObject(payload);
    String thermostatId = responseObject["thermostat"];
    JsonArray& sensors = responseObject["sensors"];
    bool thermostatState = responseObject["state"].as<bool>();
   // Serial.println("thermostat Id:" + thermostatId);
   // Serial.println("sensors Id:" + sensorId);

   //tour on thermostat
    if(thermostatState){
       Serial.println("THERMOSTAT ON"); 
      digitalWrite(thermostatPin, HIGH);
    }
    else{
       Serial.println("THERMOSTAT OFF"); 
      digitalWrite(thermostatPin, LOW);
    }
    if(sensors.size() == numberOfDevices) {
       String messageBody = "{";

  
      for(int i=0; i<numberOfDevices; i++){
        float tempC = DS18B20.getTempC( devAddr[i] ); //Measuring temperature in Celsius
       // float tempC = 28.2;
        String sensorId = sensors[i];
        char ts[6];
        String temperature = dtostrf(tempC, 2, 2, ts);
        if (i > 0){
          messageBody += ", ";
        }
        messageBody += "\"" + GetAddressToString(devAddr[i]) + "\" : \"" + tempC + "\""; 
      }

      messageBody += "}";

      Serial.println("Posting measurement..."); 
      http.setAuthorization(username, userPassword);
      http.begin("http://graziano-thermostat.herokuapp.com/thermostat/measurements?thermostat_id=" + thermostatId);  //Specify request destination 
      http.addHeader("Content-Type", "application/json");
      httpCode = http.POST(messageBody); 
      if (httpCode == 200) { //Check the returning code
        String payload = http.getString();   //Get the request response payload
         Serial.println(httpCode); //Print the response payload
        Serial.println(payload); 
      }
      else {
        String payload = http.getString();  
        Serial.println(httpCode);
        Serial.println("Connection error: " + payload); 
      }
      http.end(); 
    }

    
    DS18B20.setWaitForConversion(false); //No waiting for measurement
    DS18B20.requestTemperatures(); //Initiate the temperature measurement
    lastTemp = millis();  //Remember the last time measurement
  }
    
  }

//------------------------------------------
void HandleRoot(){
  String message = "Number of devices: ";
  message += numberOfDevices;
  message += "\r\n<br>";
  char temperatureString[6];

  message += "<table border='1'>\r\n";
  message += "<tr><td>Device id</td><td>Temperature</td></tr>\r\n";
  for(int i=0;i<numberOfDevices;i++){
    dtostrf(tempDev[i], 2, 2, temperatureString);
    Serial.print( "Sending temperature: " );
    Serial.println( temperatureString );

    message += "<tr><td>";
    message += GetAddressToString( devAddr[i] );
    message += "</td>\r\n";
    message += "<td>";
    message += temperatureString;
    message += "</td></tr>\r\n";
    message += "\r\n";
  }
  message += "</table>\r\n";
  
  server.send(200, "text/html", message );
}

void HandleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", message);
}


//------------------------------------------
void setup() {
  //Setup Serial port speed
  Serial.begin(115200);


  pinMode(thermostatPin, OUTPUT);
  
  //Setup WIFI
  WiFi.begin(ssid, wifiPassword);
  Serial.println("");

  //Wait for WIFI connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HandleRoot);
  server.onNotFound( HandleNotFound );
  server.begin();
  Serial.println("HTTP server started at ip " + WiFi.localIP().toString() );



  //Setup DS18b20 temperature sensor
  SetupDS18B20();
}

void loop() {
  long t = millis();
  
  server.handleClient();
  TempLoop( t );
}
