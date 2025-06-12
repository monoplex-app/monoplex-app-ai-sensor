#include "s3_manager.h"
#include "config.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <string.h>

// 내부 상태 변수
static char g_current_upload_url[512] = "";
static char g_current_s3_host[128] = "";
static unsigned long g_upload_start_time = 0;

void s3_manager_init()
{
    Serial.println("[S3] S3 관리 모듈 초기화");
    
    // 기본값으로 초기화
    memset(g_current_upload_url, 0, sizeof(g_current_upload_url));
    memset(g_current_s3_host, 0, sizeof(g_current_s3_host));
    
    Serial.println("[S3] S3 관리 모듈 초기화 완료");
}

bool s3_manager_extract_host_from_url(const char* url, char* host_buffer, size_t buffer_size)
{
    if (!url || !host_buffer || buffer_size == 0) {
        return false;
    }
    
    // "https://" 확인
    const char* prefix = "https://";
    if (strncmp(url, prefix, strlen(prefix)) != 0) {
        return false;
    }
    
    // "https://" 다음 부분 찾기
    const char* host_start = url + strlen(prefix);
    const char* slash_pos = strchr(host_start, '/');
    
    size_t host_length;
    if (slash_pos) {
        host_length = slash_pos - host_start;
    } else {
        host_length = strlen(host_start);
    }
    
    // 버퍼 크기 확인
    if (host_length >= buffer_size) {
        return false;
    }
    
    // 호스트명 복사
    strncpy(host_buffer, host_start, host_length);
    host_buffer[host_length] = '\0';
    
    return true;
}

void s3_manager_set_upload_url(const char* url, const char* host)
{
    if (url) {
        strncpy(g_current_upload_url, url, sizeof(g_current_upload_url) - 1);
        g_current_upload_url[sizeof(g_current_upload_url) - 1] = '\0';
    }
    
    if (host) {
        strncpy(g_current_s3_host, host, sizeof(g_current_s3_host) - 1);
        g_current_s3_host[sizeof(g_current_s3_host) - 1] = '\0';
    } else if (url) {
        // URL에서 자동으로 호스트 추출
        if (!s3_manager_extract_host_from_url(url, g_current_s3_host, sizeof(g_current_s3_host))) {
            // 추출 실패 시 기본값 사용
            strncpy(g_current_s3_host, fpstr_to_cstr(FPSTR(S3_DEFAULT_HOST)), sizeof(g_current_s3_host) - 1);
            g_current_s3_host[sizeof(g_current_s3_host) - 1] = '\0';
        }
    }
    
    Serial.printf("[S3] 업로드 URL 설정: %s\n", g_current_upload_url);
    Serial.printf("[S3] S3 호스트: %s\n", g_current_s3_host);
}

const char* s3_manager_get_upload_url()
{
    if (strlen(g_current_upload_url) == 0) {
        return fpstr_to_cstr(FPSTR(S3_DEFAULT_URL));
    }
    return g_current_upload_url;
}

const char* s3_manager_get_host()
{
    if (strlen(g_current_s3_host) == 0) {
        return fpstr_to_cstr(FPSTR(S3_DEFAULT_HOST));
    }
    return g_current_s3_host;
}

bool s3_manager_upload_image(camera_fb_t* fb)
{
    Serial.println("[S3] 이미지 업로드 시작");
    g_upload_start_time = millis();

    if (!fb || fb->len == 0) {
        Serial.println("[S3] ⚠️ 유효하지 않은 이미지 프레임");
        return false;
    }

    const char* upload_url = s3_manager_get_upload_url();
    const char* s3_host = s3_manager_get_host();

    WiFiClientSecure wifiNet;
    wifiNet.setInsecure();

    if (!wifiNet.connect(s3_host, 443)) {
        Serial.println("[S3] ❌ S3 연결 실패!");
        return false;
    }
    Serial.println("[S3] ✅ S3 연결 성공");

    // URL에서 경로 부분 추출
    String path = String(upload_url);
    path.replace(String("https://") + s3_host, "");

    // HTTP PUT 요청 헤더 전송
    wifiNet.print(String("PUT ") + path + " HTTP/1.1\r\n" +
                  String("Host: ") + s3_host + "\r\n" +
                  "Content-Type: image/jpeg\r\n" +
                  String("Content-Length: ") + fb->len + "\r\n" +
                  "Connection: close\r\n\r\n");

    // 이미지 데이터 청크 단위로 전송
    size_t totalSent = 0;
    const size_t chunkSize = S3_UPLOAD_CHUNK_SIZE;
    
    while (totalSent < fb->len) {
        size_t remaining = fb->len - totalSent;
        size_t toSend = (remaining < chunkSize) ? remaining : chunkSize;

        size_t sent = wifiNet.write(fb->buf + totalSent, toSend);
        if (sent == 0) {
            Serial.println("[S3] *** write() 실패, 0 바이트 전송됨");
            break;
        }

        totalSent += sent;
        Serial.printf("[S3] *** Sent %u bytes (Total: %u / %u)\n", sent, totalSent, fb->len);
    }

    Serial.printf("[S3] *** 총 전송 바이트: %u / %u\n", totalSent, fb->len);
    Serial.printf("[S3] Time to upload: %lu msec\n", millis() - g_upload_start_time);

    wifiNet.stop();
    
    bool success = (totalSent == fb->len);
    if (success) {
        Serial.println("[S3] *** 업로드 성공");
    } else {
        Serial.println("[S3] *** 업로드 실패");
    }

    return success;
} 