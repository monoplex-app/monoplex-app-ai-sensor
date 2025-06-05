#include "camera_manager.h"
#include "config.h"      // 카메라 모델 정의가 필요
#include "camera_pins.h" // 핀 정의 (config.h 다음에 와야 함)
#include <Arduino.h>
#include <BLEDevice.h>
#include <Wire.h>
#include "esp_heap_caps.h"

// 내부 상태 변수
static bool g_camera_initialized = false;
static camera_config_t s_camera_config;

// 메모리 상태 체크 함수
static void check_memory()
{
    Serial.printf("[MEM] Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("[MEM] Free PSRAM: %u bytes\n", ESP.getFreePsram());
    Serial.printf("[MEM] Min free heap: %u bytes\n", ESP.getMinFreeHeap());
    Serial.printf("[MEM] Max alloc heap: %u bytes\n", ESP.getMaxAllocHeap());
}

// 카메라 설정 최적화 (내부 함수)
static void optimize_camera_config(camera_config_t *config)
{
    // 메모리 사용량 최적화
    config->frame_size = FRAMESIZE_UXGA;    // 800x600 (UXGA 대신)
    config->jpeg_quality = 32;              // 품질 조정 (10-63, 낮을수록 고품질)
    config->fb_count = 2;                   // 프레임 버퍼 개수 최소화
    config->grab_mode = CAMERA_GRAB_LATEST; // 최신 프레임만 사용

    // PSRAM 사용 설정
    if (psramFound()) {
        config->fb_location = CAMERA_FB_IN_PSRAM;
        Serial.println("[CAMERA] PSRAM 사용 설정 (최적화)");
    } else {
        config->fb_location = CAMERA_FB_IN_DRAM;
        Serial.println("[CAMERA] DRAM 사용 설정 (최적화)");
    }
}

// 카메라 시스템 초기화 (공개 함수) - 단순화된 버전
bool camera_init_system()
{
    Serial.println(F("[CAMERA] 카메라 시스템 초기화 시작"));
    
    // I2C 충돌 방지를 위한 안정화 대기
    Serial.println(F("[CAMERA] 시스템 안정화 대기"));
    delay(500);
    
    // 하드웨어 상태 디버그 정보 출력
    Serial.println(F("[CAMERA] === 카메라 하드웨어 디버그 정보 ==="));
    Serial.printf("[CAMERA] PSRAM 상태: %s\n", psramFound() ? "감지됨" : "없음");
    Serial.printf("[CAMERA] PSRAM 크기: %d bytes\n", ESP.getPsramSize());
    Serial.printf("[CAMERA] Free heap: %d bytes\n", ESP.getFreeHeap());
    
    // 핀 설정 정보 출력
    Serial.println(F("[CAMERA] === 핀 설정 정보 ==="));
    Serial.printf("[CAMERA] XCLK: %d, PCLK: %d\n", XCLK_GPIO_NUM, PCLK_GPIO_NUM);
    Serial.printf("[CAMERA] VSYNC: %d, HREF: %d\n", VSYNC_GPIO_NUM, HREF_GPIO_NUM);
    Serial.printf("[CAMERA] SDA(SIOD): %d, SCL(SIOC): %d\n", SIOD_GPIO_NUM, SIOC_GPIO_NUM);
    Serial.printf("[CAMERA] PWDN: %d, RESET: %d\n", PWDN_GPIO_NUM, RESET_GPIO_NUM);
    Serial.printf("[CAMERA] 센서 I2C - SDA: %d, SCL: %d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    
    // I2C 핀 충돌 검사
    if (SIOD_GPIO_NUM == I2C_SDA_PIN || SIOC_GPIO_NUM == I2C_SCL_PIN) {
        Serial.println(F("[CAMERA] ⚠️ 경고: 카메라와 센서 I2C 핀이 충돌합니다!"));
        Serial.printf("[CAMERA] 카메라 I2C: SDA=%d, SCL=%d\n", SIOD_GPIO_NUM, SIOC_GPIO_NUM);
        Serial.printf("[CAMERA] 센서 I2C: SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    }
    
    // 카메라 핀 설정
    s_camera_config.ledc_channel = LEDC_CHANNEL_0;
    s_camera_config.ledc_timer = LEDC_TIMER_0;
    s_camera_config.pin_d0 = Y2_GPIO_NUM;
    s_camera_config.pin_d1 = Y3_GPIO_NUM;
    s_camera_config.pin_d2 = Y4_GPIO_NUM;
    s_camera_config.pin_d3 = Y5_GPIO_NUM;
    s_camera_config.pin_d4 = Y6_GPIO_NUM;
    s_camera_config.pin_d5 = Y7_GPIO_NUM;
    s_camera_config.pin_d6 = Y8_GPIO_NUM;
    s_camera_config.pin_d7 = Y9_GPIO_NUM;
    s_camera_config.pin_xclk = XCLK_GPIO_NUM;
    s_camera_config.pin_pclk = PCLK_GPIO_NUM;
    s_camera_config.pin_vsync = VSYNC_GPIO_NUM;
    s_camera_config.pin_href = HREF_GPIO_NUM;
    s_camera_config.pin_sccb_sda = SIOD_GPIO_NUM;
    s_camera_config.pin_sccb_scl = SIOC_GPIO_NUM;
    s_camera_config.pin_pwdn = PWDN_GPIO_NUM;
    s_camera_config.pin_reset = RESET_GPIO_NUM;

    // 기본 카메라 설정
    s_camera_config.xclk_freq_hz = 6000000;
    s_camera_config.frame_size = FRAMESIZE_UXGA;
    s_camera_config.pixel_format = PIXFORMAT_JPEG;
    s_camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    // 설정 최적화 적용
    optimize_camera_config(&s_camera_config);

    // PSRAM 감지 및 추가 설정
    if (psramFound()) {
        Serial.println(F("[CAMERA] PSRAM 감지됨. 프레임버퍼에 PSRAM 사용."));
    } else {
        Serial.println(F("[CAMERA] PSRAM 없음. 프레임버퍼에 DRAM 사용."));
    }

    // 카메라 초기화 전 추가 안정화 대기
    Serial.println(F("[CAMERA] 카메라 하드웨어 초기화 준비..."));
    delay(200);

    // 카메라 초기화
    esp_err_t err = esp_camera_init(&s_camera_config);
    if (err != ESP_OK) {
        Serial.printf("[CAMERA] 카메라 초기화 실패, 오류 0x%x\n", err);
        
        // 오류 코드별 상세 정보 출력
        switch(err) {
            case ESP_ERR_NOT_FOUND:
                Serial.println(F("[CAMERA] ❌ ESP_ERR_NOT_FOUND: 카메라 하드웨어를 찾을 수 없습니다."));
                Serial.println(F("[CAMERA] 가능한 원인:"));
                Serial.println(F("[CAMERA] - 카메라 모듈이 물리적으로 연결되지 않음"));
                Serial.println(F("[CAMERA] - 핀 설정이 하드웨어와 일치하지 않음"));
                Serial.println(F("[CAMERA] - 카메라 모듈 불량"));
                Serial.println(F("[CAMERA] - I2C 주소 충돌"));
                break;
            case ESP_ERR_NO_MEM:
                Serial.println(F("[CAMERA] ❌ ESP_ERR_NO_MEM: 메모리 부족"));
                break;
            case ESP_ERR_INVALID_ARG:
                Serial.println(F("[CAMERA] ❌ ESP_ERR_INVALID_ARG: 잘못된 인수"));
                break;
            default:
                Serial.printf("[CAMERA] ❌ 알 수 없는 오류: 0x%x\n", err);
                break;
        }
        
        return false;
    }
    
    Serial.println(F("[CAMERA] esp_camera_init 성공."));
    
    // 카메라 초기화 후 센서 안정화 대기
    delay(300);

    // 센서 설정
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        Serial.println(F("[CAMERA] 카메라 센서 가져오기 실패!"));
        return false;
    }
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
    Serial.println(F("[CAMERA] 카메라 센서 설정 적용 완료."));
    
    // 센서 설정 후 안정화 대기
    delay(200);
    
    // 카메라 초기화 성공 후 I2C 센서 재초기화
    Serial.println(F("[CAMERA] 센서 I2C 재연결"));
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // 센서용 I2C 핀으로 복원
    delay(100);
    
    // I2C 재연결 후 최종 안정화
    delay(200);
    
    return true;
}

// 카메라 시스템 종료 (공개 함수)
void camera_deinit_system()
{
    Serial.println(F("[CAMERA] 카메라 시스템 종료 중..."));
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        Serial.printf("[CAMERA] 카메라 종료 오류: 0x%x\n", err);
    } else {
        Serial.println(F("[CAMERA] 카메라 시스템 종료 완료"));
    }
}

// 공개 인터페이스 함수들
bool camera_manager_init()
{
    Serial.println("[CAMERA] 카메라 관리 모듈 초기화 시작");
    delay(1000);
    
    check_memory();
    
    g_camera_initialized = camera_init_system();
    
    if (g_camera_initialized) {
        Serial.println("[CAMERA] 카메라 시스템 초기화 성공");
    } else {
        Serial.println("[CAMERA] 카메라 시스템 초기화 실패");
    }
    
    return g_camera_initialized;
}

bool camera_manager_reinit()
{
    Serial.println("[CAMERA] 카메라 재초기화 시작...");

    esp_camera_deinit();
    delay(1000);

    ESP.getFreeHeap();
    check_memory();

    g_camera_initialized = camera_init_system();
    
    if (g_camera_initialized) {
        Serial.println("[CAMERA] 카메라 재초기화 성공");
    } else {
        Serial.println("[CAMERA] 카메라 재초기화 실패");
    }

    return g_camera_initialized;
}

camera_fb_t* camera_manager_capture_safe(int max_retries)
{
    check_memory();

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        Serial.println("[CAMERA] 센서 포인터 없음!");
        return nullptr;
    }

    for (int i = 0; i < max_retries; i++) {
        Serial.printf("[CAMERA] Capturing image (attempt %d/%d)...\n", i + 1, max_retries);

        if (i > 0) {
            delay(500);
        }

        if (i > 1) {
            ESP.getFreeHeap();
            delay(100);
        }

        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            if (fb->len > 0 && fb->buf != nullptr) {
                Serial.printf("[CAMERA] Image captured: %dx%d, size: %zu bytes\n",
                              fb->width, fb->height, fb->len);
                return fb;
            } else {
                Serial.println("[CAMERA] 유효하지 않은 프레임 데이터");
                esp_camera_fb_return(fb);
            }
        } else {
            Serial.println("[CAMERA] esp_camera_fb_get() failed.");
        }

        if (i == max_retries - 2) {
            Serial.println("[CAMERA] 센서 설정 재적용 시도...");
            s->set_pixformat(s, PIXFORMAT_JPEG);
            s->set_framesize(s, FRAMESIZE_SVGA);
            s->set_quality(s, 15);
            s->set_brightness(s, 0);
            s->set_contrast(s, 0);
            s->set_saturation(s, 0);
            s->set_whitebal(s, 1);
            s->set_awb_gain(s, 1);
            s->set_exposure_ctrl(s, 1);
            s->set_gain_ctrl(s, 1);
            delay(300);
        }
    }

    Serial.println("[CAMERA] Camera capture failed after all attempts.");
    return nullptr;
}

bool camera_manager_is_initialized()
{
    return g_camera_initialized;
}

void camera_manager_reinit_with_ble_safety()
{
    Serial.println(F("[CAMERA] BLE 안전 모드 카메라 재초기화 시작..."));
    
    BLEDevice::stopAdvertising();
    delay(500);
    
    Serial.printf("[MEM] Free heap before camera: %d bytes\n", 
                  heap_caps_get_free_size(MALLOC_CAP_8BIT));
    
    // 카메라 시스템 완전 종료
    camera_deinit_system();
    delay(1000); // 더 긴 대기로 완전한 종료 보장
    
    // 시스템 안정화를 위한 추가 대기
    Serial.println(F("[CAMERA] 시스템 안정화 대기 중..."));
    delay(500);
    
    g_camera_initialized = camera_init_system();
    
    if (g_camera_initialized) {
        Serial.println(F("[CAMERA] 카메라 시스템 재초기화 성공."));
    } else {
        Serial.println(F("[CAMERA] 카메라 시스템 재초기화 실패!"));
    }
    
    delay(200); // BLE 재시작 전 안정화
    BLEDevice::startAdvertising();
    Serial.println(F("[CAMERA] BLE 안전 모드 카메라 재초기화 완료"));
} 