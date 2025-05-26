#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "ble_provisioning.h"
#include "device_identity.h"
#include <nvs_flash.h>

#include <Wire.h>
#include <Adafruit_VCNL4040.h>
#include <SparkFun_BMI270_Arduino_Library.h>
#include <PubSubClient.h> // MQTT 클라이언트 라이브러리
#include <ArduinoJson.h>  // JSON 라이브러리

#include "camera_handler.h"

// AWS IoT Core 설정
// const char AWS_IOT_ENDPOINT[] PROGMEM = "YOUR_AWS_IOT_ENDPOINT"; // config.h에 정의된 값 사용
#define AWS_MQTT_PORT 8883

WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient(wifiClientSecure);

Adafruit_VCNL4040 vcnl4040;
BMI270 imu;

// MQTT 메시지 수신 콜백 함수 (필요시 확장)
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print(F("[MQTT] 메시지 수신 ["));
  Serial.print(topic);
  Serial.print(F("]: "));
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// MQTT 연결 함수
void setup_mqtt()
{
  // AWS IoT Core는 보통 8883 포트를 사용합니다.
  // 인증서 설정은 WiFiClientSecure 객체 수준에서 이루어져야 합니다.
  // WiFiManager 또는 유사한 메커니즘을 통해 인증서를 설정했다고 가정합니다.
  // 예: wifiClientSecure.setCACert(rootCA);
  //     wifiClientSecure.setCertificate(clientCert);
  //     wifiClientSecure.setPrivateKey(privateKey);
  // 위 설정은 WiFiManager 내부 또는 이 함수 호출 전에 완료되어야 합니다.

  mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_MQTT_PORT); // config.h의 AWS_IOT_ENDPOINT 사용
  mqttClient.setCallback(mqtt_callback);

  String clientId = DeviceIdentity::getDeviceId();
  Serial.print(F("[MQTT] 연결 시도 중, Client ID: "));
  Serial.println(clientId);

  if (mqttClient.connect(clientId.c_str()))
  {
    Serial.println(F("[MQTT] AWS IoT Core 연결 성공!"));
    // 필요한 토픽 구독 (예시)
    // String subscribeTopic = "devices/" + clientId + "/commands";
    // mqttClient.subscribe(subscribeTopic.c_str());
    // Serial.print(F("[MQTT] 구독 시작: "));
    // Serial.println(subscribeTopic);
    digitalWrite(LED_BLUE, LED_ON); // MQTT 연결 성공 시 BLUE LED ON (WiFi 연결 시 이미 켜져있을 수 있음)
  }
  else
  {
    Serial.print(F("[MQTT] 연결 실패, rc="));
    Serial.print(mqttClient.state());
    Serial.println(F(" 5초 후 재시도..."));
    // 재시도 로직은 loop에서 처리하거나 여기서 delay 후 재시도할 수 있습니다.
    // 여기서는 loop에서 주기적으로 재연결 시도하도록 합니다.
  }
}

// 센서 데이터를 MQTT로 발행하는 함수
void publish_sensor_data()
{
  if (!mqttClient.connected())
  {
    return; // MQTT 연결 안되어있으면 발행하지 않음
  }

  StaticJsonDocument<256> jsonDoc; // JSON 문서 크기는 데이터에 맞게 조절

  // VCNL4040 센서 데이터
  uint16_t proximity = vcnl4040.getProximity();
  float ambient = vcnl4040.getLux();
  jsonDoc["proximity"] = proximity;
  jsonDoc["ambientLight"] = ambient;

  // BMI270 IMU 데이터
  if (imu.getSensorData() == BMI2_OK)
  {
    JsonObject accel = jsonDoc.createNestedObject("accelerometer");
    accel["x"] = imu.data.accelX;
    accel["y"] = imu.data.accelY;
    accel["z"] = imu.data.accelZ;

    JsonObject gyro = jsonDoc.createNestedObject("gyroscope");
    gyro["x"] = imu.data.gyroX;
    gyro["y"] = imu.data.gyroY;
    gyro["z"] = imu.data.gyroZ;
  }

  char jsonBuffer[256];
  serializeJson(jsonDoc, jsonBuffer);

  String deviceId = DeviceIdentity::getDeviceId();
  String topic = "devices/" + deviceId + "/sensors/data"; // 단일 토픽으로 통합

  Serial.print(F("[MQTT] 발행 ["));
  Serial.print(topic);
  Serial.print(F("]: "));
  Serial.println(jsonBuffer);

  if (mqttClient.publish(topic.c_str(), jsonBuffer))
  {
    Serial.println(F("[MQTT] 메시지 발행 성공"));
  }
  else
  {
    Serial.println(F("[MQTT] 메시지 발행 실패"));
  }
}

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
  // WiFiManager가 WiFiClientSecure 객체에 인증서 설정을 포함해야 합니다.
  wifiManager.init(&wifiClientSecure); // WiFiClientSecure 객체 전달

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
    wifiManager.handlePostConnectionSetup(); // 내부에서 WiFi 연결 관련 작업 수행
    if (!mqttClient.connected())
    { // MQTT가 연결되지 않았다면 연결 시도
      setup_mqtt();
    }
  }

  if (wifiManager.isConnected())
  {
    if (!mqttClient.connected())
    {
      // 짧은 간격으로 재연결 시도 (예: 5초마다)
      static unsigned long lastMqttAttempt = 0;
      if (millis() - lastMqttAttempt > 5000)
      {
        lastMqttAttempt = millis();
        setup_mqtt(); // MQTT 재연결 시도
      }
    }
    else
    {
      mqttClient.loop(); // MQTT 클라이언트 루프 (연결 유지 및 수신 메시지 처리)
    }
  }

  // 주기적인 센서 값 읽기 및 MQTT 발행
  static unsigned long lastSensorReadTime = 0;
  if (millis() - lastSensorReadTime > 2000) // 2초마다 읽기 및 발행
  {
    lastSensorReadTime = millis();
    if (wifiManager.isConnected() && mqttClient.connected())
    {
      publish_sensor_data();
    }
    else
    {
      // MQTT 미연결 시 로컬에만 출력 (옵션)
      uint16_t proximity = vcnl4040.getProximity();
      float ambient = vcnl4040.getLux();
      Serial.printf("[SENSOR_LOCAL] 근접: %u, 조도: %.2f Lux\n", proximity, ambient);
      if (imu.getSensorData() == BMI2_OK)
      {
        Serial.printf("[SENSOR_LOCAL] Accel X:%.2f Y:%.2f Z:%.2f | Gyro X:%.2f Y:%.2f Z:%.2f\n",
                      imu.data.accelX, imu.data.accelY, imu.data.accelZ,
                      imu.data.gyroX, imu.data.gyroY, imu.data.gyroZ);
      }
    }
  }

  delay(100); // 루프 지연 시간
}
