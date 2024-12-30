#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <vector>
#include <ArduinoJson.h>
#include <EEPROM.h>

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

bool overrideRelay1 = false;
bool overrideRelay2 = false;
bool relay1State = false;
bool relay2State = false;
bool relay3State = false;
bool relay4State = false;

const char* ssid = "Free Public Wi-Fi";
const char* password = "2A0R0M4AAN";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
unsigned long lastTimeUpdate = 0;
const long timeUpdateInterval = 1000;
unsigned long epochTime = 0;
unsigned long lastNTPSync = 0;
unsigned long lastScheduleCheck = 0;
unsigned long lastSecond = 0;
bool validTimeSync = false;
std::vector<Schedule> schedules;
void handleAddSchedule();
void handleDeleteSchedule();
const int EEPROM_SIZE = 512;
const int SCHEDULE_SIZE = sizeof(Schedule);
const int MAX_SCHEDULES = 10;
const int SCHEDULE_START_ADDR = 0;
ESP8266WebServer server(80);

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
    
    digitalWrite(relay1, HIGH);
    digitalWrite(relay2, HIGH);
    
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    timeClient.begin();
    timeClient.setTimeOffset(19800);
    
    if (timeClient.update()) {
        epochTime = timeClient.getEpochTime();
        lastNTPSync = millis();
        validTimeSync = true;
        Serial.println("Time sync successful");
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/relay/1", HTTP_ANY, handleRelay1);
    server.on("/relay/2", HTTP_ANY, handleRelay2);
    server.on("/time", HTTP_GET, handleTime);
    server.on("/schedules", HTTP_GET, handleGetSchedules);
    server.on("/schedule/add", HTTP_POST, handleAddSchedule);
    server.on("/schedule/delete", HTTP_DELETE, handleDeleteSchedule);
    server.on("/relay/status", HTTP_GET, handleRelayStatus);
    server.begin();
    EEPROM.begin(EEPROM_SIZE);
    loadSchedulesFromEEPROM();
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

    <table class="schedule-table" id="scheduleTable">
        <tr>
            <th>Relay</th>
            <th>On Time</th>
            <th>Off Time</th>
            <th>Status</th>
            <th>Action</th>
        </tr>
    </table>

    <script>
        let relayStates = {
            1: false,
            2: false
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
                    throw new Error('Network response was not ok');
                }
                return response.json();
            })
            .then(data => {
                console.log('Relay ' + relay + ' state:', data.state); // Debug logging
                relayStates[relay] = data.state;
                updateButtonStyle(relay);
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Failed to toggle relay ' + relay);
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
            }).then(() => loadSchedules());
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
              } else {
                  throw new Error('Server returned error');
              }
          })
          .catch(error => {
              console.error('Error:', error);
              alert('Failed to delete schedule: ' + error.message);
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
              });
      }

        

        setInterval(updateTime, 1000);
        updateTime();
        loadSchedules();
        getInitialStates();
    </script>
</body>
</html>
)html";

void loop() {
    server.handleClient();

    if (digitalRead(switch1Pin) == LOW) {
      overrideRelay1 = true;
      activateRelay(1);
    } else {
      overrideRelay1 = false;
    }
    if (digitalRead(switch2Pin) == LOW) {
      overrideRelay2 = true;
      activateRelay(2);
    } else {
      overrideRelay2 = false;
    }

    if (!validTimeSync) {
        if (timeClient.update()) {
            epochTime = timeClient.getEpochTime();
            lastNTPSync = millis();
            validTimeSync = true;
            Serial.println("Time sync successful (retry)");
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
}

void checkSchedules() {
    unsigned long hours = ((epochTime % 86400L) / 3600);
    unsigned long minutes = ((epochTime % 3600) / 60);
    unsigned long seconds = (epochTime % 60);
    
    for (const Schedule& schedule : schedules) {
        if (!schedule.enabled) continue;
        
        if (hours == schedule.onHour && minutes == schedule.onMinute && seconds == 0) {
            activateRelay(schedule.relayNumber);
        }
        else if (hours == schedule.offHour && minutes == schedule.offMinute && seconds == 0) {
            deactivateRelay(schedule.relayNumber);
        }
    }
}

void activateRelay(int relayNum) {
    switch(relayNum) {
        case 1: digitalWrite(relay1, LOW); relay1State = true; break;
        case 2: digitalWrite(relay2, LOW); relay2State = true; break;
    }
}

void deactivateRelay(int relayNum) {
    switch(relayNum) {
        case 1: digitalWrite(relay1, HIGH); relay1State = false; break;
        case 2: digitalWrite(relay2, HIGH); relay2State = false; break;
    }
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
        StaticJsonDocument<200> doc;
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
            
            schedules.push_back(newSchedule);
            saveSchedulesToEEPROM(); 
            server.send(200, "application/json", "{\"status\":\"success\"}");
            return;
        }
    }
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleDeleteSchedule() {
    if (server.hasArg("id")) {
        int id = server.arg("id").toInt();
        Serial.println("Delete request for schedule ID: " + String(id));
        
        if (id >= 0 && id < schedules.size()) {
            schedules.erase(schedules.begin() + id);
            saveSchedulesToEEPROM();
            Serial.println("Schedule deleted successfully");
            server.send(200, "application/json", "{\"status\":\"success\"}");
            return;
        }
    }
    Serial.println("Invalid delete request");
    server.send(400, "application/json", "{\"error\":\"Invalid schedule ID\"}");
}

void handleRoot() {
  server.send(200, "text/html", html);
}

void toggleRelay(int relayPin, bool &relayState) {
    if ((relayPin == relay1 && overrideRelay1) || (relayPin == relay2 && overrideRelay2)) {
        Serial.println("Physical override active, ignoring toggle.");
        return;
      }
    relayState = !relayState;
    digitalWrite(relayPin, relayState ? LOW : HIGH);
    Serial.printf("Relay state changed to: %d\n", relayState);
}

void handleRelay1() {
    if (server.method() == HTTP_POST) {
        toggleRelay(relay1, relay1State);
        server.send(200, "application/json", "{\"state\":" + String(relay1State) + "}");
    } else if (server.method() == HTTP_GET) {
        server.send(200, "application/json", "{\"state\":" + String(relay1State) + "}");
    }
}

void handleRelay2() {
    if (server.method() == HTTP_POST) {
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