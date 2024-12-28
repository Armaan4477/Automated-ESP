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
        
        # Status label
        self.status_label = QLabel("ESP8266 Control Panel")
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
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
        
    def update_button_styles(self):
        for btn in self.buttons:
            if btn.isChecked():
                btn.setStyleSheet(
                    "QPushButton {background-color: #4CAF50; color: white;}"
                )
            else:
                btn.setStyleSheet(
                    "QPushButton {background-color: #f44336; color: white;}"
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
                # Revert button state on error
                self.buttons[relay_num-1].setChecked(
                    not self.buttons[relay_num-1].isChecked()
                )
                
        except requests.exceptions.RequestException as e:
            self.status_label.setText("Connection Error")
            # Revert button state on error
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