#include <WiFi.h>
#include <WiFiManager.h>   // Include WiFiManager library
#include <PubSubClient.h>  // Include the MQTT library

// MQTT Broker details
const char* mqtt_server = "0.tcp.in.ngrok.io";  // Replace with your MQTT broker address (ngrok or local)
const int mqtt_port = 15006;                    // Replace with your MQTT port (from ngrok or local)

// Create object for MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Get the ESP32's MAC address
String uniqueId = String(WiFi.macAddress());  // Use the MAC address as unique ID

// MQTT topic dynamically created with the unique ID
String commandTopic = "esp32/device/" + uniqueId + "/commands";  // Topic to receive commands
String sensorTopic = "esp32/device/" + uniqueId + "/sensor";     // Topic to send sensor data

// Dummy sensor data
int dummyTemperature = 25;  // Dummy temperature in Celsius
int relayStatus = 0;        // Dummy relay status (0 for OFF, 1 for ON)

// MQTT callback function when a message arrives on the subscribed topic
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");

  // Print the received message
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Example: Handle the command (turn relay on or off)
  String message = String((char*)payload);
  if (message == "SET:ON") {
    relayStatus = 1;  // Turn relay on
    Serial.println("Relay ON");
  } else if (message == "SET:OFF") {
    relayStatus = 0;  // Turn relay off
    Serial.println("Relay OFF");
  }
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
      client.subscribe(commandTopic.c_str());  // Subscribe to the dynamic command topic
    } else {
      Serial.print("Failed to connect, retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// Function to publish dummy sensor data
void publishSensorData() {
  // Publish dummy temperature data
  String payload = String(dummyTemperature);             // Convert temperature to string
  client.publish(sensorTopic.c_str(), payload.c_str());  // Publish to sensor topic

  // For demonstration, print the published sensor data to Serial
  Serial.print("Published sensor data: ");
  Serial.println(payload);

  // Optionally, update dummy data (for real sensors, this would come from the actual sensors)
  dummyTemperature++;  // Increment temperature (dummy example)
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

  // Publish dummy sensor data every 5 seconds
  publishSensorData();
  delay(5000);  // 5-second delay between sensor data publications
}
