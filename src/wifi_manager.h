#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"

class WiFiManager
{
public:
    WiFiManager();

    // WiFi 초기화 및 저장된 설정으로 연결 시도
    bool init();

    // 저장된 WiFi 설정으로 연결 시도
    bool connectWithSavedSettings();

    // 새 WiFi 설정으로 연결
    bool connect(const char *ssid, const char *password);

    // WiFi 스캔
    String scanNetworks();

    // WiFi 설정 저장
    void saveSettings(const char *ssid, const char *password);

    // WiFi 상태 확인
    bool isConnected() const;

    // 연결 상태 주기적 확인 및 필요시 재연결 (loop에서 호출)
    void update();

    // 연결 해제
    void disconnect();

    // IP 주소 얻기
    String getLocalIP() const;

private:
    Preferences preferences;
    unsigned long lastReconnectAttempt;

    void readSavedSettings(char *ssid, size_t ssidSize, char *pass, size_t passSize);
};

// 전역 인스턴스
extern WiFiManager wifiManager;