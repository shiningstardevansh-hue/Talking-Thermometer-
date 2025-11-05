#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Wi-Fi credentials (replace or set via serial before upload)
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
const char* serverUrl = "http://192.168.1.100:3000/esp_reading"; // POST endpoint

#define ONE_WIRE_BUS D2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(" connected");
  sensors.begin();
}

void loop() {
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  if (t != DEVICE_DISCONNECTED_C) {
    String payload = "{";
    payload += "\"tempC\":" + String(t, 4);
    payload += ",\"ts\":" + String(millis());
    payload += "}";

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");
      int code = http.POST(payload);
      Serial.printf("Posted: %s => %d\n", payload.c_str(), code);
      http.end();
    }
  }
  delay(10000); // post every 10s
}
