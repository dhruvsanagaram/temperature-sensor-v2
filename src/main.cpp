
#include <Arduino.h>
#include <SPIFFS.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

const int readPin = 13;  // Analog pin to read

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-0987654321ba"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
String fileData = "";  // Stores SPIFFS file data for BLE transmission

void setup() {
    Serial.begin(9600);
    delay(500); // Allow Serial time to initialize

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to initialize SPIFFS");
        return;
    }
    Serial.println("SPIFFS initialized successfully");

    // Check if the file exists and print its contents, then delete it
    if (SPIFFS.exists("/analog_data.txt")) {
        Serial.println("File exists. Preparing data for BLE transmission:");

        // Open the file and read its contents into a string
        File file = SPIFFS.open("/analog_data.txt", FILE_READ);
        if (file) {
            while (file.available()) {
                String line = file.readStringUntil('\n');
                fileData += line + "\n";  // Append each line to fileData
                Serial.println(line);  // Print each line to Serial
            }
            file.close();
            Serial.println("End of file contents.");

            // Remove the file after reading
            SPIFFS.remove("/analog_data.txt");
            Serial.println("Previous data file deleted");
        } else {
            Serial.println("Failed to open file for reading.");
        }
    } else {
        fileData = "No data available in SPIFFS.";
    }

    // Initialize BLE
    BLEDevice::init("ESP32_SPIFFS_BLE_Server");
    BLEServer *pServer = BLEDevice::createServer();

    // BLE Service and Characteristic
    BLEService *pService = pServer->createService(SERVICE_UUID);
    Serial.println("Service created");

    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                      );
    Serial.println("Characteristic created");

    pCharacteristic->addDescriptor(new BLE2902());
    Serial.println("Characteristic descriptor added");

    // Set the initial value of the characteristic
    pCharacteristic->setValue(fileData.c_str());

    // Start the service
    pService->start();
    Serial.println("Service started");

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // For compatibility
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("BLE server is ready and advertising...");
}

void loop() {
    // Read analog value
    int analogValue = analogRead(readPin);
    Serial.print("Analog Value: ");
    Serial.println(analogValue);

    // Map analog value to byte range (0-255) for storage
    byte valueToStore = (byte)((analogValue / 4095.0) * 255.0);

    // Open the file in append mode and write the value
    File file = SPIFFS.open("/analog_data.txt", FILE_APPEND);
    if (file) {
        file.println(valueToStore);  // Write the value as a new line
        file.close();
        Serial.print("Stored Value in SPIFFS: ");
        Serial.println(valueToStore);
    } else {
        Serial.println("Failed to open file for writing");
    }

    delay(1000);  // Delay for readability
}

