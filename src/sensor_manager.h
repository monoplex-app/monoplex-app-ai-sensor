#pragma once

#include <ArduinoJson.h>

/**
 * @file sensor_manager.h
 * @brief 센서 관리 모듈
 * 
 * VCNL4040 (조도/근접) 및 BMI270 (6축 IMU) 센서 초기화 및 데이터 수집 기능을 제공합니다.
 */

/**
 * @brief 센서 관리 모듈 초기화
 * @param sda_pin I2C SDA 핀 번호
 * @param scl_pin I2C SCL 핀 번호
 * @return true: 초기화 성공, false: 초기화 실패
 */
bool sensor_manager_init(int sda_pin, int scl_pin);

/**
 * @brief 모든 센서 데이터를 JSON 문서에 저장
 * @param doc JSON 문서 참조
 * @return true: 데이터 읽기 성공, false: 실패
 */
bool sensor_manager_get_data_json(JsonDocument& doc);

/**
 * @brief 현재 조도 값 가져오기
 * @return 조도 값 (lux), 오류 시 -1 반환
 */
float sensor_manager_get_ambient_light();

/**
 * @brief 현재 근접 센서 값 가져오기
 * @return 근접 센서 값, 오류 시 0 반환
 */
uint16_t sensor_manager_get_proximity();

/**
 * @brief 센서 초기화 상태 확인
 * @return true: 초기화됨, false: 초기화되지 않음
 */
bool sensor_manager_is_initialized(); 