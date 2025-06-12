#include "image_capture_handler.h"
#include "config.h"
#include "sensor_manager.h"
#include "lighting_controller.h"
#include "camera_manager.h"
#include "s3_manager.h"
#include "mqtt_manager.h"
#include <Arduino.h>

void image_capture_handler_init()
{
    Serial.println("[IMAGE_HANDLER] 이미지 캡처 핸들러 초기화");
    // 각 모듈은 이미 개별적으로 초기화되어 있어야 함
    Serial.println("[IMAGE_HANDLER] 이미지 캡처 핸들러 초기화 완료");
}

void image_capture_handler_process()
{
    unsigned long localStartTime = millis();
    Serial.println("[CAPTURE] Attempting to capture image...");

    // 현재 조도 값 가져오기
    float ambient = sensor_manager_get_ambient_light();
    if (ambient < 0) {
        ambient = 20.0; // 센서 오류 시 기본값
    }
    
    // 조명 제어 (조도에 따른 자동 켜짐)
    bool light_was_on = lighting_controller_manage(ambient);

    // 카메라 초기화 확인
    if (!camera_manager_is_initialized()) {
        Serial.println("[CAPTURE] Camera not initialized. Attempting to initialize...");
        if (!camera_manager_init()) {
            Serial.println("[CAPTURE] Camera initialization failed. Cannot capture image.");
            
            // 조명 정리
            if (light_was_on && ambient >= LIGHT_NIGHT_THRESHOLD) {
                lighting_controller_force_off();
            }
            
            // MQTT로 실패 통보
            mqtt_manager_publish_image_result(false, "camera_init_failed");
            return;
        }
        Serial.println("[CAPTURE] Camera initialized successfully.");
    }

    // 이미지 캡처 시도
    camera_fb_t *fb = camera_manager_capture_safe(CAMERA_CAPTURE_MAX_RETRIES);

    if (!fb) {
        Serial.println("[CAPTURE] 첫 번째 캡처 실패, 카메라 재초기화 시도...");
        if (camera_manager_reinit()) {
            fb = camera_manager_capture_safe(3);
        }
    }

    if (!fb) {
        Serial.println("[CAPTURE] Failed to get camera frame after retries and reinit.");
        
        // 조명 정리
        if (light_was_on && ambient >= LIGHT_NIGHT_THRESHOLD) {
            lighting_controller_force_off();
        }

        // MQTT로 실패 통보
        mqtt_manager_publish_image_result(false, "camera_capture_failed");
        return;
    }

    // 이미지 캡처 완료 후 조명 끄기 (필요시)
    if (light_was_on && ambient >= LIGHT_NIGHT_THRESHOLD) {
        lighting_controller_force_off();
    }

    Serial.printf("[CAPTURE] Time to capture: %lu ms\n", millis() - localStartTime);
    Serial.printf("[CAPTURE] Image Size: %zu KB\n", fb->len / 1024);
    Serial.println("[CAPTURE] Image captured. Preparing to send to S3...");

    // S3 업로드
    bool uploadResult = s3_manager_upload_image(fb);

    // 프레임 버퍼 해제
    esp_camera_fb_return(fb);

    // MQTT로 완료 통보
    if (uploadResult) {
        mqtt_manager_publish_image_result(true);
    } else {
        mqtt_manager_publish_image_result(false, "s3_upload_failed");
    }
}

void image_capture_handler_process_with_url(const char* url)
{
    Serial.printf("[CAPTURE] 새로운 S3 URL로 이미지 캡처: %s\n", url);
    
    // S3 업로드 URL 설정
    s3_manager_set_upload_url(url);
    
    // 일반 캡처 프로세스 실행
    image_capture_handler_process();
} 