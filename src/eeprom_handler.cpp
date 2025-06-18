#include "eeprom_handler.h"
#include "config.h" // EEPROM 주소 및 크기 설정을 위해 포함
#include <EEPROM.h>

// 모듈 내부에서만 사용할 변수
static bool credentialsExist = false;

// 함수 구현

void initEEPROM() {
    // 1. EEPROM 시작
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("EEPROM 초기화 실패");
        return;
    }

    // 2. 초기화 확인 (Magic Byte 확인)
    // EEPROM을 처음 사용하는지, 아니면 데이터가 이미 있는지 확인하는 절차
    if (EEPROM.read(EEPROM_INIT_FLAG_ADDR) != EEPROM_INIT_CHECK_VALUE) {
        Serial.println("EEPROM 초기화 안됨. 포맷 중...");
        clearAllCredentials(); // 내용을 모두 지우고
        EEPROM.write(EEPROM_INIT_FLAG_ADDR, EEPROM_INIT_CHECK_VALUE); // 초기화되었다는 표시를 남김
        EEPROM.commit();
        credentialsExist = false;
        Serial.println("EEPROM 포맷 완료.");
    } else {
        // 이미 초기화된 경우, Wi-Fi 정보가 저장되어 있는지 확인
        if (EEPROM.read(EEPROM_AP_INFO_FLAG_ADDR) == 1) {
            credentialsExist = true;
            Serial.println("EEPROM에 기존 Wi-Fi 정보 발견.");
        } else {
            credentialsExist = false;
            Serial.println("EEPROM 초기화됨, 하지만 정보 없음.");
        }
    }
}

void loadWiFiCredentials(String& targetSsid, String& targetPassword) {
    if (credentialsExist) {
        // EEPROM.readString()을 사용하면 훨씬 간편하게 문자열을 읽을 수 있음
        targetSsid = EEPROM.readString(EEPROM_SSID_ADDR);
        targetPassword = EEPROM.readString(EEPROM_PASSWD_ADDR);
    }
}

void saveWiFiCredentials(const String& newSsid, const String& newPassword) {
    Serial.println("EEPROM에 새로운 Wi-Fi 정보 저장 중...");
    // EEPROM.writeString()을 사용하면 훨씬 간편하게 문자열을 저장할 수 있음
    EEPROM.writeString(EEPROM_SSID_ADDR, newSsid);
    EEPROM.writeString(EEPROM_PASSWD_ADDR, newPassword);
    EEPROM.write(EEPROM_AP_INFO_FLAG_ADDR, 1); // Wi-Fi 정보가 저장되었다는 플래그 설정

    if (EEPROM.commit()) {
        Serial.println("EEPROM 저장 완료.");
        credentialsExist = true;
    } else {
        Serial.println("ERROR! EEPROM 저장 실패.");
    }
}

bool areCredentialsAvailable() {
    return credentialsExist;
}

void clearAllCredentials() {
    // 0으로 채워서 기존 데이터를 덮어쓰는 방식
    for (int i = EEPROM_AP_INFO_FLAG_ADDR; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.write(EEPROM_AP_INFO_FLAG_ADDR, 0); // 정보 없음 플래그
    EEPROM.commit();
    credentialsExist = false;
    Serial.println("EEPROM에서 모든 정보 삭제.");
}