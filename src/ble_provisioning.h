#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <esp_bt_device.h>
#include "config.h"
#include "wifi_manager.h"
#include "certificate_manager.h"
#include "device_identity.h"

class BLEProvisioning
{
public:
    BLEProvisioning();

    // BLE 서비스 초기화
    void init(const char *deviceName);

    // 상태 업데이트
    void updateStatus(const char *status);

    // BLE 서버 시작
    void start();

    // BLE 서버 중지
    void stop();

private:
    BLEServer *server;
    BLEService *service;
    BLECharacteristic *statusCharacteristic;
    BLECharacteristic *deviceIdCharacteristic;

    class WifiProvCallback;
    class WifiScanCallback;

    // WiFi 프로비저닝 콜백 클래스 정의
    class WifiProvCallback : public BLECharacteristicCallbacks
    {
        void onWrite(BLECharacteristic *ch) override;
    };

    // WiFi 스캔 콜백 클래스 정의
    class WifiScanCallback : public BLECharacteristicCallbacks
    {
        void onRead(BLECharacteristic *ch) override;
    };
};

// 전역 인스턴스
extern BLEProvisioning bleProvisioning;