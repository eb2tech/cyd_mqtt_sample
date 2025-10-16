#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <ESPmDNS.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "secrets.h"

#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS
#define LCD_BACKLIGHT_PIN 21

// WiFi credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// MQTT broker - will be discovered via mDNS
String mqtt_server = ""; // Will be set by mDNS discovery
int mqtt_port = 1883;    // Will be set by mDNS discovery
bool mqtt_broker_found = false;
const char *mqtt_topic = "home/livingroom/temperature";

// Display
// Display configuration - matches EEZ Studio project settings
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static lv_color_t buf1[screenWidth * 20]; // 20 lines buffer (20 horizontal rows)

TFT_eSPI tft = TFT_eSPI(); // Uses settings from User_Setup.h

WiFiClient espClient;
PubSubClient client(espClient);

// Device identification functions
String getDeviceIdentifier()
{
  // Use MAC address as serial number (most common approach)
  return WiFi.macAddress();
}

uint64_t getChipId()
{
  return ESP.getEfuseMac();
}

String getChipIdString()
{
  uint64_t chipid = ESP.getEfuseMac();
  return String((uint32_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
}

void printDeviceInfo()
{
  Serial.println("=== Device Information ===");
  Serial.println("MAC Address (Serial): " + getDeviceIdentifier());
  Serial.println("Chip ID: " + getChipIdString());
  Serial.println("Chip Model: " + String(ESP.getChipModel()));
  Serial.println("Chip Revision: " + String(ESP.getChipRevision()));
  Serial.println("Flash Size: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  Serial.println("Heap Size: " + String(ESP.getHeapSize()) + " bytes");
  Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("==========================");
}

// LVGL log callback
void log_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushPixels((uint16_t *)color_p, w * h);
  tft.endWrite();

  lv_display_flush_ready(display); // Tell LVGL you are ready with the flushing
}

void setup_wifi()
{
  delay(10);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("  Connecting...");
    lv_label_set_text(objects.label_wifi_connected_state, "Connecting...");
  }
  Serial.println("Connected to WiFi: " + WiFi.localIP().toString());
  lv_label_set_text(objects.label_wifi_connected_state, "Connected");
}

void setup_mdns()
{
  Serial.println("Setting up mDNS responder...");
  while (!MDNS.begin(MDNS_HOSTNAME))
  {
    Serial.println("Error setting up MDNS responder...");
    delay(1000);
  }
  Serial.println("mDNS responder started");
}

bool discover_mqtt_broker()
{
  Serial.println("Discovering MQTT broker via mDNS...");
  lv_label_set_text(objects.label_mqtt_connection_state, "Discovering broker...");

  // Query for MQTT service
  int n = MDNS.queryService("mqtt", "tcp");
  if (n == 0)
  {
    Serial.println("No MQTT services found");
    return false;
  }
  else
  {
    Serial.printf("Found %d MQTT service(s)\n", n);
    for (int i = 0; i < n; ++i)
    {
      Serial.printf("  %d: %s.local:%d\n", i, MDNS.hostname(i).c_str(), MDNS.port(i));

      // Use the first MQTT service found
      if (i == 0)
      {
        mqtt_server = MDNS.IP(i).toString(); // Get IP address
        mqtt_port = MDNS.port(i);
        mqtt_broker_found = true;

        Serial.printf("Using MQTT broker: %s:%d\n", mqtt_server.c_str(), mqtt_port);
        String status = "Found: " + mqtt_server + ":" + String(mqtt_port);
        lv_label_set_text(objects.label_mqtt_connection_state, status.c_str());
        return true;
      }
    }
  }
  return false;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  String message(payload, length);
  message = message + "°C";

  Serial.println("Message arrived: " + message);
  if (String(topic) == mqtt_topic)
  {
    lv_label_set_text(objects.label_temperature, message.c_str());
  }
}

void reconnect()
{
  if (!mqtt_broker_found)
  {
    Serial.println("No MQTT broker discovered - attempting discovery again...");
    if (!discover_mqtt_broker())
    {
      delay(5000); // Wait before trying discovery again
      return;
    }
    // Reconfigure client with newly discovered broker
    client.setServer(mqtt_server.c_str(), mqtt_port);
    client.setCallback(callback);
  }

  Serial.println("Reconnecting to MQTT...");

  while (!client.connected())
  {
    if (client.connect("ESP32-CYD", MQTT_USER, MQTT_PASSWORD))
    {
      client.subscribe(mqtt_topic);
    }
    else
    {
      Serial.println("Failed to connect, retrying...");
      lv_label_set_text(objects.label_mqtt_connection_state, "Connection failed");
      delay(5000);
    }
  }

  Serial.println("Reconnected to MQTT");
  lv_label_set_text(objects.label_mqtt_connection_state, "Connected");
}

void setup_mqtt()
{
  // Discover MQTT broker via mDNS
  if (discover_mqtt_broker())
  {
    client.setServer(mqtt_server.c_str(), mqtt_port);
    client.setCallback(callback);
    Serial.println("MQTT client configured with discovered broker");
  }
  else
  {
    Serial.println("Failed to discover MQTT broker - check mDNS service");
    lv_label_set_text(objects.label_mqtt_connection_state, "Broker not found");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);

  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  Serial.println("In setup()");

  // Print device information
  printDeviceInfo();

  // Initialize TFT
  tft.init();
  tft.setRotation(2);

  // Initialize LVGL
  lv_init();
  lv_log_register_print_cb(log_print);

  // Create display (LVGL 9.x API)
  lv_display_t *display = lv_display_create(screenWidth, screenHeight);

  // Set display buffer with single buffering to save memory
  lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Set display flush callback
  lv_display_set_flush_cb(display, display_flush);

  // Initialize EEZ Studio generated UI
  ui_init();

  Serial.println("UI initialized and ready!");

  // Force initial screen refresh
  lv_refr_now(display);

  setup_wifi();
  setup_mdns();
  setup_mqtt();

  Serial.println("Awaiting messages...");
}

void loop()
{
  // CRITICAL: Tell LVGL how much time has passed
  lv_tick_inc(3); // 3ms matches our delay below for faster refresh

  // Handle LVGL tasks
  lv_timer_handler();

  // Handle EEZ Studio UI updates
  ui_tick();

  // Small delay to prevent watchdog issues - reduced for faster refresh
  delay(3);

  // Only try MQTT operations if we have a broker
  if (mqtt_broker_found)
  {
    if (!client.connected())
    {
      reconnect();
    }
    client.loop();
  }
  else
  {
    // Periodically retry mDNS discovery
    static unsigned long lastDiscoveryAttempt = 0;
    if (millis() - lastDiscoveryAttempt > 10000)
    { // Try every 10 seconds
      lastDiscoveryAttempt = millis();
      discover_mqtt_broker();
      if (mqtt_broker_found)
      {
        client.setServer(mqtt_server.c_str(), mqtt_port);
        client.setCallback(callback);
      }
    }
  }
}
