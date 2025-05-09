#include "wifi_manager.h"
#include <time.h>
#include "certificate_manager.h"
#include "aws_mqtt_handler.h"

// 전역 인스턴스 생성
WiFiManager wifiManager;

WiFiManager::WiFiManager() : lastReconnectAttempt(0)
{
}

bool WiFiManager::init()
{
    WiFi.mode(WIFI_STA);
    return connectWithSavedSettings();
}

bool WiFiManager::connectWithSavedSettings()
{
    char ssid[33] = {0}; // WiFi SSID 최대 32자 + NULL
    char pass[64] = {0}; // WiFi 비밀번호

    readSavedSettings(ssid, sizeof(ssid), pass, sizeof(pass));

    if (strlen(ssid) == 0)
    {
        Serial.println(F("[WIFI] 저장된 설정 없음"));
        return false;
    }

    return connect(ssid, pass);
}

bool WiFiManager::connect(const char *ssid, const char *password)
{
    if (strlen(ssid) == 0)
    {
        Serial.println(F("[WIFI] SSID가 비어있습니다"));
        return false;
    }

    Serial.print(F("[WIFI] 연결 시도: "));
    Serial.println(ssid);

    // 이미 연결되어 있으면 연결 해제
    if (WiFi.isConnected())
    {
        disconnect();
    }

    WiFi.begin(ssid, password);

    // 연결 대기 (최대 10초)
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
    {
        delay(200);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print(F("[WIFI] 연결 성공! IP: "));
        Serial.println(WiFi.localIP().toString());

        // NTP 설정
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 10000))
        {
            Serial.print(F("[NTP] 시간 동기화: "));
            Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
        }
        else
        {
            Serial.println(F("[NTP] 시간 가져오기 실패"));
        }

        // MQTT 초기화
        mqttHandler.init();

        // 파일 시스템 초기화 및 인증서 초기화
        certManager.init();

        return true;
    }
    else
    {
        Serial.println(F("[WIFI] 연결 실패 (타임아웃)"));
        WiFi.disconnect(true);
        return false;
    }
}

String WiFiManager::scanNetworks()
{
    int n = WiFi.scanNetworks(false, false);
    String payload;

    for (int i = 0; i < n; ++i)
    {
        int rssi = WiFi.RSSI(i);
        if (rssi < WIFI_RSSI_THRES)
            continue;

        String ssid_scan = WiFi.SSID(i);
        if (ssid_scan.indexOf('|') >= 0 || ssid_scan.indexOf(';') >= 0)
            continue;

        if (!payload.isEmpty())
            payload += ";";

        payload += ssid_scan + "|" + String(rssi);
    }

    if (payload.isEmpty())
        payload = "NONE";

    Serial.printf("[WIFI] 스캔 결과 %d개 네트워크\n", n);
    WiFi.scanDelete();

    return payload;
}

void WiFiManager::saveSettings(const char *ssid, const char *password)
{
    preferences.begin("wifi-config", false);
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", password);
    preferences.end();
    Serial.println(F("[NVS] WiFi 설정 저장 완료"));
}

bool WiFiManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::update()
{
    if (!isConnected())
    {
        unsigned long currentMillis = millis();
        if (currentMillis - lastReconnectAttempt > WIFI_RECONNECT_INTERVAL)
        {
            lastReconnectAttempt = currentMillis;

            // 저장된 설정으로 재연결 시도
            connectWithSavedSettings();
        }
    }
}

void WiFiManager::disconnect()
{
    if (WiFi.isConnected())
    {
        Serial.println(F("[WIFI] 연결 해제"));
        WiFi.disconnect(true);
        delay(100);
    }
}

String WiFiManager::getLocalIP() const
{
    return WiFi.localIP().toString();
}

void WiFiManager::readSavedSettings(char *ssid, size_t ssidSize, char *pass, size_t passSize)
{
    preferences.begin("wifi-config", true);
    String stored_ssid = preferences.getString("wifi_ssid", "");
    String stored_pass = preferences.getString("wifi_pass", "");
    preferences.end();

    if (stored_ssid.length() > 0)
    {
        Serial.println(F("[NVS] 저장된 WiFi 설정 로드"));
        stored_ssid.toCharArray(ssid, ssidSize);
        stored_pass.toCharArray(pass, passSize);
    }
}