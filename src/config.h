#pragma once

// 카메라 모델 선택 (하나만 활성화)
#ifndef CAMERA_MODEL_ESP32S3_EYE
#define CAMERA_MODEL_ESP32S3_EYE // 기본 설정
#endif

#include <Arduino.h>

// WiFi 및 BLE 설정
#define WIFI_RSSI_THRES -75
#define SERVICE_UUID "c20b0d0e-d8c2-4741-b26b-4e639bc40001"
#define DEVICE_ID_CHAR_UUID "c20b0d0e-d8c2-4741-b26b-4e639bc41001"
#define WIFI_PROV_CHAR_UUID "c20b0d0e-d8c2-4741-b26b-4e639bc41002"
#define STATUS_CHAR_UUID "c20b0d0e-d8c2-4741-b26b-4e639bc41003"
#define WIFI_SCAN_CHAR_UUID "c20b0d0e-d8c2-4741-b26b-4e639bc41004"

// AWS IoT Core 설정
const char AWS_IOT_ENDPOINT[] PROGMEM = "ac1scbno22vjk-ats.iot.ap-northeast-2.amazonaws.com";
const char PROVISIONING_TEMPLATE_NAME[] PROGMEM = "MonoplexProvisioningTemplate";
#define AWS_MQTT_PORT 8883

// S3 업로드 기본 설정
const char S3_DEFAULT_URL[] PROGMEM = "https://monoplex-esp32-image-test-bucket-2025-05-22.s3.amazonaws.com/images/test-image.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIA57VDLR6OGZOOJLGP%2F20250609%2Fap-northeast-2%2Fs3%2Faws4_request&X-Amz-Date=20250609T070653Z&X-Amz-Expires=604800&X-Amz-SignedHeaders=content-type%3Bhost&X-Amz-Signature=9e01d860a645aa81bdbdecb718e2fc26365cb9e8cc21607d7e037221566729da";
const char S3_DEFAULT_HOST[] PROGMEM = "monoplex-esp32-image-test-bucket-2025-05-22.s3.amazonaws.com";
#define S3_UPLOAD_CHUNK_SIZE 16384

// 조명 제어 설정
#define LIGHT_PIN 19
#define LIGHT_ON 1
#define LIGHT_OFF 0
#define LIGHT_NIGHT_THRESHOLD 10.0

// 센서 데이터 발행 간격
#define SENSOR_PUBLISH_INTERVAL_MS 5000

// MQTT 연결 관리 설정
#define MQTT_RETRY_INTERVAL_MS 20000        // MQTT 재연결 시도 간격
#define MQTT_KEEPALIVE_INTERVAL_MS 25000    // MQTT heartbeat 간격
#define MAX_MQTT_RECONNECT_ATTEMPTS 5       // MQTT 최대 재연결 시도 횟수

// 카메라 설정
#define CAMERA_CAPTURE_MAX_RETRIES 5        // 카메라 캡처 최대 재시도 횟수

// NTP 설정
const char NTP_SERVER[] PROGMEM = "pool.ntp.org";
const long GMT_OFFSET_SEC = 9 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

// 파일 경로
const char LFS_ROOT_CA_PATH[] PROGMEM = "/root_ca.pem";
const char LFS_CLAIM_CRT_PATH[] PROGMEM = "/claim.crt";
const char LFS_CLAIM_KEY_PATH[] PROGMEM = "/claim.key";
const char LFS_DEVICE_CRT_PATH[] PROGMEM = "/device.crt";
const char LFS_DEVICE_KEY_PATH[] PROGMEM = "/device.key";

// MQTT 토픽
const char CERTIFICATE_CREATE_TOPIC[] PROGMEM = "$aws/certificates/create/json";
const char CERTIFICATE_CREATE_ACCEPTED_TOPIC[] PROGMEM = "$aws/certificates/create/json/accepted";
const char CERTIFICATE_CREATE_REJECTED_TOPIC[] PROGMEM = "$aws/certificates/create/json/rejected";

const char PROVISIONING_REQUEST_TOPIC[] PROGMEM = "$aws/provisioning-templates/MonoplexProvisioningTemplate/provision/json";
const char PROVISIONING_ACCEPTED_TOPIC[] PROGMEM = "$aws/provisioning-templates/MonoplexProvisioningTemplate/provision/json/accepted";
const char PROVISIONING_REJECTED_TOPIC[] PROGMEM = "$aws/provisioning-templates/MonoplexProvisioningTemplate/provision/json/rejected";

// 타이머 설정
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;    // 10초
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;     // 5초
const unsigned long MQTT_KEEP_ALIVE = 120;              // 120초
const unsigned long MQTT_STATUS_CHECK_INTERVAL = 60000; // 60초
const unsigned long CERT_RETRY_TIMEOUT = 120;           // 2분(초 단위)

// 버퍼 크기
const size_t JSON_BUFFER_SIZE = 128;
const size_t MQTT_BUFFER_SIZE = 2048;
const size_t CERT_TOKEN_SIZE = 256;
const size_t DEVICE_ID_SIZE = 17;     // MLX_ + 12자리 MAC + NULL 종료자
const size_t AWS_CLIENT_ID_SIZE = 20; // "esp32-" + 12자리 MAC + NULL

// FPSTR 매크로 결과를 const char*로 변환하는 헬퍼 함수
inline const char *fpstr_to_cstr(const __FlashStringHelper *fpstr)
{
    return reinterpret_cast<const char *>(fpstr);
}

// LED 핀 (외주 코드 및 현재 프로젝트 필요에 따라 통합)
#define LED_BLUE 2 // MQTT 연결 상태 표시
#define LED_RED 20 // 카메라 상태 표시
#define LED_ON 0
#define LED_OFF 1

// I2C 핀 (센서용)
#define I2C_SDA_PIN 41
#define I2C_SCL_PIN 42

// BLE 디바이스 이름
#define BLE_DEVICE_NAME "MONOPLEX_AI_SENSOR"