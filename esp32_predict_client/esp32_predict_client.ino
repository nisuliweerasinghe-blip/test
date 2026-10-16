#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"

const char* ssid     = "sachintha shehara’s iPhone";
const char* password = "111111234132";

// Use https:// with ngrok (free tunnels redirect plain http)
const char* serverUrl = "https://snowfield-botanist-tidings.ngrok-free.dev/predict";
// ---- AI-Thinker ESP32-CAM pin map ----
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;   // 640x480 keeps uploads small
  config.jpeg_quality = 12;              // 0-63, lower = better quality
  config.fb_count     = 1;

  return esp_camera_init(&config) == ESP_OK;
}

void readSensors(float &tempC, float &humidity, float &visibilityKm) {
  // TODO: replace with real sensor reads (DHT22, BME280, ...)
  tempC = 29.0;
  humidity = 88.0;
  visibilityKm = 3.5;
}

void sendReading() {
  float t, h, v;
  readSensors(t, h, v);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  const String boundary = "----ESP32Boundary7MA4YWxkTrZu0gW";

  // Text fields + image part header
  String head;
  head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"temp_c\"\r\n\r\n" + String(t, 1) + "\r\n";
  head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"humidity_pct\"\r\n\r\n" + String(h, 1) + "\r\n";
  head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"visibility_km\"\r\n\r\n" + String(v, 1) + "\r\n";
  head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"photo.jpg\"\r\n";
  head += "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  // Binary JPEG can't live in a String, so build one raw byte buffer
  size_t total = head.length() + fb->len + tail.length();
  uint8_t* buf = (uint8_t*)ps_malloc(total);   // PSRAM
  if (!buf) buf = (uint8_t*)malloc(total);
  if (!buf) {
    Serial.println("Out of memory");
    esp_camera_fb_return(fb);
    return;
  }
  memcpy(buf, head.c_str(), head.length());
  memcpy(buf + head.length(), fb->buf, fb->len);
  memcpy(buf + head.length() + fb->len, tail.c_str(), tail.length());
  esp_camera_fb_return(fb);

  WiFiClientSecure client;
  client.setInsecure();                        // fine for ngrok demos
  HTTPClient http;
  http.begin(client, serverUrl);
  http.setTimeout(20000);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("ngrok-skip-browser-warning", "1");  // skips ngrok's interstitial page

  int code = http.POST(buf, total);            // raw-bytes overload
  free(buf);

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

  if (!initCamera()) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }

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
