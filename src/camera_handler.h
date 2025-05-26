#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include "esp_camera.h" // camera_config_t, sensor_t 등을 위해 필요
#include "Arduino.h"    // Serial, psramFound 등을 위해 필요

/**
 * @brief 카메라 시스템을 초기화합니다.
 *
 * 내부적으로 카메라 설정을 구성하고 esp_camera_init을 호출하며,
 * 센서 설정을 적용합니다.
 *
 * @return bool 초기화 성공 시 true, 실패 시 false를 반환합니다.
 */
bool camera_init_system();

void camera_deinit_system();

#endif // CAMERA_HANDLER_H