/**
 * @file    driver.c
 * @brief   Driver implementation — Raspberry Pi IoT Node
 * @author  Sameeksha R
 *
 * Uses Linux userspace APIs:
 *  - termios  for UART (/dev/ttyUSB0)
 *  - i2c-dev  for I2C  (/dev/i2c-1)
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "driver.h"
 
/* ── Internal state ─────────────────────────────────────────────────── */
static int uart_fd = -1;
static int i2c_fd  = -1;
 
/* ── Internal helper: ms sleep ──────────────────────────────────────── */
static void delay_ms(uint32_t ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
 
/* ════════════════════════════════════════════════════════════════════
 *  UART
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_uart_init(void)
{
    uart_fd = open(UART_DEVICE, O_RDWR | O_NOCTTY | O_SYNC);
    if (uart_fd < 0) {
        perror("[UART] open");
        return DRIVER_ERR;
    }
 
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(uart_fd, &tty) != 0) {
        perror("[UART] tcgetattr");
        return DRIVER_ERR;
    }
 
    cfsetospeed(&tty, UART_BAUD_RATE);
    cfsetispeed(&tty, UART_BAUD_RATE);
 
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  /* 8-bit chars */
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag  = 0;
    tty.c_oflag  = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 10;   /* 1 second read timeout */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
 
    if (tcsetattr(uart_fd, TCSANOW, &tty) != 0) {
        perror("[UART] tcsetattr");
        return DRIVER_ERR;
    }
 
    printf("[UART] Opened %s\n", UART_DEVICE);
    return DRIVER_OK;
}
 
int driver_uart_send_at(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    if (uart_fd < 0) return DRIVER_ERR;
 
    char full_cmd[AT_CMD_MAX_LEN];
    snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);
    write(uart_fd, full_cmd, strlen(full_cmd));
    printf("[AT >>] %s", full_cmd);
 
    char    rx[UART_BUF_SIZE] = {0};
    int     pos     = 0;
    uint32_t waited = 0;
 
    while (waited < timeout_ms) {
        int n = read(uart_fd, rx + pos, sizeof(rx) - pos - 1);
        if (n > 0) {
            pos += n;
            rx[pos] = '\0';
            if (expected && strstr(rx, expected)) {
                printf("[AT <<] %s\n", rx);
                return DRIVER_OK;
            }
        }
        delay_ms(50);
        waited += 50;
    }
 
    printf("[AT] Timeout for '%s'\n", expected ? expected : "OK");
    return DRIVER_ERR;
}
 
int driver_uart_read(char *buf, int maxlen)
{
    if (uart_fd < 0) return DRIVER_ERR;
    int n = read(uart_fd, buf, maxlen - 1);
    if (n > 0) buf[n] = '\0';
    else        buf[0] = '\0';
    return n > 0 ? n : DRIVER_ERR;
}
 
/* ════════════════════════════════════════════════════════════════════
 *  I2C
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_i2c_init(void)
{
    i2c_fd = open(I2C_DEVICE, O_RDWR);
    if (i2c_fd < 0) {
        perror("[I2C] open");
        return DRIVER_ERR;
    }
    printf("[I2C] Opened %s\n", I2C_DEVICE);
    return DRIVER_OK;
}
 
int driver_i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    if (i2c_fd < 0) return DRIVER_ERR;
    if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
        perror("[I2C] ioctl");
        return DRIVER_ERR;
    }
    if (write(i2c_fd, data, len) != (ssize_t)len) {
        perror("[I2C] write");
        return DRIVER_ERR;
    }
    return DRIVER_OK;
}
 
int driver_i2c_read(uint8_t addr, uint8_t *buf, size_t len)
{
    if (i2c_fd < 0) return DRIVER_ERR;
    if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
        perror("[I2C] ioctl");
        return DRIVER_ERR;
    }
    if (read(i2c_fd, buf, len) != (ssize_t)len) {
        perror("[I2C] read");
        return DRIVER_ERR;
    }
    return DRIVER_OK;
}
 
/* ════════════════════════════════════════════════════════════════════
 *  GSM — A7672S (same AT command set as ESP32 version)
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_gsm_init(void)
{
    printf("[GSM] Initialising A7672S...\n");
 
    if (driver_uart_send_at("AT",    "OK",    AT_TIMEOUT_MS) != DRIVER_OK) return DRIVER_ERR;
    driver_uart_send_at("ATE0",      "OK",    AT_TIMEOUT_MS);
 
    if (driver_uart_send_at("AT+CPIN?", "READY", AT_TIMEOUT_MS) != DRIVER_OK) {
        fprintf(stderr, "[GSM] SIM not ready\n");
        return DRIVER_ERR;
    }
 
    for (int i = 0; i < 10; i++) {
        if (driver_uart_send_at("AT+CREG?", "+CREG: 0,1", AT_TIMEOUT_MS) == DRIVER_OK) {
            printf("[GSM] Network registered\n");
            break;
        }
        delay_ms(2000);
    }
 
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
        strncpy(imei_buf, "000000000000000", 16);
        return DRIVER_OK;
    }
    return DRIVER_ERR;
}
 
/* ════════════════════════════════════════════════════════════════════
 *  MQTT
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_mqtt_connect(const char *broker, uint16_t port, const char *client_id)
{
    char cmd[AT_CMD_MAX_LEN];
 
    if (driver_uart_send_at("AT+CMQTTSTART", "OK", 5000) != DRIVER_OK) return DRIVER_ERR;
 
    snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\"", client_id);
    if (driver_uart_send_at(cmd, "OK", AT_TIMEOUT_MS) != DRIVER_OK) return DRIVER_ERR;
 
    snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1", broker, port);
    if (driver_uart_send_at(cmd, "+CMQTTCONNECT: 0,0", 10000) != DRIVER_OK) return DRIVER_ERR;
 
    printf("[MQTT] Connected to %s:%d\n", broker, port);
    return DRIVER_OK;
}
 
int driver_mqtt_publish(const char *topic, const char *payload)
{
    char cmd[AT_CMD_MAX_LEN];
 
    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%zu", strlen(topic));
    if (driver_uart_send_at(cmd, ">", AT_TIMEOUT_MS) != DRIVER_OK) return DRIVER_ERR;
    driver_uart_send_at(topic, "OK", AT_TIMEOUT_MS);
 
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%zu", strlen(payload));
    if (driver_uart_send_at(cmd, ">", AT_TIMEOUT_MS) != DRIVER_OK) return DRIVER_ERR;
    driver_uart_send_at(payload, "OK", AT_TIMEOUT_MS);
 
    return driver_uart_send_at("AT+CMQTTPUB=0,1,60", "+CMQTTPUB: 0,0", 5000);
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
    driver_uart_send_at("AT+CMQTTREL=0",      "OK", AT_TIMEOUT_MS);
    driver_uart_send_at("AT+CMQTTSTOP",        "OK", AT_TIMEOUT_MS);
    return DRIVER_OK;
}
 
/* ════════════════════════════════════════════════════════════════════
 *  Sensor — SHT31 via Linux i2c-dev
 * ════════════════════════════════════════════════════════════════════ */
 
int driver_sensor_read(float *temperature, float *humidity)
{
    uint8_t cmd[2]  = {0x2C, 0x06};
    uint8_t data[6] = {0};
 
    if (driver_i2c_write(SENSOR_I2C_ADDR, cmd, 2) != DRIVER_OK) return DRIVER_ERR;
    delay_ms(15);
    if (driver_i2c_read(SENSOR_I2C_ADDR, data, 6) != DRIVER_OK) return DRIVER_ERR;
 
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_hum  = (data[3] << 8) | data[4];
 
    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity    = 100.0f * ((float)raw_hum  / 65535.0f);
 
    if (*humidity > 100.0f) *humidity = 100.0f;
    if (*humidity <   0.0f) *humidity =   0.0f;
 
    return DRIVER_OK;
}
