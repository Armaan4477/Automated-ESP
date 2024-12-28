from PyQt6.QtWidgets import (QApplication, QMainWindow, QPushButton, 
                           QVBoxLayout, QWidget, QLabel)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QPalette, QColor
import sys
import requests

class ESPControl(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP8266 Control")
        self.setFixedSize(400, 500)
        self.esp_ip = "192.168.29.17"
        
        # Create central widget and layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        # Time display label
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
        
        # Separator line
        separator = QLabel()
        separator.setStyleSheet("QLabel { background-color: #cccccc; }")
        separator.setFixedHeight(1)
        layout.addWidget(separator)
        
        # Status label
        self.status_label = QLabel("ESP8266 Control Panel")
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.status_label.setStyleSheet("QLabel { font-size: 16px; padding: 10px; }")
        layout.addWidget(self.status_label)
        
        # Create relay buttons
        self.buttons = []
        for i in range(4):
            btn = QPushButton(f"Relay {i+1}")
            btn.setCheckable(True)
            btn.clicked.connect(lambda checked, x=i+1: self.toggle_relay(x))
            btn.setMinimumHeight(50)
            layout.addWidget(btn)
            self.buttons.append(btn)
            
        # Style buttons
        self.update_button_styles()
        
        # Setup timer for time updates
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_time)
        self.timer.start(1000)  # Update every second
        
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
            response = requests.get(url, timeout=5)
            
            if response.status_code == 200:
                self.status_label.setText(f"Relay {relay_num} toggled successfully")
                self.update_button_styles()
            else:
                self.status_label.setText(f"Error: {response.status_code}")
                self.buttons[relay_num-1].setChecked(
                    not self.buttons[relay_num-1].isChecked()
                )
                
        except requests.exceptions.RequestException as e:
            self.status_label.setText("Connection Error")
            self.buttons[relay_num-1].setChecked(
                not self.buttons[relay_num-1].isChecked()
            )

def main():
    app = QApplication(sys.argv)
    window = ESPControl()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()