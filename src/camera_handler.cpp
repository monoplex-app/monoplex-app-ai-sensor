#include "camera_handler.h"
#include "config.h" // 핀 번호 등 설정 값을 위해 포함
#include "esp_camera.h"
#include "WiFiClientSecure.h" // S3 업로드를 위해 필요
#include <Wire.h> // I2C 통신을 위해 필요

// 이 모듈이 상호작용해야 하는 다른 핸들러 포함
#include "wifi_handler.h" 
#include "sensor_handler.h" // 조도 센서 값을 얻기 위해 필요
#include "mqtt_handler.h"   // 업로드 완료 후 MQTT 메시지를 보내기 위해 필요

// 모듈 내부에서만 사용할 함수 (업로드 로직)
static bool uploadImageToS3(camera_fb_t* fb, const String& url);

bool initCamera() {
    Serial.println("=== 카메라 초기화 시작 ===");
    Serial.flush(); // 즉시 출력
    delay(10);
    
    // 카메라 모델 확인
    #ifdef CAMERA_MODEL_ESP32S3_EYE
    Serial.println("카메라 모델: ESP32S3_EYE");
    #else
    Serial.println("카메라 모델: 알 수 없음");
    #endif
    Serial.flush();
    
    // 핀 설정 확인
    Serial.printf("카메라 핀 설정:\n");
    Serial.printf("  XCLK: %d, SIOD: %d, SIOC: %d\n", XCLK_GPIO_NUM, SIOD_GPIO_NUM, SIOC_GPIO_NUM);
    Serial.printf("  VSYNC: %d, HREF: %d, PCLK: %d\n", VSYNC_GPIO_NUM, HREF_GPIO_NUM, PCLK_GPIO_NUM);
    Serial.printf("  PWDN: %d, RESET: %d\n", PWDN_GPIO_NUM, RESET_GPIO_NUM);
    
    camera_config_t config;
    Serial.println("camera_config_t 구조체 생성 완료");
    
    // 카메라 설정
    Serial.println("카메라 기본 설정 시작...");
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
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    Serial.println("핀 설정 완료, PSRAM 확인 중...");
    
    // PSRAM에 따른 설정 분기
    if(psramFound()){
        Serial.println("PSRAM 감지됨 - 카메라 촬영 실패 방지 모드로 설정");
        
        // ========= 카메라 촬영 실패 방지 설정 =========
        config.frame_size = FRAMESIZE_UXGA;          // 1600x1200, PSRAM 환경에서 고품질
        config.jpeg_quality = 10;   // 32(보라색 필터 효과)
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST; // CAMERA_GRAB_WHEN_EMPTY
        config.fb_location = CAMERA_FB_IN_PSRAM;
        // ============================================
    } else {
        Serial.println("PSRAM 없음 - 낮은 품질 설정 사용");
        config.frame_size = FRAMESIZE_SVGA;           // 800x600
        config.jpeg_quality = 24;                     // 메모리 제약 내에서 품질 상향
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }
    Serial.println("카메라 설정 분기 완료");
    
    Serial.printf("카메라 설정:\n");
    Serial.printf("  해상도: %d\n", config.frame_size);
    Serial.printf("  JPEG 품질: %d\n", config.jpeg_quality);
    Serial.printf("  프레임 버퍼 개수: %d\n", config.fb_count);
    Serial.printf("  버퍼 위치: %s\n", 
                  config.fb_location == CAMERA_FB_IN_PSRAM ? "PSRAM" : "DRAM");
    
    // 카메라 초기화 시도
    Serial.println("esp_camera_init() 호출 시작...");
    esp_err_t err = esp_camera_init(&config);
    Serial.printf("esp_camera_init() 완료, 결과: 0x%x\n", err);
    
    if (err != ESP_OK) {
        Serial.printf("❌ 카메라 초기화 실패, 오류 코드: 0x%x\n", err);
        
        // 오류 코드별 상세 설명
        switch(err) {
            case ESP_ERR_NO_MEM:
                Serial.println("  원인: 메모리 부족");
                break;
            case ESP_ERR_INVALID_ARG:
                Serial.println("  원인: 잘못된 인수 (핀 설정 문제 가능성)");
                break;
            case ESP_ERR_INVALID_STATE:
                Serial.println("  원인: 잘못된 상태 (이미 초기화된 상태)");
                break;
            case ESP_FAIL:
                Serial.println("  원인: 일반적인 실패 (하드웨어 연결 문제 가능성)");
                break;
            case 0x263:  // ESP_ERR_NOT_FOUND
                Serial.println("  원인: 카메라 하드웨어를 찾을 수 없음");
                break;
            default:
                Serial.printf("  원인: 알 수 없는 오류 (0x%x)\n", err);
        }
        
        // 복구 시도: 카메라 해제 후 재시도
        Serial.println("카메라 해제 후 재시도...");
        esp_camera_deinit();
        delay(1000);
        
        // 더 안전한 설정으로 재시도
        config.frame_size = FRAMESIZE_QVGA; // 320x240
        config.jpeg_quality = 40;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        
        Serial.println("낮은 설정으로 재시도 중...");
        err = esp_camera_init(&config);
        
        if (err != ESP_OK) {
            Serial.printf("재시도도 실패: 0x%x\n", err);
            return false;
        } else {
            Serial.println("낮은 설정으로 초기화 성공!");
        }
    } else {
        Serial.println("✅ 카메라 초기화 성공!");
    }

    // 센서 추가 설정
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
        // 방향/기본 색감
        s->set_vflip(s, 1);
        s->set_brightness(s, 0);
        s->set_saturation(s, 0);

        // 화질 개선: 자동 화이트밸런스/노출/게인 활성화 및 렌즈 보정
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_exposure_ctrl(s, 1);
        s->set_gain_ctrl(s, 1);
        s->set_gainceiling(s, GAINCEILING_8X);
        s->set_lenc(s, 1);

        Serial.println("센서 설정 완료 (화질 향상 모드)");
    } else {
        Serial.println("센서 설정 실패");
    }

    // 카메라 안정화를 위한 대기 시간
    Serial.println("카메라 안정화 대기 중...");
    delay(1000);
    
    Serial.println("카메라 초기화 성공");
    
    // 초기화 후 메모리 상태
    Serial.printf("초기화 후 메모리 상태:\n");
    Serial.printf("  힙 메모리: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  PSRAM: %d bytes\n", ESP.getFreePsram());
    
    return true;
}

void triggerCameraCapture(const String& uploadUrl) {
    Serial.println("이미지 촬영 및 업로드 시작 (강화된 버퍼 플러시 방식)");

    // 1단계: 강화된 버퍼 플러시 - 모든 이전 버퍼 완전 제거
    Serial.println("카메라 버퍼 완전 플러시 중...");
    for (int i = 0; i < 3; i++) {
        camera_fb_t * dummy_fb = esp_camera_fb_get();
        if (dummy_fb) {
            Serial.printf("이전 버퍼 %d 제거 완료\n", i + 1);
            esp_camera_fb_return(dummy_fb);
            delay(50); // 버퍼 간 대기
        }
    }
    
    // 2단계: 카메라 센서 강제 리프레시
    Serial.println("카메라 센서 설정 리프레시...");
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
        // 센서 설정을 약간 변경했다가 다시 원래대로 (캐시 무효화)
        s->set_brightness(s, 2);
        delay(100);
        s->set_brightness(s, 1);
        delay(100);
    }
    
    // 3단계: 카메라 센서 충분한 안정화 대기
    Serial.println("카메라 센서 안정화 대기...");
    delay(500); // 더 긴 대기 시간으로 새로운 이미지 보장
    
    // 4단계: 실제 촬영 수행
    Serial.println("실제 카메라 촬영 시작...");
    camera_fb_t * fb = esp_camera_fb_get();
    
    if (fb) {
        Serial.println("✅ 새로운 이미지 촬영 성공!");
        Serial.printf("촬영 시각: %lu ms\n", millis());
    }
    
    if (!fb) {
        Serial.println("카메라 촬영 실패!");
        String pubTopic = getMacAddress() + "/cdone";
        String payload = "{\"upload\":0,\"error\":\"capture_failed\"}";
        publishMqttMessage(pubTopic, payload);
        return;
    }
    
    Serial.printf("이미지 촬영 완료. 크기: %zu bytes\n", fb->len);

    // 업로드 (Wi-Fi가 켜져 있으므로 바로 진행)
    bool success = uploadImageToS3(fb, uploadUrl);
    
    // 프레임 버퍼 메모리 해제
    esp_camera_fb_return(fb);
    
    // 업로드 완료 후 MQTT로 결과 알림
    String pubTopic = getMacAddress() + "/cdone";
    String payload = "{\"upload\":" + String(success ? "1" : "0") + "}";
    publishMqttMessage(pubTopic, payload);

    Serial.println("촬영 및 업로드 완료.");
}

void testCameraCapture() {
    Serial.println("=== 카메라 테스트 시작 ===");
    
    // 카메라 센서 상태 확인
    sensor_t * s = esp_camera_sensor_get();
    if (!s) {
        Serial.println("카메라 센서를 가져올 수 없음");
        return;
    }
    Serial.println("카메라 센서 접근 가능");
    
    // 메모리 상태 확인
    Serial.printf("메모리 상태:\n");
    Serial.printf("  힙 메모리: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  PSRAM: %d bytes\n", ESP.getFreePsram());
    
    // 테스트 촬영 시도 (강화된 버퍼 플러시 방식)
    Serial.println("이전 버퍼 완전 플러시 중...");
    for (int i = 0; i < 3; i++) {
        camera_fb_t * dummy_fb = esp_camera_fb_get();
        if (dummy_fb) {
            Serial.printf("이전 버퍼 %d 제거 완료\n", i + 1);
            esp_camera_fb_return(dummy_fb);
            delay(50);
        }
    }
    
    Serial.println("센서 설정 리프레시...");
    if (s) {
        s->set_brightness(s, 2);
        delay(100);
        s->set_brightness(s, 1);
        delay(100);
    }
    
    Serial.println("센서 안정화 대기...");
    delay(500);
    
    Serial.println("테스트 촬영 시도 중...");
    camera_fb_t * fb = esp_camera_fb_get();
    
    if (!fb) {
        Serial.println("테스트 촬영 실패");
        
        // 추가 진단
        Serial.println("카메라 상태 재확인:");
        Serial.printf("  PSRAM 사용 가능: %s\n", psramFound() ? "예" : "아니오");
        Serial.printf("  최소 힙 메모리: %d bytes\n", ESP.getMinFreeHeap());
        
        // 카메라 재설정 시도
        Serial.println("카메라 센서 재설정 시도...");
        s->set_framesize(s, FRAMESIZE_VGA); // 더 작은 크기로 시도
        delay(100);
        
        // 재시도 (강화된 버퍼 플러시 포함)
        Serial.println("재시도 전 버퍼 완전 플러시...");
        for (int i = 0; i < 3; i++) {
            camera_fb_t * retry_dummy_fb = esp_camera_fb_get();
            if (retry_dummy_fb) {
                Serial.printf("재시도 버퍼 %d 제거\n", i + 1);
                esp_camera_fb_return(retry_dummy_fb);
                delay(50);
            }
        }
        
        // 센서 리프레시
        s->set_brightness(s, 2);
        delay(100);
        s->set_brightness(s, 1);
        delay(200);
        
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("재시도 촬영도 실패");
            return;
        }
    }
    
    Serial.println("테스트 촬영 성공!");
    Serial.printf("이미지 정보:\n");
    Serial.printf("  크기: %zu bytes\n", fb->len);
    Serial.printf("  해상도: %dx%d\n", fb->width, fb->height);
    Serial.printf("  형식: %d\n", fb->format);
    
    // 메모리 해제
    esp_camera_fb_return(fb);
    
    Serial.println("=== 카메라 테스트 완료 ===");
}


// 내부(static) 함수 구현

static bool uploadImageToS3(camera_fb_t* fb, const String& url) {
    long startTime = millis();
    
    // 1. URL에서 호스트(host)와 경로(path) 분리
    String host, path;
    int hostStart = url.indexOf("://") + 3;
    if (hostStart < 3) {
        Serial.println("❌ URL 형식 오류.");
        return false;
    }
    int pathStart = url.indexOf('/', hostStart);
    host = url.substring(hostStart, pathStart);
    path = url.substring(pathStart);

    Serial.printf("S3 업로드 시작:\n");
    Serial.printf("  호스트: %s\n", host.c_str());
    Serial.printf("  경로: %s\n", path.c_str());
    Serial.printf("  이미지 크기: %zu bytes\n", fb->len);

    // 2. 보안 클라이언트 설정 및 서버 연결
    WiFiClientSecure uploadClient;
    uploadClient.setCACert(ROOT_CA_CERT); // config.h의 Root CA 사용
    uploadClient.setTimeout(30000); // 30초 타임아웃 설정
    
    Serial.printf("S3 서버 연결 중: %s\n", host.c_str());

    if (!uploadClient.connect(host.c_str(), 443)) {
        Serial.println("❌ S3 서버 연결 실패!");
        return false;
    }
    Serial.println("✅ S3 서버 연결 성공");

    // 3. HTTP PUT 요청 헤더 생성
    String requestHeader = "PUT " + path + " HTTP/1.1\r\n" +
                         "Host: " + host + "\r\n" +
                         "Content-Type: image/jpeg\r\n" +
                         "Content-Length: " + String(fb->len) + "\r\n" +
                         "Connection: close\r\n\r\n";
    
    Serial.println("HTTP 헤더 전송 중...");
    Serial.println(requestHeader);
    
    // 4. 헤더와 이미지 데이터 전송
    uploadClient.print(requestHeader);
    
    Serial.println("이미지 데이터 청크 전송 시작...");
    
    // 큰 이미지를 청크 단위로 나누어 전송 (WiFi 버퍼 제한 해결)
    const size_t CHUNK_SIZE = 8192; // 8KB 청크 크기
    size_t totalSent = 0;
    size_t remaining = fb->len;
    uint8_t* dataPtr = fb->buf;
    
    while (remaining > 0) {
        size_t chunkSize = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
        Serial.printf("청크 전송: %zu bytes (진행률: %.1f%%)\n", 
                      chunkSize, (float)totalSent / fb->len * 100.0);
        
        size_t sent = uploadClient.write(dataPtr, chunkSize);
        
        if (sent != chunkSize) {
            Serial.printf("❌ 청크 전송 실패: %zu / %zu bytes\n", sent, chunkSize);
            uploadClient.stop();
            return false;
        }
        
        totalSent += sent;
        remaining -= sent;
        dataPtr += sent;
        
        // 청크 간 짧은 대기 (버퍼 안정화)
        if (remaining > 0) {
            delay(10);
        }
    }
    
    Serial.printf("✅ 전체 이미지 전송 완료: %zu / %zu bytes\n", totalSent, fb->len);
    
    // 5. 서버 응답 확인 (중요!)
    Serial.println("서버 응답 대기 중...");
    unsigned long responseTimeout = millis() + 10000; // 10초 대기
    
    while (uploadClient.connected() && !uploadClient.available()) {
        if (millis() > responseTimeout) {
            Serial.println("❌ 서버 응답 타임아웃!");
            uploadClient.stop();
            return false;
        }
        delay(10);
    }
    
    // HTTP 응답 읽기
    String response = "";
    String statusLine = "";
    bool isFirstLine = true;
    
    while (uploadClient.available()) {
        String line = uploadClient.readStringUntil('\n');
        if (isFirstLine) {
            statusLine = line;
            isFirstLine = false;
            Serial.printf("HTTP 상태: %s\n", statusLine.c_str());
        }
        response += line + "\n";
        
        // 너무 많은 데이터 읽기 방지
        if (response.length() > 2048) break;
    }
    
    Serial.printf("서버 응답:\n%s\n", response.c_str());
    
    // HTTP 상태 코드 확인
    bool uploadSuccess = false;
    if (statusLine.indexOf("200") > 0 || statusLine.indexOf("201") > 0) {
        uploadSuccess = true;
        Serial.println("✅ S3 업로드 성공!");
    } else {
        Serial.println("❌ S3 업로드 실패!");
        Serial.printf("상태 코드: %s\n", statusLine.c_str());
        
        // 일반적인 오류 코드 설명
        if (statusLine.indexOf("403") > 0) {
            Serial.println("원인: 권한 없음 (Presigned URL 만료 또는 잘못된 서명)");
        } else if (statusLine.indexOf("404") > 0) {
            Serial.println("원인: 버킷 또는 경로를 찾을 수 없음");
        } else if (statusLine.indexOf("400") > 0) {
            Serial.println("원인: 잘못된 요청 (헤더 또는 데이터 문제)");
        }
    }
    
    long duration = millis() - startTime;
    Serial.printf("업로드 처리 완료, 소요 시간: %ld ms, 성공: %s\n", 
                  duration, uploadSuccess ? "예" : "아니오");
    
    uploadClient.stop();
    return uploadSuccess;
}