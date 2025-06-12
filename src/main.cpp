#include <Arduino.h>
#include "config.h"

// 모듈 헤더들
#include "wifi_manager.h"
#include "ble_provisioning.h"
#include "device_identity.h"
#include "lighting_controller.h"
#include "sensor_manager.h"
#include "camera_manager.h"
#include "s3_manager.h"
#include "mqtt_manager.h"
#include "image_capture_handler.h"

// ESP32 라이브러리
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "esp_heap_caps.h"

// 전역 객체
WiFiClientSecure wifiClientSecure;

// 시스템 상태 변수
float ambient = 20.0;
bool needCameraReinit = false;

// 카메라 초기화 관련 변수
static int cameraInitAttempts = 0;
static unsigned long lastCameraInitAttempt = 0;
static const int MAX_CAMERA_INIT_ATTEMPTS = 5;
static const unsigned long CAMERA_INIT_RETRY_INTERVAL = 30000; // 30초 간격

// MQTT 콜백 함수
void mqtt_callback(char *topic, byte *payload, unsigned int length);

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    Serial.println(F("--- MQTT Message Received ---"));
    Serial.print(F("[MQTT] Raw Topic: "));
    Serial.println(topic);
    Serial.print(F("[MQTT] Payload: "));
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    Serial.println(F("-----------------------------"));

    String deviceId = DeviceIdentity::getDeviceId();
    String captureTopic = "devices/" + deviceId + "/image/capture";

    if (String(topic).equals(captureTopic)) {
        Serial.println(F("[MQTT] Image capture command received!"));

        String message = String((char *)payload).substring(0, length);
        JsonDocument jsonDoc;
        DeserializationError error = deserializeJson(jsonDoc, message);

        if (!error && jsonDoc["url"].is<const char *>()) {
            const char* newUrl = jsonDoc["url"].as<const char*>();
            Serial.print(F("[MQTT] New S3 URL received: "));
            Serial.println(newUrl);
            
            // 새 URL로 이미지 캡처 실행
            image_capture_handler_process_with_url(newUrl);
        } else {
            Serial.println(F("[MQTT] No valid 'url' in payload. Using default."));
            
            // 기본 URL로 이미지 캡처 실행
            image_capture_handler_process();
        }
    } else {
        Serial.println(F("[MQTT] Received message on an unhandled topic."));
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println(F("\n[SETUP] 시스템 부팅 중..."));

    // LED 초기화
    Serial.println(F("[SETUP] LED 초기화 중..."));
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_BLUE, LED_OFF);
    digitalWrite(LED_RED, LED_ON);
    Serial.println(F("[SETUP] LED 초기화 완료."));

    // NVS 초기화
    Serial.println(F("[SETUP] NVS 초기화 중..."));
    esp_err_t err_nvs = nvs_flash_init();
    if (err_nvs == ESP_ERR_NVS_NO_FREE_PAGES || err_nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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

    // WiFi 드라이버 리셋
    esp_wifi_stop();
    esp_wifi_deinit();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    delay(1000);

    // 모든 모듈 초기화
    Serial.println(F("[SETUP] 모듈 초기화 시작..."));
    
    // WiFi 초기화
    wifiManager.init(&wifiClientSecure);
    
    // BLE 서비스 초기화
    bleProvisioning.init(BLE_DEVICE_NAME);
    bleProvisioning.start();
    
    // 조명 제어 모듈 초기화
    lighting_controller_init(LIGHT_PIN, LIGHT_NIGHT_THRESHOLD);
    
    // 센서 관리 모듈 초기화
    sensor_manager_init(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // S3 관리 모듈 초기화
    s3_manager_init();
    
    // MQTT 관리 모듈 초기화
    mqtt_manager_init(wifiClientSecure, mqtt_callback);
    
    // 이미지 캡처 핸들러 초기화
    image_capture_handler_init();

    Serial.println(F("[SETUP] 모든 초기화 단계 완료."));
    Serial.printf("[MEM] Free DRAM heap: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void loop()
{
    // WiFi 관리
    wifiManager.update();

    // WiFi 연결 상태에 따른 MQTT 관리
    if (wifiManager.isConnected()) {
        // MQTT 연결 유지 및 관리
        if (!mqtt_manager_maintain_connection()) {
            // MQTT 연결이 끊어졌거나 문제가 있을 때만 재연결 시도
            mqtt_manager_connect();
        }
    } else {
        // WiFi 연결이 끊어진 경우
        if (mqtt_manager_is_connected()) {
            Serial.println(F("[WIFI] WiFi 연결 끊어짐, MQTT 상태 리셋"));
            digitalWrite(LED_BLUE, LED_OFF);
        }
    }

    // BLE로 WiFi 정보가 바뀌었거나, WiFi/MQTT 재연결 성공 시 카메라 재초기화
    if (needCameraReinit && wifiManager.isConnected() && mqtt_manager_is_connected()) {
        Serial.println(F("[MAIN] BLE WiFi 변경으로 인한 카메라 재초기화 시도..."));
        camera_manager_reinit_with_ble_safety();
        if (camera_manager_is_initialized()) {
            digitalWrite(LED_RED, LED_OFF);
            cameraInitAttempts = 0; // 재초기화 성공 시 카운터 리셋
            Serial.println(F("[MAIN] 카메라 재초기화 성공."));
        } else {
            Serial.println(F("[MAIN] 카메라 재초기화 실패. 일반 초기화 로직에서 재시도됩니다."));
        }
        needCameraReinit = false;
    }

    // WiFi 재연결 처리
    if (wifiManager.isPendingPostConnectionSetup() && wifiManager.isConnected()) {
        mqtt_manager_disconnect();
        wifiManager.handlePostConnectionSetup();
        if (wifiManager.isConnected()) {
            Serial.println(F("[MAIN] WiFi re-established or setup complete."));
            delay(1000);
        }
        mqtt_manager_connect();
    }

    // 카메라 초기화 (MQTT 연결 후) - 재시도 제한 적용
    if (wifiManager.isConnected() && mqtt_manager_is_connected() && 
        !camera_manager_is_initialized() && 
        cameraInitAttempts < MAX_CAMERA_INIT_ATTEMPTS) {
        
        unsigned long currentTime = millis();
        
        // 첫 시도이거나 재시도 간격이 지났을 때만 시도
        if (cameraInitAttempts == 0 || (currentTime - lastCameraInitAttempt > CAMERA_INIT_RETRY_INTERVAL)) {
            lastCameraInitAttempt = currentTime;
            cameraInitAttempts++;
            
            Serial.printf("[MAIN] 카메라 초기화 시도 %d/%d...\n", cameraInitAttempts, MAX_CAMERA_INIT_ATTEMPTS);
            
            if (camera_manager_init()) {
                Serial.println(F("[MAIN] 카메라 시스템 초기화 성공."));
                digitalWrite(LED_RED, LED_OFF);
                cameraInitAttempts = 0; // 성공 시 카운터 리셋
            } else {
                Serial.printf("[MAIN] 카메라 초기화 실패 (%d/%d).\n", cameraInitAttempts, MAX_CAMERA_INIT_ATTEMPTS);
                
                if (cameraInitAttempts >= MAX_CAMERA_INIT_ATTEMPTS) {
                    Serial.println(F("[MAIN] 카메라 초기화 최대 시도 횟수 도달. 카메라 없이 계속 동작합니다."));
                    Serial.println(F("[MAIN] 시스템은 센서 데이터 수집과 MQTT 통신을 계속 수행합니다."));
                }
            }
        }
    }

    // 주기적인 센서 값 읽기 및 MQTT 발행
    static unsigned long lastSensorReadTime = 0;
    if (millis() - lastSensorReadTime > SENSOR_PUBLISH_INTERVAL_MS) {
        lastSensorReadTime = millis();
        
        if (wifiManager.isConnected() && mqtt_manager_is_connected()) {
            // 센서 데이터 읽기 및 MQTT 발행
            JsonDocument jsonDoc;
            if (sensor_manager_get_data_json(jsonDoc)) {
                // 전역 ambient 변수 업데이트 (조명 제어용)
                if (jsonDoc["ambientLight"].is<float>()) {
                    ambient = jsonDoc["ambientLight"];
                }
                
                mqtt_manager_publish_sensor_data(jsonDoc);
            }
        } else {
            // MQTT 미연결 시 로컬에만 출력
            JsonDocument localDoc;
            if (sensor_manager_get_data_json(localDoc)) {
                char localBuffer[256];
                serializeJson(localDoc, localBuffer);
                Serial.printf("[SENSOR_LOCAL] %s\n", localBuffer);
                
                // 전역 ambient 변수 업데이트
                if (localDoc["ambientLight"].is<float>()) {
                    ambient = localDoc["ambientLight"];
                }
            } else {
                Serial.println(F("[SENSOR_LOCAL] 센서 데이터 읽기 실패"));
            }
        }
    }

    delay(100); // 루프 지연 시간
}