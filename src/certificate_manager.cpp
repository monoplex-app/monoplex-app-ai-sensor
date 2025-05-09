#include "certificate_manager.h"
#include "device_identity.h"
#include "aws_mqtt_handler.h"

// 전역 인스턴스 생성
CertificateManager certManager;

CertificateManager::CertificateManager() : rootCACert(""), deviceCert(""), deviceKey(""), claimCert(""), claimKey("")
{
}

void CertificateManager::init()
{
    // LittleFS 초기화
    if (!initializeFileSystem())
    {
        return;
    }

    // 인증서 초기화
    initCertificates();
}

bool CertificateManager::initializeFileSystem()
{
    if (LittleFS.begin())
    {
        Serial.println(F("[LFS] LittleFS 마운트 성공."));
        return true;
    }

    // 마운트 실패 시 포맷 시도
    Serial.println(F("[LFS_WARN] LittleFS 마운트 실패. 포맷 시도 중..."));
    if (!LittleFS.format())
    {
        Serial.println(F("[LFS_ERROR] LittleFS 포맷 실패."));
        return false;
    }

    Serial.println(F("[LFS_INFO] LittleFS 포맷 성공."));
    if (!LittleFS.begin())
    {
        Serial.println(F("[LFS_ERROR] 포맷 후에도 마운트 실패."));
        return false;
    }

    Serial.println(F("[LFS_INFO] LittleFS 마운트 성공."));
    return true;
}

void CertificateManager::initCertificates()
{
    Serial.println(F("[CERT] 인증서 초기화 시작"));

    // 1. 루트 CA 인증서 로드
    if (!loadRootCA())
    {
        Serial.println(F("[CERT] 루트 CA 인증서 로드 실패. 중지됨."));
        return;
    }

    // // 2. 디바이스 인증서 확인
    // if (loadDeviceCert())
    // {
    //     Serial.println(F("[CERT] 디바이스 인증서 로드 성공"));
    //     return;
    // }

    // 3. 디바이스 인증서가 없으면 클레임 인증서 로드
    Serial.println(F("[CERT] 클레임 인증서 로드 시도"));
    if (!loadClaimCert())
    {
        Serial.println(F("[CERT] 클레임 인증서 로드 실패. 중지됨."));
        return;
    }

    Serial.println(F("[CERT] 클레임 인증서 로드 성공"));

    if (!requestNewCertificate())
    {
        Serial.println(F("[CERT] 인증서 요청 실패"));
        return;
    }

    Serial.println(F("[CERT] 인증서 요청 성공"));

    // // 최대 3회 재시도
    // for (int attempt = 0; attempt < 3; attempt++)
    // {
    //     Serial.printf_P(PSTR("[CERT] 인증서 요청 시도 %d/3\n"), attempt + 1);
    //     if (requestNewCertificate())
    //     {
    //         Serial.println(F("[CERT] 인증서 요청 성공"));
    //         return;
    //     }
    //     delay(1000); // 재시도 전 잠시 대기
    // }

    // Serial.println(F("[CERT] 인증서 요청 최대 시도 횟수 초과"));
}

bool CertificateManager::loadRootCA()
{
    File rootCAFile = LittleFS.open(reinterpret_cast<const char *>(F(LFS_ROOT_CA_PATH)), "r");
    if (!rootCAFile)
    {
        Serial.println(F("[LFS_ERROR] 루트 CA 파일 열기 실패."));
        return false;
    }

    rootCACert = rootCAFile.readString();
    rootCAFile.close();

    if (rootCACert.length() == 0)
    {
        Serial.println(F("[LFS_ERROR] 루트 CA 파일 읽기 실패."));
        return false;
    }

    Serial.println(F("[LFS] 루트 CA 로드 성공."));
    return true;
}

bool CertificateManager::loadDeviceCert()
{
    File deviceCertFile = LittleFS.open(reinterpret_cast<const char *>(F(LFS_DEVICE_CRT_PATH)), "r");
    File deviceKeyFile = LittleFS.open(reinterpret_cast<const char *>(F(LFS_DEVICE_KEY_PATH)), "r");

    if (!deviceCertFile || !deviceKeyFile)
    {
        Serial.println(F("[LFS_ERROR] 영구 인증서 파일 열기 실패."));
        if (deviceCertFile)
            deviceCertFile.close();
        if (deviceKeyFile)
            deviceKeyFile.close();
        return false;
    }

    deviceCert = deviceCertFile.readString();
    deviceKey = deviceKeyFile.readString();

    deviceCertFile.close();
    deviceKeyFile.close();

    if (deviceCert.length() == 0 || deviceKey.length() == 0)
    {
        Serial.println(F("[LFS_ERROR] 영구 인증서 파일 읽기 실패."));
        return false;
    }

    return true;
}

bool CertificateManager::loadClaimCert()
{
    File claimCertFile = LittleFS.open(reinterpret_cast<const char *>(F(LFS_CLAIM_CRT_PATH)), "r");
    File claimKeyFile = LittleFS.open(reinterpret_cast<const char *>(F(LFS_CLAIM_KEY_PATH)), "r");

    if (!claimCertFile || !claimKeyFile)
    {
        Serial.println(F("[LFS_ERROR] 클레임 인증서 파일 열기 실패."));
        if (claimCertFile)
            claimCertFile.close();
        if (claimKeyFile)
            claimKeyFile.close();
        return false;
    }

    claimCert = claimCertFile.readString();
    claimKey = claimKeyFile.readString();

    claimCertFile.close();
    claimKeyFile.close();

    if (claimCert.length() == 0 || claimKey.length() == 0)
    {
        Serial.println(F("[LFS_ERROR] 클레임 인증서 파일 읽기 실패."));
        return false;
    }

    return true;
}

bool CertificateManager::requestNewCertificate()
{
    // AWS IoT 설정
    const char *aws_endpoint = fpstr_to_cstr(FPSTR(AWS_IOT_ENDPOINT));

    // 올바른 요청 페이로드 사용
    const char *request = "{}";

    // 보안 클라이언트 설정
    std::unique_ptr<WiFiClientSecure> secureClient(new WiFiClientSecure());
    secureClient->setCACert(rootCACert.c_str());
    secureClient->setCertificate(claimCert.c_str());
    secureClient->setPrivateKey(claimKey.c_str());

    // 인증서 응답 변수
    bool certificateReceived = false;
    String deviceCert = "";
    String deviceKey = "";
    String certificateId = "";
    String certificateOwnershipToken = "";

    // MQTT 연결
    Serial.println(F("[MQTT] 연결 시도"));
    if (!mqttHandler.connect())
    {
        Serial.println(F("[MQTT] 연결 실패"));
        return false;
    }

    Serial.println(F("[MQTT] 연결 성공"));

    // 응답 토픽 구독
    const char *cert_accepted = CERTIFICATE_CREATE_ACCEPTED_TOPIC;
    const char *cert_rejected = CERTIFICATE_CREATE_REJECTED_TOPIC;
    const char *prov_accepted = PROVISIONING_ACCEPTED_TOPIC;
    const char *prov_rejected = PROVISIONING_REJECTED_TOPIC;
    mqttHandler.subscribe(cert_accepted);
    mqttHandler.subscribe(cert_rejected);
    mqttHandler.subscribe(prov_accepted);
    mqttHandler.subscribe(prov_rejected);

    // 콜백 설정
    mqttHandler.setCallback([&](char *topic, byte *payload, unsigned int length)
                            {
        Serial.printf_P(PSTR("[MQTT] 메시지 수신: %s (길이: %d)\n"), topic, length);
        
        // 페이로드를 null 종료 문자열로 변환
        std::unique_ptr<char[]> message(new char[length + 1]);
        memcpy(message.get(), payload, length);
        message[length] = '\0';
        
        // 응답 토픽 확인
        if (strcmp(topic, cert_accepted) == 0) {
            Serial.println(F("[MQTT] 인증서 생성 요청 승인됨 (CERTIFICATE_CREATE_ACCEPTED_TOPIC)"));
            
            // 구독 취소 (인증서 생성 관련 토픽만)
            mqttHandler.unsubscribe(cert_accepted);
            mqttHandler.unsubscribe(cert_rejected);

            // 수락된 인증서 처리
            processAcceptedCertificate(message.get(), deviceCert, deviceKey, 
                                    certificateId, certificateOwnershipToken
                                    );
        } else if (strcmp(topic, cert_rejected) == 0) {
            Serial.println(F("[MQTT] 인증서 생성 요청 거부됨 (CERTIFICATE_CREATE_REJECTED_TOPIC)"));
            Serial.print(F("[MQTT] 오류 메시지: "));
            Serial.println(message.get());
        }
        else if (strcmp(topic, prov_accepted) == 0) {
            Serial.println(F("[MQTT] 프로비저닝 요청 승인됨 (PROVISIONING_ACCEPTED_TOPIC)"));

            // 구독 취소 (프로비저닝 관련 토픽만)
            mqttHandler.unsubscribe(prov_accepted);
            mqttHandler.unsubscribe(prov_rejected);

            savePermanentCertificate(deviceCert, deviceKey, certificateOwnershipToken);
        } else if (strcmp(topic, prov_rejected) == 0) {
            Serial.println(F("[MQTT] 프로비저닝 요청 거부됨 (PROVISIONING_REJECTED_TOPIC)"));
            Serial.print(F("[MQTT] 오류 메시지: "));
            Serial.println(message.get());
        }        
        else {
            Serial.printf_P(PSTR("[MQTT] 처리되지 않은 토픽 수신: %s\n"), topic);
        }
        
        message.reset(); });

    delay(200); // 구독 처리 시간 대기

    // 요청 전송
    Serial.printf_P(PSTR("[MQTT] 요청 페이로드: %s\n"), request);
    Serial.printf_P(PSTR("[MQTT] 토픽 '%s'에 발행 시도\n"), CERTIFICATE_CREATE_TOPIC);

    bool success = mqttHandler.publish(CERTIFICATE_CREATE_TOPIC, request);
    if (!success)
    {
        Serial.println(F("[MQTT] 인증서 발행 요청 실패"));
        return false;
    }

    Serial.println(F("[MQTT] 인증서 발행 요청 성공"));
}

void CertificateManager::processAcceptedCertificate(
    const char *message,
    String &deviceCert,
    String &deviceKey,
    String &certificateId,
    String &certificateOwnershipToken)
{
    Serial.println(F("[CERT] 인증서 생성 요청 승인됨"));

    // JSON 파싱
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.printf_P(PSTR("[CERT] JSON 파싱 실패: %s\n"), error.c_str());
        return;
    }

    // 필요한 데이터 추출
    deviceCert = doc["certificatePem"].as<String>();
    deviceKey = doc["privateKey"].as<String>();
    certificateId = doc["certificateId"].as<String>();
    certificateOwnershipToken = doc["certificateOwnershipToken"].as<String>();

    Serial.printf_P(PSTR("[CERT] 인증서 및 개인키 수신 성공: %s, %s\n"), deviceCert.c_str(), deviceKey.c_str());
    Serial.printf_P(PSTR("[CERT] 인증서 ID: %s\n"), certificateId.c_str());
    Serial.printf_P(PSTR("[CERT] 토큰(처음 20자): %s\n"), certificateOwnershipToken.substring(0, 20).c_str());

    if (deviceCert.length() > 0 && deviceKey.length() > 0 && certificateId.length() > 0 && certificateOwnershipToken.length() > 0)
    {
        Serial.println(F("[CERT] 인증서, 개인키, 토큰 수신 성공"));

        // 프로비저닝 요청 시작
        requestProvisioning(certificateOwnershipToken);
    }
    else
    {
        Serial.println(F("[CERT] 인증서 데이터 누락"));
    }
}

// 프로비저닝 요청 메서드 추가
bool CertificateManager::requestProvisioning(const String &certificateOwnershipToken)
{
    Serial.println(F("[PROV] 프로비저닝 요청 시작"));

    // 디버그 메시지 추가
    Serial.print(F("[PROV] 템플릿 이름: "));
    Serial.println(FPSTR(PROVISIONING_TEMPLATE_NAME));

    // 간소화된 페이로드 사용
    StaticJsonDocument<512> jsonDoc;

    // 토큰 검증 - 길이 확인
    if (certificateOwnershipToken.length() < 20)
    {
        Serial.println(F("[PROV] 토큰이 너무 짧거나 비어 있음"));
        return false;
    }

    // 페이로드 구성
    jsonDoc["certificateOwnershipToken"] = certificateOwnershipToken;
    // 템플릿 파라미터에 일치하는 키 사용
    JsonObject parameters = jsonDoc.createNestedObject("parameters");
    parameters["SerialNumber"] = DeviceIdentity::getDeviceId();

    // 충분히 큰 버퍼 사용 (512바이트로 충분)
    char provisionPayload[512] = {0};
    size_t len = serializeJson(jsonDoc, provisionPayload, sizeof(provisionPayload));

    // 길이 확인 디버깅
    Serial.printf("[PROV] JSON 길이: %d 바이트\n", len);

    // 토큰 디버깅
    Serial.print(F("[PROV] 토큰 유효성: "));
    Serial.println(certificateOwnershipToken.indexOf("eyJ") == 0 ? "유효함" : "의심됨");
    Serial.printf_P(PSTR("[PROV] 토큰 길이: %d 바이트\n"), certificateOwnershipToken.length());

    // 발행시 길이 명시적으로 전달
    if (!mqttHandler.publish(PROVISIONING_REQUEST_TOPIC, provisionPayload, len))
    {
        Serial.println(F("[PROV] 프로비저닝 요청 발행 실패"));
        return false;
    }

    Serial.println(F("[PROV] 프로비저닝 요청 발행 성공"));
    return true;
}

void CertificateManager::savePermanentCertificate(
    const String &permanentCert,
    const String &permanentKey,
    const String &certificateOwnershipToken)
{
    Serial.println(F("[CERT] 영구 인증서 파일 저장 중..."));

    // 기존 파일 삭제
    if (LittleFS.exists(LFS_DEVICE_CRT_PATH))
    {
        LittleFS.remove(LFS_DEVICE_CRT_PATH);
    }
    if (LittleFS.exists(LFS_DEVICE_KEY_PATH))
    {
        LittleFS.remove(LFS_DEVICE_KEY_PATH);
    }

    // 새 인증서 파일 저장
    File certFile = LittleFS.open(LFS_DEVICE_CRT_PATH, "w");
    if (certFile)
    {
        certFile.print(permanentCert);
        certFile.close();
        Serial.println(F("[CERT] 영구 인증서 저장 완료"));
    }
    else
    {
        Serial.println(F("[CERT] 영구 인증서 파일 생성 실패"));
    }

    // 새 개인키 파일 저장
    File keyFile = LittleFS.open(LFS_DEVICE_KEY_PATH, "w");
    if (keyFile)
    {
        keyFile.print(permanentKey);
        keyFile.close();
        Serial.println(F("[CERT] 영구 개인키 저장 완료"));
    }
    else
    {
        Serial.println(F("[CERT] 영구 개인키 파일 생성 실패"));
    }

    // 토큰 저장 (프로비저닝에 사용됨)
    Preferences preferences;
    if (preferences.begin("cert-state", false))
    {
        preferences.putString("cert-token", certificateOwnershipToken.c_str());
        preferences.end();
        Serial.println(F("[CERT] 인증서 토큰 저장 완료"));
    }

    // 메모리에 인증서 로드
    deviceCert = permanentCert;
    deviceKey = permanentKey;
    Serial.println(F("[CERT] 새 영구 인증서 메모리에 로드 완료"));
}