#define MAX_CHANNELS 12

struct Channel {
  int pin;
  bool isActive;  // active-low relay module
};

// Array to hold channel configurations
Channel channels[MAX_CHANNELS];
// currently active channel (configured) (state -> HIGH/LOW)
int configuredChannelsCount = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Arduino is ready and waiting for commands...");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    // <COMMAND>:<CHANNEL_PIN>:<VALUE>\n
    command.trim();  // Remove any leading/trailing whitespace or newline characters
    // <COMMAND>:<CHANNEL_PIN>:<VALUE>
    processCommand(command);
  }
}

void processCommand(String command) {
  command.trim();
  // <COMMAND>:<CHANNEL_PIN>:<VALUE>
  if (command.startsWith("ADD:")) {
    int pin = command.substring(4).toInt();
    addChannel(pin);
  } else if (command.startsWith("REMOVE:")) {
    int pin = command.substring(7).toInt();
    removeChannel(pin);
  } else if (command.startsWith("SET:")) {
    int sep1 = command.indexOf(':');
    int sep2 = command.indexOf(':', sep1 + 1);
    int pin = command.substring(sep1 + 1, sep2).toInt();
    String value = command.substring(sep2 + 1);
    setChannel(pin, value);
  } else if (command == "LIST") {
    listChannels();
  } else {
    Serial.println("Unknown command.");
  }
}

void addChannel(int pin) {
  // if max channels added
  if (configuredChannelsCount >= MAX_CHANNELS) {
    Serial.println("Max channels reached.");
    return;
  }

  // if tha pin has already assigned to same channel
  for (int i = 0; i < configuredChannelsCount; i++) {
    if (channels[i].pin == pin) {
      Serial.println("Channel already added.");
      return;
    }
  }

  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);  // (deactivate) active-low
  channels[configuredChannelsCount++] = { pin, false };
  Serial.print("Channel added on pin ");
  Serial.println(pin);
}

void removeChannel(int pin) {
  for (int i = 0; i < configuredChannelsCount; i++) {
    if (channels[i].pin == pin) {
      // deactivate the channel before removing
      digitalWrite(pin, HIGH);  // (deactivate) active-low
      for (int j = i; j < configuredChannelsCount - 1; j++) {
        channels[j] = channels[j + 1];
      }
      configuredChannelsCount--;
      Serial.print("Channel removed from pin ");
      Serial.println(pin);
      return;
    }
  }
  Serial.println("Channel not found.");
}

void setChannel(int pin, String state) {
  for (int i = 0; i < configuredChannelsCount; i++) {
    if (channels[i].pin == pin) {
      if (state == "ON") {
        digitalWrite(pin, LOW);  // active-low relay
        channels[i].isActive = true;
        Serial.print("Channel ON at pin ");
        Serial.println(pin);
      } else if (state == "OFF") {
        digitalWrite(pin, HIGH);
        channels[i].isActive = false;
        Serial.print("Channel OFF at pin ");
        Serial.println(pin);
      } else {
        Serial.println("Invalid state.");
      }
      return;
    }
  }
  Serial.println("Channel not found.");
}

void listChannels() {
  if (!configuredChannelsCount) {
    Serial.println("No configured channels found.");
    return;
  }

  Serial.println("Configured channels:");
  for (int i = 0; i < configuredChannelsCount; i++) {
    Serial.print("Pin ");
    Serial.print(channels[i].pin);
    Serial.print(" - State: ");
    Serial.println(channels[i].isActive == 1 ? "ON" : "OFF");
  }
}
