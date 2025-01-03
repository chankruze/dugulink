#include <WiFi.h>
#include <WiFiManager.h>   // Include WiFiManager library
#include <PubSubClient.h>  // Include the MQTT library

// MQTT Broker details
const char* mqtt_server = "0.tcp.in.ngrok.io";  // Replace with your MQTT broker address (ngrok or local)
const int mqtt_port = 10303;                    // Replace with your MQTT port (from ngrok or local)

// Create object for MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Define dynamic variables
String uniqueId;             // Unique ID for the device
String commandTopic;         // Topic to receive commands
String acknowledgmentTopic;  // Topic to send acknowledgments

// Function to log subscribed topics
void logSubscription(const String& topic) {
  Serial.print("Subscribed to topic: ");
  Serial.println(topic);
}

// MQTT callback function when a message arrives on the subscribed topic
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
  WiFiManager wifiManager;
  wifiManager.autoConnect("BlueLink Setup");

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

    // Get the MAC address and generate unique topics
    uniqueId = String(WiFi.macAddress());
    commandTopic = "esp32/device/" + uniqueId + "/commands";
    acknowledgmentTopic = "esp32/device/" + uniqueId + "/acknowledgments";
    Serial.print("Device Unique ID: ");
    Serial.println(uniqueId);
  } else {
    Serial.println("Failed to connect to Wi-Fi after multiple attempts.");
  }
}

// Function to connect to MQTT server
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");

    // Use unique ID as the MQTT client ID
    String clientId = "ESP32-" + uniqueId;

    // Try connecting to the MQTT broker
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected to MQTT broker!");
      // Subscribe to the dynamic command topic
      client.subscribe(commandTopic.c_str());
      logSubscription(commandTopic);
    } else {
      Serial.print("Failed to connect, retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);  // Start Serial communication with Arduino
  delay(1000);

  // Setup Wi-Fi connection using WiFiManager and retry if necessary
  Serial.println("Connecting to Wi-Fi...");
  setupWiFi();

  // Setup MQTT connection
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);  // Set the callback function to handle messages

  // Connect to MQTT
  connectMQTT();
}

void loop() {
  // Ensure MQTT connection stays active
  if (!client.connected()) {
    connectMQTT();  // Reconnect to MQTT if disconnected
  }
  client.loop();  // Keep the MQTT connection alive and process messages
}
