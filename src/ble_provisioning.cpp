#include "ble_provisioning.h"
#include "device_identity.h"
#include "camera_handler.h"
#include "esp_camera.h"

// 전역 인스턴스 생성
BLEProvisioning bleProvisioning;

BLEProvisioning::BLEProvisioning() : server(nullptr), service(nullptr),
                                     statusCharacteristic(nullptr), deviceIdCharacteristic(nullptr)
{
}

void BLEProvisioning::init(const char *deviceName)
{
    BLEDevice::init(deviceName);
    server = BLEDevice::createServer();
    service = server->createService(SERVICE_UUID);

    // 상태 특성 생성
    statusCharacteristic = service->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    statusCharacteristic->setValue("INIT");

    // 디바이스 ID 특성 생성
    deviceIdCharacteristic = service->createCharacteristic(
        DEVICE_ID_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ);

    // 디바이스 ID 설정 - DeviceIdentity 사용
    deviceIdCharacteristic->setValue(DeviceIdentity::getDeviceId());

    // WiFi 프로비저닝 특성 생성
    BLECharacteristic *wifiChar = service->createCharacteristic(
        WIFI_PROV_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    wifiChar->setCallbacks(new WifiProvCallback());

    // WiFi 스캔 특성 생성
    BLECharacteristic *scanChar = service->createCharacteristic(
        WIFI_SCAN_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ);
    scanChar->setCallbacks(new WifiScanCallback());

    Serial.println(F("[BLE] 서비스 초기화 완료"));
}

void BLEProvisioning::updateStatus(const char *status)
{
    if (statusCharacteristic)
    {
        statusCharacteristic->setValue(status);
        Serial.print(F("[BLE] 상태 업데이트: "));
        Serial.println(status);
    }
}

void BLEProvisioning::start()
{
    if (service)
    {
        service->start();

        BLEAdvertising *advertising = BLEDevice::getAdvertising();
        BLEAdvertisementData advData;
        advData.setCompleteServices(BLEUUID(SERVICE_UUID));
        advertising->setAdvertisementData(advData);
        advertising->setScanResponse(false);
        BLEDevice::startAdvertising();

        Serial.println(F("[BLE] 광고 시작"));
    }
}

void BLEProvisioning::stop()
{
    BLEDevice::stopAdvertising();
    Serial.println(F("[BLE] 광고 중지"));
}

// WiFi 프로비저닝 콜백 구현
void BLEProvisioning::WifiProvCallback::onWrite(BLECharacteristic *ch)
{
    extern void safeMqttDisconnect(); // main.cpp에 정의된 MQTT 안전 해제 함수 사용
    extern void onWifiCredentialsChanged();
    std::string value = ch->getValue();
    if (value.empty())
    {
        bleProvisioning.updateStatus("FAIL:EMPTY");
        return;
    }

    Serial.println(F("[BLE] WiFi 자격증명 수신"));

    // 고정 크기 버퍼 사용 - 스택 메모리 활용
    char credentials[64] = {0}; // SSID + 구분자 + 비밀번호 저장용
    char ssid[33] = {0};        // WiFi SSID 최대 32자 + NULL
    char password[32] = {0};    // WiFi 비밀번호 저장용

    // 안전하게 데이터 복사
    size_t valueLen = value.length() > sizeof(credentials) - 1 ? sizeof(credentials) - 1 : value.length();
    memcpy(credentials, value.data(), valueLen);
    credentials[valueLen] = '\0';

    // SSID와 비밀번호 분리
    char *delimiter = strchr(credentials, '|');
    if (!delimiter)
    {
        bleProvisioning.updateStatus("FAIL:FORMAT");
        Serial.println(F("[WIFI] 잘못된 형식"));
        return;
    }

    // 구분자 위치에서 문자열 분리
    *delimiter = '\0';
    strncpy(ssid, credentials, sizeof(ssid) - 1);
    strncpy(password, delimiter + 1, sizeof(password) - 1);

    // 상태 업데이트
    bleProvisioning.updateStatus("CONNECTING...");

    // **중요: MQTT 연결 먼저 해제**
    Serial.println(F("[BLE] 기존 MQTT 연결 해제"));
    safeMqttDisconnect();

    // WiFi 저장 및 연결 (안전한 방식 사용)
    wifiManager.saveSettings(ssid, password);

    // BLE 연결을 잠시 중단하여 WiFi에 리소스 집중
    Serial.println(F("[BLE] WiFi 연결을 위해 BLE 일시 중단"));
    BLEDevice::getAdvertising()->stop();
    delay(1000); // BLE 중단 대기

    bool connectSuccess = false;
    for (int retry = 0; retry < 3; retry++)
    {
        Serial.printf("[BLE] WiFi 연결 시도 %d/3\n", retry + 1);
        if (wifiManager.connect(ssid, password))
        {
            connectSuccess = true;
            break;
        }
        if (retry < 2) // 마지막 시도가 아니면
        {
            Serial.println(F("[BLE] 연결 실패, 잠시 대기 후 재시도"));
            delay(2000);
        }
    }

    // BLE 광고 재시작
    Serial.println(F("[BLE] BLE 광고 재시작"));
    BLEDevice::getAdvertising()->start();

    if (connectSuccess)
    {
        // 연결 성공, IP 주소로 상태 업데이트
        char ipAddress[16];
        wifiManager.getLocalIP().toCharArray(ipAddress, sizeof(ipAddress));

        char statusBuffer[20];
        snprintf(statusBuffer, sizeof(statusBuffer), "OK:%s", ipAddress);
        bleProvisioning.updateStatus(statusBuffer);

        Serial.println(F("[BLE] WiFi 연결 성공, BLE 서비스 일시 중단"));
        // 성공 시 BLE 서비스를 잠시 중단하여 WiFi 안정성 확보
        delay(5000);
        onWifiCredentialsChanged();
    }
    else
    {
        bleProvisioning.updateStatus("FAIL:TIMEOUT");
        Serial.println(F("[BLE] WiFi 연결 실패"));
    }
}

// WiFi 스캔 콜백 구현
void BLEProvisioning::WifiScanCallback::onRead(BLECharacteristic *ch)
{
    String networks = wifiManager.scanNetworks();
    ch->setValue(networks.c_str());
}

void camera_deinit_system()
{
    esp_camera_deinit();
}