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
const int relay3 = 14; // D5
const int relay4 = 12; // D6

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
    pinMode(relay3, OUTPUT);
    pinMode(relay4, OUTPUT);
    
    digitalWrite(relay1, HIGH);
    digitalWrite(relay2, HIGH);
    digitalWrite(relay3, HIGH);
    digitalWrite(relay4, HIGH);
    
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nConnected to WiFi");
    timeClient.begin();
    timeClient.setTimeOffset(19800);
    
    if (timeClient.update()) {
        epochTime = timeClient.getEpochTime();
        lastNTPSync = millis();
        validTimeSync = true;
        Serial.println("Time sync successful");
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/relay/1", HTTP_GET, handleRelay1);
    server.on("/relay/2", HTTP_GET, handleRelay2);
    server.on("/relay/3", HTTP_GET, handleRelay3);
    server.on("/relay/4", HTTP_GET, handleRelay4);
    server.on("/time", HTTP_GET, handleTime);
    server.on("/schedules", HTTP_GET, handleGetSchedules);
    server.on("/schedule/add", HTTP_POST, handleAddSchedule);
    server.on("/schedule/delete", HTTP_DELETE, handleDeleteSchedule);
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
        #time {
            font-size: 24px;
            margin: 20px;
            font-family: monospace;
        }
        .schedule-form {
            margin: 20px;
            padding: 20px;
            border: 1px solid #ccc;
        }
        .schedule-table {
            margin: 20px;
            border-collapse: collapse;
        }
        .schedule-table td, .schedule-table th {
            border: 1px solid #ddd;
            padding: 8px;
        }
    </style>
</head>
<body>
    <div id="time">Loading time...</div>
    <button class="button" onclick="toggleRelay(1)" id="btn1">Relay 1</button>
    <button class="button" onclick="toggleRelay(2)" id="btn2">Relay 2</button>
    <button class="button" onclick="toggleRelay(3)" id="btn3">Relay 3</button>
    <button class="button" onclick="toggleRelay(4)" id="btn4">Relay 4</button>
    
    <div class="schedule-form">
        <h3>Add Schedule</h3>
        <select id="relaySelect">
            <option value="1">Relay 1</option>
            <option value="2">Relay 2</option>
            <option value="3">Relay 3</option>
            <option value="4">Relay 4</option>
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
        function updateTime() {
            fetch('/time')
                .then(response => response.text())
                .then(time => {
                    document.getElementById('time').innerHTML = time;
                });
        }

        function toggleRelay(relay) {
            fetch('/relay/' + relay)
                .then(response => response.json())
                .then(data => {
                    let btn = document.getElementById('btn' + relay);
                    btn.className = 'button ' + (data.state ? 'on' : 'off');
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

        

        setInterval(updateTime, 1000);
        updateTime();
        loadSchedules();
    </script>
</body>
</html>
)html";

void loop() {
    server.handleClient();
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
        case 3: digitalWrite(relay3, LOW); relay3State = true; break;
        case 4: digitalWrite(relay4, LOW); relay4State = true; break;
    }
}

void deactivateRelay(int relayNum) {
    switch(relayNum) {
        case 1: digitalWrite(relay1, HIGH); relay1State = false; break;
        case 2: digitalWrite(relay2, HIGH); relay2State = false; break;
        case 3: digitalWrite(relay3, HIGH); relay3State = false; break;
        case 4: digitalWrite(relay4, HIGH); relay4State = false; break;
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
  relayState = !relayState;
  digitalWrite(relayPin, relayState ? LOW : HIGH);
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

void handleTime() {
  unsigned long hours = ((epochTime % 86400L) / 3600);
  unsigned long minutes = ((epochTime % 3600) / 60);
  unsigned long seconds = (epochTime % 60);

  String formattedTime = String(hours) + ":" + 
                        (minutes < 10 ? "0" : "") + String(minutes) + ":" + 
                        (seconds < 10 ? "0" : "") + String(seconds);
                        
  server.send(200, "text/plain", formattedTime);
}