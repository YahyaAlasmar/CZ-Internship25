# ESP32 Smart Safety Node

This is an ESP32-based Smart Safety Node system using:

- **BMP180**: Temperature and pressure sensor
- **MQ135**: Gas/air quality sensor
- **PIR Sensor**: Motion detection
- **SSD1306 OLED**: Real-time display
- **ThingSpeak**: Cloud logging of sensor data
- **LEDs & Button**: Visual alert system and reset

## Features

- Monitors gas levels and motion
- Activates alert when both conditions are dangerous
- Displays live data on OLED screen
- Sends data to ThingSpeak every 16 seconds

## Hardware Setup

- ESP32 WROOM module
- I2C: GPIO 21 (SDA), GPIO 22 (SCL)
- MQ135: GPIO34 (ADC1_CH6)
- PIR Sensor: GPIO14
- Red LED: GPIO25
- Green LED: GPIO26
- Button: GPIO27

## Wi-Fi & Cloud

- Sends temperature, pressure, gas level, and motion status to ThingSpeak
- Update interval: every 8 loops (~16 seconds)

## Schematic

You can view the circuit schematic for this project here:

[📄 View Schematic (PDF)](HardWare/smarthome-draft1-PCB.pdf)


## Author

[Yahya Alasmar](https://github.com/YahyaAlasmar)
