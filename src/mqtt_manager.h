#pragma once

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

/**
 * @file mqtt_manager.h
 * @brief MQTT 관리 모듈
 * 
 * AWS IoT Core MQTT 연결, 메시지 발행/구독, 콜백 처리 기능을 제공합니다.
 */

// MQTT 콜백 함수 타입 정의
typedef void (*MqttMessageCallback)(char* topic, byte* payload, unsigned int length);

/**
 * @brief MQTT 관리 모듈 초기화
 * @param wifiClient WiFi 클라이언트 참조
 * @param callback 메시지 수신 콜백 함수
 */
void mqtt_manager_init(WiFiClientSecure& wifiClient, MqttMessageCallback callback);

/**
 * @brief MQTT 연결 설정 및 시도
 * @return true: 연결 성공, false: 연결 실패
 */
bool mqtt_manager_connect();

/**
 * @brief MQTT 연결 상태 확인 및 유지
 * @return true: 연결됨, false: 연결 끊어짐
 */
bool mqtt_manager_maintain_connection();

/**
 * @brief 안전한 MQTT 연결 해제
 */
void mqtt_manager_disconnect();

/**
 * @brief 센서 데이터 발행
 * @param jsonDoc 센서 데이터가 포함된 JSON 문서
 * @return true: 발행 성공, false: 발행 실패
 */
bool mqtt_manager_publish_sensor_data(JsonDocument& jsonDoc);

/**
 * @brief 이미지 업로드 결과 발행
 * @param success 업로드 성공 여부
 * @param error_message 오류 메시지 (실패 시)
 * @return true: 발행 성공, false: 발행 실패
 */
bool mqtt_manager_publish_image_result(bool success, const String& error_message = "");

/**
 * @brief MQTT 연결 상태 확인
 * @return true: 연결됨, false: 연결 끊어짐
 */
bool mqtt_manager_is_connected();

/**
 * @brief MQTT 클라이언트 객체 참조 반환
 * @return PubSubClient 참조
 */
PubSubClient& mqtt_manager_get_client(); 