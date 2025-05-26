#include "camera_handler.h"
#include "config.h"      // CAMERA_MODEL_ESP32S3_EYE 정의 및 핀 정의 참조 위함
#include "camera_pins.h" // 실제 핀 번호 매크로 참조 위함

// 카메라 설정을 위한 정적 변수
static camera_config_t s_camera_config;

bool camera_init_system()
{
    s_camera_config.ledc_channel = LEDC_CHANNEL_0;
    s_camera_config.ledc_timer = LEDC_TIMER_0;
    s_camera_config.pin_d0 = Y2_GPIO_NUM;
    s_camera_config.pin_d1 = Y3_GPIO_NUM;
    s_camera_config.pin_d2 = Y4_GPIO_NUM;
    s_camera_config.pin_d3 = Y5_GPIO_NUM;
    s_camera_config.pin_d4 = Y6_GPIO_NUM;
    s_camera_config.pin_d5 = Y7_GPIO_NUM;
    s_camera_config.pin_d6 = Y8_GPIO_NUM;
    s_camera_config.pin_d7 = Y9_GPIO_NUM;
    s_camera_config.pin_xclk = XCLK_GPIO_NUM;
    s_camera_config.pin_pclk = PCLK_GPIO_NUM;
    s_camera_config.pin_vsync = VSYNC_GPIO_NUM;
    s_camera_config.pin_href = HREF_GPIO_NUM;
    s_camera_config.pin_sccb_sda = SIOD_GPIO_NUM;
    s_camera_config.pin_sccb_scl = SIOC_GPIO_NUM;
    s_camera_config.pin_pwdn = PWDN_GPIO_NUM;
    s_camera_config.pin_reset = RESET_GPIO_NUM;

    // xclk_freq_hz 값은 이전 논의를 바탕으로 6MHz 또는 20MHz 중 안정적인 값을 선택합니다.
    // 현재는 6MHz로 설정되어 있습니다. (사용자가 수정한 값 유지)
    s_camera_config.xclk_freq_hz = 6000000;
    s_camera_config.frame_size = FRAMESIZE_SVGA;
    s_camera_config.pixel_format = PIXFORMAT_JPEG;
    s_camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    if (psramFound())
    {
        Serial.println(F("[CAMERA] PSRAM 감지됨. 프레임버퍼에 PSRAM 사용."));
        s_camera_config.fb_location = CAMERA_FB_IN_PSRAM;
        s_camera_config.jpeg_quality = 10;
        s_camera_config.grab_mode = CAMERA_GRAB_LATEST;
        s_camera_config.frame_size = FRAMESIZE_UXGA;
        s_camera_config.fb_count = 2;
    }
    else
    {
        Serial.println(F("[CAMERA] PSRAM 없음. 프레임버퍼에 DRAM 사용."));
        s_camera_config.fb_location = CAMERA_FB_IN_DRAM;
        s_camera_config.jpeg_quality = 12;
        s_camera_config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&s_camera_config);
    if (err != ESP_OK)
    {
        Serial.printf("[CAMERA] 카메라 초기화 실패, 오류 0x%x\n", err);
        return false;
    }
    Serial.println(F("[CAMERA] esp_camera_init 성공."));

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
    {
        Serial.println(F("[CAMERA] 카메라 센서 가져오기 실패!"));
        return false;
    }
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
    Serial.println(F("[CAMERA] 카메라 센서 설정 적용 완료."));
    return true;
}