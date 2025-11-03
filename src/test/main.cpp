#include <Arduino.h>
#include "esp_camera.h"
#include "esp_system.h"

#define OV3660_PID 0x3660
#define OV3661_PID 0x3661

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32-S3 OV3660 Camera Test ===");
    if (ESP.getPsramSize() > 0) {
        Serial.printf("PSRAM detected: %d bytes\n", ESP.getPsramSize());
    } else {
        Serial.println("No PSRAM detected - camera will likely fail.");
    }
    Serial.printf("Internal heap: %d bytes free\n", ESP.getFreeHeap());
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 11;
    config.pin_d1 = 9;
    config.pin_d2 = 8;
    config.pin_d3 = 10;
    config.pin_d4 = 12;
    config.pin_d5 = 18;
    config.pin_d6 = 17;
    config.pin_d7 = 16;
    config.pin_xclk = 15;
    config.pin_pclk = 13;
    config.pin_vsync = 6;
    config.pin_href = 7;
    config.pin_sccb_sda = 4;
    config.pin_sccb_scl = 5;
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        Serial.printf("Common causes: No PSRAM, wiring error, or incompatible hardware.\n");
        return;
    }
    Serial.println("Camera detected and initialized successfully.");
    sensor_t * s = esp_camera_sensor_get();
    Serial.printf("Sensor PID: 0x%04X\n", s->id.PID);
    if (s->id.PID == OV3660_PID || s->id.PID == OV3661_PID) {
        Serial.println("OV3660/3661 sensor confirmed.");
    } else {
        Serial.printf("Unexpected sensor: 0x%04X\n", s->id.PID);
    }
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed - check power/wiring.");
        return;
    }
    Serial.printf("Capture successful: %u x %u, %u bytes\n", fb->width, fb->height, fb->len);
    esp_camera_fb_return(fb);
    Serial.println("Frame buffer returned. Test complete.");
    Serial.printf("Post-test heap: %d bytes free\n", ESP.getFreeHeap());
}

void loop() {
    delay(5000);
}

