#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include <Arduino.h>

bool initCamera();

void triggerCameraCapture(const String& uploadUrl);
void testCameraCapture(); // 테스트용 함수

// 단순 카메라 테스트 (별도 파일)
void simpleCameraTest();
void alternativeCameraTest();

#endif // CAMERA_HANDLER_H