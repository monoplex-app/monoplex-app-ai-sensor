#include "sensor_handler.h"
#include "config.h" // I2C 핀 등 설정 값을 위해 포함

// 센서 라이브러리 포함
#include <Wire.h>
#include <Adafruit_VCNL4040.h>
#include <SparkFun_BMI270_Arduino_Library.h>

// 모듈 내부에서만 사용할 객체 및 변수
static Adafruit_VCNL4040 vcnl;
static BMI270 imu;
static SensorData currentSensorData; // 현재 센서 값을 저장할 내부 변수

// 함수 구현

bool initSensors() {
    // 1. I2C 버스 시작
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // 2. VCNL4040 (조도/근접) 센서 초기화
    if (!vcnl.begin()) {
        Serial.println("VCNL4040 센서 미발견. 회로 연결 확인!");
        return false;
    }
    Serial.println("VCNL4040 센서 발견.");
    // VCNL4040 설정
    vcnl.setProximityLEDCurrent(VCNL4040_LED_CURRENT_200MA);
    vcnl.setProximityIntegrationTime(VCNL4040_PROXIMITY_INTEGRATION_TIME_8T);
    vcnl.setProximityLEDDutyCycle(VCNL4040_LED_DUTY_1_40);
    vcnl.setAmbientIntegrationTime(VCNL4040_AMBIENT_INTEGRATION_TIME_80MS);
    
    // 3. BMI270 (IMU) 센서 초기화
    // 연결될 때까지 몇 번 시도
    int retry = 0;
    while(imu.beginI2C(0x68) != BMI2_OK && retry < 3) {
        Serial.println("BMI270 연결 안됨, 재시도...");
        delay(1000);
        retry++;
    }
    if (retry >= 3) {
        Serial.println("BMI270 초기화 실패!");
        return false;
    }
    Serial.println("BMI270 센서 연결 성공!");

    return true;
}

void readAllSensors() {
    // VCNL4040 센서 값 읽기
    currentSensorData.proximity = vcnl.getProximity();
    currentSensorData.ambientLight = vcnl.getLux();

    // BMI270 센서 값 읽기
    if (imu.getSensorData() == BMI2_OK) {
        currentSensorData.accelX = imu.data.accelX;
        currentSensorData.accelY = imu.data.accelY;
        currentSensorData.accelZ = imu.data.accelZ;
        currentSensorData.gyroX = imu.data.gyroX;
        currentSensorData.gyroY = imu.data.gyroY;
        currentSensorData.gyroZ = imu.data.gyroZ;
    } else {
        Serial.println("IMU 데이터 읽기 실패.");
    }
}

float getAmbientLight() {
    return currentSensorData.ambientLight;
}

SensorData getSensorDataStruct() {
    return currentSensorData;
}

String getSensorDataJson() {
    char jsonBuffer[256];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"proximity\":%d,\"lux\":%d,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f}",
             currentSensorData.proximity,
             (int)currentSensorData.ambientLight,
             currentSensorData.accelX,
             currentSensorData.accelY,
             currentSensorData.accelZ,
             currentSensorData.gyroX,
             currentSensorData.gyroY,
             currentSensorData.gyroZ);
    return String(jsonBuffer);
}