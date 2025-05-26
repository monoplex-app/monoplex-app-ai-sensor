#include "wifi_manager.h"
#include <time.h>
#include "certificate_manager.h"

// 전역 인스턴스 생성
WiFiManager wifiManager;

WiFiManager::WiFiManager() : m_wifiClientSecure(nullptr), lastReconnectAttempt(0), connectRetryCount(0), m_pendingPostConnectionSetup(false)
{
}

bool WiFiManager::init(WiFiClientSecure *client)
{
    m_wifiClientSecure = client; // 멤버 변수에 WiFiClientSecure 객체 포인터 저장
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

    if (WiFi.isConnected())
    {
        disconnect();
    }

    Serial.println(F("ssid: "));
    Serial.println(ssid);
    Serial.println(F("password: "));
    Serial.println(password);

    WiFi.begin(ssid, password);

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
        connectRetryCount = 0;
        m_pendingPostConnectionSetup = true; // 후처리 작업 플래그 설정
        return true;
    }
    else
    {
        Serial.println(F("[WIFI] 연결 실패 (타임아웃)"));
        WiFi.disconnect(true);
        connectRetryCount++;
        Serial.printf("[WIFI] 연결 재시도 횟수: %d/%d\n", connectRetryCount, MAX_CONNECT_RETRIES);
        m_pendingPostConnectionSetup = false; // 실패 시 플래그 해제
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

        // 암호화 유형 확인 (0=개방형, 그 외=보안됨)
        int encType = WiFi.encryptionType(i);
        String securityStatus = (encType == WIFI_AUTH_OPEN) ? "UNLOCKED" : "LOCKED";

        if (!payload.isEmpty())
            payload += ";";

        // SSID|RSSI|SECURITY_STATUS 형식으로 변경
        payload += ssid_scan + "|" + String(rssi) + "|" + securityStatus;
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

            if (connectRetryCount < MAX_CONNECT_RETRIES)
            {
                Serial.println(F("[WIFI] 저장된 설정으로 재연결 시도..."));
                connectWithSavedSettings();
            }
            else
            {
                Serial.println(F("[WIFI] 최대 연결 재시도 횟수 도달. 저장된 WiFi 설정을 삭제합니다."));
                clearSavedSettings();
                // 여기서 connectRetryCount를 0으로 리셋하거나,
                // 혹은 특정 상태 플래그를 설정하여 더 이상 자동 재연결을 시도하지 않도록 할 수 있습니다.
                // 현재는 설정만 삭제하고, connectRetryCount는 유지하여 수동 연결 시 다시 카운트되도록 합니다.
                // 만약 완전히 멈추고 싶다면 connectRetryCount를 MAX_CONNECT_RETRIES보다 큰 값으로 설정할 수 있습니다.
            }
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

void WiFiManager::clearSavedSettings()
{
    preferences.begin("wifi-config", false);
    if (preferences.isKey("wifi_ssid"))
    {
        preferences.remove("wifi_ssid");
        Serial.println(F("[NVS] 저장된 SSID 삭제됨"));
    }
    if (preferences.isKey("wifi_pass"))
    {
        preferences.remove("wifi_pass");
        Serial.println(F("[NVS] 저장된 비밀번호 삭제됨"));
    }
    preferences.end();
    connectRetryCount = 0; // 설정 삭제 후 재시도 횟수 초기화 (필요에 따라 조정)
    // 또는 특정 값으로 설정하여 자동 재연결 시도를 막을 수 있음
    // 예: connectRetryCount = MAX_CONNECT_RETRIES + 1;
    Serial.println(F("[WIFI] 저장된 WiFi 설정이 삭제되었습니다. BLE를 통해 재설정해주세요."));
}

bool WiFiManager::isPendingPostConnectionSetup() const
{
    return m_pendingPostConnectionSetup;
}

void WiFiManager::handlePostConnectionSetup()
{
    if (m_pendingPostConnectionSetup && isConnected())
    {
        Serial.println(F("[WIFI] 연결 후 설정 작업 시작..."));
        // NTP 설정
        Serial.println(F("[WIFI] NTP 시간 동기화 시도..."));
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 10000))
        {
            Serial.print(F("[NTP] 시간 동기화 성공: "));
            Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
        }
        else
        {
            Serial.println(F("[NTP] 시간 가져오기 실패"));
        }

        // 파일 시스템 초기화 및 인증서 초기화
        Serial.println(F("[WIFI] 인증서 관리자 초기화 시도..."));
        certManager.init(); // 반환값 없음. 내부적으로 파일시스템 및 기본 설정 초기화 가정.

        bool certificatesLoaded = false;
        if (certManager.loadRootCA())
        { // 루트 CA 로드 시도
            if (certManager.loadDeviceCert())
            { // 디바이스 인증서 및 키 로드 시도
                certificatesLoaded = true;
                Serial.println(F("[CERT] 필요한 모든 인증서 로드 성공."));
            }
            else
            {
                Serial.println(F("[CERT_ERROR] 디바이스 인증서 또는 키 로드 실패."));
            }
        }
        else
        {
            Serial.println(F("[CERT_ERROR] 루트 CA 인증서 로드 실패."));
        }

        // WiFiClientSecure에 인증서 설정
        if (m_wifiClientSecure && certificatesLoaded)
        {
            Serial.println(F("[WIFI] WiFiClientSecure에 인증서 설정 중..."));

            const char *rootCA_cstr = certManager.getRootCACert().c_str();
            const char *clientCert_cstr = certManager.getDeviceCert().c_str();
            const char *privateKey_cstr = certManager.getDeviceKey().c_str();

            // certManager의 String 객체가 비어있지 않은지 추가로 확인 가능
            if (strlen(rootCA_cstr) > 0 && strlen(clientCert_cstr) > 0 && strlen(privateKey_cstr) > 0)
            {
                m_wifiClientSecure->setCACert(rootCA_cstr);
                m_wifiClientSecure->setCertificate(clientCert_cstr);
                m_wifiClientSecure->setPrivateKey(privateKey_cstr);
                Serial.println(F("[WIFI] WiFiClientSecure에 인증서 설정 완료."));
            }
            else
            {
                Serial.println(F("[WIFI_ERROR] 하나 이상의 인증서 내용이 비어있습니다. WiFiClientSecure 설정 실패."));
                if (strlen(rootCA_cstr) == 0)
                    Serial.println(F(" - 루트 CA 내용 없음"));
                if (strlen(clientCert_cstr) == 0)
                    Serial.println(F(" - 디바이스 인증서 내용 없음"));
                if (strlen(privateKey_cstr) == 0)
                    Serial.println(F(" - 디바이스 개인 키 내용 없음"));
            }
        }
        else
        {
            if (!m_wifiClientSecure)
            {
                Serial.println(F("[WIFI_ERROR] WiFiClientSecure 객체가 초기화되지 않았습니다."));
            }
            if (!certificatesLoaded)
            {
                Serial.println(F("[WIFI_ERROR] 인증서 로드 실패. WiFiClientSecure에 인증서를 설정할 수 없습니다."));
            }
        }

        m_pendingPostConnectionSetup = false; // 작업 완료 후 플래그 해제
        Serial.println(F("[WIFI] 연결 후 설정 작업 완료."));
    }
}