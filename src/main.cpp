#include <Arduino.h>
#include <SPIFFS.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Pin and hardware constants
const int readPin = 13;  // Analog pin to read
const int pHReadPin = 14;
const float ADC_RESOLUTION = 4095.0; // 12-bit ADC resolution
const float VOLTAGE_REF = 3.3;       // Reference voltage of ESP32 (3.3V)
const float atlas_coeff = -5.6548;
const float atlas_const = 15.509;
const int lin_actuate = 21;          // Linear actuator control pin 1
const int lin_deactuate = 23;        // Linear actuator control pin 2
const int duration = 3000;           // Actuation duration (milliseconds)

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-0987654321ba"
#define SERVICE_UUID_PH "12345678-5678-5678-5678-1234567890cd"
#define CHARACTERISTIC_UUID_PH "87654321-8765-8765-8765-0987654321dc"

// BLE setup
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
String fileData = "";  // Stores SPIFFS file data for BLE transmission

BLECharacteristic *pCharacteristic_pH;
String pH_fileData = "";


// Timing variables for actuator control
unsigned long lastActuationTime = 0;
bool actuatorState = false; // Track the state of the actuator

// Timing variables for data logging
unsigned long lastLogTime = 0;
const unsigned long logInterval = 1000; // 1-second interval for SPIFFS logging

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

    if(SPIFFS.exists("/pH_data.txt")){
        Serial.println("pH file exists. Preparing data for BLE transmission:");
        
        File pH_file = SPIFFS.open("/pH_data.txt", FILE_READ);
        if(pH_file) {
            while(pH_file.available()){
                String pH_line = pH_file.readStringUntil('\n');
                pH_fileData = pH_line + "\n";
                Serial.println(pH_line);
            }
            pH_file.close();
            Serial.println("End of pH file contents.");

            SPIFFS.remove("/pH_data.txt");
            Serial.println("Previous pH file deleted");
        } else {
            Serial.println("Failed to open pH file for reading");
        }
    } else {
        pH_fileData = "No data available in SPIFFS.";
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
    pCharacteristic->setValue(fileData.c_str());
    pService->start();
    Serial.println("Service started");

    BLEService *pService_pH = pServer->createService(SERVICE_UUID_PH);
    Serial.println("pH Service created");
    pCharacteristic_pH = pService_pH->createCharacteristic(
                            CHARACTERISTIC_UUID_PH, 
                            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                         );
    Serial.println("pH Characteristic created");
    pCharacteristic_pH->addDescriptor(new BLE2902());
    Serial.println("pH Characteristic descriptor added");
    pCharacteristic_pH->setValue(pH_fileData.c_str());
    pService_pH->start();
    Serial.println("pH Service created");

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->addServiceUUID(SERVICE_UUID_PH);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // For compatibility
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("BLE server is ready and advertising...");

    // Configure linear actuator pins
    pinMode(lin_actuate, OUTPUT);
    pinMode(lin_deactuate, OUTPUT);

    // Initialize actuator to a safe state
    digitalWrite(lin_actuate, LOW);
    digitalWrite(lin_deactuate, LOW);
}

void loop() {
    unsigned long currentTime = millis();

    // **Actuator Control Task**
    // This block toggles the linear actuator state every "duration" milliseconds
    if (currentTime - lastActuationTime >= duration) {
        lastActuationTime = currentTime;

        if (actuatorState) {
            // De-actuate
            digitalWrite(lin_actuate, LOW);
            digitalWrite(lin_deactuate, HIGH);
            Serial.println("Linear actuator de-actuated");
        } else {
            // Actuate
            digitalWrite(lin_actuate, HIGH);
            digitalWrite(lin_deactuate, LOW);
            Serial.println("Linear actuator actuated");
        }

        // Toggle actuator state
        actuatorState = !actuatorState;
    }

    // **Data Logging Task**
    // This block logs analog and pH data every "logInterval" milliseconds
    if (currentTime - lastLogTime >= logInterval) {
        lastLogTime = currentTime;

        // Read and log analog data
        int analogValue = analogRead(readPin);
        Serial.print("\nAnalog Value: ");
        Serial.println(analogValue);

        byte valueToStore = (byte)((analogValue / ADC_RESOLUTION) * 255.0);
        File file = SPIFFS.open("/analog_data.txt", FILE_APPEND);
        if (file) {
            file.println(valueToStore);
            file.close();
            Serial.print("Stored Value in SPIFFS: ");
            Serial.println(valueToStore);
        } else {
            Serial.println("Failed to open file for writing");
        }

        // Read and log pH data
        int pHValueRaw = analogRead(pHReadPin);
        float pHVoltage = (pHValueRaw / ADC_RESOLUTION) * VOLTAGE_REF;
        float pHActual = (atlas_coeff * pHVoltage) + atlas_const;
        Serial.print("pH Actual Value: ");
        Serial.println(pHActual, 3);
        String pHString = String(pHActual, 3);

        File filePH = SPIFFS.open("/pH_data.txt", FILE_APPEND);
        if (filePH) {
            filePH.println(String(pHActual, 3));  // Store the float as a string with 3 decimal places
            filePH.close();
            Serial.print("Stored pH Value in SPIFFS: ");
            Serial.println(String(pHActual, 3));
        } else {
            Serial.println("Failed to open file for writing pH data");
        }

        // pCharacteristic_pH->setValue(pHString.c_str());
        // pCharacteristic_pH->notify();
        // Serial.print("Notified pH Value: ");
        // Serial.println(pHString);
    }
}
