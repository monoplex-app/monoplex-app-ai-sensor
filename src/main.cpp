#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "ble_provisioning.h"
#include "device_identity.h"
#include <nvs_flash.h> // NVS 초기화용 헤더

WiFiClientSecure wifiClientSecure;

void setup()
{
  Serial.begin(115200);
  delay(2000); // 시리얼 통신 안정화를 위한 딜레이 추가
  Serial.println(F("\n[SETUP] 시스템 부팅 중..."));

  // 리셋 디버그용 딜레이 추가
  delay(1000);

  // NVS 초기화 - ESP32 내부 저장소
  Serial.println(F("[SETUP] NVS 초기화 중..."));
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    Serial.println(F("[SETUP] NVS 파티션 지우는 중..."));
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  Serial.println(F("[SETUP] NVS 초기화 완료"));

  // 디바이스 ID 초기화 - 반드시 먼저 초기화해야 함
  DeviceIdentity::init();
  Serial.print(F("[SETUP] 디바이스 ID: "));
  Serial.println(DeviceIdentity::getDeviceId());

  // WiFi 초기화 및 저장된 설정으로 연결 시도
  wifiManager.init();

  // BLE 서비스 초기화 - 디바이스 ID 초기화 이후에 수행
  bleProvisioning.init("MONOPLEX_AI_SENSOR");
  bleProvisioning.start();

  Serial.println(F("[SETUP] 시스템 초기화 완료"));
}

void loop()
{
  // WiFi 관리
  wifiManager.update();

  delay(100);
}
