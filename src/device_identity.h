#pragma once

#include <Arduino.h>
#include "config.h"

// 디바이스 ID 관련 기능을 제공하는 클래스
class DeviceIdentity
{
public:
    // 디바이스 ID 초기화
    static void init();

    // 기본 MAC 주소를 사용하여 디바이스 ID 생성
    static void generateDeviceId(char *deviceIdBuf, size_t bufSize);

    // 저장된 디바이스 ID 가져오기 (초기화 후 사용 가능)
    static const char *getDeviceId();

private:
    static char s_deviceId[DEVICE_ID_SIZE];
    static bool s_initialized;
};