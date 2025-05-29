#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "ble_provisioning.h"
#include "device_identity.h"
#include <nvs_flash.h>
#include <esp_wifi.h>

#include <Wire.h>
#include <Adafruit_VCNL4040.h>
#include <SparkFun_BMI270_Arduino_Library.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "camera_handler.h"
extern void camera_deinit_system();
#include "esp_heap_caps.h"

// AWS IoT Core 설정
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
#define LIGHT_NIGHT 10.0

float ambient = 20.0;
unsigned long startTime = 0;
bool cameraOk = false;

// MQTT 연결 관리 변수들
bool mqttConnected = false;
unsigned long lastMqttAttempt = 0;
const unsigned long MQTT_RETRY_INTERVAL = 20000; // 20초로 단축
unsigned long lastMqttKeepAlive = 0;
const unsigned long MQTT_KEEPALIVE_INTERVAL = 25000; // 25초로 증가 (heartbeat 주기)
int mqttReconnectAttempts = 0;
const int MAX_MQTT_RECONNECT_ATTEMPTS = 5; // 재시도 횟수 증가
unsigned long lastMqttActivity = 0;        // 마지막 MQTT 활동 시간

// 함수 프로토타입 선언
void send_captured_image();
void checkMemory();
void setup_mqtt();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
bool maintainMqttConnection();

void checkMemory()
{
  Serial.printf("[MEM] Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("[MEM] Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("[MEM] Min free heap: %u bytes\n", ESP.getMinFreeHeap());
  Serial.printf("[MEM] Max alloc heap: %u bytes\n", ESP.getMaxAllocHeap());
}

camera_fb_t *safeGetCameraFrame(int maxRetries = 5)
{
  checkMemory();

  sensor_t *s = esp_camera_sensor_get();
  if (!s)
  {
    Serial.println("[CAMERA] 센서 포인터 없음!");
    return nullptr;
  }

  for (int i = 0; i < maxRetries; i++)
  {
    Serial.printf("[CAMERA] Capturing image (attempt %d/%d)...\n", i + 1, maxRetries);

    if (i > 0)
    {
      delay(500);
    }

    if (i > 1)
    {
      ESP.getFreeHeap();
      delay(100);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb)
    {
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

    if (i == maxRetries - 2)
    {
      Serial.println("[CAMERA] 센서 설정 재적용 시도...");
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
      delay(300);
    }
  }

  Serial.println("[CAMERA] Camera capture failed after all attempts.");
  return nullptr;
}

bool reinitializeCamera()
{
  Serial.println("[CAMERA] 카메라 재초기화 시작...");

  esp_camera_deinit();
  delay(1000);

  ESP.getFreeHeap();
  checkMemory();

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
  Serial.println("[S3] 이미지 업로드 시작");
  startTime = millis();

  if (!fb || fb->len == 0)
  {
    Serial.println("[S3] ⚠️ 유효하지 않은 이미지 프레임");
    return false;
  }

  WiFiClientSecure wifiNet;
  wifiNet.setInsecure();

  if (!wifiNet.connect(host, 443))
  {
    Serial.println("[S3] ❌ S3 연결 실패!");
    return false;
  }
  Serial.println("[S3] ✅ S3 연결 성공");

  String path = String(presigned_url);
  path.replace(String("https://") + host, "");

  wifiNet.print(String("PUT ") + path + " HTTP/1.1\r\n" +
                String("Host: ") + host + "\r\n" +
                "Content-Type: image/jpeg\r\n" +
                String("Content-Length: ") + fb->len + "\r\n" +
                "Connection: close\r\n\r\n");

  size_t totalSent = 0;
  const size_t chunkSize = 16384;
  while (totalSent < fb->len)
  {
    size_t remaining = fb->len - totalSent;
    size_t toSend = (remaining < chunkSize) ? remaining : chunkSize;

    size_t sent = wifiNet.write(fb->buf + totalSent, toSend);
    if (sent == 0)
    {
      Serial.println("[S3] *** write() 실패, 0 바이트 전송됨");
      break;
    }

    totalSent += sent;
    Serial.printf("[S3] *** Sent %u bytes (Total: %u / %u)\n", sent, totalSent, fb->len);
  }

  Serial.printf("[S3] *** 총 전송 바이트: %u / %u\n", totalSent, fb->len);
  Serial.printf("[S3] Time to upload: %lu msec\n", millis() - startTime);

  wifiNet.stop();
  Serial.println("[S3] *** 업로드 완료");

  return (totalSent == fb->len);
}

void send_captured_image()
{
  int flag = 0;
  if (ambient < LIGHT_NIGHT)
  {
    flag = 1;
    Serial.println("[LIGHT] LIGHT ON ...");
    digitalWrite(LIGHT_PIN, LIGHT_ON);
  }

  unsigned long localStartTime = millis();
  Serial.println("[CAPTURE] Attempting to capture image...");

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

  camera_fb_t *fb = safeGetCameraFrame(5);

  if (!fb)
  {
    Serial.println("[CAPTURE] 첫 번째 캡처 실패, 카메라 재초기화 시도...");
    cameraOk = reinitializeCamera();
    if (cameraOk)
    {
      fb = safeGetCameraFrame(3);
    }
  }

  if (!fb)
  {
    Serial.println("[CAPTURE] Failed to get camera frame after retries and reinit.");
    if (flag == 1)
    {
      digitalWrite(LIGHT_PIN, LIGHT_OFF);
    }

    // MQTT로 실패 통보
    if (mqttClient.connected())
    {
      String deviceId = DeviceIdentity::getDeviceId();
      String topic = "devices/" + deviceId + "/image/done";
      mqttClient.publish(topic.c_str(), "{\"upload\":0, \"error\":\"camera_capture_failed\"}");
    }
    return;
  }

  if (flag == 1)
  {
    digitalWrite(LIGHT_PIN, LIGHT_OFF);
  }

  Serial.printf("[CAPTURE] Time to capture: %lu ms\n", millis() - localStartTime);
  Serial.printf("[CAPTURE] Image Size: %zu KB\n", fb->len / 1024);
  Serial.println("[CAPTURE] Image captured. Preparing to send to S3...");

  const char *uploadUrlToUse = currentUploadUrl.isEmpty() ? s3Url_default : currentUploadUrl.c_str();
  const char *s3HostToUse = currentS3Host.isEmpty() ? s3Host_default : currentS3Host.c_str();

  Serial.printf("[S3_UPLOAD] Using URL: %s\n", uploadUrlToUse);
  Serial.printf("[S3_UPLOAD] Using Host: %s\n", s3HostToUse);

  bool uploadResult = uploadToS3Raw(fb, uploadUrlToUse, s3HostToUse);

  esp_camera_fb_return(fb);

  // MQTT로 완료 통보
  if (mqttClient.connected())
  {
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
}

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

    String message = String((char *)payload).substring(0, length);

    JsonDocument jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, message);

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

void safeMqttDisconnect()
{
  if (mqttClient.connected())
  {
    Serial.println(F("[MQTT] Disconnecting safely..."));
    mqttClient.disconnect();
    delay(100);
    mqttConnected = false;
    Serial.println(F("[MQTT] Disconnected."));
  }
}

bool maintainMqttConnection()
{
  if (!wifiManager.isConnected())
  {
    if (mqttConnected)
    {
      Serial.println(F("[MQTT] WiFi 연결 끊어짐, MQTT 연결 상태 리셋"));
      mqttConnected = false;
    }
    return false;
  }

  // MQTT 연결 상태 확인
  bool currentlyConnected = mqttClient.connected();

  if (currentlyConnected)
  {
    // 메시지 처리 - 매 루프마다 호출
    mqttClient.loop();

    // 연결 상태가 바뀌었는지 확인
    if (!mqttConnected)
    {
      Serial.println(F("[MQTT] 연결 복구됨"));
      mqttConnected = true;
      mqttReconnectAttempts = 0;
      digitalWrite(LED_BLUE, LED_ON);
    }

    // 주기적으로 가벼운 heartbeat 전송 (덜 자주)
    if (millis() - lastMqttKeepAlive > MQTT_KEEPALIVE_INTERVAL)
    {
      lastMqttKeepAlive = millis();

      // 가벼운 heartbeat 메시지
      String deviceId = DeviceIdentity::getDeviceId();
      String heartbeatTopic = "devices/" + deviceId + "/status";
      String heartbeatMsg = "{\"status\":\"online\"}";

      // heartbeat 실패해도 바로 연결을 끊지 않음
      if (!mqttClient.publish(heartbeatTopic.c_str(), heartbeatMsg.c_str()))
      {
        Serial.println(F("[MQTT] Heartbeat 전송 실패 (연결 상태 모니터링 중)"));
      }
    }

    return true;
  }
  else
  {
    // 연결이 끊어진 경우
    if (mqttConnected)
    {
      Serial.print(F("[MQTT] 연결이 끊어졌습니다. 상태 코드: "));
      Serial.println(mqttClient.state());

      // 연결 끊김 원인 분석
      switch (mqttClient.state())
      {
      case -4:
        Serial.println(F("[MQTT] 서버 연결 타임아웃"));
        break;
      case -3:
        Serial.println(F("[MQTT] 네트워크 연결 끊어짐"));
        break;
      case -2:
        Serial.println(F("[MQTT] 네트워크 연결 실패"));
        break;
      case -1:
        Serial.println(F("[MQTT] 연결 거부됨"));
        break;
      case 1:
        Serial.println(F("[MQTT] 잘못된 프로토콜 버전"));
        break;
      case 2:
        Serial.println(F("[MQTT] 클라이언트 ID 거부됨"));
        break;
      case 3:
        Serial.println(F("[MQTT] 서버 사용 불가"));
        break;
      case 4:
        Serial.println(F("[MQTT] 잘못된 사용자명 또는 비밀번호"));
        break;
      case 5:
        Serial.println(F("[MQTT] 권한 없음"));
        break;
      }

      mqttConnected = false;
      digitalWrite(LED_BLUE, LED_OFF);
    }
    return false;
  }
}

void setup_mqtt()
{
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
    return;
  }

  if (mqttReconnectAttempts >= MAX_MQTT_RECONNECT_ATTEMPTS)
  {
    Serial.println(F("[MQTT] 최대 재연결 시도 횟수 초과, 대기 중..."));
    if (millis() - lastMqttAttempt > MQTT_RETRY_INTERVAL * 3)
    {
      mqttReconnectAttempts = 0;
    }
    return;
  }

  lastMqttAttempt = millis();
  mqttReconnectAttempts++;

  Serial.println(F("[MQTT] setup_mqtt() called."));
  Serial.printf("[MQTT] 재연결 시도 횟수: %d/%d\n", mqttReconnectAttempts, MAX_MQTT_RECONNECT_ATTEMPTS);

  // 기존 연결 정리 (중요!)
  if (mqttClient.connected())
  {
    mqttClient.disconnect();
    delay(100);
  }

  // MQTT 클라이언트 재설정
  mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_MQTT_PORT);
  mqttClient.setCallback(mqtt_callback);
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
  mqttClient.setKeepAlive(30);     // Keep alive 시간을 30초로 단축
  mqttClient.setSocketTimeout(15); // 소켓 타임아웃 15초

  String clientId = DeviceIdentity::getDeviceId();
  Serial.print(F("[MQTT] Attempting to connect with Client ID: "));
  Serial.println(clientId);

  // 연결 옵션 설정
  bool cleanSession = true; // Clean session 활성화

  Serial.println(F("[MQTT] 연결 시도 중..."));

  if (mqttClient.connect(clientId.c_str(), NULL, NULL, NULL, 0, cleanSession, NULL))
  {
    Serial.println(F("[MQTT] AWS IoT Core connection SUCCESSFUL!"));
    mqttConnected = true;
    mqttReconnectAttempts = 0;
    digitalWrite(LED_BLUE, LED_ON);

    // 구독을 약간 지연시켜 연결 안정화
    delay(500);

    String imageCaptureTopic = "devices/" + clientId + "/image/capture";
    Serial.print(F("[MQTT] Subscribing to topic: "));
    Serial.println(imageCaptureTopic);

    if (mqttClient.subscribe(imageCaptureTopic.c_str(), 1)) // QoS 1 사용
    {
      Serial.println(F("[MQTT] Subscription to imageCaptureTopic SUCCESSFUL."));
    }
    else
    {
      Serial.println(F("[MQTT] Subscription to imageCaptureTopic FAILED."));
    }

    // 연결 성공 상태 메시지 발행
    String statusTopic = "devices/" + clientId + "/status";
    String statusMsg = "{\"status\":\"connected\",\"timestamp\":" + String(millis()) + "}";
    mqttClient.publish(statusTopic.c_str(), statusMsg.c_str(), true); // Retained 메시지

    Serial.println(F("[MQTT] 초기 상태 메시지 발행 완료"));
  }
  else
  {
    Serial.print(F("[MQTT] AWS IoT Core connection FAILED, rc="));
    Serial.print(mqttClient.state());
    Serial.println(F(" Will retry..."));

    // 상세한 연결 실패 원인 분석
    switch (mqttClient.state())
    {
    case -4:
      Serial.println(F("[MQTT] 서버 연결 타임아웃 - 네트워크 또는 방화벽 문제 가능성"));
      break;
    case -3:
      Serial.println(F("[MQTT] 네트워크 연결 끊어짐 - WiFi 신호 또는 인터넷 연결 확인 필요"));
      break;
    case -2:
      Serial.println(F("[MQTT] 네트워크 연결 실패 - DNS 또는 라우팅 문제 가능성"));
      break;
    case -1:
      Serial.println(F("[MQTT] 연결 거부됨 - 일반적인 연결 오류"));
      break;
    case 1:
      Serial.println(F("[MQTT] 잘못된 프로토콜 버전 - MQTT 버전 호환성 문제"));
      break;
    case 2:
      Serial.println(F("[MQTT] 클라이언트 ID 거부됨 - 중복 연결 또는 잘못된 ID"));
      break;
    case 3:
      Serial.println(F("[MQTT] 서버 사용 불가 - AWS IoT Core 서비스 문제"));
      break;
    case 4:
      Serial.println(F("[MQTT] 잘못된 사용자명 또는 비밀번호 - 인증 문제"));
      break;
    case 5:
      Serial.println(F("[MQTT] 권한 없음 - AWS IoT 정책 또는 인증서 문제"));
      break;
    }

    mqttConnected = false;
    digitalWrite(LED_BLUE, LED_OFF);

    // 연결 실패 시 약간의 추가 지연
    delay(1000);
  }
}

void publish_sensor_data()
{
  if (!mqttClient.connected())
  {
    return;
  }

  JsonDocument jsonDoc;

  uint16_t proximity = vcnl4040.getProximity();
  ambient = vcnl4040.getLux();
  jsonDoc["proximity"] = proximity;
  jsonDoc["ambientLight"] = ambient;

  if (imu.getSensorData() == BMI2_OK)
  {
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

bool needCameraReinit = false;

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

  // LED 및 GPIO 초기화
  Serial.println(F("[SETUP] LED 및 GPIO 초기화 중..."));
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LED_BLUE, LED_OFF);
  digitalWrite(LED_RED, LED_ON);
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
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  delay(1000);

  // WiFi 초기화
  wifiManager.init(&wifiClientSecure);

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

  Serial.println(F("[SETUP] 모든 초기화 단계 완료."));
  Serial.printf("[MEM] Free DRAM heap: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void loop()
{
  wifiManager.update();

  // WiFi 연결 상태에 따른 MQTT 관리
  if (wifiManager.isConnected())
  {
    // MQTT 연결 유지 및 관리
    if (!maintainMqttConnection())
    {
      // MQTT 연결이 끊어졌거나 문제가 있을 때만 재연결 시도
      // 너무 자주 재연결 시도하지 않도록 제한
      if (millis() - lastMqttAttempt > 5000) // 최소 5초 간격
      {
        setup_mqtt();
      }
    }
  }
  else
  {
    // WiFi 연결이 끊어진 경우
    if (mqttConnected)
    {
      Serial.println(F("[WIFI] WiFi 연결 끊어짐, MQTT 상태 리셋"));
      mqttConnected = false;
      digitalWrite(LED_BLUE, LED_OFF);
    }
  }

  // BLE로 WiFi 정보가 바뀌었거나, WiFi/MQTT 재연결 성공 시 카메라 재초기화
  if (needCameraReinit && wifiManager.isConnected() && mqttClient.connected())
  {
    reinit_camera_with_ble_safety();
    needCameraReinit = false;
  }

  if (wifiManager.isPendingPostConnectionSetup() && wifiManager.isConnected())
  {
    safeMqttDisconnect();
    wifiManager.handlePostConnectionSetup();
    if (wifiManager.isConnected())
    {
      Serial.println(F("[MAIN] WiFi re-established or setup complete."));
      delay(1000);
    }
    setup_mqtt();
  }

  // 카메라 초기화 (MQTT 연결 후)
  if (wifiManager.isConnected() && mqttClient.connected() && !cameraOk)
  {
    Serial.println(F("[MAIN] MQTT 연결됨, 카메라 초기화 시도..."));
    cameraOk = camera_init_system();
    if (cameraOk)
    {
      Serial.println(F("[MAIN] 카메라 시스템 초기화 성공."));
      digitalWrite(LED_RED, LED_OFF);
    }
    else
    {
      Serial.println(F("[MAIN] 카메라 시스템 초기화 실패!"));
    }
  }

  // 주기적인 센서 값 읽기 및 MQTT 발행
  static unsigned long lastSensorReadTime = 0;
  if (millis() - lastSensorReadTime > 5000)
  {
    lastSensorReadTime = millis();
    if (wifiManager.isConnected() && mqttClient.connected())
    {
      publish_sensor_data();
    }
    else
    {
      // MQTT 미연결 시 로컬에만 출력
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