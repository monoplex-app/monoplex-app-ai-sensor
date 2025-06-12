#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>

// 모든 센서 데이터를 담을 구조체
struct SensorData {
    uint16_t proximity;
    float ambientLight;
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
};

/**
 * @brief I2C 버스와 모든 센서(VCNL4040, BMI270)를 초기화합니다.
 * setup()에서 한 번 호출됩니다.
 * @return 모든 센서 초기화 성공 시 true, 하나라도 실패 시 false.
 */
bool initSensors();

/**
 * @brief 모든 센서로부터 최신 데이터를 읽어옵니다.
 * 주기적으로 메인 loop()에서 호출하여 센서 값을 갱신해야 합니다.
 */
void readAllSensors();

/**
 * @brief 현재 조도(lux) 값을 반환합니다.
 * 카메라 핸들러에서 야간 촬영 여부를 판단할 때 사용합니다.
 * @return 조도 값 (lux).
 */
float getAmbientLight();

/**
 * @brief 현재 센서 데이터 전체를 JSON 문자열 형식으로 반환합니다.
 * MQTT로 센서 데이터를 게시할 때 사용됩니다.
 * @return JSON 형식의 센서 데이터 문자열.
 */
String getSensorDataJson();

/**
 * @brief 현재 센서 데이터 전체를 SensorData 구조체로 반환합니다.
 * @return SensorData 구조체.
 */
SensorData getSensorDataStruct();

#endif // SENSOR_HANDLER_H