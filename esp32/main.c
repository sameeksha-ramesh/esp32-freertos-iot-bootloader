/**
 * @file    main.c
 * @brief   ESP32 IoT Firmware — FreeRTOS + A7672S GSM + MQTT
 * @author  Sameeksha R
 *
 * Initializes FreeRTOS tasks for:
 *  - Sensor data acquisition (temperature, humidity via I2C)
 *  - GSM/MQTT communication via A7672S module over UART
 *  - JSON payload construction and cloud publish
 *
 * Hardware : ESP32-WROOM-32, A7672S GSM Module
 * Toolchain: ESP-IDF / FreeRTOS
 */
 
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver.h"
 
/* ── Configuration ─────────────────────────────────────────────────── */
#define MQTT_BROKER     "broker.hivemq.com"
#define MQTT_PORT       1883
#define MQTT_TOPIC      "iot/sameeksha/sensor"
#define MQTT_CLIENT_ID  "ESP32_NODE_01"
 
#define SENSOR_INTERVAL_MS   5000
#define PUBLISH_INTERVAL_MS  6000
 
/* ── FreeRTOS handles ───────────────────────────────────────────────── */
static QueueHandle_t    sensor_queue;
static SemaphoreHandle_t uart_mutex;
 
/* ── Sensor payload structure ───────────────────────────────────────── */
typedef struct {
    float temperature;
    float humidity;
    uint32_t timestamp_ms;
} sensor_data_t;
 
/* ── Task: Sensor Acquisition ───────────────────────────────────────── */
/**
 * @brief Reads sensor data every SENSOR_INTERVAL_MS and pushes to queue.
 */
static void task_sensor_acquire(void *pvParameters)
{
    sensor_data_t data;
 
    while (1) {
        /* Read temperature and humidity from I2C sensor */
        driver_sensor_read(&data.temperature, &data.humidity);
        data.timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
 
        /* Push to queue — non-blocking, drop if full */
        if (xQueueSend(sensor_queue, &data, 0) != pdTRUE) {
            printf("[SENSOR] Queue full, dropping sample\n");
        } else {
            printf("[SENSOR] T=%.2f°C  H=%.2f%%  t=%lums\n",
                   data.temperature, data.humidity, data.timestamp_ms);
        }
 
        vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
    }
}
 
/* ── Task: MQTT Publish ─────────────────────────────────────────────── */
/**
 * @brief Dequeues sensor data, builds JSON, publishes over GSM/MQTT.
 */
static void task_mqtt_publish(void *pvParameters)
{
    sensor_data_t data;
    char json_buf[256];
 
    /* Connect GSM and open MQTT session */
    if (driver_gsm_init() != DRIVER_OK) {
        printf("[MQTT] GSM init failed — halting publish task\n");
        vTaskDelete(NULL);
    }
 
    if (driver_mqtt_connect(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID) != DRIVER_OK) {
        printf("[MQTT] Broker connect failed\n");
        vTaskDelete(NULL);
    }
 
    printf("[MQTT] Connected to %s:%d\n", MQTT_BROKER, MQTT_PORT);
 
    while (1) {
        /* Wait for sensor sample (block up to PUBLISH_INTERVAL_MS) */
        if (xQueueReceive(sensor_queue, &data, pdMS_TO_TICKS(PUBLISH_INTERVAL_MS)) == pdTRUE) {
 
            /* Build JSON payload */
            snprintf(json_buf, sizeof(json_buf),
                     "{\"client\":\"%s\",\"temperature\":%.2f,"
                     "\"humidity\":%.2f,\"uptime_ms\":%lu}",
                     MQTT_CLIENT_ID,
                     data.temperature,
                     data.humidity,
                     data.timestamp_ms);
 
            /* Acquire UART mutex and publish */
            if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
                driver_mqtt_publish(MQTT_TOPIC, json_buf);
                xSemaphoreGive(uart_mutex);
                printf("[MQTT] Published: %s\n", json_buf);
            }
        }
    }
}
 
/* ── app_main ───────────────────────────────────────────────────────── */
void app_main(void)
{
    printf("\n=== ESP32 IoT Firmware — Sameeksha R ===\n");
    printf("Broker : %s:%d\n", MQTT_BROKER, MQTT_PORT);
    printf("Topic  : %s\n\n", MQTT_TOPIC);
 
    /* Initialise peripherals */
    driver_uart_init();
    driver_i2c_init();
 
    /* Create FreeRTOS primitives */
    sensor_queue = xQueueCreate(10, sizeof(sensor_data_t));
    uart_mutex   = xSemaphoreCreateMutex();
 
    /* Spawn tasks */
    xTaskCreate(task_sensor_acquire, "SensorAcq",  4096, NULL, 5, NULL);
    xTaskCreate(task_mqtt_publish,   "MQTTPublish", 8192, NULL, 4, NULL);
 
    /* app_main returns — scheduler takes over */
}
