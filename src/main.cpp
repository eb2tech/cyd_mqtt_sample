#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <ESPmDNS.h>
#include "secrets.h"

// WiFi credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// MQTT broker
const char* mqtt_server = MQTT_BROKER_IP;
const int mqtt_port = 1883;
const char* mqtt_topic = "home/livingroom/temperature";

// Display
TFT_eSPI tft = TFT_eSPI();  // Uses settings from User_Setup.h

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  tft.println("Connecting to WiFi...");

  delay(10);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("  Connecting...");
  }
  Serial.println("Connected to WiFi: " + WiFi.localIP().toString());
  tft.println("Connected");
}

void setup_mdns() {
  Serial.println("Setting up mDNS responder...");
  while (!MDNS.begin(MDNS_HOSTNAME)) {
    Serial.println("Error setting up MDNS responder...");
    delay(1000);
  }
  Serial.println("mDNS responder started");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message(payload, length);

  // Display the message
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 50);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(3);
  tft.print("Temp: " + message + " C");
}

void reconnect() {
  Serial.println("Reconnecting to MQTT...");

  while (!client.connected()) {
    if (client.connect("ESP32-CYD", MQTT_USER, MQTT_PASSWORD)) {
      client.subscribe(mqtt_topic);
    } else {
      Serial.println("Failed to connect, retrying...");
      delay(5000);
    }
  }

  Serial.println("Reconnected to MQTT");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("In setup()");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);

  setup_wifi();
  setup_mdns();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  tft.println("Awaiting messages...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
