#include "sensor_manager.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VCNL4040.h>
#include <SparkFun_BMI270_Arduino_Library.h>

// 센서 객체 (static)
static Adafruit_VCNL4040 g_vcnl4040;
static BMI270 g_imu;

// 상태 변수
static bool g_is_initialized = false;
static bool g_vcnl4040_available = false;
static bool g_imu_available = false;
static float g_last_ambient_light = 0.0;
static uint16_t g_last_proximity = 0;

bool sensor_manager_init(int sda_pin, int scl_pin)
{
    Serial.println(F("[SENSOR] 센서 관리 모듈 초기화 시작"));
    
    // I2C 초기화
    Wire.begin(sda_pin, scl_pin);
    Serial.printf("[SENSOR] I2C 초기화 완료 - SDA: %d, SCL: %d\n", sda_pin, scl_pin);
    
    // VCNL4040 초기화 (조도/근접 센서)
    g_vcnl4040_available = g_vcnl4040.begin();
    if (g_vcnl4040_available) {
        Serial.println(F("[SENSOR] VCNL4040 (조도/근접) 센서 초기화 성공"));
    } else {
        Serial.println(F("[SENSOR] VCNL4040 센서 초기화 실패"));
    }
    
    // BMI270 초기화 (6축 IMU)
    g_imu_available = (g_imu.beginI2C(0x68) == BMI2_OK);
    if (g_imu_available) {
        Serial.println(F("[SENSOR] BMI270 IMU 센서 초기화 성공"));
    } else {
        Serial.println(F("[SENSOR] BMI270 IMU 센서 초기화 실패"));
    }
    
    g_is_initialized = true;
    
    // 초기화 결과 요약
    Serial.printf("[SENSOR] 센서 초기화 완료 - VCNL4040: %s, BMI270: %s\n", 
                  g_vcnl4040_available ? "OK" : "FAIL",
                  g_imu_available ? "OK" : "FAIL");
    
    return (g_vcnl4040_available || g_imu_available); // 하나라도 성공하면 true
}

bool sensor_manager_get_data_json(JsonDocument& doc)
{
    if (!g_is_initialized) {
        Serial.println(F("[SENSOR] 오류: 초기화되지 않음"));
        return false;
    }
    
    bool success = false;
    
    // VCNL4040 데이터 읽기
    if (g_vcnl4040_available) {
        g_last_proximity = g_vcnl4040.getProximity();
        g_last_ambient_light = g_vcnl4040.getLux();
        
        doc["proximity"] = g_last_proximity;
        doc["ambientLight"] = g_last_ambient_light;
        
        success = true;
    }
    
    // BMI270 데이터 읽기
    if (g_imu_available && g_imu.getSensorData() == BMI2_OK) {
        // 가속도계 데이터
        JsonObject accel = doc["accelerometer"].to<JsonObject>();
        accel["x"] = g_imu.data.accelX;
        accel["y"] = g_imu.data.accelY;
        accel["z"] = g_imu.data.accelZ;
        
        // 자이로스코프 데이터
        JsonObject gyro = doc["gyroscope"].to<JsonObject>();
        gyro["x"] = g_imu.data.gyroX;
        gyro["y"] = g_imu.data.gyroY;
        gyro["z"] = g_imu.data.gyroZ;
        
        success = true;
    }
    
    return success;
}

float sensor_manager_get_ambient_light()
{
    if (!g_is_initialized || !g_vcnl4040_available) {
        return -1.0; // 오류 표시
    }
    
    g_last_ambient_light = g_vcnl4040.getLux();
    return g_last_ambient_light;
}

uint16_t sensor_manager_get_proximity()
{
    if (!g_is_initialized || !g_vcnl4040_available) {
        return 0; // 오류 표시
    }
    
    g_last_proximity = g_vcnl4040.getProximity();
    return g_last_proximity;
}

bool sensor_manager_is_initialized()
{
    return g_is_initialized;
} 