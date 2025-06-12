#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// === 전역 상태 변수 선언 ===
// 이 변수들은 main.cpp에서 정의되고, 다른 모듈들에서 참조됩니다.

/**
 * @brief 장치의 고유 식별자 (MAC 주소 기반)
 */
extern String deviceUid;

/**
 * @brief WiFi 연결 상태
 * true: 연결됨, false: 연결 안됨
 */
extern bool isWifiConnected;

/**
 * @brief MQTT 서버 연결 상태
 * true: 연결됨, false: 연결 안됨
 */
extern bool isMqttConnected;

/**
 * @brief BLE 클라이언트 연결 상태
 * true: 클라이언트가 연결됨, false: 연결 안됨
 */
extern bool isBleClientConnected;

// === LED 상태 관리 변수 ===
extern unsigned long lastLedToggleTime;
extern const long ledToggleInterval;
extern bool ledState;

#endif // GLOBALS_H 