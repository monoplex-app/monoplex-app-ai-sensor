#pragma once

#include "esp_camera.h"
#include <Arduino.h>
#include <WString.h>

/**
 * @file s3_manager.h
 * @brief S3 업로드 관리 모듈
 * 
 * AWS S3 presigned URL을 통한 이미지 업로드 기능을 제공합니다.
 */

/**
 * @brief S3 관리 모듈 초기화
 */
void s3_manager_init();

/**
 * @brief URL에서 호스트명 추출
 * @param url 전체 URL
 * @param host_buffer 호스트명을 저장할 버퍼
 * @param buffer_size 버퍼 크기
 * @return true: 성공, false: 실패
 */
bool s3_manager_extract_host_from_url(const char* url, char* host_buffer, size_t buffer_size);

/**
 * @brief S3 업로드 URL 설정
 * @param url presigned URL
 * @param host S3 호스트명 (NULL이면 자동 추출)
 */
void s3_manager_set_upload_url(const char* url, const char* host = nullptr);

/**
 * @brief 현재 설정된 업로드 URL 가져오기
 * @return 현재 업로드 URL (기본값 포함)
 */
const char* s3_manager_get_upload_url();

/**
 * @brief 현재 설정된 S3 호스트 가져오기
 * @return 현재 S3 호스트 (기본값 포함)
 */
const char* s3_manager_get_host();

/**
 * @brief 이미지를 S3에 업로드
 * @param fb 카메라 프레임 버퍼
 * @return true: 업로드 성공, false: 업로드 실패
 */
bool s3_manager_upload_image(camera_fb_t* fb); 