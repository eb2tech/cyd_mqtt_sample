#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <vector>
#include <lvgl.h>
#include "ui/ui.h"
#include "MQTTDispatcher.h"
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

// MQTT broker - will be discovered via provisioning service
String mqtt_server = ""; // Will be set by provisioning service
int mqtt_port = 1883;    // Will be set by provisioning service
String mqtt_username = ""; // Will be set by provisioning service
String mqtt_password = ""; // Will be set by provisioning service
bool mqtt_broker_found = false;
const char *mqtt_topic = "home/livingroom/temperature";
const char *mqtt_ha_topic = "homeassistant/sensor/#";

// Display
// Display configuration - matches EEZ Studio project settings
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static lv_color_t buf1[screenWidth * 20]; // 20 lines buffer (20 horizontal rows)

TFT_eSPI tft = TFT_eSPI(); // Uses settings from User_Setup.h

WiFiClient espClient;
PubSubClient client(espClient);
MQTTDispatcher mqttDispatcher;

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

// Helper function to parse chunked HTTP response
String parseChunkedResponse(String chunkedData)
{
  String result = "";
  int pos = 0;
  
  while (pos < chunkedData.length())
  {
    // Find the chunk size (hex number before \r\n)
    int crlfPos = chunkedData.indexOf('\n', pos);
    if (crlfPos == -1) break;
    
    String chunkSizeStr = chunkedData.substring(pos, crlfPos);
    chunkSizeStr.trim(); // Remove \r and whitespace
    
    // Convert hex chunk size to integer
    long chunkSize = strtol(chunkSizeStr.c_str(), nullptr, 16);
    
    if (chunkSize == 0) break; // End of chunks
    
    // Extract the chunk data
    pos = crlfPos + 1;
    if (pos + chunkSize > chunkedData.length()) break;
    
    result += chunkedData.substring(pos, pos + chunkSize);
    pos += chunkSize;
    
    // Skip the trailing \r\n after chunk data
    if (pos < chunkedData.length() && chunkedData.charAt(pos) == '\r') pos++;
    if (pos < chunkedData.length() && chunkedData.charAt(pos) == '\n') pos++;
  }
  
  return result;
}

bool discover_mqtt_broker()
{
  Serial.println("Discovering provisioning service via mDNS...");
  lv_label_set_text(objects.label_mqtt_connection_state, "Finding provisioning...");

  // Query for CYD provisioning service
  int n = MDNS.queryService("cyd-provision", "tcp");
  if (n == 0)
  {
    Serial.println("No CYD provisioning services found");
    lv_label_set_text(objects.label_mqtt_connection_state, "No provisioning service");
    return false;
  }

  Serial.printf("Found %d provisioning service(s)\n", n);
  
  // Use the first provisioning service found
  String provisionIP = MDNS.IP(0).toString();
  int provisionPort = MDNS.port(0);
  
  Serial.printf("Contacting provisioning service: %s:%d\n", provisionIP.c_str(), provisionPort);
  lv_label_set_text(objects.label_mqtt_connection_state, "Contacting provisioning...");

  // Connect to provisioning service via TCP
  WiFiClient provisionClient;
  if (!provisionClient.connect(provisionIP.c_str(), provisionPort))
  {
    Serial.println("Failed to connect to provisioning service");
    lv_label_set_text(objects.label_mqtt_connection_state, "Provisioning failed");
    return false;
  }

  // Create provisioning request JSON
  JsonDocument requestDoc;
  requestDoc["device_id"] = getDeviceIdentifier();
  requestDoc["device_type"] = "cyd-esp32";
  requestDoc["mac_address"] = getChipIdString();
  requestDoc["request_type"] = "mqtt_config";
  
  String requestJson;
  serializeJson(requestDoc, requestJson);
  
  Serial.println("Sending provisioning request: " + requestJson);

  // Send HTTP-like request to provisioning service
  provisionClient.println("POST /provision HTTP/1.1");
  provisionClient.println("Host: " + provisionIP);
  provisionClient.println("Content-Type: application/json");
  provisionClient.println("Content-Length: " + String(requestJson.length()));
  provisionClient.println();
  provisionClient.println(requestJson);

  // Wait for response
  unsigned long timeout = millis() + 5000; // 5 second timeout
  while (!provisionClient.available() && millis() < timeout)
  {
    delay(10);
  }

  if (!provisionClient.available())
  {
    Serial.println("Provisioning service timeout");
    provisionClient.stop();
    lv_label_set_text(objects.label_mqtt_connection_state, "Provisioning timeout");
    return false;
  }

  // Read complete HTTP response
  String fullResponse = "";
  while (provisionClient.available())
  {
    fullResponse += provisionClient.readString();
    delay(10); // Small delay to ensure all data is received
  }
  provisionClient.stop();

  Serial.println("Full HTTP response:");
  Serial.println(fullResponse);

  // Find the JSON body (after the double CRLF that ends headers)
  String responseJson = "";
  int bodyStart = fullResponse.indexOf("\r\n\r\n");
  if (bodyStart != -1)
  {
    responseJson = fullResponse.substring(bodyStart + 4);
  }
  else
  {
    // Try alternative line endings
    bodyStart = fullResponse.indexOf("\n\n");
    if (bodyStart != -1)
    {
      responseJson = fullResponse.substring(bodyStart + 2);
    }
    else
    {
      Serial.println("Could not find HTTP body separator");
      lv_label_set_text(objects.label_mqtt_connection_state, "Invalid HTTP response");
      return false;
    }
  }

  // Handle chunked encoding - remove chunk size indicators
  if (fullResponse.indexOf("Transfer-Encoding: chunked") != -1)
  {
    Serial.println("Handling chunked response");
    responseJson = parseChunkedResponse(responseJson);
  }

  // Clean up any remaining whitespace or control characters
  responseJson.trim();

  Serial.println("Extracted JSON response:");
  Serial.println("Length: " + String(responseJson.length()));
  Serial.println("Content: " + responseJson);
  
  // Additional cleanup - remove any non-printable characters at start/end
  while (responseJson.length() > 0 && (responseJson.charAt(0) < 32 || responseJson.charAt(0) > 126))
  {
    responseJson = responseJson.substring(1);
  }
  while (responseJson.length() > 0 && (responseJson.charAt(responseJson.length()-1) < 32))
  {
    responseJson = responseJson.substring(0, responseJson.length()-1);
  }
  
  Serial.println("Cleaned JSON: " + responseJson);

  // Parse JSON response
  JsonDocument responseDoc;
  DeserializationError error = deserializeJson(responseDoc, responseJson);
  
  if (error)
  {
    Serial.println("Failed to parse provisioning response: " + String(error.c_str()));
    lv_label_set_text(objects.label_mqtt_connection_state, "Invalid response");
    return false;
  }

  // Extract MQTT configuration
  if (responseDoc["status"] == "success")
  {
    mqtt_server = responseDoc["mqtt_broker"].as<String>();
    mqtt_port = responseDoc["mqtt_port"].as<int>();
    mqtt_username = responseDoc["mqtt_username"].as<String>();
    mqtt_password = responseDoc["mqtt_password"].as<String>();
    
    mqtt_broker_found = true;

    Serial.printf("Provisioned MQTT broker: %s:%d\n", mqtt_server.c_str(), mqtt_port);
    Serial.printf("MQTT credentials: %s / %s\n", mqtt_username.c_str(), mqtt_password.c_str());
    
    String status = "Provisioned: " + mqtt_server + ":" + String(mqtt_port);
    lv_label_set_text(objects.label_mqtt_connection_state, status.c_str());
    return true;
  }
  else
  {
    String error = responseDoc["error"] | "Unknown error";
    Serial.println("Provisioning failed: " + error);
    lv_label_set_text(objects.label_mqtt_connection_state, ("Error: " + error).c_str());
    return false;
  }
}

void test_handler(const std::string& topic, const std::string& message)
{
  std::string newMessage = message + std::string(" °C");
  lv_label_set_text(objects.label_temperature, newMessage.c_str());
}

// Simple split function similar to boost::split
std::vector<std::string> split(const std::string& str, char delimiter)
{
  std::vector<std::string> tokens;
  size_t start = 0;
  size_t end = 0;
  
  while ((end = str.find(delimiter, start)) != std::string::npos)
  {
    if (end != start) // Skip empty tokens
    {
      tokens.push_back(str.substr(start, end - start));
    }
    start = end + 1;
  }
  
  // Add the last token
  if (start < str.length())
  {
    tokens.push_back(str.substr(start));
  }
  
  return tokens;
}

void ha_handler(const std::string& topic, const std::string& message)
{
  if (topic.find("temperature") != std::string::npos && topic.rfind("state") == topic.length() - 5)
  {
    std::string newMessage = message + std::string(" °F");
    lv_label_set_text(objects.label_temperature, newMessage.c_str());
    
    // Split topic by '/' and extract the third segment (index 2)
    std::vector<std::string> topicParts = split(topic, '/');
    std::string topicSegment = "";
    
    if (topicParts.size() > 2)
    {
      topicSegment = topicParts[2]; // Extract 'c' from "a/b/c/d"
    }
    else
    {
      topicSegment = topic; // Fallback to full topic
    }
    
    lv_label_set_text(objects.label_mqtt_topic, topicSegment.c_str());
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  std::string message((char*)payload, length);

  Serial.print("Message arrived: ");
  Serial.print(message.c_str());
  Serial.print(" on topic: ");
  Serial.println(topic);

  mqttDispatcher.dispatch(topic, message);
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
    // Use provisioned credentials if available, otherwise fall back to secrets.h
    const char* username = mqtt_username.length() > 0 ? mqtt_username.c_str() : MQTT_USER;
    const char* password = mqtt_password.length() > 0 ? mqtt_password.c_str() : MQTT_PASSWORD;
    
    if (client.connect("ESP32-CYD", username, password))
    {
      client.subscribe(mqtt_topic);
      client.subscribe(mqtt_ha_topic);
      Serial.println("Connected to MQTT with provisioned credentials");
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

    mqttDispatcher.registerHandler(mqtt_topic, test_handler);
    mqttDispatcher.registerHandler(mqtt_ha_topic, ha_handler);
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
