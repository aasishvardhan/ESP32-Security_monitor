# ESP32 Security Monitor

An ESP32-based security monitoring system that detects motion, vibration, and proximity breaches using multiple sensors. The system provides local visual and audible alerts while also hosting a real-time Wi-Fi dashboard for remote monitoring and control.

The project also implements ESP32 deep sleep to reduce power consumption when the system is not actively monitoring.

## Features

* Motion detection using a PIR sensor
* Vibration detection using an MPU6500 IMU
* Proximity detection using an HC-SR04 ultrasonic sensor
* Three system states: DISARMED, ARMED, and ALERT
* OLED status display
* RGB LED status indication
* Audible buzzer alarm
* Wi-Fi connectivity
* Real-time web dashboard
* WebSocket-based live sensor updates
* Physical arm button
* ESP32 deep sleep and wake-up
* FreeRTOS tasks for concurrent sensor and system handling

## System Overview

The ESP32 continuously monitors several sensors and changes its operating state based on detected events.

```text
                 ┌────────────────────┐
                 │       ESP32        │
                 └─────────┬──────────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        ▼                  ▼                  ▼
     PIR Sensor         MPU6500           HC-SR04
      Motion           Vibration          Distance
        │                  │                  │
        └──────────────────┼──────────────────┘
                           ▼
                    Detection Logic
                           │
                ┌──────────┴──────────┐
                ▼                     ▼
             ARMED                  ALERT
                │                     │
                │              ┌──────┴──────┐
                │              ▼             ▼
                │           Buzzer        Red LED
                │
                └──────────────┐
                               ▼
                         Wi-Fi Dashboard
                               │
                           WebSockets
                               │
                           Web Browser
```

## System States

### DISARMED

The system is not actively alarming.

During this state, the ESP32:

* Initializes the PIR sensor
* Performs a warm-up period
* Monitors for movement
* Enters deep sleep if no movement is detected for the configured period

The physical button can be used to transition into the armed state.

### ARMED

The system actively monitors all three detection mechanisms:

* Motion
* Vibration
* Proximity

If any configured trigger is detected, the system transitions to `ALERT`.

### ALERT

When an alarm condition is detected:

* Red LED is activated
* Buzzer sounds
* OLED displays the detected event
* Web dashboard changes to `ALERT`
* Sensor status is displayed in real time

After the alarm sequence, the system returns to the armed state.

## Detection Methods

### Motion Detection

A PIR sensor is connected to an ESP32 interrupt pin.

When the PIR sensor detects movement, the system sets the motion flag and transitions to the alert state when armed.

### Vibration Detection

An MPU6500 accelerometer measures acceleration along the X, Y, and Z axes.

The program calculates the change between consecutive readings:

```text
delta = |ax - previous_ax|
      + |ay - previous_ay|
      + |az - previous_az|
```

If the calculated change exceeds the configured threshold, vibration is detected.

### Proximity Detection

An HC-SR04 ultrasonic sensor measures the distance to nearby objects.

The current proximity threshold is:

```text
Distance < 30 cm → Proximity Breach
```

Ten ultrasonic measurements are averaged for each update to reduce measurement noise.

## Web Dashboard

The ESP32 hosts a web-based security monitoring dashboard.

The dashboard displays:

* Current system state
* Distance measurement
* Motion status
* Vibration status
* Proximity status

It also provides:

* ARM control
* DISARM control

The dashboard receives sensor updates through a WebSocket connection, allowing the displayed values to update without manually refreshing the page.

## Hardware

* ESP32 development board
* MPU6500 accelerometer/gyroscope
* HC-SR04 ultrasonic sensor
* PIR motion sensor
* 1.3-inch SH1106 OLED display
* RGB LED
* Buzzer
* Push button
* Resistors
* Breadboard
* Jumper wires

## Pin Configuration

| Component       | ESP32 Pin |
| --------------- | --------: |
| RGB Red LED     |   GPIO 15 |
| RGB Green LED   |    GPIO 2 |
| RGB Blue LED    |    GPIO 4 |
| Buzzer          |   GPIO 19 |
| HC-SR04 Echo    |   GPIO 13 |
| HC-SR04 Trigger |   GPIO 12 |
| PIR Sensor      |   GPIO 34 |
| Arm Button      |   GPIO 33 |
| MPU6500         |       I²C |
| SH1106 OLED     |       I²C |

The MPU6500 is configured at I²C address `0x68` and the OLED at `0x3C`.

## Software Architecture

The project uses multiple FreeRTOS tasks to allow different parts of the system to operate concurrently.

Main tasks include:

```text
Display Task
Vibration Task
Distance Task
System State Task
Wi-Fi / WebSocket Task
```

The Wi-Fi task runs independently from the sensor tasks and handles:

* HTTP requests
* WebSocket connections
* Dashboard updates
* ARM/DISARM commands

## Deep Sleep

When the system is disarmed and no movement is detected during the monitoring period, the ESP32 disables Wi-Fi and enters deep sleep.

The ESP32 can wake using:

* Timer wake-up
* External button interrupt

This reduces power consumption when active monitoring is not required.

## Libraries

The project uses:

* `Wire`
* `WiFi`
* `FastIMU`
* `WebServer`
* `Adafruit_GFX`
* `Adafruit_Sensor`
* `Adafruit_SH110X`
* `WebSocketsServer`
* `esp_sleep`

Install the required libraries through the Arduino IDE Library Manager where applicable.

## Wi-Fi Configuration

Before uploading the program, enter your own Wi-Fi credentials:

```cpp
const char* SSID = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

**Do not commit real Wi-Fi credentials to a public repository.**

After connecting to Wi-Fi, the ESP32 prints its local IP address to the Serial Monitor.

Open that IP address in a browser connected to the same network to access the dashboard.

## Dashboard Examples

### Disarmed

![Disarmed Dashboard](images/DISARED_dashboard_website.png)

### Armed

![Armed Dashboard](images/ARMED_dashboard_webpage.png)

### Motion Alert

![Motion Alert](images/Motion_alert_webpage.png)

### Proximity Breach

![Proximity Breach](images/Proximity_breach_webpage.png)

### Vibration Alert

![Vibration Alert](images/Vibration_alert_webpage.png)

## Hardware Photos

### Disarmed State

![Disarmed State](images/DiSARMED_state.jpeg)

### Armed State

![Armed State](images/ARMED_state.jpeg)

### Motion Alert

![Motion Alert Hardware](images/Motion_alert.jpeg)

### Proximity Breach

![Proximity Breach Hardware](images/proximity_breach.jpeg)

## Demo

Two demonstration videos are included in the `demo` folder showing the system operating in different states.

* `DISARMED_state_working.mp4`
* `ARMED_state_working.mp4`

## Project Structure

```text
ESP32-security-monitor/
├── README.md
│
├── security_monitor/
│   └── security_monitor.ino
│
├── images/
│   ├── DISARED_dashboard_website.png
│   ├── DiSARMED_state.jpeg
│   ├── ARMED_dashboard_webpage.png
│   ├── ARMED_state.jpeg
│   ├── Motion_alert_webpage.png
│   ├── Motion_alert.jpeg
│   ├── Proximity_breach_webpage.png
│   ├── proximity_breach.jpeg
│   └── Vibration_alert_webpage.png
│
└── demo/
    ├── DISARMED_state_working.mp4
    └── ARMED_state_working.mp4
```
For the circuit connections visit: https://app.cirkitdesigner.com/project/d76789f4-6846-4cf1-885b-f4adeda357bc?utm_source=chatgpt.com

## Future Improvements

* Add authentication to the web dashboard
* Add notifications to a phone or messaging service
* Add battery monitoring
* Improve false-trigger filtering
* Add adjustable proximity and vibration thresholds
* Add event logging
* Add a mobile interface
* Store alarm events in a database
* Add a physical enclosure
* Improve power management during deep sleep
