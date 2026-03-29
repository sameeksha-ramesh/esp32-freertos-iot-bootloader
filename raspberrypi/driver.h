/**
 * @file    driver.h
 * @brief   Driver interface — Raspberry Pi IoT Node
 * @author  Sameeksha R
 *
 * Exposes APIs for:
 *  - UART  (/dev/ttyUSB0 — A7672S GSM module)
 *  - I2C   (/dev/i2c-1  — sensor bus via linux i2c-dev)
 *  - GSM   (A7672S network attach and PDP context)
 *  - MQTT  (broker connect, publish, subscribe via AT+CMQTT)
 *  - Sensor (temperature + humidity)
 */
 
#ifndef DRIVER_H
#define DRIVER_H
 
#include <stdint.h>
#include <stddef.h>
 
/* ── Return codes ───────────────────────────────────────────────────── */
#define DRIVER_OK    0
#define DRIVER_ERR  -1
 
/* ── UART (GSM) config ──────────────────────────────────────────────── */
#define UART_DEVICE      "/dev/ttyUSB0"
#define UART_BAUD_RATE   B115200
#define AT_TIMEOUT_MS    3000
#define AT_CMD_MAX_LEN   256
#define UART_BUF_SIZE    1024
 
/* ── I2C config ─────────────────────────────────────────────────────── */
#define I2C_DEVICE       "/dev/i2c-1"
#define SENSOR_I2C_ADDR  0x44           /* SHT31 */
 
/* ── MQTT limits ────────────────────────────────────────────────────── */
#define MQTT_PAYLOAD_MAX  512
#define MQTT_TOPIC_MAX    128
 
/* ════════════════════════════════════════════════════════════════════
 *  UART
 * ════════════════════════════════════════════════════════════════════ */
 
/** @brief Open and configure UART port for A7672S. */
int driver_uart_init(void);
 
/** @brief Send AT command, wait for expected response substring. */
int driver_uart_send_at(const char *cmd, const char *expected, uint32_t timeout_ms);
 
/** @brief Read raw response from UART into buf (null-terminated). */
int driver_uart_read(char *buf, int maxlen);
 
/* ════════════════════════════════════════════════════════════════════
 *  I2C
 * ════════════════════════════════════════════════════════════════════ */
 
/** @brief Open /dev/i2c-1 for sensor communication. */
int driver_i2c_init(void);
 
/** @brief Write bytes to I2C device at addr. */
int driver_i2c_write(uint8_t addr, const uint8_t *data, size_t len);
 
/** @brief Read bytes from I2C device at addr. */
int driver_i2c_read(uint8_t addr, uint8_t *buf, size_t len);
 
/* ════════════════════════════════════════════════════════════════════
 *  GSM — A7672S
 * ════════════════════════════════════════════════════════════════════ */
 
/** @brief Init A7672S: handshake, SIM check, network attach, PDP context. */
int driver_gsm_init(void);
 
/** @brief Check network registration status. */
int driver_gsm_check_network(void);
 
/** @brief Get module IMEI (min 16-byte buffer). */
int driver_gsm_get_imei(char *imei_buf);
 
/* ════════════════════════════════════════════════════════════════════
 *  MQTT
 * ════════════════════════════════════════════════════════════════════ */
 
/** @brief Connect to MQTT broker via AT+CMQTT commands. */
int driver_mqtt_connect(const char *broker, uint16_t port, const char *client_id);
 
/** @brief Publish payload string to topic. */
int driver_mqtt_publish(const char *topic, const char *payload);
 
/** @brief Subscribe to topic with given QoS. */
int driver_mqtt_subscribe(const char *topic, uint8_t qos);
 
/** @brief Cleanly disconnect from broker. */
int driver_mqtt_disconnect(void);
 
/* ════════════════════════════════════════════════════════════════════
 *  Sensor
 * ════════════════════════════════════════════════════════════════════ */
 
/** @brief Read temperature (°C) and humidity (%) from I2C sensor. */
int driver_sensor_read(float *temperature, float *humidity);
 
#endif /* DRIVER_H */
 
