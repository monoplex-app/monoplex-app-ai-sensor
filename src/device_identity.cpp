#include "device_identity.h"
#include <esp_system.h>

// 정적 멤버 변수 초기화
char DeviceIdentity::s_deviceId[DEVICE_ID_SIZE] = {0};
bool DeviceIdentity::s_initialized = false;

void DeviceIdentity::init()
{
    if (!s_initialized)
    {
        Serial.println(F("[DEVICE] 디바이스 ID 초기화 시작"));
        generateDeviceId(s_deviceId, DEVICE_ID_SIZE);
        s_initialized = true;
    }

    Serial.print(F("[DEVICE] 디바이스 ID 초기화 완료: "));
    Serial.println(s_deviceId);
}

void DeviceIdentity::generateDeviceId(char *deviceIdBuf, size_t bufSize)
{
    if (deviceIdBuf == nullptr || bufSize < 17) // "MLX_" + 12 MAC + 1 NULL
    {
        Serial.println(F("[DEVICE] 버퍼 크기 부족"));
        return;
    }

    // 버퍼 초기화
    memset(deviceIdBuf, 0, bufSize);

    // "MLX_" 접두사 추가
    strcpy(deviceIdBuf, "MLX_");

    // 항상 기본 MAC 주소만 사용
    uint8_t baseMac[6];
    esp_efuse_mac_get_default(baseMac);

    // MAC 주소를 16진수 문자열로 변환하여 접두사 뒤에 추가
    // 메모리 안전성을 위해 snprintf 사용, 버퍼 크기 제한
    snprintf(deviceIdBuf + 4, bufSize - 4, "%02X%02X%02X%02X%02X%02X",
             baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    Serial.print(F("[DEVICE] 기본 MAC 기반 디바이스 ID 생성: "));
    Serial.println(deviceIdBuf);
}

const char *DeviceIdentity::getDeviceId()
{
    if (!s_initialized)
    {
        Serial.println(F("[DEVICE] getDeviceId() 호출 시 초기화되지 않음, 초기화 수행"));
        init();
    }

    return s_deviceId;
}
