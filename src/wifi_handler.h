#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <WiFi.h>
#include <ArduinoJson.h>
#include "globals.h"

// --- 전역 변수 선언 ---
// 다른 파일에서 이 변수들을 참조할 수 있도록 extern 키워드를 사용합니다.
extern String ssid;
extern String password;
// isWifiConnected는 globals.h에서 선언됨

// --- 함수 프로토타입 (이 모듈이 제공하는 기능 목록) ---

/**
 * @brief 저장된 SSID와 비밀번호로 Wi-Fi 연결을 시작합니다.
 */
void initWiFi();

/**
 * @brief Wi-Fi 연결 상태를 지속적으로 확인하고 관리합니다.
 * 연결이 끊어지면 상태를 갱신하고, 새로 연결되면 IP 주소를 출력합니다.
 * 이 함수는 메인 loop()에서 계속 호출되어야 합니다.
 */
void handleWiFiConnection();

/**
 * @brief EEPROM 등에서 Wi-Fi 정보(SSID)를 성공적으로 불러왔는지 확인합니다.
 * @return SSID가 비어있지 않으면 true, 비어있으면 false를 반환합니다.
 */
bool areWiFiCredentialsAvailable();

/**
 * @brief ESP32 모듈의 고유 MAC 주소를 문자열 형태로 가져옵니다.
 * @return 12자리의 MAC 주소 문자열 (예: "A0B1C2D3E4F5").
 */
String getMacAddress();

/**
 * @brief 주변 Wi-Fi 네트워크를 스캔하여 신호 강도 순으로 정렬된 JSON 목록을 반환합니다.
 * @param macId JSON에 포함될 이 디바이스의 MAC 주소.
 * @return 스캔 결과가 담긴 JSON 형식의 문자열.
 */
String scanWifiNetworks(const String& macId);

#endif // WIFI_HANDLER_H