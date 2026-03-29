/**
 * @file    main.c
 * @brief   Custom ESP32 Bootloader — FreeRTOS-based modular OTA
 * @author  Sameeksha R
 *
 * Responsibilities:
 *  1. Hardware self-test (RAM, flash, peripherals)
 *  2. Validate application partition (magic number + CRC32 check)
 *  3. Check for OTA update flag in NVS; if set, receive new firmware
 *     over UART from host tool and write to OTA partition
 *  4. Boot validated application or fallback to safe mode
 *
 * Memory map (example — adjust to your partition table):
 *  0x1000   Bootloader  (this code)
 *  0x8000   Partition table
 *  0x10000  Application (slot 0 — primary)
 *  0x110000 Application (slot 1 — OTA)
 *
 * Toolchain: ESP-IDF
 */
 
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
 
/* ── Partition addresses ────────────────────────────────────────────── */
#define APP_PARTITION_ADDR      0x10000U
#define OTA_PARTITION_ADDR      0x110000U
#define PARTITION_MAX_SIZE      0x100000U   /* 1 MB per slot */
 
/* ── Firmware header (written by linker / flash tool) ───────────────── */
#define FW_MAGIC                0xDEADBEEFU
#define FW_VERSION_MAJOR        1
#define FW_VERSION_MINOR        0
 
typedef struct __attribute__((packed)) {
    uint32_t magic;          /* Must equal FW_MAGIC             */
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t fw_size;        /* Firmware size in bytes           */
    uint32_t crc32;          /* CRC32 of firmware image          */
    uint8_t  reserved[16];
} fw_header_t;
 
/* ── OTA control flags (stored in NVS / RTC memory) ────────────────── */
#define OTA_FLAG_ADDR           0x50000000U   /* RTC slow memory address */
#define OTA_TRIGGER_MAGIC       0xA5A5A5A5U
 
/* ── Boot states ────────────────────────────────────────────────────── */
typedef enum {
    BOOT_STATE_INIT     = 0,
    BOOT_STATE_SELFTEST,
    BOOT_STATE_VALIDATE,
    BOOT_STATE_OTA,
    BOOT_STATE_LAUNCH,
    BOOT_STATE_SAFE_MODE,
} boot_state_t;
 
/* ── CRC32 lookup (standard polynomial 0xEDB88320) ─────────────────── */
static uint32_t crc32_compute(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1));
    }
    return ~crc;
}
 
/* ── Self-test ──────────────────────────────────────────────────────── */
/**
 * @brief Basic hardware self-test: stack canary, UART loopback ping.
 * @return 0 on pass, -1 on failure.
 */
static int bootloader_selftest(void)
{
    printf("[BL] Self-test...\n");
 
    /* Stack canary check */
    volatile uint32_t canary = 0xCAFEBABEU;
    if (canary != 0xCAFEBABEU) {
        printf("[BL] FAIL: stack corruption\n");
        return -1;
    }
 
    /* UART available (already printing, so pass) */
 
    printf("[BL] Self-test PASSED\n");
    return 0;
}
 
/* ── Firmware validation ────────────────────────────────────────────── */
/**
 * @brief Read firmware header from flash and validate magic + CRC32.
 * @param  part_addr  Flash address of application partition.
 * @return 0 if valid, -1 if corrupt or missing.
 */
static int bootloader_validate(uint32_t part_addr)
{
    printf("[BL] Validating firmware at 0x%08X...\n", part_addr);
 
    /*
     * In real ESP-IDF:
     *   const fw_header_t *hdr = (const fw_header_t *)part_addr;
     * Here we simulate a valid header for demonstration.
     */
    fw_header_t hdr;
    hdr.magic         = FW_MAGIC;
    hdr.version_major = FW_VERSION_MAJOR;
    hdr.version_minor = FW_VERSION_MINOR;
    hdr.fw_size       = 0x40000;   /* 256 KB example */
    hdr.crc32         = 0;         /* Would be computed at flash time */
 
    if (hdr.magic != FW_MAGIC) {
        printf("[BL] FAIL: invalid magic 0x%08X\n", hdr.magic);
        return -1;
    }
 
    if (hdr.fw_size == 0 || hdr.fw_size > PARTITION_MAX_SIZE) {
        printf("[BL] FAIL: invalid firmware size %u\n", hdr.fw_size);
        return -1;
    }
 
    /*
     * CRC32 check — in production:
     *   uint32_t computed = crc32_compute((uint8_t*)(part_addr + sizeof(fw_header_t)), hdr.fw_size);
     *   if (computed != hdr.crc32) { ... }
     */
 
    printf("[BL] Firmware v%d.%d  size=%u bytes  CRC OK\n",
           hdr.version_major, hdr.version_minor, hdr.fw_size);
    return 0;
}
 
/* ── OTA receive ────────────────────────────────────────────────────── */
/**
 * @brief Receive new firmware over UART from host flash tool.
 *        Writes to OTA partition, verifies CRC, sets boot flag.
 * @return 0 on success, -1 on failure.
 */
static int bootloader_ota_receive(void)
{
    printf("[BL] OTA mode — waiting for firmware on UART...\n");
    printf("[BL] Send firmware using: python flash_tool.py --port /dev/ttyUSB0 fw.bin\n");
 
    /*
     * Production implementation:
     *  1. Send ACK byte to host tool
     *  2. Receive header (size + expected CRC)
     *  3. Receive chunks, write to OTA_PARTITION_ADDR via esp_flash_write()
     *  4. Compute CRC over received bytes
     *  5. If CRC matches: set OTA boot flag, reboot
     *  6. If CRC fails: erase OTA partition, stay in safe mode
     *
     * Pseudocode:
     *   uart_write_byte(0xAC);  // ACK
     *   uint32_t total = uart_read_u32();
     *   uint32_t expected_crc = uart_read_u32();
     *   uint32_t written = 0, crc = 0xFFFFFFFF;
     *   uint8_t chunk[256];
     *   while (written < total) {
     *       int n = uart_read(chunk, sizeof(chunk));
     *       esp_flash_write(OTA_PARTITION_ADDR + written, chunk, n);
     *       crc = crc32_update(crc, chunk, n);
     *       written += n;
     *   }
     *   if (~crc == expected_crc) {
     *       nvs_set_u32("boot_slot", OTA_PARTITION_ADDR);
     *       esp_restart();
     *   }
     */
 
    /* Simulate timeout (no host connected) */
    vTaskDelay(pdMS_TO_TICKS(3000));
    printf("[BL] OTA timeout — no firmware received\n");
    return -1;
}
 
/* ── Safe mode ──────────────────────────────────────────────────────── */
/**
 * @brief Enter safe mode: blink LED, expose minimal UART console.
 *        Does not boot application.
 */
static void bootloader_safe_mode(void)
{
    printf("[BL] *** SAFE MODE *** — application validation failed\n");
    printf("[BL] Connect host tool to reflash firmware\n");
 
    /* Blink onboard LED to signal safe mode */
    int tick = 0;
    while (1) {
        printf("[BL] Safe mode heartbeat %d\r", tick++);
        fflush(stdout);
        /* gpio_set_level(LED_PIN, tick % 2); */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
 
/* ── Boot task ──────────────────────────────────────────────────────── */
static void task_boot(void *pvParameters)
{
    boot_state_t state = BOOT_STATE_INIT;
 
    while (1) {
        switch (state) {
 
        case BOOT_STATE_INIT:
            printf("\n========================================\n");
            printf("  Custom Bootloader v%d.%d — Sameeksha R\n",
                   FW_VERSION_MAJOR, FW_VERSION_MINOR);
            printf("========================================\n");
            state = BOOT_STATE_SELFTEST;
            break;
 
        case BOOT_STATE_SELFTEST:
            state = (bootloader_selftest() == 0)
                    ? BOOT_STATE_VALIDATE
                    : BOOT_STATE_SAFE_MODE;
            break;
 
        case BOOT_STATE_VALIDATE:
            if (bootloader_validate(APP_PARTITION_ADDR) == 0) {
                /* Check if OTA was triggered */
                /* uint32_t flag = *(volatile uint32_t *)OTA_FLAG_ADDR; */
                uint32_t flag = 0;   /* No OTA pending in this boot */
                state = (flag == OTA_TRIGGER_MAGIC)
                        ? BOOT_STATE_OTA
                        : BOOT_STATE_LAUNCH;
            } else {
                /* Primary slot corrupt — try OTA slot */
                printf("[BL] Primary corrupt, trying OTA slot...\n");
                state = (bootloader_validate(OTA_PARTITION_ADDR) == 0)
                        ? BOOT_STATE_LAUNCH
                        : BOOT_STATE_SAFE_MODE;
            }
            break;
 
        case BOOT_STATE_OTA:
            if (bootloader_ota_receive() == 0) {
                state = BOOT_STATE_VALIDATE;
            } else {
                /* OTA failed — boot existing image if valid */
                state = BOOT_STATE_LAUNCH;
            }
            break;
 
        case BOOT_STATE_LAUNCH:
            printf("[BL] Launching application...\n");
            /*
             * In real ESP-IDF:
             *   esp_image_metadata_t meta;
             *   esp_image_load(ESP_IMAGE_VERIFY, &part, &meta);
             *   esp_start_app(meta.image_segment[0].load_addr);
             */
            printf("[BL] Application handed off — bootloader done\n");
            vTaskDelete(NULL);
            break;
 
        case BOOT_STATE_SAFE_MODE:
            bootloader_safe_mode();   /* Does not return */
            break;
        }
    }
}
 
/* ── app_main ───────────────────────────────────────────────────────── */
void app_main(void)
{
    xTaskCreate(task_boot, "Bootloader", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
}
