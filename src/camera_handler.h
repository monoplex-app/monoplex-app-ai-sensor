#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include <Arduino.h>

bool initCamera();

void triggerCameraCapture(const String& uploadUrl);
void testCameraCapture(); // 테스트용 함수

#endif // CAMERA_HANDLER_H