#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <vector>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <string>
#include <Ticker.h>

struct Schedule {
    int id;
    int relayNumber;
    int onHour;
    int onMinute;
    int offHour;
    int offMinute;
    bool enabled;
};

const int relay1 = 5;  // D1
const int relay2 = 4;  // D2
const int switch1Pin = 14; // D5
const int switch2Pin = 12; // D6
const int errorLEDPin = 13; // D7

bool overrideRelay1 = false;
bool overrideRelay2 = false;
bool relay1State = false;
bool relay2State = false;
bool relay3State = false;
bool relay4State = false;

const char* ssid = "Free Public Wi-Fi";
const char* password = "2A0R0M4AAN";
std::vector<String> logBuffer;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
Ticker watchdogTicker;
unsigned long lastLoopTime = 0;
const unsigned long watchdogTimeout = 5000;
unsigned long lastTimeUpdate = 0;
const long timeUpdateInterval = 1000;
unsigned long epochTime = 0;
unsigned long lastNTPSync = 0;
unsigned long lastScheduleCheck = 0;
unsigned long lastSecond = 0;
bool validTimeSync = false;
bool hasError = false;
bool hasLaunchedSchedules = false;
std::vector<Schedule> schedules;
void handleAddSchedule();
void handleDeleteSchedule();
void handleClearError();
void handleGetErrorStatus();
const int EEPROM_SIZE = 512;
const int SCHEDULE_SIZE = sizeof(Schedule);
const int MAX_SCHEDULES = 10;
const int SCHEDULE_START_ADDR = 0;
const std::vector<String> allowedIPs = {
    "192.168.29.3",
    "192.168.29.5",
    "192.168.29.6",
    "192.168.29.9",
    "192.168.29.10"
};
const char* authUsername = "admin";
const char* authPassword = "12345678";
ESP8266WebServer server(80);

WebSocketsServer webSocket = WebSocketsServer(81);

void logMessage(const String &message) {
    Serial.println(message);
    logBuffer.push_back(message);
    if (logBuffer.size() > 100) {
        logBuffer.erase(logBuffer.begin());
    }
}

void handleGetLogs() {
    StaticJsonDocument<1024> doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& log : logBuffer) {
        array.add(log);
    }
    
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

void resetWatchdog() {
    lastLoopTime = millis();
}

void checkWatchdog() {
    if (millis() - lastLoopTime > watchdogTimeout) {
        ESP.restart();
    }
}

void indicateError() {
    logMessage("Error triggered.");
    digitalWrite(errorLEDPin, HIGH);
    hasError = true;
}

void clearError() {
    logMessage("Error cleared.");
    digitalWrite(errorLEDPin, LOW);
    hasError = false;
}

void saveSchedulesToEEPROM() {
    int addr = SCHEDULE_START_ADDR;
    EEPROM.write(addr, schedules.size());
    addr++;
    
    for(const Schedule& schedule : schedules) {
        EEPROM.put(addr, schedule);
        addr += SCHEDULE_SIZE;
    }
    EEPROM.commit();
}

void loadSchedulesFromEEPROM() {
    schedules.clear();
    int addr = SCHEDULE_START_ADDR;
    int count = EEPROM.read(addr);
    addr++;
    
    for(int i = 0; i < count && i < MAX_SCHEDULES; i++) {
        Schedule schedule;
        EEPROM.get(addr, schedule);
        schedules.push_back(schedule);
        addr += SCHEDULE_SIZE;
    }
}

void IRAM_ATTR setup() {
    pinMode(relay1, OUTPUT);
    pinMode(relay2, OUTPUT);
    pinMode(switch1Pin, INPUT_PULLUP);
    pinMode(switch2Pin, INPUT_PULLUP);
    
    pinMode(errorLEDPin, OUTPUT);
    digitalWrite(errorLEDPin, LOW);
    
    digitalWrite(relay1, HIGH);
    digitalWrite(relay2, HIGH);
    
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    unsigned long wifiStartTime = millis();
    const unsigned long wifiTimeout = 20000;
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - wifiStartTime > wifiTimeout) {
            logMessage("WiFi connection failed.");
            indicateError();
            break;
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        logMessage("Connected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        clearError();
    }
    
    timeClient.begin();
    timeClient.setTimeOffset(19800);
    
    if (timeClient.update()) {
        epochTime = timeClient.getEpochTime();
        lastNTPSync = millis();
        validTimeSync = true;
        logMessage("Time sync successful");
        clearError();
    } else {
        logMessage("Time sync failed.");
        indicateError();
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/logs", HTTP_GET, handleGetLogs);
    server.on("/relay/1", HTTP_ANY, handleRelay1);
    server.on("/relay/2", HTTP_ANY, handleRelay2);
    server.on("/time", HTTP_GET, handleTime);
    server.on("/schedules", HTTP_GET, handleGetSchedules);
    server.on("/schedule/add", HTTP_POST, handleAddSchedule);
    server.on("/schedule/delete", HTTP_DELETE, handleDeleteSchedule);
    server.on("/relay/status", HTTP_GET, handleRelayStatus);
    server.on("/error/clear", HTTP_POST, handleClearError);
    server.on("/error/status", HTTP_GET, handleGetErrorStatus);
    server.begin();
    EEPROM.begin(EEPROM_SIZE);
    loadSchedulesFromEEPROM();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    resetWatchdog();
    watchdogTicker.attach(1, checkWatchdog);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("WebSocket[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("WebSocket[%u] Connected from %d.%d.%d.%d url: %s\n", 
                          num, ip[0], ip[1], ip[2], ip[3], payload);
            
            String message = "{\"relay1\":" + String(relay1State || overrideRelay1) +
                             ",\"relay2\":" + String(relay2State || overrideRelay2) + "}";
            webSocket.sendTXT(num, message);
            }
            break;
        default:
            break;
    }
}

bool checkAuthentication() {
    String clientIP = server.client().remoteIP().toString();
    for (const auto& ip : allowedIPs) {
        if (clientIP == ip) {
            return true;
        }
    }
    if (!server.authenticate(authUsername, authPassword)) {
        server.requestAuthentication();
        return false;
    }
    return true;
}

const char* html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            margin: 0;
            padding: 0;
            font-family: Arial, sans-serif;
            background-color: #f4f4f4;
            font-size: 18px; 
        }
        #time {
            font-size: 36px; 
            margin: 20px;
            font-family: monospace;
            color: #333;
            text-align: center;
        }
        .relay-buttons {
            display: flex;
            justify-content: center;
            align-items: center;
            flex-wrap: wrap;
            width: 100%;
            margin: 20px auto;
        }
        .button {
            padding: 15px 25px;
            color: #fff;
            background-color: #008CBA;
            border: none;
            border-radius: 4px;
            font-size: 18px;
            cursor: pointer;
            min-width: 150px;
            text-align: center;
            text-decoration: none;
            margin: 5px;
        }
        .on {
            background-color: #4CAF50;
        }
        .off {
            background-color: #f44336;
        }
        .schedule-form {
            margin: 20px auto;
            padding: 20px;
            max-width: 400px;
            background-color: #fff;
            border: 1px solid #ccc;
            border-radius: 4px;
        }
        .schedule-table {
            margin: 20px auto;
            width: 90%;
            border-collapse: collapse;
            background-color: #fff;
        }
        .schedule-table th {
            background-color: #f2f2f2;
            padding: 10px;
            text-align: left;
        }
        .schedule-table td {
            border: 1px solid #ddd;
            padding: 8px;
        }
        #errorSection {
            text-align: center;
            margin: 20px;
            color: red;
            display: none;
        }
        #clearErrorBtn {
            padding: 10px 20px;
            background-color: #f44336;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
        }
        #logSection {
            margin: 20px;
            padding: 10px;
            background-color: #eee;
            border: 1px solid #ccc;
            max-height: 300px;
            overflow-y: scroll;
        }
        @media screen and (max-width: 600px) {
            body {
                font-size: 16px; 
            }
            .button {
                width: 100%;
                margin: 5px 0;
            }
        }
    </style>
</head>
<body>
    <div id="time">Loading time...</div>
    <button class="button" onclick="toggleRelay(1)" id="btn1">Relay 1</button>
    <button class="button" onclick="toggleRelay(2)" id="btn2">Relay 2</button>
    <button class="button" onclick="showLogs()">Show Logs</button>
    
    <div class="schedule-form">
        <h3>Add Schedule</h3>
        <select id="relaySelect">
            <option value="1">Relay 1</option>
            <option value="2">Relay 2</option>
        </select>
        <input type="time" id="onTime">
        <input type="time" id="offTime">
        <button onclick="addSchedule()">Add Schedule</button>
    </div>

    <div id="errorSection">
        <p>Error detected!</p>
        <button id="clearErrorBtn" onclick="clearError()">Clear Error</button>
    </div>

    <table class="schedule-table" id="scheduleTable">
        <tr>
            <th>Relay</th>
            <th>On Time</th>
            <th>Off Time</th>
            <th>Status</th>
            <th>Action</th>
        </tr>
    </table>

      <div id="logSection" style="display:none;">
        <h3>Logs</h3>
        <pre id="logs"></pre>
    </div>

    <script>
        let relayStates = {
            1: false,
            2: false
        };

        // Establish WebSocket connection
        let socket = new WebSocket('ws://' + window.location.hostname + ':81/');

        socket.onopen = function() {
            console.log('WebSocket connection established');
        };

        socket.onmessage = function(event) {
            try {
                let data = JSON.parse(event.data);
                if (data.relay1 !== undefined) {
                    relayStates[1] = data.relay1;
                    updateButtonStyle(1);
                }
                if (data.relay2 !== undefined) {
                    relayStates[2] = data.relay2;
                    updateButtonStyle(2);
                }
            } catch (e) {
                console.error('Error parsing WebSocket message:', e);
            }
        };

        socket.onclose = function() {
            console.log('WebSocket connection closed');
            checkErrorStatus();
        };

        socket.onerror = function(error) {
            console.error('WebSocket error:', error);
            checkErrorStatus();
        };

        function updateTime() {
            fetch('/time')
                .then(response => response.text())
                .then(time => {
                    document.getElementById('time').innerHTML = time;
                });
        }

        function toggleRelay(relay) {
            fetch('/relay/' + relay, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                }
            })
            .then(response => {
                if (!response.ok) {
                    if (response.status === 403) {
                        return response.json().then(data => { throw new Error(data.error); });
                    }
                    throw new Error('Network response was not ok');
                }
                return response.json();
            })
            .then(data => {
                console.log('Relay ' + relay + ' state:', data.state);
                relayStates[relay] = data.state;
                updateButtonStyle(relay);
            })
            .catch(error => {
                console.error('Error:', error);
                alert(error.message);
                checkErrorStatus();
            });
        }

        function addSchedule() {
            const relay = document.getElementById('relaySelect').value;
            const onTime = document.getElementById('onTime').value;
            const offTime = document.getElementById('offTime').value;
            
            fetch('/schedule/add', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    relay: relay,
                    onTime: onTime,
                    offTime: offTime
                })
            }).then(response => {
                if (!response.ok) {
                    throw new Error('Failed to add schedule');
                }
                return response.json();
            }).then(data => {
                loadSchedules();
                checkErrorStatus();
            }).catch(error => {
                console.error('Error:', error);
                alert('Failed to add schedule: ' + error.message);
                checkErrorStatus();
            });
        }

        function deleteSchedule(id) {
          fetch('/schedule/delete?id=' + id, {
              method: 'DELETE',
              headers: {
                  'Content-Type': 'application/json'
              }
          })
          .then(response => {
              if (!response.ok) {
                  throw new Error('Failed to delete schedule');
              }
              return response.json();
          })
          .then(data => {
              if (data.status === 'success') {
                  loadSchedules();
                  checkErrorStatus();
              } else {
                  throw new Error('Server returned error');
              }
          })
          .catch(error => {
              console.error('Error:', error);
              alert('Failed to delete schedule: ' + error.message);
              checkErrorStatus();
          });
      }

        function loadSchedules() {
            fetch('/schedules')
                .then(response => response.json())
                .then(schedules => {
                    const table = document.getElementById('scheduleTable');
                    while (table.rows.length > 1) {
                        table.deleteRow(1);
                    }
                    
                    schedules.forEach((schedule, index) => {
                        const row = table.insertRow();
                        row.insertCell(0).textContent = `Relay ${schedule.relay}`;
                        row.insertCell(1).textContent = `${schedule.onHour}:${schedule.onMinute}`;
                        row.insertCell(2).textContent = `${schedule.offHour}:${schedule.offMinute}`;
                        row.insertCell(3).textContent = schedule.enabled ? 'Active' : 'Inactive';
                        const deleteBtn = document.createElement('button');
                        deleteBtn.textContent = 'Delete';
                        deleteBtn.onclick = () => deleteSchedule(index);
                        row.insertCell(4).appendChild(deleteBtn);
                    });
                }).catch(error => {
                    console.error('Error loading schedules:', error);
                    checkErrorStatus();
                });
        }
        function updateButtonStyle(relay) {
            const btn = document.getElementById('btn' + relay);
            if (btn) {
                btn.className = 'button ' + (relayStates[relay] ? 'on' : 'off');
                btn.textContent = 'Relay ' + relay + (relayStates[relay] ? ' (ON)' : ' (OFF)');
            }
        }

        function getInitialStates() {
            fetch('/relay/status')
                .then(response => response.json())
                .then(data => {
                    relayStates = data;
                    for(let relay = 1; relay <= 4; relay++) {
                        updateButtonStyle(relay);
                    }
                }).catch(error => {
                    console.error('Error getting relay status:', error);
                    checkErrorStatus();
                });
        }

        function checkErrorStatus() {
            fetch('/error/status')
                .then(response => response.json())
                .then(data => {
                    if (data.hasError) {
                        document.getElementById('errorSection').style.display = 'block';
                    } else {
                        document.getElementById('errorSection').style.display = 'none';
                    }
                })
                .catch(error => {
                    console.error('Error checking error status:', error);
                    document.getElementById('errorSection').style.display = 'block';
                });
        }

        function clearError() {
            fetch('/error/clear', {
                method: 'POST'
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('Failed to clear error');
                }
                return response.json();
            })
            .then(data => {
                if (data.status === 'success') {
                    document.getElementById('errorSection').style.display = 'none';
                } else {
                    throw new Error('Server returned error');
                }
            })
            .catch(error => {
                console.error('Error clearing error:', error);
                alert('Failed to clear error: ' + error.message);
            });
        }

        function showLogs() {
            fetch('/logs')
                .then(response => response.json())
                .then(data => {
                    const logSection = document.getElementById('logSection');
                    const logs = document.getElementById('logs');
                    logs.textContent = data.join('\n');
                    logSection.style.display = 'block';
                })
                .catch(error => {
                    console.error('Error fetching logs:', error);
                    alert('Failed to load logs.');
                });
        }

        setInterval(updateTime, 1000);
        setInterval(checkErrorStatus, 2000);
        updateTime();
        loadSchedules();
        getInitialStates();
        checkErrorStatus();
    </script>
</body>
</html>
)html";

void loop() {
    server.handleClient();
    webSocket.loop();
    resetWatchdog();

    if (digitalRead(switch1Pin) == LOW) {
        if (!overrideRelay1) {
            overrideRelay1 = true;
            activateRelay(1, true);
        }
    } else {
        if (overrideRelay1) {
            overrideRelay1 = false;
            deactivateRelay(1, true);
        }
    }

    if (digitalRead(switch2Pin) == LOW) {
        if (!overrideRelay2) {
            overrideRelay2 = true;
            activateRelay(2, true);
        }
    } else {
        if (overrideRelay2) {
            overrideRelay2 = false;
            deactivateRelay(2, true);
        }
    }

    if (!validTimeSync) {
        if (timeClient.update()) {
            epochTime = timeClient.getEpochTime();
            lastNTPSync = millis();
            validTimeSync = true;
            logMessage("Time sync successful (retry)");
            clearError();
        } else {
            logMessage("Time sync failed (retry).");
            indicateError();
        }
    }

    unsigned long currentMillis = millis();
    if (currentMillis - lastTimeUpdate >= 1000) {
        epochTime++;
        lastTimeUpdate = currentMillis;

        if (validTimeSync) {
            checkSchedules();
        }
    }

    if (!hasLaunchedSchedules && validTimeSync) {
        checkScheduleslaunch();
        logMessage("Startup Schedule Check Success");
        hasLaunchedSchedules = true;
    }

    yield();
}

void checkSchedules() {
    unsigned long hours = ((epochTime % 86400L) / 3600);
    unsigned long minutes = ((epochTime % 3600) / 60);
    unsigned long seconds = (epochTime % 60);
    
    for (const Schedule& schedule : schedules) {
        if (!schedule.enabled) continue;
        
        if (hours == schedule.onHour && minutes == schedule.onMinute && seconds == 0) {
            activateRelay(schedule.relayNumber, false);
        }
        else if (hours == schedule.offHour && minutes == schedule.offMinute && seconds == 0) {
            deactivateRelay(schedule.relayNumber, false);
        }
    }
}

void checkScheduleslaunch() {
    unsigned long hours = ((epochTime % 86400L) / 3600);
    unsigned long minutes = ((epochTime % 3600) / 60);
    unsigned long currentTime = hours * 60 + minutes;
    unsigned long seconds = (epochTime % 60);

    for (Schedule& schedule : schedules) {
        if (!schedule.enabled) continue;
        unsigned long onMinutes = schedule.onHour * 60 + schedule.onMinute;
        unsigned long offMinutes = schedule.offHour * 60 + schedule.offMinute;

        if (hours == schedule.onHour && minutes == schedule.onMinute && seconds == 0) {
            activateRelay(schedule.relayNumber, false);
        }
        else if (hours == schedule.offHour && minutes == schedule.offMinute && seconds == 0) {
            deactivateRelay(schedule.relayNumber, false);
        }

        if (offMinutes > onMinutes) {
            if (currentTime >= onMinutes && currentTime < offMinutes) {
                activateRelay(schedule.relayNumber, false);
            } else {
                deactivateRelay(schedule.relayNumber, false);
            }
        } else {
            if (currentTime >= onMinutes || currentTime < offMinutes) {
                activateRelay(schedule.relayNumber, false);
            } else {
                deactivateRelay(schedule.relayNumber, false);
            }
        }
    }
}

void activateRelay(int relayNum, bool manual) {
    if (!manual && ((relayNum == 1 && overrideRelay1) || (relayNum == 2 && overrideRelay2))) {
        Serial.printf("Relay %d is overridden. Activation skipped.\n", relayNum);
        return;
    }
    
    switch(relayNum) {
        case 1: 
            digitalWrite(relay1, LOW); 
            relay1State = true; 
            logMessage("Relay 1 activated.");
            break;
        case 2: 
            digitalWrite(relay2, LOW); 
            relay2State = true; 
            logMessage("Relay 2 activated.");
            break;
    }
    broadcastRelayStates();
}

void deactivateRelay(int relayNum, bool manual) {
    if (!manual && ((relayNum == 1 && overrideRelay1) || (relayNum == 2 && overrideRelay2))) {
        Serial.printf("Relay %d is overridden. Deactivation skipped.\n", relayNum);
        return;
    }
    
    switch(relayNum) {
        case 1: 
            digitalWrite(relay1, HIGH); 
            relay1State = false; 
            logMessage("Relay 1 deactivated.");
            break;
        case 2: 
            digitalWrite(relay2, HIGH); 
            relay2State = false; 
            logMessage("Relay 2 deactivated.");
            break;
    }
    broadcastRelayStates();
}

void broadcastRelayStates() {
    String message = "{\"relay1\":" + String(relay1State || overrideRelay1) +
                     ",\"relay2\":" + String(relay2State || overrideRelay2) + "}";
    webSocket.broadcastTXT(message);
}

void handleGetSchedules() {
    String json = "[";
    for (size_t i = 0; i < schedules.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"id\":" + String(i) + ",";
        json += "\"relay\":" + String(schedules[i].relayNumber) + ",";
        json += "\"onHour\":" + String(schedules[i].onHour) + ",";
        json += "\"onMinute\":" + String(schedules[i].onMinute) + ",";
        json += "\"offHour\":" + String(schedules[i].offHour) + ",";
        json += "\"offMinute\":" + String(schedules[i].offMinute) + ",";
        json += "\"enabled\":" + String(schedules[i].enabled ? "true" : "false") + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleAddSchedule() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        StaticJsonDocument<300> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (!error) {
            Schedule newSchedule;
            newSchedule.id = schedules.size();
            newSchedule.relayNumber = doc["relay"];
            String onTime = doc["onTime"];
            String offTime = doc["offTime"];

            newSchedule.onHour = onTime.substring(0, 2).toInt();
            newSchedule.onMinute = onTime.substring(3).toInt();
            newSchedule.offHour = offTime.substring(0, 2).toInt();
            newSchedule.offMinute = offTime.substring(3).toInt();
            newSchedule.enabled = true;
            
            bool conflict = false;
            for (const Schedule& existing : schedules) {
                if (existing.relayNumber == newSchedule.relayNumber && existing.enabled) {
                    int existingStart = existing.onHour * 60 + existing.onMinute;
                    int existingEnd = existing.offHour * 60 + existing.offMinute;
                    int newStart = newSchedule.onHour * 60 + newSchedule.onMinute;
                    int newEnd = newSchedule.offHour * 60 + newSchedule.offMinute;
                    
                    if (existingEnd <= existingStart) existingEnd += 1440;
                    if (newEnd <= newStart) newEnd += 1440;
                    
                    if ((newStart < existingEnd) && (existingStart < newEnd)) {
                        conflict = true;
                        break;
                    }
                }
            }
            
            if (conflict) {
                server.send(409, "application/json", "{\"error\":\"Schedule conflict detected\"}");
                logMessage("Schedule conflict detected for relay " + String(newSchedule.relayNumber));
                return;
            }
            
            schedules.push_back(newSchedule);
            saveSchedulesToEEPROM(); 
            server.send(200, "application/json", "{\"status\":\"success\"}");
            clearError();
            broadcastRelayStates();
            return;
        }
        indicateError();
    }
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleDeleteSchedule() {
    if (server.hasArg("id")) {
        int id = server.arg("id").toInt();
        logMessage("Delete request for schedule ID: " + String(id));
        
        if (id >= 0 && id < schedules.size()) {
            schedules.erase(schedules.begin() + id);
            saveSchedulesToEEPROM();
            logMessage("Schedule deleted successfully");
            server.send(200, "application/json", "{\"status\":\"success\"}");
            clearError();
            broadcastRelayStates();
            return;
        }
        indicateError();
    }
    logMessage("Invalid delete request");
    server.send(400, "application/json", "{\"error\":\"Invalid schedule ID\"}");
}

void handleRoot() {
  if (!checkAuthentication()) return;
  server.send(200, "text/html", html);
}

void toggleRelay(int relayPin, bool &relayState) {
    if ((relayPin == relay1 && overrideRelay1) || (relayPin == relay2 && overrideRelay2)) {
        logMessage("Physical override active, ignoring toggle.");
        return;
    }
    relayState = !relayState;
    digitalWrite(relayPin, relayState ? LOW : HIGH);
    logMessage("Relay state changed to: " + String(relayState));
    broadcastRelayStates();
}

void handleRelay1() {
    if (server.method() == HTTP_POST) {
        if (overrideRelay1) {
            server.send(403, "application/json", "{\"error\":\"Physical override active\"}");
            return;
        }
        toggleRelay(relay1, relay1State);
        server.send(200, "application/json", "{\"state\":" + String(relay1State) + "}");
    } else if (server.method() == HTTP_GET) {
        server.send(200, "application/json", "{\"state\":" + String(relay1State) + "}");
    }
}

void handleRelay2() {
    if (server.method() == HTTP_POST) {
        if (overrideRelay2) {
            server.send(403, "application/json", "{\"error\":\"Physical override active\"}");
            return;
        }
        toggleRelay(relay2, relay2State);
        server.send(200, "application/json", "{\"state\":" + String(relay2State) + "}");
    } else if (server.method() == HTTP_GET) {
        server.send(200, "application/json", "{\"state\":" + String(relay2State) + "}");
    }
}

void handleTime() {
  unsigned long hours = ((epochTime % 86400L) / 3600);
  unsigned long minutes = ((epochTime % 3600) / 60);
  unsigned long seconds = (epochTime % 60);

  String formattedTime = String(hours) + ":" + 
                        (minutes < 10 ? "0" : "") + String(minutes) + ":" + 
                        (seconds < 10 ? "0" : "") + String(seconds);
                        
  server.send(200, "text/plain", formattedTime);
}

void handleRelayStatus() {
    String json = "{";
    json += "\"1\":" + String(relay1State || overrideRelay1) + ","; 
    json += "\"2\":" + String(relay2State || overrideRelay2) + "}";
    server.send(200, "application/json", json);
}

void handleClearError() {
    clearError();
    server.send(200, "application/json", "{\"status\":\"success\"}");
}

void handleGetErrorStatus() {
    String json = "{\"hasError\":" + String(hasError ? "true" : "false") + "}";
    server.send(200, "application/json", json);
}