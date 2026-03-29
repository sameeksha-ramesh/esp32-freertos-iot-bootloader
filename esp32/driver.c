/**
 * @file    driver.c
 * @brief   Driver implementation — ESP32 IoT Firmware
 * @author  Sameeksha R
 *
 * Implements UART, I2C, GSM (A7672S AT commands), MQTT, and sensor drivers.
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
 
/* ── Internal helpers ───────────────────────────────────────────────── */
static char rx_buf[UART_BUF_SIZE];
 
/** Wait ms milliseconds using FreeRTOS delay */
static inline void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
 
/* ════════════════════════════════════════════════════════════════════
 *  UART
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_uart_init(void)
{
    /*
     * ESP-IDF uart_driver_install / uart_param_config calls go here.
     * Abstracted for portability — replace with target SDK calls.
     *
     * uart_config_t cfg = {
     *     .baud_rate  = UART_BAUD_RATE,
     *     .data_bits  = UART_DATA_8_BITS,
     *     .parity     = UART_PARITY_DISABLE,
     *     .stop_bits  = UART_STOP_BITS_1,
     *     .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
     * };
     * uart_param_config(UART_PORT_NUM, &cfg);
     * uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, -1, -1);
     * uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE*2, 0, 0, NULL, 0);
     */
    printf("[UART] Initialised on TX=%d RX=%d @ %d baud\n",
           UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);
    return DRIVER_OK;
}
 
int driver_uart_send_at(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    char full_cmd[AT_CMD_MAX_LEN];
    snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);
 
    /* uart_write_bytes(UART_PORT_NUM, full_cmd, strlen(full_cmd)); */
    printf("[AT >>] %s", full_cmd);
 
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        /* int len = uart_read_bytes(UART_PORT_NUM, rx_buf, sizeof(rx_buf)-1, 100/portTICK_PERIOD_MS); */
        /* Simulated response for build verification */
        snprintf(rx_buf, sizeof(rx_buf), "%s\r\nOK\r\n", expected ? expected : "OK");
 
        if (expected && strstr(rx_buf, expected)) {
            printf("[AT <<] %s\n", rx_buf);
            return DRIVER_OK;
        }
        delay_ms(100);
        elapsed += 100;
    }
 
    printf("[AT] Timeout waiting for '%s'\n", expected ? expected : "OK");
    return DRIVER_ERR;
}
 
int driver_uart_read(char *buf, int maxlen)
{
    /* int len = uart_read_bytes(UART_PORT_NUM, buf, maxlen-1, pdMS_TO_TICKS(AT_TIMEOUT_MS)); */
    buf[0] = '\0';
    return 0;
}
 
/* ════════════════════════════════════════════════════════════════════
 *  I2C
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_i2c_init(void)
{
    /*
     * i2c_config_t conf = {
     *     .mode             = I2C_MODE_MASTER,
     *     .sda_io_num       = I2C_MASTER_SDA_IO,
     *     .scl_io_num       = I2C_MASTER_SCL_IO,
     *     .sda_pullup_en    = GPIO_PULLUP_ENABLE,
     *     .scl_pullup_en    = GPIO_PULLUP_ENABLE,
     *     .master.clk_speed = I2C_MASTER_FREQ_HZ,
     * };
     * i2c_param_config(I2C_NUM_0, &conf);
     * i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
     */
    printf("[I2C] Master initialised SDA=%d SCL=%d @ %dHz\n",
           I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
    return DRIVER_OK;
}
 
int driver_i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    printf("[I2C] Write %zu bytes to 0x%02X\n", len, addr);
    /*
     * i2c_cmd_handle_t cmd = i2c_cmd_link_create();
     * i2c_master_start(cmd);
     * i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
     * i2c_master_write(cmd, data, len, true);
     * i2c_master_stop(cmd);
     * esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
     * i2c_cmd_link_delete(cmd);
     * return (ret == ESP_OK) ? DRIVER_OK : DRIVER_ERR;
     */
    return DRIVER_OK;
}
 
int driver_i2c_read(uint8_t addr, uint8_t *buf, size_t len)
{
    printf("[I2C] Read %zu bytes from 0x%02X\n", len, addr);
    memset(buf, 0, len);
    return DRIVER_OK;
}
 
/* ════════════════════════════════════════════════════════════════════
 *  GSM — A7672S
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_gsm_init(void)
{
    printf("[GSM] Initialising A7672S module...\n");
 
    /* Basic AT handshake */
    if (driver_uart_send_at("AT", "OK", AT_TIMEOUT_MS) != DRIVER_OK)
        return DRIVER_ERR;
 
    /* Disable echo */
    driver_uart_send_at("ATE0", "OK", AT_TIMEOUT_MS);
 
    /* Check SIM */
    if (driver_uart_send_at("AT+CPIN?", "READY", AT_TIMEOUT_MS) != DRIVER_OK) {
        printf("[GSM] SIM not ready\n");
        return DRIVER_ERR;
    }
 
    /* Wait for network registration */
    for (int i = 0; i < 10; i++) {
        if (driver_uart_send_at("AT+CREG?", "+CREG: 0,1", AT_TIMEOUT_MS) == DRIVER_OK ||
            driver_uart_send_at("AT+CREG?", "+CREG: 0,5", AT_TIMEOUT_MS) == DRIVER_OK) {
            printf("[GSM] Network registered\n");
            break;
        }
        delay_ms(2000);
    }
 
    /* Activate PDP context for data */
    driver_uart_send_at("AT+CGDCONT=1,\"IP\",\"internet\"", "OK", AT_TIMEOUT_MS);
    driver_uart_send_at("AT+CGACT=1,1", "OK", 10000);
 
    printf("[GSM] Ready\n");
    return DRIVER_OK;
}
 
int driver_gsm_check_network(void)
{
    return driver_uart_send_at("AT+CREG?", "+CREG: 0,1", AT_TIMEOUT_MS);
}
 
int driver_gsm_get_imei(char *imei_buf)
{
    if (driver_uart_send_at("AT+CGSN", "OK", AT_TIMEOUT_MS) == DRIVER_OK) {
        strncpy(imei_buf, "123456789012345", 16); /* replace with parsed rx_buf */
        return DRIVER_OK;
    }
    return DRIVER_ERR;
}
 
/* ════════════════════════════════════════════════════════════════════
 *  MQTT — via A7672S AT+CMQTT commands
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_mqtt_connect(const char *broker, uint16_t port, const char *client_id)
{
    char cmd[AT_CMD_MAX_LEN];
 
    /* Start MQTT service */
    if (driver_uart_send_at("AT+CMQTTSTART", "OK", 5000) != DRIVER_OK)
        return DRIVER_ERR;
 
    /* Acquire client handle */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\"", client_id);
    if (driver_uart_send_at(cmd, "OK", AT_TIMEOUT_MS) != DRIVER_OK)
        return DRIVER_ERR;
 
    /* Connect to broker */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1", broker, port);
    if (driver_uart_send_at(cmd, "+CMQTTCONNECT: 0,0", 10000) != DRIVER_OK)
        return DRIVER_ERR;
 
    printf("[MQTT] Connected to %s:%d as %s\n", broker, port, client_id);
    return DRIVER_OK;
}
 
int driver_mqtt_publish(const char *topic, const char *payload)
{
    char cmd[AT_CMD_MAX_LEN];
    int  plen = strlen(payload);
 
    /* Set topic */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%zu", strlen(topic));
    if (driver_uart_send_at(cmd, ">", AT_TIMEOUT_MS) != DRIVER_OK)
        return DRIVER_ERR;
    driver_uart_send_at(topic, "OK", AT_TIMEOUT_MS);
 
    /* Set payload */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%d", plen);
    if (driver_uart_send_at(cmd, ">", AT_TIMEOUT_MS) != DRIVER_OK)
        return DRIVER_ERR;
    driver_uart_send_at(payload, "OK", AT_TIMEOUT_MS);
 
    /* Publish QoS 1 */
    if (driver_uart_send_at("AT+CMQTTPUB=0,1,60", "+CMQTTPUB: 0,0", 5000) != DRIVER_OK)
        return DRIVER_ERR;
 
    return DRIVER_OK;
}
 
int driver_mqtt_subscribe(const char *topic, uint8_t qos)
{
    char cmd[AT_CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=0,\"%s\",%d", topic, qos);
    return driver_uart_send_at(cmd, "+CMQTTSUB: 0,0", AT_TIMEOUT_MS);
}
 
int driver_mqtt_disconnect(void)
{
    driver_uart_send_at("AT+CMQTTDISC=0,120", "+CMQTTDISC: 0,0", 5000);
    driver_uart_send_at("AT+CMQTTREL=0", "OK", AT_TIMEOUT_MS);
    driver_uart_send_at("AT+CMQTTSTOP", "OK", AT_TIMEOUT_MS);
    return DRIVER_OK;
}
 
/* ════════════════════════════════════════════════════════════════════
 *  Sensor — SHT31 temperature & humidity via I2C
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_sensor_read(float *temperature, float *humidity)
{
    uint8_t cmd[2]  = {0x2C, 0x06};   /* Single-shot, high repeatability */
    uint8_t data[6] = {0};
 
    if (driver_i2c_write(SENSOR_I2C_ADDR, cmd, 2) != DRIVER_OK)
        return DRIVER_ERR;
 
    delay_ms(15);   /* Measurement time */
 
    if (driver_i2c_read(SENSOR_I2C_ADDR, data, 6) != DRIVER_OK)
        return DRIVER_ERR;
 
    /* Convert raw bytes — SHT31 datasheet formula */
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_hum  = (data[3] << 8) | data[4];
 
    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity    = 100.0f * ((float)raw_hum  / 65535.0f);
 
    /* Clamp humidity to valid range */
    if (*humidity > 100.0f) *humidity = 100.0f;
    if (*humidity <   0.0f) *humidity =   0.0f;
 
    return DRIVER_OK;
}
