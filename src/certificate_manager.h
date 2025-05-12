#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"

class CertificateManager
{
public:
    CertificateManager();

    // 초기화 함수
    void init();

    // 파일 시스템 초기화
    bool initializeFileSystem();

    // 통합 인증서 초기화 함수
    void initCertificates();

    // 루트 CA 인증서 로드
    bool loadRootCA();

    // 디바이스 인증서 로드
    bool loadDeviceCert();

    // 클레임 인증서 로드
    bool loadClaimCert();

    // 새 인증서 요청 처리
    bool requestNewCertificate();

    // 인증서 생성 승인 처리
    void processAcceptedCertificate(
        const char *message,
        String &permanentCert,
        String &permanentKey,
        String &certificateId,
        String &certificateOwnershipToken);

    // 영구 인증서 저장
    void savePermanentCertificate(
        const String &permanentCert,
        const String &permanentKey,
        const String &certificateOwnershipToken);

    // 인증서 얻기
    const String &getRootCACert() const { return rootCACert; }
    const String &getDeviceCert() const { return deviceCert; }
    const String &getDeviceKey() const { return deviceKey; }
    const String &getClaimCert() const { return claimCert; }
    const String &getClaimKey() const { return claimKey; }

private:
    String rootCACert;
    String claimCert;
    String claimKey;
    String deviceCert;
    String deviceKey;
    bool requestProvisioning(const String &certificateOwnershipToken);

    // 임시 저장용 멤버 변수
    String tempDeviceCert;
    String tempDeviceKey;
    String tempCertificateId;
    String tempCertificateOwnershipToken;

    String m_provisioningPayload;
};

// 전역 인스턴스
extern CertificateManager certManager;