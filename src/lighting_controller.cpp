#include "lighting_controller.h"
#include "config.h"
#include <Arduino.h>

// 내부 상태 변수 (static)
static int g_light_pin = -1;
static float g_night_threshold = 10.0;
static bool g_is_initialized = false;
static bool g_current_state = false; // false=OFF, true=ON

void lighting_controller_init(int pin, float night_threshold)
{
    g_light_pin = pin;
    g_night_threshold = night_threshold;
    
    // GPIO 핀 초기화
    pinMode(g_light_pin, OUTPUT);
    digitalWrite(g_light_pin, LIGHT_OFF);
    g_current_state = false;
    
    g_is_initialized = true;
    
    Serial.printf("[LIGHTING] 초기화 완료 - Pin: %d, 야간 임계값: %.1f lux\n", 
                  g_light_pin, g_night_threshold);
}

bool lighting_controller_manage(float current_ambient_light)
{
    if (!g_is_initialized) {
        Serial.println("[LIGHTING] 오류: 초기화되지 않음");
        return false;
    }
    
    bool should_be_on = (current_ambient_light < g_night_threshold);
    
    if (should_be_on != g_current_state) {
        if (should_be_on) {
            lighting_controller_force_on();
            Serial.printf("[LIGHTING] 자동 켜짐 - 현재 조도: %.1f lux (임계값: %.1f)\n", 
                         current_ambient_light, g_night_threshold);
        } else {
            lighting_controller_force_off();
            Serial.printf("[LIGHTING] 자동 꺼짐 - 현재 조도: %.1f lux (임계값: %.1f)\n", 
                         current_ambient_light, g_night_threshold);
        }
    }
    
    return g_current_state;
}

void lighting_controller_force_on()
{
    if (!g_is_initialized) {
        Serial.println("[LIGHTING] 오류: 초기화되지 않음");
        return;
    }
    
    digitalWrite(g_light_pin, LIGHT_ON);
    g_current_state = true;
    Serial.println("[LIGHTING] 조명 강제 켜짐");
}

void lighting_controller_force_off()
{
    if (!g_is_initialized) {
        Serial.println("[LIGHTING] 오류: 초기화되지 않음");
        return;
    }
    
    digitalWrite(g_light_pin, LIGHT_OFF);
    g_current_state = false;
    Serial.println("[LIGHTING] 조명 강제 꺼짐");
}

bool lighting_controller_is_on()
{
    return g_current_state;
} 