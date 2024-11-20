
#define BLYNK_TEMPLATE_ID "TMPL33Ew-MwQU"  
#define BLYNK_TEMPLATE_NAME "ewfio"  
#define BLYNK_AUTH_TOKEN "O2j5eXxMxsEOcc-gtVgVjWh7gBkx15ty"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

const char* ssid = "JioFi3_5CE2C3";
const char* password = "fey4sux1us";
String openWeatherMapApiKey = "eb243844c7dc917199797a85c2f707ce";

String city = "Tumakuru";
String countryCode = "IN";

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;
unsigned long manualStartTime = 0;  // Store the time when manual mode is activated
const unsigned long manualDuration = 20000;  // 20 seconds in milliseconds

String jsonBuffer;

// Soil moisture sensor pin
const int soilMoisturePin = A0;  // ESP8266 only has one ADC pin (A0)

// Thresholds for motor control
const int soilMoistureThreshold = 400;  // Adjust this value based on your sensor and soil type
const float rainThreshold = 10.0;  // 5mm of rain, adjust as needed

// Motor and valve control variables
bool motorOn = false;
bool valveOn = false;
bool manualControl = false;

// Motor and valve control pins
const int motorPin = D1;  // Using D1 (GPIO5) for motor control
const int valvePin = D2;  // Using D2 (GPIO4) for valve control

// This function will be called every time V10 changes state (motor control)
BLYNK_WRITE(V10)
{
  manualControl = true;
  manualStartTime = millis();  // Record the time when manual mode starts
  motorOn = param.asInt();
  digitalWrite(motorPin, motorOn ? HIGH : LOW);
  Serial.printf("Manual motor control: %s\n", motorOn ? "ON" : "OFF");
}

// This function will be called every time V9 changes state (valve control)
BLYNK_WRITE(V9)
{
  manualControl = true;
  manualStartTime = millis();  // Record the time when manual mode starts
  valveOn = param.asInt();
  digitalWrite(valvePin, valveOn ? HIGH : LOW);
  Serial.printf("Manual valve control: %s\n", valveOn ? "ON" : "OFF");
}

void setup()
{
  Serial.begin(115200);
  
  pinMode(motorPin, OUTPUT);
  pinMode(valvePin, OUTPUT);
  digitalWrite(motorPin, LOW);  // Start with motor off
  digitalWrite(valvePin, LOW);  // Start with valve off
  
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
   
  Serial.println("Timer set to 10 seconds (timerDelay variable), it will take 10 seconds before publishing the first reading.");
}

void loop()
{
  Blynk.run();

  // Revert to automatic mode after 20 seconds of manual control
  if (manualControl && (millis() - manualStartTime > manualDuration)) {
    manualControl = false;
    Serial.println("Reverting to automatic control...");
  }

  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;
      
      String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openWeatherMapApiKey;
      
      http.begin(client, serverPath);  // ESP8266 requires a client object to be passed
      
      int httpResponseCode = http.GET();
      
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        jsonBuffer = http.getString();
        Serial.println(jsonBuffer);
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, jsonBuffer);
    
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
          http.end();
          return;
        }
      
        float temp = doc["main"]["temp"].as<float>() - 273.15;
        
        // Check if "rain" object exists and get "1h" value if it does
        float rain = 0;
        if (doc.containsKey("rain") && doc["rain"].containsKey("1h")) {
          rain = doc["rain"]["1h"].as<float>();
        }
        
        // Get weather description
        String weatherDescription = doc["weather"][0]["description"].as<String>();

        // Read soil moisture sensor
        int soilMoisture = analogRead(soilMoisturePin);  // ESP8266 ADC range is 0-1023

        // Convert soil moisture to percentage (ESP8266 ADC is 10-bit, so max value is 1023)
        int soilMoisturePercentage = map(soilMoisture, 0, 1023, 0, 100);

        soilMoisture = 1024 - soilMoisture;

        // Determine motor and valve states if not in manual control
        if (!manualControl) {
          if (soilMoisture > soilMoistureThreshold || rain > rainThreshold) {
            motorOn = false;
            valveOn = false;
          } else {
            motorOn = true;
            valveOn = true;
          }
          digitalWrite(motorPin, motorOn ? HIGH : LOW);
          digitalWrite(valvePin, valveOn ? HIGH : LOW);
        }

        // Send data to Blynk
        Blynk.virtualWrite(V0, temp);
        Blynk.virtualWrite(V1, rain);
        Blynk.virtualWrite(V2, weatherDescription);
        Blynk.virtualWrite(V3, 100 - soilMoisturePercentage);
        Blynk.virtualWrite(V9, valveOn ? 1 : 0);  // Update valve state in Blynk
        Blynk.virtualWrite(V10, motorOn ? 1 : 0);  // Update motor state in Blynk

        // Print data to serial monitor
        Serial.printf("Temperature: %.2f°C\n", temp);
        Serial.printf("Rain: %.2f mm\n", rain);
        Serial.printf("Weather Description: %s\n", weatherDescription.c_str());
        Serial.printf("Soil Moisture: %d%%\n", 100 - soilMoisturePercentage);
        Serial.printf("Soil analog level: %d\n", soilMoisture);
        Serial.printf("Motor State: %s\n", motorOn ? "ON" : "OFF");
        Serial.printf("Valve State: %s\n", valveOn ? "ON" : "OFF");
        Serial.printf("Control Mode: %s\n", manualControl ? "Manual" : "Automatic");
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}
