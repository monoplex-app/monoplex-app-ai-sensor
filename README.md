# Monoplex ESP32-S3 AWS IoT 프로젝트

## 1. 프로젝트 개요

이 프로젝트는 ESP32-S3 디바이스를 사용하여 AWS IoT Core와 안전하게 연동하고, MQTT 통신을 통해 데이터를 주고받는 기능을 구현합니다. 주요 기능은 다음과 같습니다:

- **AWS IoT 연동**: 안전한 MQTT 통신을 위한 인증 및 연결 관리.
- **Fleet Provisioning (선택 사항)**: 디바이스 자동 등록 및 인증서 발급 기능 지원.
- **센서 데이터 처리 및 명령 수신**: 다양한 센서 데이터를 AWS IoT Core로 전송하고, 원격 명령을 수신하여 처리.
- **파일 시스템 활용**: LittleFS를 사용하여 설정 및 인증서 파일 관리.

---

## 2. 시작하기

### 2.1. 사전 준비물

- **하드웨어**:
  - ESP32-S3 개발 보드
- **소프트웨어**:
  - PlatformIO IDE (VS Code 확장 권장)
  - Git
- **AWS 계정 및 서비스**:
  - AWS IoT Core (엔드포인트, 필요한 경우 프로비저닝 템플릿 및 정책 설정)
  - (선택 사항) Amazon S3, AWS Lambda, DynamoDB 등 연동 서비스

### 2.2. 환경 설정

1.  **프로젝트 클론**:
    ```bash
    git clone <저장소_URL>
    cd <프로젝트_디렉토리>
    ```
2.  **상수 설정**:
    - `include/constants.h` (또는 관련 설정 파일)에 다음 정보를 정확히 입력합니다:
      - `WIFI_SSID`: Wi-Fi 네트워크 SSID
      - `WIFI_PASSWORD`: Wi-Fi 네트워크 비밀번호
      - `AWS_IOT_ENDPOINT`: AWS IoT Core 엔드포인트 주소
      - (필요시) `PROVISIONING_TEMPLATE_NAME` 등 프로비저닝 관련 상수
3.  **인증서 준비 (필요시)**:
    - **일반적인 경우**: 디바이스 인증서(`device.crt`), 개인 키(`device.key`), 루트 CA 인증서(`root_ca.pem`)를 준비합니다.
    - **Fleet Provisioning 사용 시**: 클레임 인증서(`claim.crt`), 클레임 키(`claim.key`), 루트 CA 인증서(`root_ca.pem`)를 준비합니다.
    - 준비된 인증서 파일들은 일반적으로 `data/` 폴더에 위치시킵니다. (경로는 코드 내 설정에 따라 다를 수 있음)

### 2.3. 빌드 및 업로드

1.  **PlatformIO 초기화**:
    - PlatformIO가 프로젝트를 열 때 필요한 라이브러리를 자동으로 설치합니다.
2.  **(필요시) 파일 시스템 이미지 업로드**:
    - `data/` 폴더에 인증서나 설정 파일이 있다면, LittleFS 파일 시스템 이미지를 업로드합니다.
    ```bash
    pio run --target uploadfs
    ```
    - 이 작업은 펌웨어 업로드와 별개이며, `data/` 폴더 내용 변경 시 다시 실행해야 합니다.
3.  **펌웨어 빌드 및 업로드**:
    ```bash
    pio run --target upload
    ```
4.  **시리얼 모니터 실행**:
    ```bash
    pio device monitor
    ```
    - 부팅 로그, Wi-Fi 연결, MQTT 연결 상태 등을 확인합니다.

---

## 3. 주요 기능 및 사용법

### 3.1. AWS IoT 연결

- 부팅 시 Wi-Fi에 자동으로 연결하고, 설정된 인증서를 사용하여 AWS IoT Core에 MQTT 연결을 시도합니다.
- 연결 상태는 시리얼 모니터를 통해 확인할 수 있습니다.

### 3.2. 데이터 발행 및 명령 수신 (구현 필요)

- `src/main.cpp`의 `loop()` 함수나 MQTT 콜백 함수 내에 주기적인 데이터 발행 로직 또는 명령 수신 처리 로직을 구현할 수 있습니다.
- 예시 토픽:
  - 데이터 발행: `devices/<CLIENT_ID>/data`
  - 명령 수신: `devices/<CLIENT_ID>/commands`

### 3.3. (선택 사항) Fleet Provisioning

- 디바이스가 처음 부팅될 때 클레임 인증서를 사용하여 AWS IoT Core에 연결하고, 프로비저닝 템플릿을 통해 영구 인증서와 Thing을 자동으로 등록합니다.
- 프로비저닝 성공 후, 디바이스는 영구 인증서를 사용하여 AWS IoT Core에 연결합니다.
- **관련 설정**: `PROVISIONING_TEMPLATE_NAME`, 클레임 인증서, 관련 AWS IoT 정책 및 역할 설정이 필요합니다.

---

## 4. 문제 해결 및 디버깅

- **시리얼 모니터 확인**: 대부분의 오류 정보는 시리얼 모니터에 출력됩니다. (연결 실패, 파일 로드 실패 등)
- **AWS IoT Core 콘솔**:
  - MQTT 테스트 클라이언트를 사용하여 메시지 송수신을 확인합니다.
  - CloudWatch Logs에서 AWS IoT 관련 로그를 확인합니다.
- **인증서 및 정책**: 인증서 파일이 올바른지, AWS IoT 정책이 적절한 권한을 부여하는지 확인합니다.

---

## 5. 기여 방법

- 버그 리포트나 기능 제안은 GitHub 이슈를 통해 제출해 주십시오.
- 코드 기여 시에는 Pull Request를 생성해 주십시오.

---

**문의/피드백:**

- 담당자 또는 GitHub 이슈를 통해 문의 바랍니다.
