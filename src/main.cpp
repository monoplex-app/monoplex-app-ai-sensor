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

enum class LedPattern { solidOn, solidOff, slowBlink, fastBlink, doubleBlink };

static LedPattern currentLedPattern = LedPattern::solidOn;
static uint8_t ledPatternStep = 0;
static bool ledOutputState = false;

static void setStatusLed(bool on) {
    if (ledOutputState != on) {
        digitalWrite(LED_RED, on ? LED_ON : LED_OFF);
        ledOutputState = on;
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);

    // // 3초간 대기하여 모니터를 켤 시간을 줍니다.
    // delay(3000);
    // Serial.println("\nMONOPLEX AI SENSOR 시작");

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

    // WiFi 스캔을 먼저 수행 (BLE에서 사용할 목록 준비)
    Serial.println("주변 WiFi 네트워크 스캔 중...");
    WiFi.mode(WIFI_STA);
    delay(100);
    String wifiListJson = scanWifiNetworks(deviceUid);
    Serial.println("WiFi 스캔 완료, BLE 초기화 진행");
    
    // BLE 초기화 (스캔된 WiFi 목록을 사용)
    initBLE(deviceUid, wifiListJson);
    
    // 연결 초기화
    if (areWiFiCredentialsAvailable()) {
        initWiFi();
    }
    
    // MQTT 초기화 (WiFi 초기화 후)
    initMQTT();
    
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
    pinMode(LED_RED, OUTPUT);
    pinMode(LIGHT, OUTPUT);

    setStatusLed(true); // 부팅 시 LED 켬
    currentLedPattern = LedPattern::solidOn;
    ledPatternStep = 0;
    ledState = false;
    lastLedToggleTime = millis();
    digitalWrite(LIGHT, LIGHT_OFF);
}

void updateStatusLEDs() {
    unsigned long currentTime = millis();

    LedPattern desiredPattern;
    if (!isWifiConnected) {
        desiredPattern = isBleClientConnected ? LedPattern::fastBlink : LedPattern::slowBlink;
    } else if (!isMqttConnected) {
        desiredPattern = LedPattern::doubleBlink;
    } else {
        desiredPattern = LedPattern::solidOn;
    }

    if (desiredPattern != currentLedPattern) {
        currentLedPattern = desiredPattern;
        ledPatternStep = 0;
        ledState = false;
        lastLedToggleTime = currentTime;

        switch (currentLedPattern) {
            case LedPattern::solidOn:
                setStatusLed(true);
                break;
            case LedPattern::solidOff:
                setStatusLed(false);
                break;
            case LedPattern::slowBlink:
            case LedPattern::fastBlink:
                setStatusLed(false);
                break;
            case LedPattern::doubleBlink:
                setStatusLed(true);
                break;
        }
    }

    switch (currentLedPattern) {
        case LedPattern::solidOn:
            setStatusLed(true);
            break;
        case LedPattern::solidOff:
            setStatusLed(false);
            break;
        case LedPattern::slowBlink:
            if (currentTime - lastLedToggleTime >= static_cast<unsigned long>(ledToggleInterval)) {
                lastLedToggleTime = currentTime;
                ledState = !ledState;
                setStatusLed(ledState);
            }
            break;
        case LedPattern::fastBlink: {
            const unsigned long fastBlinkInterval = 150;
            if (currentTime - lastLedToggleTime >= fastBlinkInterval) {
                lastLedToggleTime = currentTime;
                ledState = !ledState;
                setStatusLed(ledState);
            }
            break;
        }
        case LedPattern::doubleBlink: {
            const unsigned long doubleBlinkIntervals[] = {120, 120, 120, 640};
            const bool doubleBlinkStates[] = {true, false, true, false};
            if (currentTime - lastLedToggleTime >= doubleBlinkIntervals[ledPatternStep]) {
                lastLedToggleTime = currentTime;
                ledPatternStep = (ledPatternStep + 1) % 4;
                setStatusLed(doubleBlinkStates[ledPatternStep]);
            }
            break;
        }
    }
}

void handleSensorDataPublishing() {
    static unsigned long lastSensorPublish = 0;
    static bool firstPublishDone = false;
    const unsigned long sensorPublishInterval = 1000; // 1초마다 센서 데이터 발행
    
    unsigned long currentTime = millis();
    
    // // 로컬 콘솔 로깅: MQTT 연결 여부와 관계없이 1초마다 근접센서 값을 출력
    // static unsigned long lastLocalLog = 0;
    // if (currentTime - lastLocalLog >= 1000) {
    //     lastLocalLog = currentTime;
    //     // 센서 값 갱신 후 근접센서 값 출력
    //     readAllSensors();
    //     SensorData latest = getSensorDataStruct();
    //     Serial.printf("Proximity: %u\n", latest.proximity);
    // }
    
    // MQTT 연결 후 첫 발행은 5초 후, 이후 1초마다 발행
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
