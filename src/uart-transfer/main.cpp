#include <Arduino.h>
#include "esp_camera.h"
#include "esp_system.h"
#include <stdint.h>
#include "crc32_table.h"

#define OV3660_PID 0x3660
#define OV3661_PID 0x3661
#define SERIAL_BAUD 115200
#define MAGIC 0xA5A5A5A5u
#define FRAME_HEADER 0x01
#define CHUNK_PACKET 0x02
#define CHUNK_SIZE 512

uint32_t compute_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) {
        crc = (crc >> 8) ^ crc_table[(crc ^ *data++) & 0xFFu];
    }
    return crc ^ 0xFFFFFFFFu;
}

void write_le32(uint32_t v)
{
    uint8_t b[4] = { (uint8_t)(v & 0xFFu), (uint8_t)((v >> 8) & 0xFFu), (uint8_t)((v >> 16) & 0xFFu), (uint8_t)((v >> 24) & 0xFFu) };
    Serial.write(b, 4);
}

void write_le16(uint16_t v)
{
    uint8_t b[2] = { (uint8_t)(v & 0xFFu), (uint8_t)((v >> 8) & 0xFFu) };
    Serial.write(b, 2);
}

void send_frame_header(uint32_t total_size, uint16_t chunk_size, uint16_t total_chunks)
{
    uint8_t header_magic[4] = { (uint8_t)(MAGIC & 0xFFu), (uint8_t)((MAGIC >> 8) & 0xFFu), (uint8_t)((MAGIC >> 16) & 0xFFu), (uint8_t)((MAGIC >> 24) & 0xFFu) };
    Serial.write(header_magic, 4);
    Serial.write((uint8_t)FRAME_HEADER);
    write_le32(total_size);
    write_le16(chunk_size);
    write_le16(total_chunks);
}

void send_chunk_packet(uint16_t chunk_idx, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t magic_bytes[4] = { (uint8_t)(MAGIC & 0xFFu), (uint8_t)((MAGIC >> 8) & 0xFFu), (uint8_t)((MAGIC >> 16) & 0xFFu), (uint8_t)((MAGIC >> 24) & 0xFFu) };
    Serial.write(magic_bytes, 4);
    Serial.write((uint8_t)CHUNK_PACKET);
    write_le16(chunk_idx);
    write_le16(payload_len);
    uint32_t crc = compute_crc32(payload, payload_len);
    write_le32(crc);
    Serial.write(payload, payload_len);
    Serial.flush();
}

esp_err_t init_camera()
{
    camera_config_t cam_config;
    cam_config.ledc_channel = LEDC_CHANNEL_0;
    cam_config.ledc_timer = LEDC_TIMER_0;
    cam_config.pin_d0 = 11;
    cam_config.pin_d1 = 9;
    cam_config.pin_d2 = 8;
    cam_config.pin_d3 = 10;
    cam_config.pin_d4 = 12;
    cam_config.pin_d5 = 18;
    cam_config.pin_d6 = 17;
    cam_config.pin_d7 = 16;
    cam_config.pin_xclk = 15;
    cam_config.pin_pclk = 13;
    cam_config.pin_vsync = 6;
    cam_config.pin_href = 7;
    cam_config.pin_sccb_sda = 4;
    cam_config.pin_sccb_scl = 5;
    cam_config.pin_pwdn = -1;
    cam_config.pin_reset = -1;
    cam_config.xclk_freq_hz = 20000000;
    cam_config.pixel_format = PIXFORMAT_JPEG;
    cam_config.frame_size = FRAMESIZE_QXGA;
    cam_config.jpeg_quality = 10;
    cam_config.fb_count = 2;
    if (psramFound()) {
        cam_config.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        cam_config.fb_location = CAMERA_FB_IN_DRAM;
    }
    esp_err_t r = esp_camera_init(&cam_config);
    if (r != ESP_OK) return r;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, 1);
        s->set_hmirror(s, 0);
    }
    return ESP_OK;
}

void capture_and_send_frame()
{
    sensor_t *s = esp_camera_sensor_get();
    s->set_framesize(s, FRAMESIZE_QXGA);
    for (int i = 0; i < 3; ++i) {
        camera_fb_t *old_fb = esp_camera_fb_get();
        if (old_fb) esp_camera_fb_return(old_fb);
        delay(30);
    }
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;
    uint32_t total_size = fb->len;
    uint16_t payload_chunk = CHUNK_SIZE;
    uint16_t total_chunks = (total_size + payload_chunk - 1) / payload_chunk;
    send_frame_header(total_size, payload_chunk, total_chunks);
    const uint8_t *ptr = fb->buf;
    for (uint16_t i = 0; i < total_chunks; ++i) {
        uint16_t this_len = (uint16_t)min((uint32_t)payload_chunk, (uint32_t)(total_size - (uint32_t)i * payload_chunk));
        send_chunk_packet(i, ptr + (uint32_t)i * payload_chunk, this_len);
        delay(5);
    }
    esp_camera_fb_return(fb);
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(200);
    esp_err_t r = init_camera();
    if (r != ESP_OK) {
        while (true) {
            delay(1000);
        }
    }
}

void loop()
{
    if (Serial.available()) {
        int c = Serial.read();
        if (c == 'R') {
            Serial.write((uint8_t)0xFE);
            delay(50);
            capture_and_send_frame();
        }
    }
    delay(10);
}
