#pragma once

/**
 * @file image_capture_handler.h
 * @brief 이미지 캡처 및 처리 모듈
 * 
 * 조명 제어, 카메라 캡처, S3 업로드, MQTT 결과 전송을 통합 관리합니다.
 */

/**
 * @brief 이미지 캡처 핸들러 초기화
 */
void image_capture_handler_init();

/**
 * @brief 전체 이미지 캡처 프로세스 실행
 * 조명 제어 → 카메라 캡처 → S3 업로드 → MQTT 결과 전송을 순차 실행합니다.
 */
void image_capture_handler_process();

/**
 * @brief MQTT에서 수신한 URL로 이미지 캡처 프로세스 실행
 * @param url S3 presigned URL
 */
void image_capture_handler_process_with_url(const char* url); 