#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "ble_provisioning.h"
#include "device_identity.h"
#include <nvs_flash.h>

#include <Wire.h>
#include <Adafruit_VCNL4040.h>
#include <SparkFun_BMI270_Arduino_Library.h>

#include "camera_handler.h"

WiFiClientSecure wifiClientSecure;

Adafruit_VCNL4040 vcnl4040;
BMI270 imu;

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println(F("\n[SETUP] 시스템 부팅 중..."));

  delay(1000);

  // LED 및 GPIO 초기화 (가장 먼저 수행하여 부팅 상태 시각화)
  Serial.println(F("[SETUP] LED 및 GPIO 초기화 중..."));
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LIGHT, OUTPUT);
  digitalWrite(LED_BLUE, LED_OFF);
  digitalWrite(LED_RED, LED_ON); // 부팅 시작 시 RED ON
  digitalWrite(LIGHT, LIGHT_OFF);
  Serial.println(F("[SETUP] LED 및 GPIO 초기화 완료."));

  // NVS 초기화
  Serial.println(F("[SETUP] NVS 초기화 중..."));
  esp_err_t err_nvs = nvs_flash_init();
  if (err_nvs == ESP_ERR_NVS_NO_FREE_PAGES || err_nvs == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    Serial.println(F("[SETUP] NVS 파티션 지우는 중..."));
    ESP_ERROR_CHECK(nvs_flash_erase());
    err_nvs = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err_nvs);
  Serial.println(F("[SETUP] NVS 초기화 완료"));

  // 디바이스 ID 초기화
  DeviceIdentity::init();
  Serial.print(F("[SETUP] 디바이스 ID: "));
  Serial.println(DeviceIdentity::getDeviceId());

  // WiFi 초기화 (BLE보다 먼저 시도하여, 저장된 정보가 있다면 빠르게 네트워크 연결)
  wifiManager.init();

  // BLE 서비스 초기화
  bleProvisioning.init("MONOPLEX_AI_SENSOR");
  bleProvisioning.start();

  // I2C 및 센서 초기화
  Serial.println(F("[SETUP] I2C 및 센서 초기화 중..."));
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!vcnl4040.begin())
  {
    Serial.println(F("[SENSOR] VCNL4040 없음!"));
  }
  else
  {
    Serial.println(F("[SENSOR] VCNL4040 감지됨."));
    // vcnl4040.setProximityLEDCurrent(VCNL4040_LED_CURRENT_200MA); // 필요시 설정 활성화
    // vcnl4040.setProximityIntegrationTime(VCNL4040_PROXIMITY_INTEGRATION_TIME_8T);
    // vcnl4040.setAmbientIntegrationTime(VCNL4040_AMBIENT_INTEGRATION_TIME_80MS);
  }
  if (imu.beginI2C(0x68) != BMI2_OK)
  {
    Serial.println(F("[SENSOR] BMI270 IMU 없음!"));
  }
  else
  {
    Serial.println(F("[SENSOR] BMI270 IMU 감지됨."));
  }
  Serial.println(F("[SETUP] I2C 및 센서 초기화 완료."));

  // 카메라 시스템 초기화
  Serial.println(F("[SETUP] 카메라 시스템 초기화 중..."));
  bool cameraOk = camera_init_system();
  if (cameraOk)
  {
    Serial.println(F("[SETUP] 카메라 시스템 초기화 성공."));
    digitalWrite(LED_RED, LED_OFF); // 카메라 성공 시 RED OFF
  }
  else
  {
    Serial.println(F("[SETUP] 카메라 시스템 초기화 실패!"));
    // 실패 시 RED LED는 이미 켜져 있으므로 그대로 둠 (부팅 오류 표시)
  }

  Serial.println(F("[SETUP] 모든 초기화 단계 완료."));
}

void loop()
{
  wifiManager.update();

  if (wifiManager.isPendingPostConnectionSetup() && wifiManager.isConnected())
  {
    wifiManager.handlePostConnectionSetup();
  }

  // 주기적인 센서 값 읽기 (예시)
  static unsigned long lastSensorReadTime = 0;
  if (millis() - lastSensorReadTime > 2000)
  { // 2초마다 읽기
    lastSensorReadTime = millis();
    // if (vcnl4040.begin()) { // 매번 begin() 할 필요는 없음. setup에서 한 번.
    uint16_t proximity = vcnl4040.getProximity();
    float ambient = vcnl4040.getLux();
    Serial.printf("[SENSOR] 근접: %u, 조도: %.2f Lux\n", proximity, ambient);
    // }
    // if (imu.beginI2C(0x68) == BMI2_OK) { // 매번 begin() 할 필요는 없음.
    if (imu.getSensorData() == BMI2_OK)
    {
      Serial.printf("[SENSOR] Accel X:%.2f Y:%.2f Z:%.2f | Gyro X:%.2f Y:%.2f Z:%.2f\n",
                    imu.data.accelX, imu.data.accelY, imu.data.accelZ,
                    imu.data.gyroX, imu.data.gyroY, imu.data.gyroZ);
    }
    else
    {
      // Serial.println("[SENSOR] IMU 데이터 읽기 실패."); // 너무 자주 출력될 수 있으므로 주의
    }
    // }
  }

  delay(100); // 루프 지연 시간
}
