/**
 * @file    driver.h
 * @brief   Driver interface — ESP32 IoT Firmware
 * @author  Sameeksha R
 *
 * Exposes APIs for:
 *  - UART (A7672S GSM communication)
 *  - I2C  (sensor bus)
 *  - GSM  (network attach, AT commands)
 *  - MQTT (connect, publish, subscribe)
 *  - Sensor (temperature + humidity read)
 */
 
#ifndef DRIVER_H
#define DRIVER_H
 
#include <stdint.h>
 
/* ── Return codes ───────────────────────────────────────────────────── */
#define DRIVER_OK    0
#define DRIVER_ERR  -1
 
/* ── UART config ────────────────────────────────────────────────────── */
#define UART_PORT_NUM       1
#define UART_BAUD_RATE      115200
#define UART_TX_PIN         17
#define UART_RX_PIN         16
#define UART_BUF_SIZE       1024
#define AT_TIMEOUT_MS       3000
 
/* ── I2C config ─────────────────────────────────────────────────────── */
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_FREQ_HZ  100000
#define SENSOR_I2C_ADDR     0x44      /* e.g. SHT31 */
 
/* ── GSM / MQTT limits ──────────────────────────────────────────────── */
#define AT_CMD_MAX_LEN      256
#define MQTT_PAYLOAD_MAX    512
#define MQTT_TOPIC_MAX      128
 
/* ════════════════════════════════════════════════════════════════════
 *  UART — low-level serial to A7672S
 * ════════════════════════════════════════════════════════════════════ */
 
/**
 * @brief  Initialise UART port for A7672S GSM module.
 * @return DRIVER_OK on success, DRIVER_ERR on failure.
 */
int driver_uart_init(void);
 
/**
 * @brief  Send an AT command and wait for expected response.
 * @param  cmd        Null-terminated AT command string (without \\r\\n).
 * @param  expected   Substring to match in response (e.g. "OK").
 * @param  timeout_ms Milliseconds to wait.
 * @return DRIVER_OK if expected found, DRIVER_ERR on timeout/error.
 */
int driver_uart_send_at(const char *cmd, const char *expected, uint32_t timeout_ms);
 
/**
 * @brief  Read raw bytes from UART into buf (null-terminated).
 * @param  buf     Output buffer.
 * @param  maxlen  Maximum bytes to read.
 * @return Number of bytes read, or DRIVER_ERR.
 */
int driver_uart_read(char *buf, int maxlen);
 
/* ════════════════════════════════════════════════════════════════════
 *  I2C — sensor bus
 * ════════════════════════════════════════════════════════════════════ */
 
/**
 * @brief  Initialise I2C master bus.
 * @return DRIVER_OK on success.
 */
int driver_i2c_init(void);
 
/**
 * @brief  Write bytes to an I2C device.
 * @param  addr    7-bit device address.
 * @param  data    Bytes to write.
 * @param  len     Number of bytes.
 * @return DRIVER_OK on success.
 */
int driver_i2c_write(uint8_t addr, const uint8_t *data, size_t len);
 
/**
 * @brief  Read bytes from an I2C device.
 * @param  addr    7-bit device address.
 * @param  buf     Output buffer.
 * @param  len     Number of bytes to read.
 * @return DRIVER_OK on success.
 */
int driver_i2c_read(uint8_t addr, uint8_t *buf, size_t len);
 
/* ════════════════════════════════════════════════════════════════════
 *  GSM — A7672S network management
 * ════════════════════════════════════════════════════════════════════ */
 
/**
 * @brief  Initialise A7672S: check module, attach to network, open PDP context.
 * @return DRIVER_OK on success, DRIVER_ERR on failure.
 */
int driver_gsm_init(void);
 
/**
 * @brief  Query current network registration status.
 * @return DRIVER_OK if registered, DRIVER_ERR otherwise.
 */
int driver_gsm_check_network(void);
 
/**
 * @brief  Get module IMEI string.
 * @param  imei_buf  Output buffer (min 16 bytes).
 * @return DRIVER_OK on success.
 */
int driver_gsm_get_imei(char *imei_buf);
 
/* ════════════════════════════════════════════════════════════════════
 *  MQTT — broker communication via A7672S AT commands
 * ════════════════════════════════════════════════════════════════════ */
 
/**
 * @brief  Open MQTT connection to broker.
 * @param  broker     Broker hostname or IP.
 * @param  port       Broker port (typically 1883).
 * @param  client_id  Unique client identifier string.
 * @return DRIVER_OK on success.
 */
int driver_mqtt_connect(const char *broker, uint16_t port, const char *client_id);
 
/**
 * @brief  Publish a message to an MQTT topic.
 * @param  topic    Topic string.
 * @param  payload  Null-terminated JSON or string payload.
 * @return DRIVER_OK on success.
 */
int driver_mqtt_publish(const char *topic, const char *payload);
 
/**
 * @brief  Subscribe to an MQTT topic.
 * @param  topic  Topic string.
 * @param  qos    Quality of Service (0 or 1).
 * @return DRIVER_OK on success.
 */
int driver_mqtt_subscribe(const char *topic, uint8_t qos);
 
/**
 * @brief  Disconnect from MQTT broker cleanly.
 * @return DRIVER_OK on success.
 */
int driver_mqtt_disconnect(void);
 
/* ════════════════════════════════════════════════════════════════════
 *  Sensor — temperature & humidity (I2C)
 * ════════════════════════════════════════════════════════════════════ */
 
/**
 * @brief  Read temperature and humidity from I2C sensor.
 * @param  temperature  Output: degrees Celsius.
 * @param  humidity     Output: relative humidity %.
 * @return DRIVER_OK on success, DRIVER_ERR on I2C fault.
 */
int driver_sensor_read(float *temperature, float *humidity);
 
#endif /* DRIVER_H */
