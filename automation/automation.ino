#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// WiFi credentials
const char* ssid = "Free Public Wi-Fi";
const char* password = "2A0R0M4AAN";

// Web server
ESP8266WebServer server(80);

// Relay pins (GPIO numbers)
const int relay1 = 5; // D1
const int relay2 = 4; // D2
const int relay3 = 0; // D3
const int relay4 = 2; // D4

bool relay1State = false;
bool relay2State = false;
bool relay3State = false;
bool relay4State = false;

// HTML page with buttons
const char* html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        .button { 
            padding: 15px 25px;
            margin: 10px;
            font-size: 16px;
            cursor: pointer;
            display: block;
            width: 200px;
        }
        .on { background-color: #4CAF50; }
        .off { background-color: #f44336; }
    </style>
</head>
<body>
    <button class="button" onclick="toggleRelay(1)" id="btn1">Relay 1</button>
    <button class="button" onclick="toggleRelay(2)" id="btn2">Relay 2</button>
    <button class="button" onclick="toggleRelay(3)" id="btn3">Relay 3</button>
    <button class="button" onclick="toggleRelay(4)" id="btn4">Relay 4</button>
    <script>
        function toggleRelay(relay) {
            fetch('/relay/' + relay)
                .then(response => response.json())
                .then(data => {
                    let btn = document.getElementById('btn' + relay);
                    btn.className = 'button ' + (data.state ? 'on' : 'off');
                });
        }
    </script>
</body>
</html>
)html";

void setup() {
  Serial.begin(115200);

  // Set relay pins as outputs
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  // Initialize relays to off (assuming active-low relays)
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/relay/1", HTTP_GET, handleRelay1);
  server.on("/relay/2", HTTP_GET, handleRelay2);
  server.on("/relay/3", HTTP_GET, handleRelay3);
  server.on("/relay/4", HTTP_GET, handleRelay4);
  
  server.begin();
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  server.send(200, "text/html", html);
}

void toggleRelay(int relayPin, bool &relayState) {
  relayState = !relayState;
  digitalWrite(relayPin, relayState ? LOW : HIGH); // Assuming active-low relays
}

void handleRelay1() {
  toggleRelay(relay1, relay1State);
  server.send(200, "application/json", "{\"state\":" + String(relay1State) + "}");
}

void handleRelay2() {
  toggleRelay(relay2, relay2State);
  server.send(200, "application/json", "{\"state\":" + String(relay2State) + "}");
}

void handleRelay3() {
  toggleRelay(relay3, relay3State);
  server.send(200, "application/json", "{\"state\":" + String(relay3State) + "}");
}

void handleRelay4() {
  toggleRelay(relay4, relay4State);
  server.send(200, "application/json", "{\"state\":" + String(relay4State) + "}");
}