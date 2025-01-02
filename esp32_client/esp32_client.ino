#include <WiFi.h>
#include <WebServer.h>  // Include the WebServer library

// Wi-Fi credentials
const char* ssid = "motorola";
const char* password = "Hello@123";

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

// Function to ensure Wi-Fi stays connected
void ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected. Attempting to reconnect...");
    WiFi.begin(ssid, password);
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 10) {  // Retry up to 10 times
      delay(1000);
      Serial.print(".");
      attempt++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to Wi-Fi!");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to reconnect to Wi-Fi.");
    }
  }
}

void setup() {
  Serial.begin(115200);  // Start Serial communication with Arduino
  delay(1000);

  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

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

  // Ensure Wi-Fi connection stays active
  ensureWiFiConnected();
}
