#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_TMP117.h>
#include <esp_wifi.h>

// ================== DEVICE CONFIG ==================
const char *DEVICE_ID = "TempGuard-01";

// ================== WIFI CONFIG ==================
const char *WIFI_SSID = "SYED,S";
const char *WIFI_PASSWORD = "@Syeds988";

// ================== BACKEND CONFIG ==================
const char *BACKEND_URL =
    "https://temp-backend-production-4599.up.railway.app/api/sensor/data";

// ================== SENSOR ==================
Adafruit_TMP117 tmp117;

// ================== TIMING ==================
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 5000; // 5 seconds

// ================== WIFI CONNECT ==================
void connectWiFi()
{
  WiFi.disconnect(true);
  delay(1000);

  WiFi.mode(WIFI_STA);

  // 🔥 Force public DNS (CRITICAL for ESP32)
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8));

  Serial.print("📡 Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n✅ WiFi Connected");
    Serial.print("📍 IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("📡 DNS Server: ");
    Serial.println(WiFi.dnsIP());
  }
  else
  {
    Serial.println("\n❌ WiFi connection failed");
  }
}

// ================== SETUP ==================
void setup()
{
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n🚀 ESP32 Live Temperature Sensor Starting");

  // I2C (ESP32 default)
  Wire.begin(21, 22);

  // TMP117 init (force default address)
  if (!tmp117.begin(0x48))
  {
    Serial.println("❌ TMP117 not detected at 0x48");
    while (true)
    {
      delay(1000);
    }
  }

  Serial.println("✅ TMP117 detected at 0x48");

  // WiFi
  connectWiFi();
}

// ================== LOOP ==================
void loop()
{
  // Ensure WiFi stays connected
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("⚠️ WiFi lost, reconnecting...");
    connectWiFi();
  }

  // Send data every interval
  if (millis() - lastSendTime >= SEND_INTERVAL)
  {
    lastSendTime = millis();

    sensors_event_t tempEvent;
    tmp117.getEvent(&tempEvent);

    float temperature = tempEvent.temperature;

    // 🔋 Battery (mock value for now)
    int battery = 78.5;

    Serial.println("\n📤 Sending sensor data:");
    Serial.print("🆔 Device ID: ");
    Serial.println(DEVICE_ID);
    Serial.print("🌡 Temperature: ");
    Serial.print(temperature, 2);
    Serial.println(" °C");
    Serial.print("🔋 Battery: ");
    Serial.print(battery);
    Serial.println(" %");

    if (WiFi.status() == WL_CONNECTED)
    {
      WiFiClientSecure client;
      client.setInsecure(); // 🔥 required for Railway HTTPS

      HTTPClient http;
      http.begin(client, BACKEND_URL);
      http.addHeader("Content-Type", "application/json");

      String payload = "{";
      payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
      payload += "\"temperature\":" + String(temperature, 2) + ",";
      payload += "\"battery\":" + String(battery);
      payload += "}";

      Serial.print("📦 Payload: ");
      Serial.println(payload);

      int httpResponseCode = http.POST(payload);

      if (httpResponseCode > 0)
      {
        Serial.print("✅ Data sent | HTTP ");
        Serial.println(httpResponseCode);
      }
      else
      {
        Serial.print("❌ Send failed | Code ");
        Serial.println(httpResponseCode);
      }

      http.end();
    }
    else
    {
      Serial.println("❌ Skipped send (WiFi not connected)");
    }
  }
}
