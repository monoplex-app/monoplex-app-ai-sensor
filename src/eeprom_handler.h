#ifndef EEPROM_HANDLER_H
#define EEPROM_HANDLER_H

#include <Arduino.h>

/**
 * @brief EEPROM을 초기화하고, 저장된 데이터가 유효한지 확인
 * setup()에서 한 번 호출
 */
void initEEPROM();

/**
 * @brief 새로운 Wi-Fi 접속 정보를 EEPROM에 저장
 * @param newSsid 저장할 새로운 SSID
 * @param newPassword 저장할 새로운 비밀번호
 */
void saveWiFiCredentials(const String& newSsid, const String& newPassword);

/**
 * @brief EEPROM에 저장된 Wi-Fi 정보를 불러옴
 * @param targetSsid 불러온 SSID가 저장될 변수 (참조 전달)
 * @param targetPassword 불러온 비밀번호가 저장될 변수 (참조 전달)
 */
void loadWiFiCredentials(String& targetSsid, String& targetPassword);

/**
 * @brief EEPROM에 유효한 Wi-Fi 정보가 저장되어 있는지 확인
 * @return 정보가 있으면 true, 없으면 false
 */
bool areCredentialsAvailable();

/**
 * @brief EEPROM에 저장된 모든 Wi-Fi 정보를 삭제
 */
void clearAllCredentials();


#endif // EEPROM_HANDLER_H