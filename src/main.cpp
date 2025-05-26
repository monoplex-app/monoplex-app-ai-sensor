#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "ble_provisioning.h"
#include "device_identity.h"
#include <nvs_flash.h>
#include <esp_wifi.h> // esp_wifi_* 및 wifi_init_config_t 사용을 위해 추가

#include <Wire.h>
#include <Adafruit_VCNL4040.h>
#include <SparkFun_BMI270_Arduino_Library.h>
#include <PubSubClient.h> // MQTT 클라이언트 라이브러리
#include <ArduinoJson.h>  // JSON 라이브러리

#include "camera_handler.h"
extern void camera_deinit_system();
#include "esp_heap_caps.h"

// AWS IoT Core 설정
// const char AWS_IOT_ENDPOINT[] PROGMEM = "YOUR_AWS_IOT_ENDPOINT"; // config.h에 정의된 값 사용
#define AWS_MQTT_PORT 8883

WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient(wifiClientSecure);

Adafruit_VCNL4040 vcnl4040;
BMI270 imu;

// S3 업로드 관련
String currentUploadUrl = "";
String currentS3Host = "";
const char *s3Url_default = "https://monoplex-esp32-image-test-bucket-2025-05-22.s3.amazonaws.com/images/test-image.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIA57VDLR6OGZOOJLGP%2F20250526%2Fap-northeast-2%2Fs3%2Faws4_request&X-Amz-Date=20250526T061131Z&X-Amz-Expires=604800&X-Amz-SignedHeaders=content-type%3Bhost&X-Amz-Signature=aab470b617a090095d21bc9f4aa3408718648a361b0fb1bdcaf6c139b521d6e9";
const char *s3Host_default = "monoplex-esp32-image-test-bucket-2025-05-22.s3.amazonaws.com";

// 조명 제어
#define LIGHT_PIN 19
#define LIGHT_ON 1
#define LIGHT_OFF 0
#define LIGHT_NIGHT 10.0 // Lux 이하일 때 조명 ON

float ambient = 20.0; // VCNL4040 등에서 주기적으로 업데이트

unsigned long startTime = 0;

bool cameraOk = false;

// --- 함수 프로토타입 선언 ---
void send_captured_image();
void checkMemory(); // 추가

// 1. 메모리 상태 체크 및 정리 함수 추가 (이전에 요청된 함수, 여기서는 checkMemory로 사용)
void checkMemory()
{
  Serial.printf("[MEM] Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("[MEM] Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("[MEM] Min free heap: %u bytes\n", ESP.getMinFreeHeap()); // ESP-IDF 특정, Arduino에서는 ESP.getFreeHeap()과 유사
  Serial.printf("[MEM] Max alloc heap: %u bytes\n", ESP.getMaxAllocHeap());

  // ESP.getFreeHeap(); // 이 호출 자체가 약간의 정리 효과를 줄 수 있지만, 명시적인 가비지 컬렉션은 아님
  // delay(100); // 특별한 이유 없이 delay는 불필요
}

// 기존 checkAndCleanMemory 함수는 checkMemory로 대체되거나 통합됩니다.
/*
void checkAndCleanMemory()
{
  // 메모리 상태 출력
  Serial.printf("[MEM] Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("[MEM] Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("[MEM] Free 8BIT: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  Serial.printf("[MEM] Free INTERNAL: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  // 가비지 컬렉션 강제 실행
  ESP.getFreeHeap(); // 내부적으로 정리 작업 수행
  delay(100);
}
*/

// 2. 안전한 카메라 캡처 함수 (기존 safeGetCameraFrame을 개선하는 방향으로)
camera_fb_t *safeGetCameraFrame(int maxRetries = 5)
{
  checkMemory();

  // 센서 상태 확인
  sensor_t *s = esp_camera_sensor_get();
  if (!s)
  {
    Serial.println("[CAMERA] 센서 포인터 없음!");
    return nullptr;
  }

  for (int i = 0; i < maxRetries; i++)
  {
    Serial.printf("[CAMERA] Capturing image (attempt %d/%d)...\n", i + 1, maxRetries);

    // 캡처 전 잠시 대기 (센서 안정화)
    if (i > 0)
    {
      delay(500); // 재시도 시 더 긴 대기
    }

    // 메모리 정리
    if (i > 1)
    {
      ESP.getFreeHeap(); // 가비지 컬렉션 유도
      delay(100);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb)
    {
      // 유효한 데이터인지 확인
      if (fb->len > 0 && fb->buf != nullptr)
      {
        Serial.printf("[CAMERA] Image captured: %dx%d, size: %zu bytes\n",
                      fb->width, fb->height, fb->len);
        return fb;
      }
      else
      {
        Serial.println("[CAMERA] 유효하지 않은 프레임 데이터");
        esp_camera_fb_return(fb);
      }
    }
    else
    {
      Serial.println("[CAMERA] esp_camera_fb_get() failed.");
    }

    // 재시도 전 센서 설정 재적용
    if (i == maxRetries - 2)
    { // 마지막에서 두 번째 시도 전
      Serial.println("[CAMERA] 센서 설정 재적용 시도...");

      // 센서 설정 재적용 (레지스터 직접 접근 대신)
      s->set_pixformat(s, PIXFORMAT_JPEG);
      s->set_framesize(s, FRAMESIZE_SVGA);
      s->set_quality(s, 15);
      s->set_brightness(s, 0);
      s->set_contrast(s, 0);
      s->set_saturation(s, 0);
      s->set_whitebal(s, 1);
      s->set_awb_gain(s, 1);
      s->set_exposure_ctrl(s, 1);
      s->set_gain_ctrl(s, 1);

      delay(300); // 설정 적용 대기
    }
  }

  Serial.println("[CAMERA] Camera capture failed after all attempts.");
  return nullptr;
}

// 카메라 재초기화 함수 추가
bool reinitializeCamera()
{
  Serial.println("[CAMERA] 카메라 재초기화 시작...");

  // 기존 카메라 해제
  esp_camera_deinit();
  delay(1000); // 충분한 대기 시간

  // 메모리 정리
  ESP.getFreeHeap();
  checkMemory();

  // 카메라 재초기화
  bool result = camera_init_system();
  if (result)
  {
    Serial.println("[CAMERA] 카메라 재초기화 성공");
  }
  else
  {
    Serial.println("[CAMERA] 카메라 재초기화 실패");
  }

  return result;
}

bool uploadToS3Raw(camera_fb_t *fb, const char *presigned_url, const char *host)
{
  Serial.println("이미지 업로드 시작");

  startTime = millis();

  if (!fb || fb->len == 0)
  {
    Serial.println("⚠️ 유효하지 않은 이미지 프레임");
    return false;
  }

  WiFiClientSecure wifiNet;
  wifiNet.setInsecure(); // 또는 wifiNet.setCACert(rootCA1);

  if (!wifiNet.connect(host, 443))
  {
    Serial.println("❌ S3 연결 실패!");
    return false;
  }
  Serial.println("✅ S3 연결 성공");

  // URL에서 path 추출
  String path = String(presigned_url);
  path.replace(String("https://") + host, "");

  // PUT 요청 헤더
  wifiNet.print(String("PUT ") + path + " HTTP/1.1\r\n" +
                String("Host: ") + host + "\r\n" +
                "Content-Type: image/jpeg\r\n" +
                String("Content-Length: ") + fb->len + "\r\n" +
                "Connection: close\r\n\r\n");

  // Chunked 데이터 전송
  size_t totalSent = 0;
  const size_t chunkSize = 16384;
  while (totalSent < fb->len)
  {
    size_t remaining = fb->len - totalSent;
    size_t toSend = (remaining < chunkSize) ? remaining : chunkSize;

    size_t sent = wifiNet.write(fb->buf + totalSent, toSend);
    if (sent == 0)
    {
      Serial.println("*** write() 실패, 0 바이트 전송됨");
      break;
    }

    totalSent += sent;
    Serial.printf("*** Sent %u bytes (Total: %u / %u)\n", sent, totalSent, fb->len);
  }

  Serial.printf("*** 총 전송 바이트: %u / %u\n", totalSent, fb->len);

  Serial.print("Time to upload: ");
  Serial.print(millis() - startTime);
  Serial.println(" msec");

  // print_line();
  // startTime = millis();

  // // 응답 수신
  // Serial.println("*** 서버 응답 수신 중...");
  // unsigned long timeout = millis();
  // while (wifiNet.connected() && !wifiNet.available()) {
  //     if (millis() - timeout > 5000) {
  //         Serial.println("*** 응답 대기 타임아웃");
  //         wifiNet.stop();
  //         return false;
  //     }
  //     delay(10);
  // }

  // while (wifiNet.available()) {
  //     String line = wifiNet.readStringUntil('\n');
  //     Serial.print("*** ");
  //     Serial.println(line);
  // }

  wifiNet.stop();
  Serial.println("*** 업로드 완료");

  // print_line();
  // Serial.print("Time to response: ");
  // Serial.print(millis()-startTime);
  // Serial.println(" msec");
  // print_line();

  return true;
}

void send_captured_image()
{
  int flag = 0;
  if (ambient < LIGHT_NIGHT)
  {
    flag = 1;
    Serial.println("LIGHT ON ...");
    digitalWrite(LIGHT_PIN, LIGHT_ON); // LIGHT_PIN 사용 확인
  }

  unsigned long localStartTime = millis(); // 지역 변수 startTime 사용 (전역 startTime과 충돌 방지)

  Serial.println("Attempting to capture image...");

  // 카메라가 초기화되지 않았다면 초기화 시도
  if (!cameraOk)
  {
    Serial.println("[CAPTURE] Camera not initialized. Attempting to initialize...");
    cameraOk = camera_init_system();
    if (!cameraOk)
    {
      Serial.println("[CAPTURE] Camera initialization failed. Cannot capture image.");
      return;
    }
    Serial.println("[CAPTURE] Camera initialized successfully.");
  }

  // 개선된 safeGetCameraFrame 함수 사용
  camera_fb_t *fb = safeGetCameraFrame(5); // 재시도 횟수 증가

  // 첫 번째 캡처 실패 시 카메라 재초기화 후 한 번 더 시도
  if (!fb)
  {
    Serial.println("[CAPTURE] 첫 번째 캡처 실패, 카메라 재초기화 시도...");
    cameraOk = reinitializeCamera();
    if (cameraOk)
    {
      fb = safeGetCameraFrame(3); // 재초기화 후 다시 시도
    }
  }

  if (!fb)
  {
    Serial.println("[CAPTURE] Failed to get camera frame after retries and reinit.");
    // ... 실패 처리 코드 ...
    return;
  }

  // 무조건 OFF (조명 제어)
  if (flag == 1)
  {
    digitalWrite(LIGHT_PIN, LIGHT_OFF);
  }

  Serial.printf("[CAPTURE] Time to capture: %lu ms\n", millis() - localStartTime);
  Serial.printf("[CAPTURE] Image Size: %zu KB\n", fb->len / 1024);

  Serial.println("[CAPTURE] Image captured. Preparing to send to S3...");

  // S3 업로드 URL 및 호스트 결정 (MQTT로 수신한 값 또는 기본값 사용)
  const char *uploadUrlToUse = currentUploadUrl.isEmpty() ? s3Url_default : currentUploadUrl.c_str();
  const char *s3HostToUse = currentS3Host.isEmpty() ? s3Host_default : currentS3Host.c_str();

  Serial.printf("[S3_UPLOAD] Using URL: %s\n", uploadUrlToUse);
  Serial.printf("[S3_UPLOAD] Using Host: %s\n", s3HostToUse);

  bool uploadResult = uploadToS3Raw(fb, uploadUrlToUse, s3HostToUse);

  esp_camera_fb_return(fb); // 프레임 버퍼 해제

  // MQTT로 완료 통보
  String deviceId = DeviceIdentity::getDeviceId();
  String topic = "devices/" + deviceId + "/image/done";
  if (uploadResult)
  {
    mqttClient.publish(topic.c_str(), "{\"upload\":1}");
    Serial.println("[MQTT] Published image upload success.");
  }
  else
  {
    mqttClient.publish(topic.c_str(), "{\"upload\":0, \"error\":\"s3_upload_failed\"}");
    Serial.println("[MQTT] Published image upload failure.");
  }
}

// MQTT 메시지 수신 콜백 함수 (필요시 확장)
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  Serial.println(F("--- MQTT Message Received ---"));
  Serial.print(F("[MQTT] Raw Topic: "));
  Serial.println(topic);
  Serial.print(F("[MQTT] Payload: "));
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println(F("-----------------------------"));

  String deviceId = DeviceIdentity::getDeviceId();
  String captureTopic = "devices/" + deviceId + "/image/capture";

  if (String(topic).equals(captureTopic))
  {
    Serial.println(F("[MQTT] Image capture command received!"));

    // payload를 문자열로 변환
    String message = String((char *)payload).substring(0, length);

    // JsonDocument 사용 (StaticJsonDocument 대신)
    JsonDocument jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, message);

    // containsKey 대신 is<>() 사용
    if (!error && jsonDoc["url"].is<const char *>())
    {
      currentUploadUrl = jsonDoc["url"].as<String>();
      Serial.print(F("[MQTT] New S3 URL received: "));
      Serial.println(currentUploadUrl);

      if (currentUploadUrl.startsWith("https://"))
      {
        String tempHost = currentUploadUrl.substring(8);
        int slashIndex = tempHost.indexOf('/');
        if (slashIndex != -1)
        {
          currentS3Host = tempHost.substring(0, slashIndex);
          Serial.print(F("[MQTT] New S3 Host extracted: "));
          Serial.println(currentS3Host);
        }
        else
        {
          Serial.println(F("[MQTT] Failed to extract S3 host from URL (no slash after host)."));
          currentS3Host = s3Host_default;
        }
      }
      else
      {
        Serial.println(F("[MQTT] Received URL does not start with https://. Using default S3 host."));
        currentS3Host = s3Host_default;
      }
    }
    else
    {
      Serial.println(F("[MQTT] No 'url' in payload or JSON parsing error. Using default S3 URL & Host."));
      currentUploadUrl = s3Url_default;
      currentS3Host = s3Host_default;
    }
    send_captured_image();
  }
  else
  {
    Serial.println(F("[MQTT] Received message on an unhandled topic."));
  }
}

// MQTT 연결 함수
bool mqttConnected = false;
unsigned long lastMqttAttempt = 0;
const unsigned long MQTT_RETRY_INTERVAL = 10000; // 10초

void safeMqttDisconnect()
{
  if (mqttClient.connected())
  {
    Serial.println(F("[MQTT] Disconnecting safely..."));
    mqttClient.disconnect();
    delay(100); // Disconnect 대기 시간 단축
    mqttConnected = false;
    Serial.println(F("[MQTT] Disconnected."));
  }
}

void setup_mqtt()
{
  Serial.println(F("[MQTT] setup_mqtt() called."));
  if (!wifiManager.isConnected())
  {
    Serial.println(F("[MQTT] WiFi not connected. MQTT setup aborted."));
    return;
  }
  if (mqttClient.connected())
  {
    Serial.println(F("[MQTT] Already connected. Skipping setup."));
    return;
  }

  if (millis() - lastMqttAttempt < MQTT_RETRY_INTERVAL)
  {
    // Serial.println(F("[MQTT] MQTT retry interval not elapsed.")); // 너무 빈번한 로그라 주석처리
    return;
  }
  lastMqttAttempt = millis();

  Serial.println(F("[MQTT] Setting server and callback..."));
  mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_MQTT_PORT);
  mqttClient.setCallback(mqtt_callback);      // 콜백 함수 등록 확인
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE); // 버퍼 사이즈 설정 확인

  String clientId = DeviceIdentity::getDeviceId();
  Serial.print(F("[MQTT] Attempting to connect with Client ID: "));
  Serial.println(clientId);

  if (mqttClient.connect(clientId.c_str()))
  {
    Serial.println(F("[MQTT] AWS IoT Core connection SUCCESSFUL!"));
    mqttConnected = true;
    digitalWrite(LED_BLUE, LED_ON); // 파란색 LED로 연결 상태 표시

    // 구독 시도
    String imageCaptureTopic = "devices/" + clientId + "/image/capture";
    Serial.print(F("[MQTT] Subscribing to topic: "));
    Serial.println(imageCaptureTopic);
    if (mqttClient.subscribe(imageCaptureTopic.c_str()))
    {
      Serial.println(F("[MQTT] Subscription to imageCaptureTopic SUCCESSFUL."));
    }
    else
    {
      Serial.println(F("[MQTT] Subscription to imageCaptureTopic FAILED."));
    }
  }
  else
  {
    Serial.print(F("[MQTT] AWS IoT Core connection FAILED, rc="));
    Serial.print(mqttClient.state());
    Serial.println(F(" Will retry..."));
    mqttConnected = false;
    digitalWrite(LED_BLUE, LED_OFF); // 연결 실패 시 LED 끄기 (또는 빨간색으로)
  }
}

// 센서 데이터 발행 함수의 JSON 처리 부분
void publish_sensor_data()
{
  if (!mqttClient.connected())
  {
    return;
  }

  // JsonDocument 사용 (StaticJsonDocument 대신)
  JsonDocument jsonDoc;

  // VCNL4040 센서 데이터
  uint16_t proximity = vcnl4040.getProximity();
  ambient = vcnl4040.getLux();
  jsonDoc["proximity"] = proximity;
  jsonDoc["ambientLight"] = ambient;

  // BMI270 IMU 데이터
  if (imu.getSensorData() == BMI2_OK)
  {
    // createNestedObject 대신 to<JsonObject>() 사용
    JsonObject accel = jsonDoc["accelerometer"].to<JsonObject>();
    accel["x"] = imu.data.accelX;
    accel["y"] = imu.data.accelY;
    accel["z"] = imu.data.accelZ;

    JsonObject gyro = jsonDoc["gyroscope"].to<JsonObject>();
    gyro["x"] = imu.data.gyroX;
    gyro["y"] = imu.data.gyroY;
    gyro["z"] = imu.data.gyroZ;
  }

  char jsonBuffer[256];
  serializeJson(jsonDoc, jsonBuffer);

  String deviceId = DeviceIdentity::getDeviceId();
  String topic = "devices/" + deviceId + "/sensors/data";

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
bool needCameraReinit = false; // 카메라 재초기화 필요 플래그

void reinit_camera_with_ble_safety()
{
  BLEDevice::stopAdvertising();
  delay(500);
  Serial.printf("[MEM] Free heap before camera: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  camera_deinit_system();
  delay(500);
  cameraOk = camera_init_system();
  if (cameraOk)
  {
    Serial.println(F("[LOOP] 카메라 시스템 재초기화 성공."));
    digitalWrite(LED_RED, LED_OFF);
  }
  else
  {
    Serial.println(F("[LOOP] 카메라 시스템 재초기화 실패!"));
  }
  BLEDevice::startAdvertising();
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
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LED_BLUE, LED_OFF);
  digitalWrite(LED_RED, LED_ON); // 부팅 시작 시 RED ON
  digitalWrite(LIGHT_PIN, LIGHT_OFF);
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

  // WiFi 드라이버 강제 리셋
  esp_wifi_stop();
  esp_wifi_deinit();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  // cfg.rx_buf_num = 6; // (주석처리: 직접 설정 불가)
  // cfg.tx_buf_num = 6; // (주석처리: 직접 설정 불가)
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  delay(1000);

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

  // 카메라 시스템 초기화 "제거" (아래로 이동)
  // bool cameraOk = camera_init_system();

  // WiFi, BLE, 인증서, MQTT 연결까지 모두 완료 후
  setup_mqtt();
  if (mqttConnected)
  {
    cameraOk = camera_init_system();
    if (cameraOk)
    {
      Serial.println(F("[SETUP] 카메라 시스템 초기화 성공."));
      digitalWrite(LED_RED, LED_OFF);
    }
    else
    {
      Serial.println(F("[SETUP] 카메라 시스템 초기화 실패!"));
    }
  }

  Serial.println(F("[SETUP] 모든 초기화 단계 완료."));

  Serial.printf("[MEM] Free DRAM heap: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void loop()
{
  wifiManager.update();

  // BLE로 WiFi 정보가 바뀌었거나, WiFi/MQTT 재연결 성공 시 카메라 재초기화
  if (needCameraReinit && wifiManager.isConnected() && mqttClient.connected())
  {
    reinit_camera_with_ble_safety();
    needCameraReinit = false;
  }

  if (wifiManager.isPendingPostConnectionSetup() && wifiManager.isConnected())
  {
    safeMqttDisconnect();                    // 이전 MQTT 연결 해제
    wifiManager.handlePostConnectionSetup(); // WiFi 관련 후처리 (NTP, 인증서 등)
    if (wifiManager.isConnected())
    {
      Serial.println(F("[MAIN] WiFi re-established or setup complete."));
      delay(1000); // 네트워크 안정화 대기
    }
    setup_mqtt(); // 새 WiFi 연결 후 MQTT 재설정 및 연결 시도
  }

  if (wifiManager.isConnected())
  {
    if (!mqttClient.connected()) // mqttConnected 플래그 대신 직접적인 client 상태 확인
    {
      // Serial.println(F("[MQTT] Not connected, attempting to setup MQTT in loop...")); // 너무 빈번할 수 있어 주석처리 또는 간헐적 로깅
      setup_mqtt(); // MQTT 연결 재시도
    }
    else
    {
      if (!mqttClient.loop())
      { // loop() 호출하고, 실패(연결 끊김 등) 시
        Serial.println(F("[MQTT] client.loop() returned false. Connection might be lost."));
        // mqttConnected = false; // 연결 상태 갱신
        // safeMqttDisconnect(); // 여기서 바로 disconnect 할 수도 있지만, setup_mqtt()에서 재시도 할 것임
      }
    }
  }
  else // WiFi 자체가 끊겼을 때
  {
    if (mqttClient.connected()) // WiFi는 끊겼는데 MQTT는 연결된 상태로 나올 경우 (드물지만)
    {
      Serial.println(F("[MQTT] WiFi disconnected, but MQTT client was connected. Disconnecting MQTT."));
      safeMqttDisconnect();
    }
    // WiFi 미연결 시 BLE 프로비저닝 로직 등 (기존 코드 유지)
    static unsigned long lastBleCheck = 0;
    if (millis() - lastBleCheck > 30000)
    {
      lastBleCheck = millis();
      bool bleAdvertising = (BLEDevice::getAdvertising() != nullptr);
      if (!BLEDevice::getInitialized())
      {
        Serial.println(F("[MAIN] WiFi not connected, BLE not initialized. Restarting BLE provisioning."));
        bleProvisioning.init("MONOPLEX_AI_SENSOR");
        bleProvisioning.start();
      }
      else if (!bleAdvertising)
      { // BLEDevice::getAdvertising()이 nullptr이면 광고 중이 아님
        Serial.println(F("[MAIN] WiFi not connected, BLE not advertising. Restarting BLE advertising."));
        bleProvisioning.start(); // 이미 init 되었다면 광고만 재시작
      }
    }
  }

  // 주기적인 센서 값 읽기 및 MQTT 발행
  static unsigned long lastSensorReadTime = 0;
  if (millis() - lastSensorReadTime > 5000) // 5초마다 읽기 및 발행
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
