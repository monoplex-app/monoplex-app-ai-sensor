#include "mqtt_handler.h"
#include "config.h" // 서버 주소, 인증서 등 설정 값을 위해 포함
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// 이 모듈에 필요한 다른 핸들러 포함
#include "wifi_handler.h"
#include "camera_handler.h" // MQTT 메시지로 카메라 촬영을 제어하기 위해 필요
#include "ble_handler.h"

// 모듈 내부에서만 사용할 객체 및 변수
static WiFiClientSecure wifiNet;
static PubSubClient mqttClient(wifiNet);
static String clientId;
static String subTopic;
static String pubCaptureTopic;
static String pubSensorTopic;
static long lastReconnectAttempt = 0;

// isMqttConnected는 main.cpp에서 이미 정의됨 - 중복 정의 제거

// 내부 함수 프로토타입
static void mqttCallback(char* topic, byte* payload, unsigned int length);
static void subscribeToTopics();

// 함수 구현

void initMQTT() {
    // 1. MQTT에서 사용할 토픽과 클라이언트 ID 설정
    String macId = getMacAddress(); // WiFi 핸들러의 함수 사용
    
    if (macId.length() == 0) {
        Serial.println("MAC 주소를 가져올 수 없음, 기본값 사용");
        macId = "DEFAULT_DEVICE";
    }
    
    clientId = "mlx-" + macId;
    subTopic = macId + "/capture";
    pubCaptureTopic = macId + "/cdone";
    pubSensorTopic = macId + "/sensor";

    // 2. 보안 연결(TLS)을 위한 인증서 설정
    wifiNet.setCACert(ROOT_CA_CERT);
    wifiNet.setCertificate(CERTIFICATE_PEM);
    wifiNet.setPrivateKey(PRIVATE_KEY);
    wifiNet.setTimeout(15000);
    
    // 3. MQTT 서버 정보 및 콜백 함수 설정
    mqttClient.setServer(IOT_ENDPOINT, MQTTS_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(30);
    mqttClient.setSocketTimeout(10);
    mqttClient.setBufferSize(2048);
    
    Serial.println("MQTT 핸들러 초기화 완료");
}

void handleMqttConnection() {
#ifdef DISABLE_MQTT_FOR_DEVELOPMENT
    // 개발 모드: MQTT 연결 비활성화
    static bool showedMessage = false;
    if (!showedMessage) {
        Serial.println("⚠️  개발 모드: MQTT 연결이 비활성화되었습니다.");
        Serial.println("   config.h에서 DISABLE_MQTT_FOR_DEVELOPMENT를 주석처리하면 활성화됩니다.");
        showedMessage = true;
    }
    return;
#endif

    // MQTT가 이미 연결되어 있다면 아무것도 하지 않음
    if (mqttClient.connected()) {
        // 연결 상태 변화 감지
        if (!isMqttConnected) {
            Serial.println("MQTT 연결 복구됨");
            isMqttConnected = true;
            subscribeToTopics();
            sendMqttStatusUpdate(true);
        }
        return;
    }

    // 이전에 연결이 끊어졌다면
    if (isMqttConnected) {
        isMqttConnected = false;
        Serial.println("MQTT 연결 끊김.");
    }
    
    // 5초에 한 번씩만 재연결 시도
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        Serial.println("MQTT 연결 시도 중...");
        
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("MQTT 연결 성공");
            isMqttConnected = true;
            subscribeToTopics();
            sendMqttStatusUpdate(true);
        } else {
            int errorCode = mqttClient.state();
            Serial.print("MQTT 연결 실패, 에러 코드: ");
            Serial.println(errorCode);
            sendMqttStatusUpdate(false, "mqtt_error_" + String(errorCode));
        }
    }
}

void mqttLoop() {
    if (isWifiConnected) {
        mqttClient.loop();
    }
}

void publishMqttMessage(const String& topic, const String& payload) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic.c_str(), payload.c_str());
    } else {
        Serial.println("MQTT 연결 안됨, 메시지 전송 불가");
    }
}

void disconnectMQTT() {
    if (mqttClient.connected()) {
        Serial.println("명령에 따라 MQTT 연결을 종료합니다.");
        mqttClient.disconnect();
        isMqttConnected = false;
        sendMqttStatusUpdate(false, "mqtt_disconnected");
    }
}



// 내부(static) 함수 구현

// 토픽 구독을 처리하는 함수
static void subscribeToTopics() {
    bool subscribed = mqttClient.subscribe(subTopic.c_str(), 0);
    if (subscribed) {
        Serial.println("토픽 구독 성공");
    } else {
        Serial.println("토픽 구독 실패");
    }
}

// MQTT 메시지 수신 시 자동으로 호출되는 콜백 함수
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // 수신된 토픽이 "capture" 명령 토픽이라면
    if (String(topic) == subTopic) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);

        if (error) {
            Serial.print("JSON 파싱 실패: ");
            Serial.println(error.c_str());
            return;
        }

        if (doc["url"].is<JsonVariant>()) {
            const char* url = doc["url"];
            Serial.println("카메라 촬영 명령 수신");
            triggerCameraCapture(String(url)); 
        }
    }
}
