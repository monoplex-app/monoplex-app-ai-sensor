#include "ble_handler.h"
#include <BLE2902.h>
#include "config.h"         // BLE_DEVICE_NAME 등 설정 값을 위해 포함
#include "wifi_handler.h"   // scanWifiNetworks() 함수를 사용하기 위해 포함
#include "eeprom_handler.h" // EEPROM에 저장된 WiFi 정보를 사용하기 위해 포함
#include <ArduinoJson.h>

// 모듈 내부에서만 사용할 변수 및 객체들
static BLEServer* pServer = nullptr;
static BLECharacteristic* pCharacteristic = nullptr;
static bool newBleDataReceived = false;
static String receivedBleData = "";
static String apListJson = ""; // 스캔된 WiFi AP 목록을 저장할 변수
static bool requestFirstMsgSend = false;
static int bleRetryCounter = 0;
static bool rebootRequested = false;
static bool awaitingMqttResult = false;

// isBleClientConnected는 main.cpp에서 이미 정의됨 - 중복 정의 제거

// BLE 콜백 클래스 정의

// 클라이언트 연결 및 해제 이벤트를 처리하는 콜백 클래스
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        isBleClientConnected = true;
        requestFirstMsgSend = true; // 연결되면 첫 메시지(AP 목록) 전송을 요청
        Serial.println("BLE 클라이언트 연결");
    }

    void onDisconnect(BLEServer* pServer) {
        isBleClientConnected = false;
        Serial.println("BLE 클라이언트 연결 끊김");
        // 연결이 끊어지면 다시 광고를 시작하여 다른 기기가 찾을 수 있도록 함
        pServer->getAdvertising()->start();
        
        // 재부팅이 요청된 상태에서 연결이 끊어졌다면 재부팅 수행
        if (rebootRequested) {
            Serial.println("재부팅 요청됨...");
            delay(500);
            ESP.restart();
        }
    }
};

// 클라이언트로부터 데이터를 받았을 때(Write) 이벤트를 처리하는 콜백 클래스
class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            receivedBleData = String(value.c_str());
            newBleDataReceived = true; // 새로운 데이터가 수신되었음을 표시
        }
    }
};

void processBleData(const String& data); // 내부에서만 사용할 함수이므로 미리 선언

void initBLE(const String& uid, const String& preScannedWifiList) {
    Serial.println("BLE 초기화 시작...");

    // 1. BLE 장치 초기화 및 이름 설정
    BLEDevice::init(BLE_DEVICE_NAME);

    // 2. BLE 서버 생성
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // 3. 서비스(Service) 생성
    BLEService* pService = pServer->createService(SERVER_SERVICE_UUID);

    // 4. 특성(Characteristic) 생성
    pCharacteristic = pService->createCharacteristic(
        SERVER_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->setCallbacks(new CharacteristicCallbacks());
    pCharacteristic->addDescriptor(new BLE2902()); // 클라이언트가 Notify를 받을 수 있도록 설정

    // 5. 서비스 시작
    pService->start();

    // 6. 광고(Advertising) 시작
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVER_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("BLE 서버 시작. 클라이언트 연결 대기...");
    
    // 7. 미리 스캔된 WiFi 목록 저장 (setup에서 전달받음)
    if (preScannedWifiList.length() > 0) {
        apListJson = preScannedWifiList;
        Serial.println("미리 스캔된 WiFi 목록 저장 완료");
    } else {
        apListJson = "";
        Serial.println("WiFi 목록이 비어있음");
    }
}

void handleBLE() {
    // 클라이언트가 방금 연결되어 첫 메시지 전송이 필요할 경우
    if (requestFirstMsgSend) {
        requestFirstMsgSend = false;
        delay(500); // 클라이언트가 서비스를 탐색할 시간을 줌
        bleRetryCounter = 0;
        
        // 미리 스캔된 WiFi 목록 전송
        Serial.println("BLE 클라이언트 연결됨, 저장된 WiFi 목록 전송");
        if (apListJson.length() > 0) {
            sendBleData(apListJson);
        } else {
            Serial.println("저장된 WiFi 목록이 없음");
            String uid = getMacAddress();
            String emptyList = "{\"macid\":\"" + uid + "\",\"numAp\":0,\"aplist\":[]}";
            sendBleData(emptyList);
        }
    }
    
    // 클라이언트로부터 새로운 데이터가 수신되었을 경우
    if (newBleDataReceived) {
        newBleDataReceived = false;
        Serial.print("BLE로 수신된 데이터: ");
        Serial.println(receivedBleData);
        processBleData(receivedBleData);
    }
}

void sendBleData(const String& data) {
    if (isBleClientConnected) {
        const int maxChunkSize = 500; // BLE MTU 제한을 고려한 청크 크기
        int dataLength = data.length();
        
        if (dataLength <= maxChunkSize) {
            // 데이터가 작으면 한 번에 전송
            pCharacteristic->setValue(data.c_str());
            pCharacteristic->notify();
            Serial.print("BLE로 전송된 데이터: ");
            Serial.println(data);
        } else {
            // 데이터가 크면 청크로 나누어 전송
            Serial.print("큰 데이터를 청크로 나누어 전송 (총 ");
            Serial.print(dataLength);
            Serial.println(" 바이트)");
            
            for (int i = 0; i < dataLength; i += maxChunkSize) {
                int chunkSize = min(maxChunkSize, dataLength - i);
                String chunk = data.substring(i, i + chunkSize);
                
                pCharacteristic->setValue(chunk.c_str());
                pCharacteristic->notify();
                
                Serial.print("청크 ");
                Serial.print(i / maxChunkSize + 1);
                Serial.print(" 전송 (");
                Serial.print(chunkSize);
                Serial.print(" 바이트): ");
                Serial.println(chunk.substring(0, min(50, chunkSize)) + "...");
                
                delay(50); // 청크 간 짧은 지연
            }
            Serial.println("모든 청크 전송 완료");
        }
    } else {
        Serial.println("데이터 전송 실패, BLE 클라이언트가 연결되지 않음.");
    }
}

// 수신된 BLE 데이터를 파싱하고 처리하는 내부 함수
void processBleData(const String& data) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);

    if (error) {
        Serial.print("deserializeJson() 실패: ");
        Serial.println(error.c_str());
        
        // JSON 파싱 실패 시 스캔 명령어만 확인
        if (data == "scan" || data.indexOf("scan") >= 0) {
            Serial.println("스캔 명령어 감지, Wi-Fi 목록 재전송");
            sendBleData(apListJson);
            return;
        }
        
        return;
    }

    // 클라이언트가 Wi-Fi 목록을 요청하는 경우 추가
    if (doc["request_wifi_list"].is<JsonVariant>()) {
        Serial.println("Wi-Fi 목록 재요청 받음");
        sendBleData(apListJson);
        return;
    }

    // 클라이언트가 AP 목록을 잘 받았다는 응답 처리
    if (doc["received_ap_list"].is<JsonVariant>()) {
        if (doc["received_ap_list"] == 1) {
            Serial.println("클라이언트가 AP 목록을 잘 받았다는 응답");
            bleRetryCounter = 0;
        } else {
            Serial.println("클라이언트가 AP 목록을 잘 받지 못함. 재시도...");
            if (bleRetryCounter < MAX_BLE_RETRY) {
                bleRetryCounter++;
                sendBleData(apListJson);
            }
        }
    }
    // 클라이언트가 선택한 SSID와 비밀번호 처리
    else if (doc["ssid"].is<JsonVariant>()) {
        String new_ssid = doc["ssid"];
        String new_password = doc["passwd"];

        Serial.println("BLE로 수신된 새로운 WiFi 자격 증명:");
        Serial.print("SSID: ");
        Serial.println(new_ssid);
        Serial.print("Password: ");
        Serial.println(new_password);

        // 새 정보를 EEPROM에 저장
        saveWiFiCredentials(new_ssid, new_password);

        // 전역 ssid/password 갱신 후 즉시 Wi‑Fi 재연결 시도
        ssid = new_ssid;
        password = new_password;
        awaitingMqttResult = false;
        reconnectWiFi();

        // 응답 전송
        sendBleData("{\"status\":\"credentials_received\"}");
        Serial.println("자격 증명 저장 및 즉시 WiFi 재연결 시도 완료.");
    }
}

void sendWifiStatusUpdate(bool success, const String& message) {
    if (!isBleClientConnected) {
        return;
    }

    JsonDocument doc;
    doc["event"] = "wifi";
    doc["status"] = success ? "connected" : "failed";
    if (message.length() > 0) {
        doc["message"] = message;
    }

    if (success) {
        awaitingMqttResult = true;
    } else {
        awaitingMqttResult = false;
    }

    String payload;
    serializeJson(doc, payload);
    sendBleData(payload);
}

void sendMqttStatusUpdate(bool success, const String& message) {
    if (!isBleClientConnected) {
        return;
    }

    if (!awaitingMqttResult && !success) {
        // 프로비저닝 흐름 중이 아닐 때의 실패 메시지는 무시
        return;
    }

    JsonDocument doc;
    doc["event"] = "mqtt";
    doc["status"] = success ? "connected" : "failed";
    if (message.length() > 0) {
        doc["message"] = message;
    }

    awaitingMqttResult = false;

    String payload;
    serializeJson(doc, payload);
    sendBleData(payload);
}
