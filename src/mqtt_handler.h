#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include "globals.h"

// isMqttConnected는 globals.h에서 선언됨

/**
 * @brief MQTT 클라이언트를 초기 설정
 * 보안 인증서, 서버 주소, 메시지 수신 콜백 함수를 설정
 * setup()에서 한 번만 호출
 */
void initMQTT();

/**
 * @brief MQTT 서버와의 연결을 확인하고, 끊어져 있다면 재연결을 시도
 * 이 함수는 Wi-Fi가 연결된 상태에서 메인 loop()에 의해 계속 호출
 */
void handleMqttConnection();

/**
 * @brief MQTT 클라이언트의 내부 루프를 실행
 * 서버로부터 메시지를 수신하고 연결을 유지하기 위해 필수적
 * 메인 loop()에서 계속 호출
 */
void mqttLoop();

/**
 * @brief 지정된 토픽으로 메시지를 발행(Publish)
 */
void publishMqttMessage(const String& topic, const String& payload);

/**
 * @brief MQTT 클라이언트 연결을 명시적으로 종료합니다.
 */
void disconnectMQTT();

#endif // MQTT_HANDLER_H