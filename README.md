# esp32-freertos-iot-bootloader
ESP32 FreeRTOS IoT Bootloader & Firmware
Author: Sameeksha R
Internship: MELSS, Chennai — May–July 2025
Hardware: ESP32-WROOM-32, A7672S GSM Module, SHT31 Sensor, Raspberry Pi 4

Overview
End-to-end IoT firmware stack built during my embedded systems internship at MELSS. The system reads sensor data (temperature & humidity), transmits it to an MQTT cloud broker via A7672S GSM module, and boots via a custom FreeRTOS bootloader with OTA update support.
Features
ESP32 Firmware

FreeRTOS task architecture — sensor and MQTT tasks run independently
A7672S GSM integration via UART using AT+CMQTT command set
MQTT publish to public broker (HiveMQ) with JSON payloads
SHT31 temperature & humidity sensor over I2C
Mutex-protected UART access for thread safety

Raspberry Pi Firmware

pthread-based dual-thread design (same sensor + publish pattern)
Linux i2c-dev for sensor communication
Linux termios UART for GSM module
Graceful reconnect on MQTT publish failure

Custom Bootloader

State-machine boot flow: Init → Self-test → Validate → Launch
CRC32 firmware integrity check on every boot


Build & Flash
ESP32
bash# Using ESP-IDF
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

Raspberry Pi
bashgcc main.c driver.c -o iot_node -lpthread -lm
sudo ./iot_node

MQTT Payload Format
json{
  "client": "ESP32_NODE_01",
  "temperature": 28.45,
  "humidity": 62.10,
  "uptime_ms": 15000
}
Monitor live on: broker.hivemq.com:1883 → topic iot/sameeksha/sensor

Dual-slot support: primary + OTA partition fallback

BOOTLOADER 

Safe mode with UART console for recovery flashing
OTA receive over UART from Python host tool
