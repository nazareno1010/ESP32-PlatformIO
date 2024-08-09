#include <HTTPClient.h>
#include "config.h"
#include <iostream>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiManager.h>

// Pin definitions
#define RST_PIN         22
#define SS_1_PIN        21
#define NR_OF_READERS   1

byte ssPins[] = {SS_1_PIN};

MFRC522 mfrc522[NR_OF_READERS];

// Function prototypes
String dumpByteArrayToHex(byte *buffer, byte bufferSize);
String getMACWithoutColons();
void setupRFIDReaders();
void handleNewCard(uint8_t reader);
void sendCardDataToServer(const String& uidString);

int main() {
    std::cout << "API: " << API << std::endl;
    return 0;
}

// Convert byte array to hexadecimal string
String dumpByteArrayToHex(byte *buffer, byte bufferSize) {
    String result = "";
    for (byte i = 0; i < bufferSize; i++) {
        result += (buffer[i] < 0x10 ? "0" : "") + String(buffer[i], HEX);
    }
    return result;
}

// Get MAC address without colons
String getMACWithoutColons() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return mac;
}

// Initialize RFID readers
void setupRFIDReaders() {
    for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
        mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN);
        Serial.print(F("Reader "));
        Serial.print(reader);
        Serial.print(F(": "));
        mfrc522[reader].PCD_DumpVersionToSerial();
    }
}

// Handle new card detection
void handleNewCard(uint8_t reader) {
    Serial.println();
    Serial.print(F("Reader: "));
    Serial.println(reader);

    String uidString = dumpByteArrayToHex(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size);
    uidString.toUpperCase();

    Serial.print(F("Card UID: "));
    Serial.println(uidString);

    Serial.print(F("PICC type: "));
    MFRC522::PICC_Type piccType = mfrc522[reader].PICC_GetType(mfrc522[reader].uid.sak);
    Serial.println(mfrc522[reader].PICC_GetTypeName(piccType));
    Serial.println();

    sendCardDataToServer(uidString);

    // Halt PICC and stop encryption on PCD
    mfrc522[reader].PICC_HaltA();
    mfrc522[reader].PCD_StopCrypto1();
}

// Send card data to server
void sendCardDataToServer(const String& uidString) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(API);
        http.addHeader("Content-Type", "application/json");

        String MAC = getMACWithoutColons();
        String httpRequestData = "{\"readerId\":\"" + MAC + "\",\"tagId\":\"" + uidString + "\"}";
        
        int httpResponseCode = http.POST(httpRequestData);
        
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("HTTP Code: " + String(httpResponseCode));
            Serial.println("Response: " + response);
        } else {
            Serial.println("HTTP request failed: " + String(httpResponseCode));
        }

        http.end();
    } else {
        Serial.println("WiFi connection error");
    }
}

void setup() {
    Serial.begin(9600);
    while (!Serial);  // Wait for serial port to connect

    SPI.begin();

    // Setup WiFi using WiFiManager
    WiFiManager wm;
    String SSID = "RFID-" + getMACWithoutColons();
    bool connected = wm.autoConnect(SSID.c_str(), "nazareno");

    if (!connected) {
        Serial.println("Failed to connect to WiFi");
        // Consider adding a delay before restarting
        // ESP.restart();
    } else {
        Serial.println("Connected to WiFi");
    }

    setupRFIDReaders();
}

void loop() {
    for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
        if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
            handleNewCard(reader);
        }
    }
}