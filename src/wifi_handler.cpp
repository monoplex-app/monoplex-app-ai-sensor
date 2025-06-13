#include "wifi_handler.h"
#include "esp_system.h"

// --- 전역 변수 정의 ---
// 헤더에서 extern으로 선언된 변수들의 실체를 여기서 정의
String ssid = "";
String password = "";
// isWifiConnected는 main.cpp에서 이미 정의됨 - 중복 정의 제거

void initWiFi() {
    // WiFi 모드 설정 및 초기화
    WiFi.mode(WIFI_STA);
    delay(100); // 초기화 시간 확보
    
    if (!areWiFiCredentialsAvailable()) {
        Serial.println("WiFi 자격 증명이 없습니다. 연결을 건너뜁니다.");
        return;
    }
    Serial.print("WiFi에 연결 시도: ");
    Serial.println(ssid);
    WiFi.begin(ssid.c_str(), password.c_str());
}

void handleWiFiConnection() {
    // WiFi 자격 증명이 없으면 아무것도 하지 않음
    if (!areWiFiCredentialsAvailable()) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        // 이전에 연결되지 않은 상태였다면 (즉, 방금 연결되었다면)
        if (!isWifiConnected) {
            isWifiConnected = true;
            Serial.println("\nWiFi 연결 성공!");
            Serial.print("IP 주소: ");
            Serial.println(WiFi.localIP());
        }
    } else {
        // 이전에 연결된 상태였다면 (즉, 방금 연결이 끊어졌다면)
        if (isWifiConnected) {
            isWifiConnected = false;
            Serial.println("\nWiFi 연결 끊김.");
            Serial.println("재연결 시도...");
            WiFi.begin(ssid.c_str(), password.c_str());
        }
    }
}

bool areWiFiCredentialsAvailable() {
    // SSID가 비어있지 않다면 자격 증명이 있는 것으로 간주
    return ssid.length() > 0;
}

String getMacAddress() {
    // BLE 연결과 일관성을 위해 Bluetooth MAC 주소 사용
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    
    char macStr[13] = {0};
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    Serial.print("ESP Bluetooth MAC 주소: ");
    Serial.println(macStr);
    
    return String(macStr);
}

String scanWifiNetworks(const String& macId) {
    Serial.println("WiFi 네트워크 스캔 시작...");
    
    // 현재 WiFi 연결 상태 저장
    bool wasConnected = (WiFi.status() == WL_CONNECTED);
    String currentSSID = "";
    String currentPassword = "";
    
    if (wasConnected) {
        currentSSID = ssid;
        currentPassword = password;
        Serial.println("WiFi 연결 중... 스캔을 위해 일시 해제");
        WiFi.disconnect();
        delay(500); // 연결 해제 대기
    }
    
    // WiFi를 스캔 전용 모드로 설정
    WiFi.mode(WIFI_STA);
    delay(100);
    
    int numNetworks = WiFi.scanNetworks();

    // 에러 처리: 음수 값은 에러를 의미함
    if (numNetworks < 0) {
        Serial.printf("WiFi 스캔 에러 발생: %d\n", numNetworks);
        Serial.println("에러 원인:");
        Serial.println("  -1: 스캔이 이미 진행 중");
        Serial.println("  -2: WiFi가 초기화되지 않음");
        return "{\"macid\":\"" + macId + "\",\"numAp\":0,\"aplist\":[],\"error\":\"wifi_scan_failed\"}";
    }

    if (numNetworks == 0) {
        Serial.println("WiFi 네트워크를 찾을 수 없습니다.");
        return "{\"macid\":\"" + macId + "\",\"numAp\":0,\"aplist\":[]}";
    }

    Serial.printf("%d 개의 네트워크를 찾았습니다.\n", numNetworks);

    // 안전 확인: 최대 네트워크 수 제한
    if (numNetworks > 50) {
        Serial.printf("경고: 네트워크 수가 너무 많음 (%d개), 50개로 제한\n", numNetworks);
        numNetworks = 50;
    }

    // 네트워크 정보를 담을 구조체와 배열 생성
    struct WiFiNetwork {
        String ssid;
        int32_t rssi;
        String securityStatus;
    };
    
    // 동적 할당 대신 고정 크기 배열 사용 (메모리 안전성)
    WiFiNetwork networks[50]; // 최대 50개 네트워크
    int validNetworks = 0;
    
    for (int i = 0; i < numNetworks && validNetworks < 50; ++i) {
        // 각 네트워크 정보가 유효한지 확인
        String ssid = WiFi.SSID(i);
        if (ssid.length() > 0) {
            // 보안 타입 확인: WIFI_AUTH_OPEN(0)이면 UNLOCKED, 나머지는 LOCKED
            String security = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "UNLOCKED" : "LOCKED";
            networks[validNetworks] = {ssid, WiFi.RSSI(i), security};
            validNetworks++;
        }
    }
    
    // 실제로 유효한 네트워크 수로 업데이트
    numNetworks = validNetworks;

    // RSSI를 기준으로 내림차순 정렬 (버블 정렬)
    for (int i = 0; i < numNetworks - 1; ++i) {
        for (int j = 0; j < numNetworks - i - 1; ++j) {
            if (networks[j].rssi < networks[j + 1].rssi) {
                WiFiNetwork temp = networks[j];
                networks[j] = networks[j + 1];
                networks[j + 1] = temp;
            }
        }
    }

    // 최대 10개까지만 선택 (신호 강도 순으로 이미 정렬됨)
    int maxNetworks = min(numNetworks, 10);
    
    // ArduinoJson을 사용하여 JSON 응답 생성
    StaticJsonDocument<1024> doc; // 크기 증가 (보안 정보 추가로 인해)
    doc["macid"] = macId;
    doc["numAp"] = maxNetworks;
    JsonArray apList = doc.createNestedArray("aplist");

    Serial.printf("--- 정렬된 WiFi 네트워크 (상위 %d개) ---\n", maxNetworks);
    for (int i = 0; i < maxNetworks; ++i) {
        // 각 네트워크를 객체로 생성하여 추가
        JsonObject network = apList.createNestedObject();
        network["ssid"] = networks[i].ssid;
        network["rssi"] = networks[i].rssi;
        network["locked"] = networks[i].securityStatus == "LOCKED";
        
        Serial.printf("  %d: %s (%d dBm) [%s]\n", 
                     i + 1, 
                     networks[i].ssid.c_str(), 
                     networks[i].rssi,
                     networks[i].securityStatus.c_str());
    }
    
    if (numNetworks > 10) {
        Serial.printf("총 %d개 네트워크 중 상위 10개만 전송\n", numNetworks);
    }
    
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    
    WiFi.scanDelete(); // 스캔 결과 메모리 해제
    
    // 이전에 연결되어 있었다면 다시 연결 시도
    if (wasConnected && currentSSID.length() > 0) {
        Serial.println("WiFi 스캔 완료, 이전 연결 복구 중...");
        WiFi.begin(currentSSID.c_str(), currentPassword.c_str());
    }
    
    return jsonOutput;
}