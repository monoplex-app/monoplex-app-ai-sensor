#include "certificate_manager.h"
#include "device_identity.h"

// PubSubClient 및 WiFiClientSecure 헤더 추가
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h" // AWS_IOT_ENDPOINT, MQTT_BUFFER_SIZE 등 필요

// 전역 인스턴스 생성
CertificateManager certManager;

CertificateManager::CertificateManager() : rootCACert(""), deviceCert(""), deviceKey(""), claimCert(""), claimKey(""), tempDeviceCert(""), tempDeviceKey(""), tempCertificateId(""), tempCertificateOwnershipToken(""), m_provisioningPayload("")
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
    // if (loadDeviceCert()) // 디바이스 인증서가 이미 있으면 MQTT 과정 불필요
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
    Serial.println(F("[LFS] 영구 인증서 로드 성공."));
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
    Serial.println(F("[LFS] 클레임 인증서 로드 성공."));
    return true;
}

bool CertificateManager::requestNewCertificate()
{
    // AWS IoT 설정
    const char *aws_endpoint = fpstr_to_cstr(FPSTR(AWS_IOT_ENDPOINT));

    // 올바른 요청 페이로드 사용
    const char *request = "{}";

    std::unique_ptr<WiFiClientSecure> secureClient(new WiFiClientSecure());
    PubSubClient mqttClient(*secureClient);

    secureClient->setCACert(rootCACert.c_str());
    secureClient->setCertificate(claimCert.c_str());
    secureClient->setPrivateKey(claimKey.c_str());

    mqttClient.setServer(aws_endpoint, 8883);
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient.setKeepAlive(60);

    Serial.println(F("[MQTT] 연결 시도"));
    const char *clientId = DeviceIdentity::getDeviceId();
    if (!mqttClient.connect(clientId, NULL, NULL, NULL, 0, false, NULL, true))
    {
        Serial.print(F("[MQTT] 연결 실패, 상태: "));
        Serial.println(mqttClient.state());
        return false;
    }

    Serial.println(F("[MQTT] 연결 성공"));
    for (int i = 0; i < 3; i++)
    {
        mqttClient.loop();
        delay(10);
    }

    const char *cert_accepted_topic_str = CERTIFICATE_CREATE_ACCEPTED_TOPIC;
    const char *cert_rejected_topic_str = CERTIFICATE_CREATE_REJECTED_TOPIC;
    const char *prov_accepted_topic_str = PROVISIONING_ACCEPTED_TOPIC;
    const char *prov_rejected_topic_str = PROVISIONING_REJECTED_TOPIC;

    mqttClient.subscribe(cert_accepted_topic_str);
    mqttClient.subscribe(cert_rejected_topic_str);
    mqttClient.subscribe(prov_accepted_topic_str);
    mqttClient.subscribe(prov_rejected_topic_str);

    // 람다 함수를 변수에 저장
    auto callbackLambda = [this, &mqttClient, cert_accepted_topic_str, cert_rejected_topic_str, prov_accepted_topic_str, prov_rejected_topic_str](char *topic, byte *payload, unsigned int length)
    {
        Serial.printf_P(PSTR("[MQTT] 메시지 수신: %s (길이: %d)\n"), topic, length);

        std::unique_ptr<char[]> message_ptr(new char[length + 1]);
        memcpy(message_ptr.get(), payload, length);
        message_ptr[length] = '\0';
        const char *message = message_ptr.get();

        if (strcmp(topic, cert_accepted_topic_str) == 0)
        {
            Serial.println(F("[MQTT] 인증서 생성 요청 승인됨 (CERTIFICATE_CREATE_ACCEPTED_TOPIC)"));
            mqttClient.unsubscribe(cert_accepted_topic_str);
            mqttClient.unsubscribe(cert_rejected_topic_str);

            String localCert, localKey, localId, localToken;
            // processAcceptedCertificate 호출하여 로컬 변수에 값 저장
            this->processAcceptedCertificate(message, localCert, localKey, localId, localToken);

            // 로컬 변수 값을 클래스 멤버(temp) 변수에 저장
            this->tempDeviceCert = localCert;
            this->tempDeviceKey = localKey;
            this->tempCertificateId = localId; // 필요시 사용
            this->tempCertificateOwnershipToken = localToken;

            if (!this->tempCertificateOwnershipToken.isEmpty())
            {
                // requestProvisioning은 페이로드만 생성하여 this->m_provisioningPayload에 저장
                if (this->requestProvisioning(this->tempCertificateOwnershipToken))
                {
                    // 람다에서 직접 MQTT 발행
                    if (!this->m_provisioningPayload.isEmpty())
                    {
                        bool provisionPublishSuccess = mqttClient.publish(PROVISIONING_REQUEST_TOPIC, reinterpret_cast<const uint8_t *>(this->m_provisioningPayload.c_str()), this->m_provisioningPayload.length());
                        if (provisionPublishSuccess)
                        {
                            Serial.println(F("[PROV] 프로비저닝 요청 발행 성공 (콜백 내)"));
                        }
                        else
                        {
                            Serial.println(F("[PROV] 프로비저닝 요청 발행 실패 (콜백 내)"));
                        }
                    }
                    else
                    {
                        Serial.println(F("[PROV_ERROR] 생성된 프로비저닝 페이로드가 비어있습니다."));
                    }
                }
                else
                {
                    Serial.println(F("[PROV_ERROR] 프로비저닝 페이로드 생성 실패."));
                }
            }
            else
            {
                Serial.println(F("[CERT_ERROR] 인증서 수신 데이터(토큰) 누락."));
            }
        }
        else if (strcmp(topic, cert_rejected_topic_str) == 0)
        {
            Serial.println(F("[MQTT] 인증서 생성 요청 거부됨 (CERTIFICATE_CREATE_REJECTED_TOPIC)"));
            Serial.print(F("[MQTT] 오류 메시지: "));
            Serial.println(message);
        }
        else if (strcmp(topic, prov_accepted_topic_str) == 0)
        {
            Serial.println(F("[MQTT] 프로비저닝 요청 승인됨 (PROVISIONING_ACCEPTED_TOPIC)"));
            mqttClient.unsubscribe(prov_accepted_topic_str);
            mqttClient.unsubscribe(prov_rejected_topic_str);
            // tempDeviceCert 등은 CERTIFICATE_CREATE_ACCEPTED_TOPIC 콜백에서 이미 설정됨
            savePermanentCertificate(this->tempDeviceCert, this->tempDeviceKey, this->tempCertificateOwnershipToken);
        }
        else if (strcmp(topic, prov_rejected_topic_str) == 0)
        {
            Serial.println(F("[MQTT] 프로비저닝 요청 거부됨 (PROVISIONING_REJECTED_TOPIC)"));
            Serial.print(F("[MQTT] 오류 메시지: "));
            Serial.println(message);
        }
        else
        {
            Serial.printf_P(PSTR("[MQTT] 처리되지 않은 토픽 수신: %s\n"), topic);
        }
    };

    // setCallback에 lvalue 람다 전달
    mqttClient.setCallback(callbackLambda);

    delay(200);

    Serial.printf_P(PSTR("[MQTT] 요청 페이로드: %s\n"), request);
    Serial.printf_P(PSTR("[MQTT] 토픽 '%s'에 발행 시도\n"), CERTIFICATE_CREATE_TOPIC);

    bool success = mqttClient.publish(CERTIFICATE_CREATE_TOPIC, request);
    if (!success)
    {
        Serial.println(F("[MQTT] 인증서 발행 요청 실패"));
        mqttClient.disconnect();
        return false;
    }

    Serial.println(F("[MQTT] 인증서 발행 요청 성공"));

    unsigned long startTime = millis();
    bool provisioningComplete = false;
    while (!provisioningComplete && (millis() - startTime < 60000))
    {
        mqttClient.loop();
        if (!deviceCert.isEmpty() && !deviceKey.isEmpty())
        {
            provisioningComplete = true;
        }
        delay(100);
    }

    mqttClient.disconnect();

    if (!provisioningComplete)
    {
        Serial.println(F("[CERT] 인증서 수신 및 프로비저닝 타임아웃 또는 실패"));
        return false;
    }

    Serial.println(F("[CERT] 인증서 수신 및 프로비저닝 완료"));
    return true;
}

// 헤더파일과 시그니처 일치시키고, 참조 인자를 통해 결과 반환
void CertificateManager::processAcceptedCertificate(
    const char *message,
    String &outDeviceCert,               // 출력용 참조 인자
    String &outDeviceKey,                // 출력용 참조 인자
    String &outCertificateId,            // 출력용 참조 인자
    String &outCertificateOwnershipToken // 출력용 참조 인자
)
{
    Serial.println(F("[CERT] 인증서 생성 요청 승인됨 (processAcceptedCertificate)"));

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.printf_P(PSTR("[CERT] JSON 파싱 실패: %s\n"), error.c_str());
        // 출력 인자들을 비워두거나 에러 상태를 나타내는 값으로 설정할 수 있습니다.
        outDeviceCert = "";
        outDeviceKey = "";
        outCertificateId = "";
        outCertificateOwnershipToken = "";
        return;
    }

    // 파싱된 값을 출력 인자에 할당
    outDeviceCert = doc["certificatePem"].as<String>();
    outDeviceKey = doc["privateKey"].as<String>();
    outCertificateId = doc["certificateId"].as<String>();
    outCertificateOwnershipToken = doc["certificateOwnershipToken"].as<String>();

    Serial.printf_P(PSTR("[CERT] 파싱된 인증서: %s...\n"), outDeviceCert.substring(0, 20).c_str());
    Serial.printf_P(PSTR("[CERT] 파싱된 개인키: %s...\n"), outDeviceKey.substring(0, 20).c_str());
    Serial.printf_P(PSTR("[CERT] 파싱된 ID: %s\n"), outCertificateId.c_str());
    Serial.printf_P(PSTR("[CERT] 파싱된 토큰: %s...\n"), outCertificateOwnershipToken.substring(0, 20).c_str());

    if (outDeviceCert.isEmpty() || outDeviceKey.isEmpty() || outCertificateId.isEmpty() || outCertificateOwnershipToken.isEmpty())
    {
        Serial.println(F("[CERT] 인증서 데이터 일부 누락됨"));
    }
    else
    {
        Serial.println(F("[CERT] 인증서, 개인키, ID, 토큰 파싱 성공"));
    }
}

// 헤더파일과 시그니처 일치. 페이로드를 m_provisioningPayload에 저장하고 성공 여부 반환.
bool CertificateManager::requestProvisioning(
    const String &certificateOwnershipTokenToUse)
{
    Serial.println(F("[PROV] 프로비저닝 페이로드 생성 시작 (requestProvisioning)"));
    m_provisioningPayload = ""; // 이전 페이로드 클리어

    Serial.print(F("[PROV] 템플릿 이름: "));
    Serial.println(FPSTR(PROVISIONING_TEMPLATE_NAME));

    StaticJsonDocument<512> jsonDoc;

    if (certificateOwnershipTokenToUse.length() < 20)
    {
        Serial.println(F("[PROV_ERROR] 토큰이 너무 짧거나 비어 있음"));
        return false;
    }

    jsonDoc["certificateOwnershipToken"] = certificateOwnershipTokenToUse;
    JsonObject parameters = jsonDoc.createNestedObject("parameters");
    parameters["SerialNumber"] = DeviceIdentity::getDeviceId();

    char provisionPayloadBuffer[4096] = {0};
    size_t len = serializeJson(jsonDoc, provisionPayloadBuffer, sizeof(provisionPayloadBuffer));

    if (len == 0 || len >= sizeof(provisionPayloadBuffer))
    {
        Serial.println(F("[PROV_ERROR] JSON 직렬화 실패 또는 버퍼 오버플로우"));
        return false;
    }

    m_provisioningPayload = String(provisionPayloadBuffer); // 생성된 페이로드를 멤버 변수에 저장

    Serial.printf("[PROV] 생성된 JSON 길이: %u 바이트\n", len);
    Serial.print(F("[PROV] 토큰 유효성: "));
    Serial.println(certificateOwnershipTokenToUse.indexOf("eyJ") == 0 ? "유효함" : "의심됨");
    Serial.printf_P(PSTR("[PROV] 토큰 길이: %u 바이트\n"), certificateOwnershipTokenToUse.length());
    Serial.println(F("[PROV] 프로비저닝 페이로드 생성 성공"));
    return true;
}

void CertificateManager::savePermanentCertificate(
    const String &permanentCertToSave,
    const String &permanentKeyToSave,
    const String &tokenToSave)
{
    Serial.println(F("[CERT] 영구 인증서 파일 저장 중..."));

    if (LittleFS.exists(LFS_DEVICE_CRT_PATH))
    {
        LittleFS.remove(LFS_DEVICE_CRT_PATH);
    }
    if (LittleFS.exists(LFS_DEVICE_KEY_PATH))
    {
        LittleFS.remove(LFS_DEVICE_KEY_PATH);
    }

    File certFile = LittleFS.open(LFS_DEVICE_CRT_PATH, "w");
    if (certFile)
    {
        certFile.print(permanentCertToSave);
        certFile.close();
        Serial.println(F("[CERT] 영구 인증서 저장 완료"));
    }
    else
    {
        Serial.println(F("[CERT] 영구 인증서 파일 생성 실패"));
    }

    File keyFile = LittleFS.open(LFS_DEVICE_KEY_PATH, "w");
    if (keyFile)
    {
        keyFile.print(permanentKeyToSave);
        keyFile.close();
        Serial.println(F("[CERT] 영구 개인키 저장 완료"));
    }
    else
    {
        Serial.println(F("[CERT] 영구 개인키 파일 생성 실패"));
    }

    Preferences preferences;
    if (preferences.begin("cert-state", false)) // "cert-state" 네임스페이스 사용
    {
        preferences.putString("cert-token", tokenToSave.c_str()); // 토큰 저장
        preferences.end();
        Serial.println(F("[CERT] 인증서 토큰 저장 완료"));
    }
    else
    {
        Serial.println(F("[PREF_ERROR] Preferences 초기화 실패 (cert-state)"));
    }

    // 실제 디바이스 인증서/키 멤버 변수 업데이트
    deviceCert = permanentCertToSave;
    deviceKey = permanentKeyToSave;

    // 임시 변수 초기화
    tempDeviceCert = "";
    tempDeviceKey = "";
    tempCertificateId = "";
    tempCertificateOwnershipToken = "";
    Serial.println(F("[CERT] 새 영구 인증서 메모리에 로드 완료"));
}