#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <ESPmDNS.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "secrets.h"

#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS
#define LCD_BACKLIGHT_PIN 21

// WiFi credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// MQTT broker
const char* mqtt_server = MQTT_BROKER_IP;
const int mqtt_port = 1883;
const char* mqtt_topic = "home/livingroom/temperature";

// Display
// Display configuration - matches EEZ Studio project settings
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static lv_color_t buf1[screenWidth * 40]; // 40 lines buffer for faster refresh
static lv_color_t buf2[screenWidth * 40]; // Second buffer for double buffering

TFT_eSPI tft = TFT_eSPI();  // Uses settings from User_Setup.h

WiFiClient espClient;
PubSubClient client(espClient);

// LVGL log callback
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)color_p, w * h);
    tft.endWrite();

    lv_display_flush_ready(display); // Tell LVGL you are ready with the flushing
}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("  Connecting...");
    lv_label_set_text(objects.label_wifi_connected_state, "Connecting...");
  }
  Serial.println("Connected to WiFi: " + WiFi.localIP().toString());
  lv_label_set_text(objects.label_wifi_connected_state, "Connected");
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
  message = message + "°C";

  Serial.println("Message arrived: " + message);
  if (String(topic) == mqtt_topic) {
    lv_label_set_text(objects.label_temperature, message.c_str());
  }
}

void reconnect() {
  Serial.println("Reconnecting to MQTT...");

  while (!client.connected()) {
    if (client.connect("ESP32-CYD", MQTT_USER, MQTT_PASSWORD)) {
      client.subscribe(mqtt_topic);
    } else {
      Serial.println("Failed to connect, retrying...");
      lv_label_set_text(objects.label_mqtt_connection_state, "Not connected");
      delay(5000);
    }
  }

  Serial.println("Reconnected to MQTT");
  lv_label_set_text(objects.label_mqtt_connection_state, "Connected");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  Serial.println("In setup()");

  // Initialize TFT
  tft.init();
  tft.setRotation(2);
  // pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
  // digitalWrite(LCD_BACKLIGHT_PIN, HIGH); // Turn on backlight

  // Initialize LVGL
  lv_init();
  lv_log_register_print_cb(log_print);
  
  // Create display (LVGL 9.x API)
  lv_display_t *display = lv_display_create(screenWidth, screenHeight);
  
  // Set display buffer with double buffering for smoother refresh
  lv_display_set_buffers(display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  
  // Set display flush callback
  lv_display_set_flush_cb(display, display_flush);

  // Initialize EEZ Studio generated UI
  ui_init();
  
  Serial.println("UI initialized and ready!");
  
  // Force initial screen refresh
  lv_refr_now(display);

  setup_wifi();
  setup_mdns();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("Awaiting messages...");
}

void loop() {
  // CRITICAL: Tell LVGL how much time has passed
  lv_tick_inc(3); // 3ms matches our delay below for faster refresh
  
  // Handle LVGL tasks
  lv_timer_handler();
  
  // Handle EEZ Studio UI updates
  ui_tick();
  
  // Small delay to prevent watchdog issues - reduced for faster refresh
  delay(3);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
