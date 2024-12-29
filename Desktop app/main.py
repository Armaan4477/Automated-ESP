from PyQt6.QtWidgets import (QApplication, QMainWindow, QPushButton, QVBoxLayout, 
                           QWidget, QLabel, QDialog, QTimeEdit, QComboBox, 
                           QHBoxLayout, QTableWidget, QTableWidgetItem, QHeaderView)
from PyQt6.QtCore import Qt, QTimer
import sys
import requests
from datetime import datetime

class ScheduleDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add Schedule")
        self.setFixedSize(400, 200)
        
        layout = QVBoxLayout()
        
        # Relay selection
        relay_layout = QHBoxLayout()
        relay_label = QLabel("Relay:")
        self.relay_combo = QComboBox()
        self.relay_combo.addItems([f"Relay {i+1}" for i in range(4)])
        relay_layout.addWidget(relay_label)
        relay_layout.addWidget(self.relay_combo)
        layout.addLayout(relay_layout)
        
        # Time selection
        time_layout = QHBoxLayout()
        
        on_time_label = QLabel("On Time:")
        self.on_time = QTimeEdit()
        self.on_time.setDisplayFormat("HH:mm")
        
        off_time_label = QLabel("Off Time:")
        self.off_time = QTimeEdit()
        self.off_time.setDisplayFormat("HH:mm")
        
        time_layout.addWidget(on_time_label)
        time_layout.addWidget(self.on_time)
        time_layout.addWidget(off_time_label)
        time_layout.addWidget(self.off_time)
        
        layout.addLayout(time_layout)
        
        # Add button
        add_btn = QPushButton("Add Schedule")
        add_btn.clicked.connect(self.accept)
        layout.addWidget(add_btn)
        
        self.setLayout(layout)

class ESPControl(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP8266 Control")
        self.setFixedSize(600, 700)
        self.esp_ip = "192.168.29.17"
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        # Existing time and status labels
        self.init_time_display(layout)
        self.init_status_display(layout)
        
        self.buttons = []
        # Relay buttons
        self.init_relay_buttons(layout)
        
        # Schedule section
        schedule_label = QLabel("Schedules")
        schedule_label.setStyleSheet("QLabel { font-size: 16px; font-weight: bold; }")
        layout.addWidget(schedule_label)
        
        # Schedule table
        self.schedule_table = QTableWidget()
        self.schedule_table.setColumnCount(5)
        self.schedule_table.setHorizontalHeaderLabels(["Relay", "On Time", "Off Time", "Status", "Action"])
        self.schedule_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.schedule_table)
        
        # Add schedule button
        add_schedule_btn = QPushButton("Add Schedule")
        add_schedule_btn.clicked.connect(self.show_add_schedule_dialog)
        layout.addWidget(add_schedule_btn)
        
        # Setup timers
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_time)
        self.timer.start(1000)
        
        self.schedule_timer = QTimer()
        self.schedule_timer.timeout.connect(self.update_schedules)
        self.schedule_timer.start(5000)
        
        # Initial updates
        self.update_schedules()
        self.fetch_relay_states()
        
    def init_time_display(self, layout):
        self.time_label = QLabel("Loading time...")
        self.time_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.time_label.setStyleSheet("""
            QLabel { 
                font-size: 24px; 
                font-family: monospace;
                padding: 10px;
                background-color: #f0f0f0;
                border-radius: 5px;
                margin: 10px;
            }
        """)
        layout.addWidget(self.time_label)
        
    def init_status_display(self, layout):
        separator = QLabel()
        separator.setStyleSheet("QLabel { background-color: #cccccc; }")
        separator.setFixedHeight(1)
        layout.addWidget(separator)
        
        self.status_label = QLabel("ESP8266 Control Panel")
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.status_label.setStyleSheet("QLabel { font-size: 16px; padding: 10px; }")
        layout.addWidget(self.status_label)
        
    def show_add_schedule_dialog(self):
        dialog = ScheduleDialog(self)
        if dialog.exec():
            relay = int(dialog.relay_combo.currentText().split()[-1])
            on_time = dialog.on_time.time().toString("HH:mm")
            off_time = dialog.off_time.time().toString("HH:mm")
            
            try:
                response = requests.post(
                    f"http://{self.esp_ip}/schedule/add",
                    json={
                        "relay": relay,
                        "onTime": on_time,
                        "offTime": off_time
                    }
                )
                if response.status_code == 200:
                    self.status_label.setText("Schedule added successfully")
                    self.update_schedules()
                else:
                    self.status_label.setText("Failed to add schedule")
            except requests.exceptions.RequestException:
                self.status_label.setText("Connection Error")
                
    def delete_schedule(self, schedule_id):
        try:
            response = requests.delete(
                f"http://{self.esp_ip}/schedule/delete",
                params={"id": schedule_id}
            )
            if response.status_code == 200:
                self.status_label.setText("Schedule deleted successfully")
                self.update_schedules()
            else:
                self.status_label.setText("Failed to delete schedule")
        except requests.exceptions.RequestException:
            self.status_label.setText("Connection Error")
            
    def update_schedules(self):
        try:
            response = requests.get(f"http://{self.esp_ip}/schedules")
            if response.status_code == 200:
                schedules = response.json()
                self.schedule_table.setRowCount(len(schedules))
                
                for i, schedule in enumerate(schedules):
                    self.schedule_table.setItem(i, 0, QTableWidgetItem(f"Relay {schedule['relay']}"))
                    self.schedule_table.setItem(i, 1, QTableWidgetItem(f"{schedule['onHour']:02d}:{schedule['onMinute']:02d}"))
                    self.schedule_table.setItem(i, 2, QTableWidgetItem(f"{schedule['offHour']:02d}:{schedule['offMinute']:02d}"))
                    self.schedule_table.setItem(i, 3, QTableWidgetItem("Active" if schedule['enabled'] else "Inactive"))
                    
                    delete_btn = QPushButton("Delete")
                    delete_btn.clicked.connect(lambda checked, x=schedule['id']: self.delete_schedule(x))
                    self.schedule_table.setCellWidget(i, 4, delete_btn)
                    
        except requests.exceptions.RequestException:
            self.status_label.setText("Failed to fetch schedules")
        
    def update_time(self):
        try:
            response = requests.get(f"http://{self.esp_ip}/time", timeout=2)
            if response.status_code == 200:
                time_text = response.text.strip()
                self.time_label.setText(time_text)
                self.time_label.setStyleSheet("""
                    QLabel { 
                        font-size: 24px; 
                        font-family: monospace;
                        padding: 10px;
                        background-color: #f0f0f0;
                        border-radius: 5px;
                        margin: 10px;
                        color: #2196F3;
                    }
                """)
            else:
                self.time_label.setText("Error fetching time")
        except requests.exceptions.RequestException:
            self.time_label.setText("Connection Error")
            self.time_label.setStyleSheet("""
                QLabel { 
                    font-size: 24px; 
                    font-family: monospace;
                    padding: 10px;
                    background-color: #ffebee;
                    border-radius: 5px;
                    margin: 10px;
                    color: #f44336;
                }
            """)

    def init_relay_buttons(self, layout):
        """Initialize the relay control buttons"""
        button_layout = QHBoxLayout()
        
        for i in range(4):
            btn = QPushButton(f"Relay {i+1}")
            btn.setCheckable(True)
            btn.setFixedSize(100, 40)
            btn.clicked.connect(lambda checked, x=i+1: self.toggle_relay(x))
            button_layout.addWidget(btn)
            self.buttons.append(btn)
            
        layout.addLayout(button_layout)
        self.update_button_styles()
                
    def update_button_styles(self):
        for btn in self.buttons:
            if btn.isChecked():
                btn.setStyleSheet(
                    "QPushButton {background-color: #4CAF50; color: white; border-radius: 5px;}"
                )
            else:
                btn.setStyleSheet(
                    "QPushButton {background-color: #f44336; color: white; border-radius: 5px;}"
                )
                
    def toggle_relay(self, relay_num):
        try:
            url = f"http://{self.esp_ip}/relay/{relay_num}"
            response = requests.post(url, json={}, timeout=5)
            if response.status_code == 200:
                data = response.json()
                state = data.get("state", False)
                self.buttons[relay_num - 1].setChecked(bool(state))
                self.status_label.setText(f"Relay {relay_num} toggled to {'ON' if state else 'OFF'}")
                self.update_button_styles()
            else:
                self.status_label.setText(f"Error: {response.status_code}")
                self.buttons[relay_num - 1].setChecked(not self.buttons[relay_num - 1].isChecked())
        except requests.exceptions.RequestException:
            self.status_label.setText("Connection Error")
            self.buttons[relay_num - 1].setChecked(not self.buttons[relay_num - 1].isChecked())

    def fetch_relay_states(self):
        try:
            response = requests.get(f"http://{self.esp_ip}/relay/status")
            if response.status_code == 200:
                states = response.json()
                for i, btn in enumerate(self.buttons, start=1):
                    btn.setChecked(states.get(str(i), False))
                self.update_button_styles()
            else:
                self.status_label.setText("Failed to fetch relay states")
        except requests.exceptions.RequestException:
            self.status_label.setText("Connection Error")

def main():
    app = QApplication(sys.argv)
    window = ESPControl()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()