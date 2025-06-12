#include <Arduino.h>
#include "esp_camera.h"
#include <Wire.h>

// ESP32S3_EYE 핀 정의 (대안 1 - 일반적인 ESP32-S3 설정)
#define PWDN_GPIO_NUM   -1              // not used
#define RESET_GPIO_NUM  -1              // not used
#define XCLK_GPIO_NUM   15    
#define SIOD_GPIO_NUM   4              // SDA
#define SIOC_GPIO_NUM   5              // SCL

#define Y2_GPIO_NUM     11
#define Y3_GPIO_NUM     9
#define Y4_GPIO_NUM     8
#define Y5_GPIO_NUM     10
#define Y6_GPIO_NUM     12
#define Y7_GPIO_NUM     18
#define Y8_GPIO_NUM     17
#define Y9_GPIO_NUM     16

#define VSYNC_GPIO_NUM  6
#define HREF_GPIO_NUM   7
#define PCLK_GPIO_NUM   13

void simpleCameraTest() {
    Serial.println("=== 단순 카메라 테스트 시작 ===");
    Serial.flush();
    
    // I2C 스캔 먼저 수행
    Serial.println("I2C 전체 스캔 시작...");
    Wire.begin(SIOD_GPIO_NUM, SIOC_GPIO_NUM);
    Wire.setClock(100000);
    
    int devices_found = 0;
    for (int addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        int result = Wire.endTransmission();
        if (result == 0) {
            Serial.printf("I2C 장치 발견: 0x%02X\n", addr);
            devices_found++;
        }
    }
    
    if (devices_found == 0) {
        Serial.println("I2C 장치를 찾을 수 없음 - 하드웨어 연결 확인 필요");
    } else {
        Serial.printf("총 %d개의 I2C 장치 발견\n", devices_found);
        
        // 알려진 카메라 주소 확인
        bool camera_found = false;
        int camera_addresses[] = {0x30, 0x21, 0x3C, 0x60}; // 일반적인 카메라 주소들
        for (int i = 0; i < 4; i++) {
            Wire.beginTransmission(camera_addresses[i]);
            if (Wire.endTransmission() == 0) {
                Serial.printf("가능한 카메라 센서 발견: 0x%02X\n", camera_addresses[i]);
                camera_found = true;
            }
        }
        
        if (!camera_found) {
            Serial.println("⚠️  알려진 카메라 센서 주소에서 장치를 찾을 수 없음");
            Serial.println("   이 보드에는 카메라 모듈이 없을 수 있습니다.");
        }
    }
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 6000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA; // 작은 크기부터 시작
    config.jpeg_quality = 40;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM; // PSRAM 문제 방지
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    Serial.println("카메라 설정 완료, 초기화 시도 중...");
    Serial.flush();

    esp_err_t err = esp_camera_init(&config);
    Serial.printf("esp_camera_init 결과: 0x%x\n", err);
    
    if (err != ESP_OK) {
        Serial.printf("카메라 초기화 실패: 0x%x\n", err);
        return;
    }
    
    Serial.println("카메라 초기화 성공! 테스트 촬영 시도...");
    
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("테스트 촬영 실패");
    } else {
        Serial.printf("테스트 촬영 성공! 크기: %zu bytes, 해상도: %dx%d\n", 
                      fb->len, fb->width, fb->height);
        esp_camera_fb_return(fb);
    }
    
    Serial.println("=== 단순 카메라 테스트 완료 ===");
}

void alternativeCameraTest() {
    Serial.println("=== 대안 카메라 설정 테스트 ===");
    
    // 대안 설정 (FREENOVE ESP32-S3-WROOM 등)
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 4;
    config.pin_d1 = 5;
    config.pin_d2 = 18;
    config.pin_d3 = 19;
    config.pin_d4 = 36;
    config.pin_d5 = 39;
    config.pin_d6 = 34;
    config.pin_d7 = 35;
    config.pin_xclk = 27;
    config.pin_pclk = 21;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_sccb_sda = 26;
    config.pin_sccb_scl = 27;
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 40;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    Serial.println("대안 설정으로 초기화 시도...");
    esp_err_t err = esp_camera_init(&config);
    Serial.printf("대안 설정 결과: 0x%x\n", err);
    
    if (err == ESP_OK) {
        Serial.println("대안 설정 성공! 촬영 시도...");
        camera_fb_t * fb = esp_camera_fb_get();
        if (fb) {
            Serial.printf("대안 설정 촬영 성공! 크기: %zu bytes\n", fb->len);
            esp_camera_fb_return(fb);
        } else {
            Serial.println("대안 설정 촬영 실패");
        }
        esp_camera_deinit();
    }
    
    Serial.println("=== 대안 카메라 설정 테스트 완료 ===");
} 