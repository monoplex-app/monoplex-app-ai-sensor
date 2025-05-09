#include "aws_mqtt_handler.h"
#include "device_identity.h"
#include "certificate_manager.h"

// 전역 인스턴스 생성
AWSMqttHandler mqttHandler;

AWSMqttHandler::AWSMqttHandler()
    : aws_endpoint(nullptr)
{
}

void AWSMqttHandler::init()
{
    // AWS 엔드포인트 설정
    aws_endpoint = fpstr_to_cstr(FPSTR(AWS_IOT_ENDPOINT));

    // MQTT 설정 초기화는 connect 시점에 수행
    Serial.print(F("[MQTT] 초기화 완료"));
}

bool AWSMqttHandler::connect()
{
    // 보안 클라이언트 설정
    const String &rootCACert = certManager.getRootCACert();
    const String &claimCert = certManager.getClaimCert();
    const String &claimKey = certManager.getClaimKey();
    const String &deviceCert = certManager.getDeviceCert();
    const String &deviceKey = certManager.getDeviceKey();

    // 보안 클라이언트 설정
    WiFiClientSecure secureClient;
    secureClient.setCACert(rootCACert.c_str());
    if (deviceCert.isEmpty() || deviceKey.isEmpty())
    {
        secureClient.setCertificate(claimCert.c_str());
        secureClient.setPrivateKey(claimKey.c_str());
    }
    else
    {
        secureClient.setCertificate(deviceCert.c_str());
        secureClient.setPrivateKey(deviceKey.c_str());
    }

    // MQTT 클라이언트 초기화
    mqttClient = PubSubClient(secureClient);
    mqttClient.setServer(aws_endpoint, 8883);
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient.setKeepAlive(60);

    // 연결 시도
    const char *clientId = DeviceIdentity::getDeviceId();
    Serial.print(F("[MQTT] 연결 시도 - 클라이언트 ID: "));
    Serial.println(clientId);

    if (mqttClient.connect(clientId, NULL, NULL, NULL, 0, false, NULL, true))
    {
        Serial.println(F("[MQTT] 연결 성공"));

        // 연결 직후 루프 실행 (안정화 목적)
        for (int i = 0; i < 3; i++)
        {
            mqttClient.loop();
            delay(10);
        }
        return true;
    }

    Serial.print(F("[MQTT] 연결 실패, 상태: "));
    Serial.print(mqttClient.state());
    Serial.print(F(" ("));
    Serial.print(getStateString());
    Serial.println(F(")"));

    return false;
}

bool AWSMqttHandler::isConnected()
{
    return mqttClient.connected();
}

bool AWSMqttHandler::reconnect()
{
    // 기존 연결 종료
    if (mqttClient.connected())
    {
        mqttClient.disconnect();
    }

    // 새로운 연결 시도
    return connect();
}

bool AWSMqttHandler::publish(const char *topic, const char *payload, bool retained)
{
    if (!mqttClient.connected())
    {
        Serial.println(F("[MQTT] 발행 실패: 연결되지 않음"));
        return false;
    }

    return mqttClient.publish(topic, payload, retained);
}

bool AWSMqttHandler::publish(const char *topic, const uint8_t *payload, unsigned int length, bool retained)
{
    if (!mqttClient.connected())
    {
        Serial.println(F("[MQTT] 발행 실패: 연결되지 않음"));
        return false;
    }

    return mqttClient.publish(topic, payload, length, retained);
}

bool AWSMqttHandler::subscribe(const char *topic)
{
    if (!mqttClient.connected())
    {
        Serial.println(F("[MQTT] 구독 실패: 연결되지 않음"));
        return false;
    }

    Serial.print(F("[MQTT] 토픽 구독: "));
    Serial.println(topic);
    return mqttClient.subscribe(topic);
}

bool AWSMqttHandler::unsubscribe(const char *topic)
{
    if (!mqttClient.connected())
    {
        return false;
    }

    return mqttClient.unsubscribe(topic);
}

void AWSMqttHandler::loop()
{
    if (mqttClient.connected())
    {
        mqttClient.loop();
    }
}

int AWSMqttHandler::getState()
{
    return mqttClient.state();
}

const char *AWSMqttHandler::getStateString()
{
    switch (mqttClient.state())
    {
    case -4:
        return "MQTT_CONNECTION_TIMEOUT";
    case -3:
        return "MQTT_CONNECTION_LOST";
    case -2:
        return "MQTT_CONNECT_FAILED";
    case -1:
        return "MQTT_DISCONNECTED";
    case 0:
        return "MQTT_CONNECTED";
    case 1:
        return "MQTT_CONNECT_BAD_PROTOCOL";
    case 2:
        return "MQTT_CONNECT_BAD_CLIENT_ID";
    case 3:
        return "MQTT_CONNECT_UNAVAILABLE";
    case 4:
        return "MQTT_CONNECT_BAD_CREDENTIALS";
    case 5:
        return "MQTT_CONNECT_UNAUTHORIZED";
    default:
        return "UNKNOWN";
    }
}

void AWSMqttHandler::setCallback(MQTT_CALLBACK_SIGNATURE)
{
    mqttClient.setCallback(callback);
}