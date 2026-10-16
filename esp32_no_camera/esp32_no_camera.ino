#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid     = "Pixel_5938";
const char* password = "11111119";

// Use https:// with ngrok (free tunnels redirect plain http)
const char* serverUrl = "https://snowfield-botanist-tidings.ngrok-free.dev/predict";

void readSensors(float &tempC, float &humidity, float &visibilityKm) {
  // TODO: replace with real sensor reads (DHT22, BME280, ...)
  tempC = 29.0;
  humidity = 88.0;
  visibilityKm = 3.5;
}

void sendReading() {
  float t, h, v;
  readSensors(t, h, v);

  String body = "temp_c=" + String(t, 1) +
                "&humidity_pct=" + String(h, 1) +
                "&visibility_km=" + String(v, 1);

  WiFiClientSecure client;
  client.setInsecure();                       // fine for ngrok demos
  HTTPClient http;
  http.begin(client, serverUrl);
  http.setTimeout(20000);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("ngrok-skip-browser-warning", "1");

  int code = http.POST(body);

  if (code > 0) {
    String response = http.getString();
    Serial.printf("HTTP %d\n", code);

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, response);
    if (!err) {
      Serial.println("--- PIPELINE OUTPUT ---");
      Serial.printf("Decision: %s\n", doc["decision"] | "unknown");
      Serial.printf("Safety Score: %d\n", doc["score"] | -1);
      Serial.printf("Calculated Risk: %.3f\n", doc["risk"] | 0.0f);
      Serial.println("-----------------------");
    } else {
      Serial.print("JSON parse failed: ");
      Serial.println(err.c_str());
    }
  } else {
    Serial.printf("POST failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    sendReading();
  } else {
    WiFi.reconnect();
  }
  delay(15000);
}
