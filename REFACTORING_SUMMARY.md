# MONOPLEX ESP32 S3 프로젝트 리팩토링 요약

## 🚨 문제 상황

프로젝트 컴파일 시 **링킹(Linking) 에러** 발생:

- `multiple definition of '...'` 오류
- C++의 **단일 정의 규칙(One Definition Rule)** 위반

## 🔍 원인 분석

1. **인증서 변수 중복 정의**: `config.h`에 정의된 인증서 변수들이 여러 `.cpp` 파일에서 중복 생성
2. **전역 상태 변수 중복 정의**: `isWifiConnected`, `isMqttConnected`, `isBleClientConnected` 등이 여러 파일에서 중복 정의

## ✅ 해결 방법

### 1. 인증서 변수 분리

#### 📁 **config.h** 수정

```cpp
// 기존: 직접 정의
const char* ROOT_CA_CERT = "...";

// 수정: extern 선언으로 변경
extern const char* ROOT_CA_CERT;
extern const char* CERTIFICATE_PEM;
extern const char* PRIVATE_KEY;
```

#### 📁 **config.cpp** 신규 생성

```cpp
#include "config.h"

// 인증서 변수들의 실제 정의 (단 한 곳에서만)
const char* ROOT_CA_CERT = "-----BEGIN CERTIFICATE-----\n...";
const char* CERTIFICATE_PEM = "-----BEGIN CERTIFICATE-----\n...";
const char* PRIVATE_KEY = "-----BEGIN RSA PRIVATE KEY-----\n...";
```

### 2. 전역 상태 변수 중복 제거

#### 📁 **globals.h** 신규 생성

```cpp
// 전역 변수들의 중앙 관리
extern String deviceUid;
extern bool isWifiConnected;
extern bool isMqttConnected;
extern bool isBleClientConnected;
extern unsigned long lastLedToggleTime;
extern const long ledToggleInterval;
extern bool ledState;
```

#### 📁 **main.cpp** 개선

```cpp
#include "globals.h"

// 전역 변수들의 실제 정의 (단 한 곳에서만)
String deviceUid = "";
bool isWifiConnected = false;
bool isMqttConnected = false;
bool isBleClientConnected = false;
// ...
```

#### 📁 **핸들러 파일들 수정**

- `wifi_handler.cpp`: `bool isWifiConnected = false;` 삭제
- `ble_handler.cpp`: `bool isBleClientConnected = false;` 삭제
- `mqtt_handler.cpp`: `bool isMqttConnected = false;` 삭제

### 3. 헤더 파일 정리

각 헤더 파일에서 중복된 extern 선언 제거하고 `globals.h` 포함:

- `wifi_handler.h` → `#include "globals.h"` 추가
- `ble_handler.h` → `#include "globals.h"` 추가
- `mqtt_handler.h` → `#include "globals.h"` 추가

## 📋 변경된 파일 목록

- ✨ **신규 생성**:

  - `src/config.cpp`
  - `src/globals.h`
  - `REFACTORING_SUMMARY.md`

- 🔧 **수정**:
  - `src/config.h`
  - `src/main.cpp`
  - `src/wifi_handler.h`
  - `src/wifi_handler.cpp`
  - `src/ble_handler.h`
  - `src/ble_handler.cpp`
  - `src/mqtt_handler.h`
  - `src/mqtt_handler.cpp`

## 🎯 개선 효과

1. **링킹 에러 해결**: 중복 정의 문제 완전 해결
2. **코드 구조 개선**: 전역 변수 중앙 관리로 유지보수성 향상
3. **가독성 향상**: 명확한 주석과 체계적인 파일 구조
4. **확장성**: 새로운 전역 변수 추가 시 일관된 패턴 적용 가능

## 🔧 C++ 설계 원칙

- **단일 정의 규칙(ODR)** 준수
- **헤더/소스 분리** 패턴 적용
- **extern 선언/정의 분리** 모범 사례 적용
- **중앙집중식 전역 상태 관리**

## 📌 앞으로의 권장사항

1. 새로운 전역 변수는 `globals.h`에 선언, `main.cpp`에 정의
2. 상수나 설정값은 `config.h`에 선언, `config.cpp`에 정의
3. 각 모듈별 내부 변수는 `static` 키워드 사용하여 캡슐화

---

**리팩토링 완료일**: `2024년 12월`  
**적용된 C++ 버전**: ESP32 Arduino Framework  
**주요 개선**: 링킹 에러 해결 및 코드 구조 최적화
