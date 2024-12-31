# Smart Home Relay Controller

## Description

The Smart Home Relay Controller is a robust solution for automating two relays in a smart home environment. It leverages both web-based controls and physical inputs to provide seamless and reliable management of home automation tasks.

## Features

- **Manual Toggle from Website:** Control relays through a user-friendly web interface.
- **Physical Toggle Button:** Override web controls with a physical button for immediate action.
- **Time-Based Scheduling:** Schedule relay operations based on time, with local time tracking after initial synchronization.
- **Watchdog Logic:** Automatically reboot the system if the device becomes unresponsive.
- **Security:** Basic password protection to prevent unauthorized access, exempting allowed local IPs.

## Scheduling Fail-Safes

- **Persistent Schedules:** Retain all schedules across shutdowns and reboots.
- **Time-Server Synchronization:** Ensure schedules are only executed after successful time synchronization.
- **Override Mechanisms:** Allow physical and web-based toggles to take precedence over scheduled tasks.
- **Resume Active Schedules:** Automatically resume active schedules upon device power-on.
- **Conflict Avoidance:** Prevent the addition of conflicting schedules.
- **Flexible Activation:** Enable activation or deactivation of schedules without needing to delete them entirely.

## Additional Features

- **Comprehensive Logging:** Access all logs through the web UI for real-time error monitoring.
- **Error Indicators:** Utilize a physical LED to signal device errors.
- **WiFi Resilience:** Operate independently if WiFi connection is lost, maintaining functionality until reconnection.
- **Priority Overrides:** Ensure physical buttons always take precedence over software controls and scheduling.

## Installation

1. **Clone the Repository:**
   ```bash
   git clone https://github.com/Armaan4477/Automated-ESP.git
