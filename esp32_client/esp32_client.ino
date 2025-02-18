#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "esp_mac.h"

// DuguLink MQTT Broker details
const char* mqtt_server = "mqtt.dugulink.xyz";
const int mqtt_port = 1883;
const char* mqtt_user = "ubuntu";  
const char* mqtt_pass = "chandu";
const int keepAliveInterval = 60;
const char* brandPrefix = "dugulink/client/";
const char* clientPrefix = "DLC";

// Create object for DuguLink MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Define dynamic variables
String clientId;             // Unique ID for the client device
String commandTopic;         // Topic to receive commands
String acknowledgmentTopic;  // Topic to send acknowledgments

// Function to log subscribed topics
void logSubscription(const String& topic) {
  Serial.print("Subscribed to topic: ");
  Serial.println(topic);
}

// DuguLink MQTT callback function when a message arrives on the subscribed topic
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Print the received message
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // TODO: send the command to aurdino
  Serial.println(message);

  // acknowledge receipt of the command
  String acknowledgmentMessage = "OK";

  // Publish acknowledgment back to the app
  client.publish(acknowledgmentTopic.c_str(), acknowledgmentMessage.c_str());
  // bool success = client.publish(acknowledgmentTopic.c_str(), acknowledgmentMessage.c_str());
  // if (success) {
  //   Serial.println("Acknowledgment published successfully.");
  // } else {
  //   Serial.println("Failed to publish acknowledgment.");
  // }
}

// Function to setup Wi-Fi with WiFiManager and retry connection if necessary
void setupWiFi() {
  // Get the MAC address and generate device ID
  clientId = generateClientID();
  String ssid = "DuguLink Node - " + clientId;

  WiFiManager wifiManager;
  wifiManager.autoConnect(ssid.c_str());

  // After successful connection, display the IP address
  Serial.println("Wi-Fi configuration complete.");
  Serial.println("Attempting to connect...");

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 10) {  // Retry up to 10 times
    Serial.print("Attempting to connect to Wi-Fi...");
    delay(5000);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wi-Fi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // construct topics using device ID
    commandTopic = brandPrefix + clientId + "/commands";
    acknowledgmentTopic = brandPrefix + clientId + "/acknowledgments";
  } else {
    Serial.println("Failed to connect to Wi-Fi after multiple attempts.");
  }
}

// Function to connect to DuguLink MQTT broker/server
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to DuguLink MQTT broker...");

    // Use clientId as the DuguLink MQTT client ID
    // Try connecting to the DuguLink MQTT broker
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Connected to DuguLink MQTT broker!");
      // Subscribe to the dynamic command topic
      client.subscribe(commandTopic.c_str());
      logSubscription(commandTopic);
    } else {
      Serial.print("Failed to connect, retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// Generate the unique device ID for the client from MAC
String generateClientID() {
  uint8_t macSta[6];
  esp_read_mac(macSta, ESP_MAC_WIFI_STA);  // Get MAC for STA mode from

  // Print the MAC address
  String macAddressSTA = "";
  for (int i = 0; i < 6; i++) {
    macAddressSTA += String(macSta[i], HEX);
  }

  macAddressSTA.replace(":", "");  // Remove colons
  macAddressSTA.toUpperCase();     // To uppercase
  return clientPrefix + macAddressSTA;
}

void setup() {
  Serial.begin(115200);  // Start Serial communication with Arduino
  delay(1000);

  // Setup Wi-Fi connection using WiFiManager and retry if necessary
  Serial.println("Connecting to Wi-Fi...");
  setupWiFi();

  // Setup DuguLink MQTT connection
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);        // Set the callback function to handle messages
  client.setKeepAlive(keepAliveInterval);  // Set the keep-alive interval (in seconds)

  // Connect to DuguLink MQTT
  connectMQTT();
}

void loop() {
  // Ensure DuguLink MQTT connection stays active
  if (!client.connected()) {
    connectMQTT();  // Reconnect to DuguLink MQTT if disconnected
  }
  client.loop();  // Keep the DuguLink MQTT connection alive and process messages
}