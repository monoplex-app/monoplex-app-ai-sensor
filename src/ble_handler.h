#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "globals.h"

// isBleClientConnected는 globals.h에서 선언됨

/**
 * @brief BLE 서버를 초기화하고 광고를 시작
 * @param uid 디바이스의 고유 ID(MAC 주소)로, Wi-Fi 스캔 결과를 전송할 때 사용
 */
void initBLE(const String& uid);

/**
 * @brief BLE 통신을 처리하는 메인 핸들러 함수
 * 클라이언트의 연결/해제, 데이터 수신 및 처리를 담당합니다.
 * 이 함수는 메인 loop()에서 계속 호출되어야 합니다.
 */
void handleBLE();

/**
 * @brief 연결된 BLE 클라이언트에게 데이터를 전송
 * @param data 전송할 문자열 데이터
 */
void sendBleData(const String& data);

#endif // BLE_HANDLER_H