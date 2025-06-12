#include "mqtt_manager.h"
#include "config.h"
#include "device_identity.h"
#include <Arduino.h>

// 내부 상태 변수
static PubSubClient* g_mqtt_client = nullptr;
static WiFiClientSecure* g_wifi_client = nullptr;
static MqttMessageCallback g_message_callback = nullptr;

static bool g_mqtt_connected = false;
static unsigned long g_last_mqtt_attempt = 0;
static unsigned long g_last_mqtt_keepalive = 0;
static int g_mqtt_reconnect_attempts = 0;
static unsigned long g_last_mqtt_activity = 0;

void mqtt_manager_init(WiFiClientSecure& wifiClient, MqttMessageCallback callback)
{
    Serial.println("[MQTT] MQTT 관리 모듈 초기화");
    
    g_wifi_client = &wifiClient;
    g_message_callback = callback;
    
    // PubSubClient 객체 생성
    static PubSubClient mqttClient(wifiClient);
    g_mqtt_client = &mqttClient;
    
    // 초기 상태 설정
    g_mqtt_connected = false;
    g_last_mqtt_attempt = 0;
    g_last_mqtt_keepalive = 0;
    g_mqtt_reconnect_attempts = 0;
    g_last_mqtt_activity = 0;
    
    Serial.println("[MQTT] MQTT 관리 모듈 초기화 완료");
}

bool mqtt_manager_connect()
{
    if (!g_mqtt_client || !g_wifi_client) {
        Serial.println("[MQTT] 오류: 초기화되지 않음");
        return false;
    }

    if (millis() - g_last_mqtt_attempt < MQTT_RETRY_INTERVAL_MS) {
        return false;
    }

    if (g_mqtt_reconnect_attempts >= MAX_MQTT_RECONNECT_ATTEMPTS) {
        Serial.println(F("[MQTT] 최대 재연결 시도 횟수 초과, 대기 중..."));
        if (millis() - g_last_mqtt_attempt > MQTT_RETRY_INTERVAL_MS * 3) {
            g_mqtt_reconnect_attempts = 0;
        }
        return false;
    }

    g_last_mqtt_attempt = millis();
    g_mqtt_reconnect_attempts++;

    Serial.printf("[MQTT] 재연결 시도 횟수: %d/%d\n", g_mqtt_reconnect_attempts, MAX_MQTT_RECONNECT_ATTEMPTS);

    // 기존 연결 정리
    if (g_mqtt_client->connected()) {
        g_mqtt_client->disconnect();
        delay(100);
    }

    // MQTT 클라이언트 재설정
    g_mqtt_client->setServer(fpstr_to_cstr(FPSTR(AWS_IOT_ENDPOINT)), AWS_MQTT_PORT);
    g_mqtt_client->setCallback(g_message_callback);
    g_mqtt_client->setBufferSize(MQTT_BUFFER_SIZE);
    g_mqtt_client->setKeepAlive(30);
    g_mqtt_client->setSocketTimeout(15);

    String clientId = DeviceIdentity::getDeviceId();
    Serial.print(F("[MQTT] Attempting to connect with Client ID: "));
    Serial.println(clientId);

    bool cleanSession = true;
    Serial.println(F("[MQTT] 연결 시도 중..."));

    if (g_mqtt_client->connect(clientId.c_str(), NULL, NULL, NULL, 0, cleanSession, NULL)) {
        Serial.println(F("[MQTT] AWS IoT Core connection SUCCESSFUL!"));
        g_mqtt_connected = true;
        g_mqtt_reconnect_attempts = 0;

        // 구독 설정
        delay(500);
        String imageCaptureTopic = "devices/" + clientId + "/image/capture";
        Serial.print(F("[MQTT] Subscribing to topic: "));
        Serial.println(imageCaptureTopic);

        if (g_mqtt_client->subscribe(imageCaptureTopic.c_str(), 1)) {
            Serial.println(F("[MQTT] Subscription to imageCaptureTopic SUCCESSFUL."));
        } else {
            Serial.println(F("[MQTT] Subscription to imageCaptureTopic FAILED."));
        }

        // 연결 성공 상태 메시지 발행
        String statusTopic = "devices/" + clientId + "/status";
        String statusMsg = "{\"status\":\"connected\",\"timestamp\":" + String(millis()) + "}";
        g_mqtt_client->publish(statusTopic.c_str(), statusMsg.c_str(), true);

        Serial.println(F("[MQTT] 초기 상태 메시지 발행 완료"));
        return true;
    } else {
        Serial.print(F("[MQTT] AWS IoT Core connection FAILED, rc="));
        Serial.print(g_mqtt_client->state());
        Serial.println(F(" Will retry..."));

        g_mqtt_connected = false;
        return false;
    }
}

bool mqtt_manager_maintain_connection()
{
    if (!g_mqtt_client) {
        return false;
    }

    bool currentlyConnected = g_mqtt_client->connected();

    if (currentlyConnected) {
        // 메시지 처리
        g_mqtt_client->loop();

        // 연결 상태가 바뀌었는지 확인
        if (!g_mqtt_connected) {
            Serial.println(F("[MQTT] 연결 복구됨"));
            g_mqtt_connected = true;
            g_mqtt_reconnect_attempts = 0;
        }

        // 주기적 heartbeat 전송
        if (millis() - g_last_mqtt_keepalive > MQTT_KEEPALIVE_INTERVAL_MS) {
            g_last_mqtt_keepalive = millis();

            String deviceId = DeviceIdentity::getDeviceId();
            String heartbeatTopic = "devices/" + deviceId + "/status";
            String heartbeatMsg = "{\"status\":\"online\"}";

            if (!g_mqtt_client->publish(heartbeatTopic.c_str(), heartbeatMsg.c_str())) {
                Serial.println(F("[MQTT] Heartbeat 전송 실패"));
            }
        }

        return true;
    } else {
        // 연결이 끊어진 경우
        if (g_mqtt_connected) {
            Serial.print(F("[MQTT] 연결이 끊어졌습니다. 상태 코드: "));
            Serial.println(g_mqtt_client->state());
            g_mqtt_connected = false;
        }
        return false;
    }
}

void mqtt_manager_disconnect()
{
    if (g_mqtt_client && g_mqtt_client->connected()) {
        Serial.println(F("[MQTT] Disconnecting safely..."));
        g_mqtt_client->disconnect();
        delay(100);
        g_mqtt_connected = false;
        Serial.println(F("[MQTT] Disconnected."));
    }
}

bool mqtt_manager_publish_sensor_data(JsonDocument& jsonDoc)
{
    if (!g_mqtt_client || !g_mqtt_client->connected()) {
        return false;
    }

    char jsonBuffer[256];
    serializeJson(jsonDoc, jsonBuffer);

    String deviceId = DeviceIdentity::getDeviceId();
    String topic = "devices/" + deviceId + "/sensors/data";

    Serial.print(F("[MQTT] 발행 ["));
    Serial.print(topic);
    Serial.print(F("]: "));
    Serial.println(jsonBuffer);

    if (g_mqtt_client->publish(topic.c_str(), jsonBuffer)) {
        Serial.println(F("[MQTT] 메시지 발행 성공"));
        return true;
    } else {
        Serial.println(F("[MQTT] 메시지 발행 실패"));
        return false;
    }
}

bool mqtt_manager_publish_image_result(bool success, const String& error_message)
{
    if (!g_mqtt_client || !g_mqtt_client->connected()) {
        return false;
    }

    String deviceId = DeviceIdentity::getDeviceId();
    String topic = "devices/" + deviceId + "/image/done";
    
    String message;
    if (success) {
        message = "{\"upload\":1}";
        Serial.println("[MQTT] Published image upload success.");
    } else {
        message = "{\"upload\":0, \"error\":\"" + error_message + "\"}";
        Serial.println("[MQTT] Published image upload failure.");
    }

    return g_mqtt_client->publish(topic.c_str(), message.c_str());
}

bool mqtt_manager_is_connected()
{
    return g_mqtt_connected;
}

PubSubClient& mqtt_manager_get_client()
{
    return *g_mqtt_client;
} 