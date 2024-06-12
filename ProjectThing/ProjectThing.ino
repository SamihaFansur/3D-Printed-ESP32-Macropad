#include <WiFi.h>
#include <WebServer.h>
#include <BleKeyboard.h>
#include <NewPing.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


const char* ssid = "ESP32-Keyboard-AP";
const char* password = "12345678";

WebServer server(80);
BleKeyboard bleKeyboard("ESP32 Keyboard", "ESP32", 100);

const int ROWS = 2;
const int COLS = 3;

int rowPins[ROWS] = {12, 13};
int colPins[COLS] = {5, 9, 10};

char keys[ROWS][COLS] = {
  {'A', 'B', 'C'},
  {'1', '2', '3'}
};

// Ultrasonic sensor pins
#define TRIGGER_PIN  A0 
#define ECHO_PIN     A2
#define MAX_DISTANCE 200 // Set maximum distance to 200 cm (2 meters)

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
int currentVolume = 40; // Default volume

const int motorPin = 11; // vibration motor control pin
int intensity = 250;
bool shouldVibrate = false; // Flag to control motor vibration


#define WIRE Wire
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &WIRE);


void setup() {
  Serial.begin(115200);
  bleKeyboard.begin();
  
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  
  for (int i = 0; i < ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }
  for (int j = 0; j < COLS; j++) {
    pinMode(colPins[j], INPUT_PULLUP);
  }

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);
  
  server.begin();
  Serial.println("Server started");

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address
  
  // Display initialization
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0,0);      // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  server.on("/", HTTP_GET, []() {
    // Print a welcome message to the display
    display.clearDisplay();
    display.println("Hello Samiha :)");
    display.display();
    server.send(200, "text/html", getHtmlPage());
  
  });

  server.on("/updateKeys", HTTP_POST, []() {
    bool volumeChanged = false; // Track if volume changes
    
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            // Generate the key name based on the grid position
            String keyName = "key" + String(i * COLS + j + 1);
    
            // Check if the argument exists and has exactly 1 character
            if (server.hasArg(keyName) && server.arg(keyName).length() == 1) {
                // Assign the first character of the argument to the appropriate place in the array
                keys[i][j] = server.arg(keyName).charAt(0);
            }
        }
    }
    
    if (server.hasArg("volume")) {
      int newVolume = server.arg("volume").toInt();
      if (newVolume != currentVolume) {
        currentVolume = newVolume;
        volumeChanged = true;
      }
    }

    if (volumeChanged) {
      shouldVibrate = true;
    }

    String message = "<p id='updateMsg'>Keys successfully updated!</p>"
                     "<script>"
                     "setTimeout(function() {"
                     "  document.getElementById('updateMsg').style.display = 'none';"
                     "}, 3000);"
                     "</script>";

    display.clearDisplay();
    display.setCursor(0,0); 
    display.println("Keys Updated!");
    display.display();
    
    server.send(200, "text/html", getHtmlPage() + message);
  });



  server.on("/getVolume", HTTP_GET, []() {
    int volume = readUltrasonicDistance();
    if (volume != currentVolume && volume != 0) { // Update current volume only if different and not zero
      currentVolume = volume;
      shouldVibrate = true;
      display.clearDisplay();
      display.setCursor(0,0); 
      display.println("Volume Set!");
      display.display();
//      vibrateMotor(); // Vibrate motor when volume is set
    }
    server.send(200, "text/plain", String(currentVolume));
  });

}

void loop() {
  server.handleClient();
  scanKeys();
  if (shouldVibrate) {
    vibrateMotor();
    shouldVibrate = false;
  }
}


String getHtmlPage() {
  int volumeLevel = readUltrasonicDistance(); //to set volume based on distance from sensor
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
                "<title>ESP32 MACROPAD</title>"
                "<style>"
                "body { font-family: Arial, sans-serif; background-color: #f4f4f8; display: flex; flex-direction: column; align-items: center; margin: 0; padding: 20px; }"
                "h2 { color: #333; margin: 20px; text-align: center; width: 100%; }"
                "h3 { color: #555; margin: 10px 0; }"
                "form { background-color: #8a8aff; padding: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.2); color: white; width: auto; }"
                ".keypad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; justify-content: center; padding: 10px; }"
                "input[type='text'] { width: 70px; padding: 10px; margin: 5px; border: 2px solid #ddd; border-radius: 5px; text-align: center; background-color: #fff; "
                "box-shadow: 3px 3px 10px rgba(0, 0, 0, 0.2), inset 1px 1px 2px rgba(255, 255, 255, 0.8), inset -1px -1px 2px rgba(0, 0, 0, 0.2); }"
                "input[type='text'] { width: 70px; padding: 10px; margin: 5px; border: 2px solid #ddd; border-radius: 5px; text-align: center; }"
                "input[type='range'] {"
                " width: 100%;"
                " margin: 10px 0;"
                " -webkit-appearance: none;"
                " appearance: none;"
                " height: 8px;"
                " background: #ddd;"  // track background
                " border-radius: 5px;"
                " outline: none;"
                "}"
                "input[type='range']::-webkit-slider-thumb {"
                " -webkit-appearance: none;"
                " appearance: none;"
                " width: 20px;"
                " height: 20px;"
                " background: #3d3d61;"
                " border-radius: 50%;"
                " cursor: pointer;"
                "}"
                "input[type='range']::-moz-range-thumb {"
                " width: 20px;"
                " height: 20px;"
                " background: #3d3d61;"
                " border-radius: 50%;"
                " cursor: pointer;"
                "}"
                "input[type='submit'] { width: 100%; padding: 10px; background-color: #7ab6fa; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 10px; }"
                "input[type='submit']:hover { background-color: #1b5796; }"  // A darker shade for hover
                "</style>"
                "</head><body>"
                "<h2>MACROPAD WITH VOLUME CONTROL</h2>"
                "<form method='POST' action='/updateKeys'>"
                "<h3>Keys</h3>"
                "<div class='keypad'>";

  


  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      char keyVal = keys[i][j];
      String keyName = "key" + String(i * COLS + j + 1);  // Generate keyName based on grid position.
      html += "<input type='text' id='" + keyName + "' name='" + keyName + "' value='" + String(keyVal) + "'>";
    }
  }
  

  html += "<br><input type='submit' value='Update'>"
          "</div><br><h3>Volume Level</h3><br>"
          "<span id='volumeLabel'>" + String(volumeLevel) + "%</span><br>"
          "<input type='range' id='volume' name='volume' min='0' max='100' value='" + String(volumeLevel) + "'>"
          "</form>"
          "<script>"
          "function updateSlider() {"
          "fetch('/getVolume')"
          ".then(response => response.text())"
          ".then(volume => {"
          "document.getElementById('volume').value = volume;"
          "document.getElementById('volumeLabel').innerText = volume + '%';"
          "});"
          "}"
          "setInterval(updateSlider, 1000);"
          "</script></body></html>";
  return html;
}


void vibrateMotor() {
  Serial.println("Turning high");
  analogWrite(motorPin, intensity); // Set the motor vibration intensity
//  digitalWrite(motorPin, HIGH); // Turn on the motor
  Serial.println("Delay");
  delay(2000); // Motor runs for 2 secs
  Serial.println("Turning low");
//  digitalWrite(motorPin, LOW); // Turn off the motor
  analogWrite(motorPin, 0); // Set the motor vibration intensity
  Serial.println("low");
}

void scanKeys() {
  for (int row = 0; row < ROWS; row++) {
    digitalWrite(rowPins[row], LOW);
    for (int col = 0; col < COLS; col++) {
      if (digitalRead(colPins[col]) == LOW) {
        
        display.clearDisplay();
        display.setCursor(0,0); 
        display.println(keys[row][col]);
        display.display();
    
        Serial.println(keys[row][col]);
        if (bleKeyboard.isConnected()) {
          bleKeyboard.print(keys[row][col]);
        }
        while (digitalRead(colPins[col]) == LOW); // Wait for key release
        delay(200); // Debounce delay
      }
    }
      digitalWrite(rowPins[row], HIGH);
  }
}

int readUltrasonicDistance() {
  long distance = sonar.ping_cm();
  Serial.println(distance);
  if (distance <= 10) {
    return map(distance, 0, 10, 0, 50) * 2;
  } else {
    return 0; // Set volume to 0% if the distance is greater than 10 cm
  }
}