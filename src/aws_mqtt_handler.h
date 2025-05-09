#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"

class AWSMqttHandler
{
public:
    AWSMqttHandler();

    // MQTT 클라이언트 초기화
    void init();

    // 연결 관리
    bool connect();
    bool isConnected();
    bool reconnect();

    // 메시지 발행
    bool publish(const char *topic, const char *payload, bool retained = false);
    bool publish(const char *topic, const uint8_t *payload, unsigned int length, bool retained = false);

    // 구독 관리
    bool subscribe(const char *topic);
    bool unsubscribe(const char *topic);

    // 통신 처리
    void loop();

    // 상태 관리
    int getState();
    const char *getStateString();

    // 콜백 설정
    void setCallback(MQTT_CALLBACK_SIGNATURE);

private:
    PubSubClient mqttClient;
    const char *aws_endpoint;
};

// 전역 인스턴스
extern AWSMqttHandler mqttHandler;
