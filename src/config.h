#pragma once

#ifndef CAMERA_MODEL_ESP32S3_EYE
#define CAMERA_MODEL_ESP32S3_EYE // 사용 중인 카메라 모델
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
#define LED_BLUE 2 // 현재 프로젝트의 기존 핀과 충돌 여부 확인
#define LED_RED 20 // 현재 프로젝트의 기존 핀과 충돌 여부 확인
#define LED_ON 0
#define LED_OFF 1
#define LIGHT 19
#define LIGHT_OFF 0
#define LIGHT_ON 1

// I2C 핀 (센서용 - 외주 코드 기준)
#define I2C_SDA_PIN 41
#define I2C_SCL_PIN 42