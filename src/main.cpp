#include "config.h"
#include "globals.h"
#include "wifi_handler.h"
#include "ble_handler.h"
#include "mqtt_handler.h"
#include "camera_handler.h"
#include "sensor_handler.h"
#include "eeprom_handler.h"

// 함수 선언
// setup()과 loop()보다 앞에 이 함수들이 존재한다고 미리 알려줌
void initPins();
void updateStatusLEDs();
void handleSensorDataPublishing();

// === 전역 변수 정의 ===
// globals.h에서 extern으로 선언된 변수들의 실제 정의 (메모리 할당)
String deviceUid = "";                  // 장치 고유 식별자
bool isWifiConnected = false;           // WiFi 연결 상태
bool isMqttConnected = false;           // MQTT 연결 상태
bool isBleClientConnected = false;      // BLE 클라이언트 연결 상태

// LED 상태 관리 변수
unsigned long lastLedToggleTime = 0;    // 마지막 LED 토글 시간
const long ledToggleInterval = 500;     // 0.5초 마다 토글
bool ledState = false;                  // LED 상태

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);

    delay(3000); // 3초간 대기하여 모니터를 켤 시간을 줍니다.
    
    Serial.println("\nMONOPLEX AI SENSOR 시작");

    // EEPROM 초기화 및 저장된 설정 로드
    initEEPROM();
    deviceUid = getMacAddress(); // MAC 주소를 고유 식별자로 사용
    loadWiFiCredentials(ssid, password);

    // 하드웨어 초기화
    initPins();
    initSensors();
    
    Serial.println("카메라 초기화 호출 시작...");
    bool cameraInitResult = initCamera();
    Serial.printf("카메라 초기화 결과: %s\n", cameraInitResult ? "성공" : "실패");

    // 연결 초기화
    if (areWiFiCredentialsAvailable()) {
        initWiFi();
    }
    
    // MQTT 초기화 (WiFi 초기화 후)
    initMQTT();
    
    initBLE(deviceUid);
    
    Serial.println("설정 완료. 메인 루프 진입");
}

void loop() {
    // WiFi 및 MQTT 연결 로직 처리
    handleWiFiConnection();
    if (isWifiConnected) {
        handleMqttConnection();
        mqttLoop(); // MQTT 메시지 수신/처리
    }

    // 수신된 BLE 통신 처리
    handleBLE();

    // LED 상태 업데이트
    updateStatusLEDs();

    // 주기적인 센서 읽기 및 MQTT 발행
    handleSensorDataPublishing();
}

void initPins() {
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LIGHT, OUTPUT);

    digitalWrite(LED_BLUE, LED_OFF);
    digitalWrite(LED_RED, LED_ON); // 부팅 시 빨간색 LED 켜기
    digitalWrite(LIGHT, LIGHT_OFF);
}

void updateStatusLEDs() {
    unsigned long currentTime = millis();
    if (currentTime - lastLedToggleTime >= ledToggleInterval) {
        lastLedToggleTime = currentTime;
        ledState = !ledState;

        // 빨간색 LED: WiFi 연결 시 깜빡임, 연결 안되면 켜짐
        if (isWifiConnected) {
            digitalWrite(LED_RED, ledState ? LED_ON : LED_OFF);
        } else {
            digitalWrite(LED_RED, LED_ON);
        }

        // 파란색 LED: BLE 클라이언트 연결 시 켜짐, 연결 안되면 꺼짐
        digitalWrite(LED_BLUE, isBleClientConnected ? LED_ON : LED_OFF);
    }
}

void handleSensorDataPublishing() {
    static unsigned long lastSensorPublish = 0;
    static bool firstPublishDone = false;
    const unsigned long sensorPublishInterval = 10000; // 10초마다 센서 데이터 발행
    
    unsigned long currentTime = millis();
    
    // MQTT 연결 후 첫 발행은 5초 후, 이후 10초마다 발행
    bool shouldPublish = false;
    if (isMqttConnected) {
        if (!firstPublishDone && currentTime > 5000) {
            shouldPublish = true;
            firstPublishDone = true;
        } else if (firstPublishDone && (currentTime - lastSensorPublish >= sensorPublishInterval)) {
            shouldPublish = true;
        }
    }
    
    if (shouldPublish) {
        lastSensorPublish = currentTime;
        
        // 센서 데이터 읽기
        readAllSensors();
        
        // JSON 형태로 센서 데이터 생성
        String sensorJson = getSensorDataJson();
        
        // MQTT로 센서 데이터 발행
        String sensorTopic = deviceUid + "/sensor";
        publishMqttMessage(sensorTopic, sensorJson);
    }
}