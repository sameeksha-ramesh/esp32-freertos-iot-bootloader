/**
 * @file    main.c
 * @brief   Raspberry Pi IoT Node — Sensor Acquisition + MQTT Publish
 * @author  Sameeksha R
 *
 * Reads sensor data over I2C (via Linux i2c-dev) and publishes
 * JSON payloads to an MQTT broker using the A7672S GSM module
 * connected over UART (/dev/ttyUSB0).
 *
 * Compile:
 *   gcc main.c driver.c -o iot_node -lpthread -lm
 *
 * Run:
 *   sudo ./iot_node
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "driver.h"
 
/* ── Configuration ─────────────────────────────────────────────────── */
#define MQTT_BROKER       "broker.hivemq.com"
#define MQTT_PORT         1883
#define MQTT_TOPIC        "iot/sameeksha/rpi/sensor"
#define MQTT_CLIENT_ID    "RPI_NODE_01"
#define SENSOR_INTERVAL_S 5
 
/* ── Shared sensor data (mutex protected) ───────────────────────────── */
typedef struct {
    float    temperature;
    float    humidity;
    uint64_t timestamp_s;
} sensor_data_t;
 
static sensor_data_t g_sensor  = {0};
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int    g_running = 1;
 
/* ── Utility: current Unix timestamp ────────────────────────────────── */
static uint64_t now_epoch(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec;
}
 
/* ── Thread: Sensor Acquisition ─────────────────────────────────────── */
static void *thread_sensor(void *arg)
{
    (void)arg;
    sensor_data_t local;
 
    while (g_running) {
        if (driver_sensor_read(&local.temperature, &local.humidity) == DRIVER_OK) {
            local.timestamp_s = now_epoch();
 
            pthread_mutex_lock(&g_mutex);
            g_sensor = local;
            pthread_mutex_unlock(&g_mutex);
 
            printf("[SENSOR] T=%.2f°C  H=%.2f%%  epoch=%llu\n",
                   local.temperature, local.humidity,
                   (unsigned long long)local.timestamp_s);
        } else {
            fprintf(stderr, "[SENSOR] Read error\n");
        }
        sleep(SENSOR_INTERVAL_S);
    }
    return NULL;
}
 
/* ── Thread: MQTT Publish ────────────────────────────────────────────── */
static void *thread_publish(void *arg)
{
    (void)arg;
    char json[256];
    sensor_data_t snap;
 
    /* Connect GSM and MQTT */
    if (driver_gsm_init() != DRIVER_OK) {
        fprintf(stderr, "[MQTT] GSM init failed\n");
        g_running = 0;
        return NULL;
    }
    if (driver_mqtt_connect(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID) != DRIVER_OK) {
        fprintf(stderr, "[MQTT] Broker connect failed\n");
        g_running = 0;
        return NULL;
    }
    printf("[MQTT] Connected to %s:%d\n", MQTT_BROKER, MQTT_PORT);
 
    while (g_running) {
        sleep(SENSOR_INTERVAL_S + 1);   /* slightly offset from sensor thread */
 
        pthread_mutex_lock(&g_mutex);
        snap = g_sensor;
        pthread_mutex_unlock(&g_mutex);
 
        if (snap.timestamp_s == 0) continue;   /* no reading yet */
 
        snprintf(json, sizeof(json),
                 "{\"client\":\"%s\",\"temperature\":%.2f,"
                 "\"humidity\":%.2f,\"epoch\":%llu}",
                 MQTT_CLIENT_ID,
                 snap.temperature,
                 snap.humidity,
                 (unsigned long long)snap.timestamp_s);
 
        if (driver_mqtt_publish(MQTT_TOPIC, json) == DRIVER_OK) {
            printf("[MQTT] Published: %s\n", json);
        } else {
            fprintf(stderr, "[MQTT] Publish failed — attempting reconnect\n");
            driver_mqtt_connect(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID);
        }
    }
 
    driver_mqtt_disconnect();
    return NULL;
}
 
/* ── main ────────────────────────────────────────────────────────────── */
int main(void)
{
    printf("\n=== Raspberry Pi IoT Node — Sameeksha R ===\n");
    printf("Broker : %s:%d\n", MQTT_BROKER, MQTT_PORT);
    printf("Topic  : %s\n\n",  MQTT_TOPIC);
 
    /* Initialise peripherals */
    if (driver_uart_init() != DRIVER_OK) {
        fprintf(stderr, "[MAIN] UART init failed\n");
        return EXIT_FAILURE;
    }
    if (driver_i2c_init() != DRIVER_OK) {
        fprintf(stderr, "[MAIN] I2C init failed\n");
        return EXIT_FAILURE;
    }
 
    /* Launch threads */
    pthread_t t_sensor, t_publish;
    pthread_create(&t_sensor,  NULL, thread_sensor,  NULL);
    pthread_create(&t_publish, NULL, thread_publish, NULL);
 
    pthread_join(t_sensor,  NULL);
    pthread_join(t_publish, NULL);
 
    printf("[MAIN] Shutdown complete\n");
    return EXIT_SUCCESS;
}
