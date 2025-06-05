#pragma once

/**
 * @file lighting_controller.h
 * @brief 조명 제어 모듈
 * 
 * 주변 조도에 따른 자동 조명 제어 및 수동 조명 제어 기능을 제공합니다.
 */

/**
 * @brief 조명 제어 모듈 초기화
 * @param pin 조명 제어용 GPIO 핀 번호
 * @param night_threshold 야간 판정 조도 임계값 (lux)
 */
void lighting_controller_init(int pin, float night_threshold);

/**
 * @brief 현재 조도에 따른 조명 자동 제어
 * @param current_ambient_light 현재 조도 값 (lux)
 * @return true: 조명이 켜진 상태, false: 조명이 꺼진 상태
 */
bool lighting_controller_manage(float current_ambient_light);

/**
 * @brief 조명 강제 켜기
 */
void lighting_controller_force_on();

/**
 * @brief 조명 강제 끄기
 */
void lighting_controller_force_off();

/**
 * @brief 현재 조명 상태 조회
 * @return true: 조명 켜짐, false: 조명 꺼짐
 */
bool lighting_controller_is_on(); 