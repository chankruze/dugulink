#include <WiFi.h>
#include <WebServer.h>    // Include the WebServer library
#include <WiFiManager.h>  // Include WiFiManager library

// Create a web server object
WebServer server(80);

// Function to handle root (landing page)
void handleRoot() {
  String html = "<html>"
                "<head><title>BlueLink</title></head>"
                "<body>"
                "<p>Use the following commands to control your devices:</p>"
                "<ul>"
                "<li><b>ADD:&lt;pin&gt;</b>: Add a channel connected to the specified pin</li>"
                "<li><b>REMOVE:&lt;pin&gt;</b>: Remove a channel on the specified pin</li>"
                "<li><b>SET:&lt;pin&gt;:ON</b>: Turn ON the channel on the specified pin</li>"
                "<li><b>SET:&lt;pin&gt;:OFF</b>: Turn OFF the channel on the specified pin</li>"
                "<li><b>LIST</b>: Request a list of active channels</li>"
                "</ul>"
                "</body>"
                "</html>";
  server.send(200, "text/html", html);
}

// Function to handle POST commands
void handlePostCommand() {
  if (server.hasArg("plain")) {  // Check if there is a body in the POST request
    String command = server.arg("plain");
    Serial.println(command);  // Send the command to the Arduino via Serial

    // Respond back to the client
    server.send(200, "text/plain", "Command received: " + command);
  } else {
    server.send(400, "text/plain", "Bad Request: No command found in body");
  }
}

// Function to setup Wi-Fi with WiFiManager and retry connection if necessary
void setupWiFi() {
  WiFiManager wifiManager;

  // Set up the config portal
  wifiManager.autoConnect("BlueLink Setup");

  // After successful connection, display the IP address
  Serial.println("Wi-Fi configuration complete.");
  Serial.println("Attempting to connect...");

  // Retry connection if the ESP32 is not connected after WiFiManager setup
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

void setup() {
  Serial.begin(115200);  // Start Serial communication with Arduino
  delay(1000);

  // Setup Wi-Fi connection using WiFiManager and retry if necessary
  Serial.println("Connecting to Wi-Fi...");
  setupWiFi();

  // Define routes
  server.on("/", HTTP_GET, handleRoot);                 // Root landing page
  server.on("/command", HTTP_POST, handlePostCommand);  // POST command handler

  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle incoming HTTP requests
  server.handleClient();
}
