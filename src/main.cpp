#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_TMP117.h>

// ================== DEVICE CONFIG ==================
const char *DEVICE_ID = "TempGuard-01";

// ================== WIFI CONFIG ==================
const char *WIFI_SSID = "OnePlus 11R";
const char *WIFI_PASSWORD = "14253647";

// ================== MQTT CONFIG ==================
const char *MQTT_BROKER =
  "ea343f92c15b4d419f30142f2702ce2e.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;

const char *MQTT_USERNAME = "tempguard-device";
const char *MQTT_PASSWORD = "TempGuard@123";

String MQTT_TOPIC = String("tempguard/") + DEVICE_ID + "/data";

// ================== I2C CONFIG ==================
#define SDA_PIN 21
#define SCL_PIN 22
#define TMP117_ADDR 0x48

// ================== SENSOR ==================
Adafruit_TMP117 tmp117;

// ================== MQTT CLIENT ==================
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// ================== TIMING ==================
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 5000;

// ================== WIFI CONNECT ==================
void connectWiFi()
{
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("📡 Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // 🔥 IMPORTANT for hotspot & college WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n✅ WiFi Connected");
    Serial.print("📍 IP Address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\n❌ WiFi Failed");
  }
}

// ================== MQTT CONNECT ==================
void connectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.print("🔌 Connecting to MQTT... ");

    if (mqttClient.connect(
          DEVICE_ID,
          MQTT_USERNAME,
          MQTT_PASSWORD))
    {
      Serial.println("✅ Connected to HiveMQ");
    }
    else
    {
      Serial.print("❌ Failed (rc=");
      Serial.print(mqttClient.state());
      Serial.println("), retrying in 3s");
      delay(3000);
    }
  }
}

// ================== TMP117 INIT ==================
bool initTMP117()
{
  Serial.print("🔍 Initializing TMP117");
  for (int i = 0; i < 3; i++)
  {
    Serial.print(".");
    if (tmp117.begin(TMP117_ADDR))
    {
      Serial.println("\n✅ TMP117 detected");
      return true;
    }
    delay(300);
  }
  Serial.println("\n❌ TMP117 not detected");
  return false;
}

// ================== SETUP ==================
void setup()
{
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n🚀 ESP32 MQTT Temperature Sensor Starting");

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  delay(300);

  if (!initTMP117())
  {
    Serial.println("🛑 Sensor failed, rebooting...");
    delay(5000);
    ESP.restart();
  }

  // WiFi
  connectWiFi();

  // MQTT TLS
  secureClient.setInsecure();   // ✅ Required for ESP32 TLS
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(60);
}

// ================== LOOP ==================
void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
  }

  if (!mqttClient.connected())
  {
    connectMQTT();
  }

  mqttClient.loop();

  if (millis() - lastSendTime >= SEND_INTERVAL)
  {
    lastSendTime = millis();

    sensors_event_t event;
    tmp117.getEvent(&event);

    float temperature = event.temperature;
    int battery = 78; // mock value

    String payload = "{";
    payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"temperature\":" + String(temperature, 2) + ",";
    payload += "\"battery\":" + String(battery);
    payload += "}";

    Serial.println("\n📤 Publishing MQTT data");
    Serial.println(payload);

    mqttClient.publish(MQTT_TOPIC.c_str(), payload.c_str(), true);
  }
}
