// 헤더 파일 보호
#ifndef CONFIG_H
#define CONFIG_H

// 시리얼 바운드 레이트
#define SERIAL_BAUD_RATE 115200

// 핀 정의
#define LED_RED 20
#define LED_ON 0
#define LED_OFF 1
#define LIGHT 19
#define LIGHT_OFF 0
#define LIGHT_ON 1

#define LIGHT_NIGHT_THRESHOLD 10.0 // 이 값보다 조도가 낮으면 조명 켬

// I2C 핀
#define I2C_SDA_PIN 41
#define I2C_SCL_PIN 42

// BLE 설정
#define BLE_DEVICE_NAME "MONOPLEX AI SENSOR"
#define SERVER_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-31914cc5c9c3"
#define SERVER_CHARACTERISTIC_UUID "9f549a79-f038-47df-b252-3a330ec61ebf"
#define MAX_BLE_RETRY 5

// 카메라 설정
// CAMERA_MODEL_ESP32S3_EYE는 PlatformIO에서 자동 정의됨 - 중복 정의 방지
#ifndef CAMERA_MODEL_ESP32S3_EYE
#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
#endif
#include "camera_pins.h"

// MQTT & AWS IoT 설정
#define MQTTS_PORT 8883
#define IOT_ENDPOINT "ac1scbno22vjk-ats.iot.ap-northeast-2.amazonaws.com"

// 개발용: MQTT 연결 비활성화 (인증서 문제 시 임시 사용)
// 실제 배포 시에는 이 줄을 주석처리하세요
// #define DISABLE_MQTT_FOR_DEVELOPMENT  // 실제 인증서 적용으로 활성화

// 인증서 변수 선언 (extern - 실제 정의는 config.cpp에서)
extern const char* ROOT_CA_CERT;
extern const char* CERTIFICATE_PEM;
extern const char* PRIVATE_KEY;

// EEPROM 설정
#define EEPROM_SIZE 96              // 128에서 96으로 축소 (UID 저장 공간 제거)
#define EEPROM_INIT_FLAG_ADDR 0
#define EEPROM_AP_INFO_FLAG_ADDR 1
#define EEPROM_SSID_ADDR 32
#define EEPROM_PASSWD_ADDR 64
// #define EEPROM_UID_ADDR 96       // 미사용으로 제거
#define EEPROM_INIT_CHECK_VALUE 0xA8

#endif // CONFIG_H
