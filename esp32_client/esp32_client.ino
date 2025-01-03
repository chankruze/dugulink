#include <WiFi.h>
#include <WiFiManager.h>  // Include WiFiManager library
#include <PubSubClient.h> // Include the MQTT library

// MQTT Broker details
const char* mqtt_server = "0.tcp.in.ngrok.io";  // Replace with your MQTT broker address (ngrok or local)
const int mqtt_port = 15006;  // Replace with your MQTT port (from ngrok or local)

// Create object for MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// MQTT callback function when a message arrives on subscribed topic
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // Add logic to handle the message and execute commands here
}

// Function to setup Wi-Fi with WiFiManager and retry connection if necessary
void setupWiFi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("BlueLink Setup");

  // After successful connection, display the IP address
  Serial.println("Wi-Fi configuration complete.");
  Serial.println("Attempting to connect...");

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 10) {  // Retry up to 5 times
    Serial.print("Attempting to connect to Wi-Fi...");
    delay(5000);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wi-Fi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to Wi-Fi after multiple attempts.");
  }
}

// Function to connect to MQTT server
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");

    // Try connecting to the MQTT broker
    if (client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT broker!");
      client.subscribe("test/topic");  // Subscribe to the topic
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
