#pragma once

#include "config.h"     // 카메라 모델 정의
#include "esp_camera.h"

/**
 * @file camera_manager.h
 * @brief 카메라 관리 모듈
 * 
 * ESP32-S3 카메라 초기화, 이미지 캡처, 재초기화 등의 기능을 제공합니다.
 */

/**
 * @brief 카메라 관리 모듈 초기화
 * @return true: 초기화 성공, false: 초기화 실패
 */
bool camera_manager_init();

/**
 * @brief 카메라 재초기화
 * @return true: 재초기화 성공, false: 재초기화 실패
 */
bool camera_manager_reinit();

/**
 * @brief 안전한 이미지 캡처 (재시도 포함)
 * @param max_retries 최대 재시도 횟수
 * @return camera_fb_t* 캡처된 이미지 프레임 버퍼 (실패 시 nullptr)
 */
camera_fb_t* camera_manager_capture_safe(int max_retries);

/**
 * @brief 카메라 초기화 상태 확인
 * @return true: 초기화됨, false: 초기화되지 않음
 */
bool camera_manager_is_initialized();

/**
 * @brief 카메라 시스템 정리 (BLE 안전 모드)
 * BLE 광고를 일시 중단하고 카메라를 재초기화합니다.
 */
void camera_manager_reinit_with_ble_safety();

// 잘 동작하던 코드와의 호환성을 위한 함수들
/**
 * @brief 카메라 시스템 직접 초기화 (기존 코드 호환성)
 * @return true: 초기화 성공, false: 초기화 실패
 */
bool camera_init_system();

/**
 * @brief 카메라 시스템 직접 종료 (기존 코드 호환성)
 */
void camera_deinit_system(); 