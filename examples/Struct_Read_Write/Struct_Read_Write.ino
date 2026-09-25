/*
 * TCAN4x5x Custom SPI Pins & Transmit Example
 * 
 * Demonstrates how to utilize the constructor overload to define custom 
 * hardware SPI pins (e.g., for ESP32/RP2040) and transmit CAN-FD packets safely.
 */

#include <TCAN4x5x.h>
#include <SPI.h>

// Define custom SPI routing
const uint8_t CS_PIN   = 5;
const uint8_t MISO_PIN = 19;
const uint8_t MOSI_PIN = 23;
const uint8_t SCK_PIN  = 18;

// Instantiate utilizing the advanced constructor
TCAN4x5x myCAN(CS_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);

unsigned long lastTxTime = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    
    // The library automatically applies the custom pins to the hardware bus 
    // on supported architectures during begin()
    myCAN.begin(&SPI);
    
    // Verify custom SPI pins are working by requesting device ID
    uint16_t version = myCAN.Device_ReadDeviceVersion();
    if (version == 0 || version == 0xFFFF) {
        Serial.println("SPI Connection FAILED. Check wiring.");
        while(1); // Halt execution
    }
    
    Serial.println("TCAN Controller Initialized on Custom SPI Pins.");
}

void loop() {
    if (millis() - lastTxTime >= 1000) {
        lastTxTime = millis();

        TCAN4x5x_MCAN_TX_Header txHeader = {0};       
        
        uint8_t txData[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}; 

        txHeader.DLC = MCAN_DLC_8B; // 8 byte payload
        txHeader.ID = 0x144;        // Standard 11-bit ID
        txHeader.FDF = 1;           // CAN-FD frame format enabled
        txHeader.BRS = 1;           // Bit Rate Switching enabled (e.g., jump to 2Mbps)
        txHeader.XTD = 0;           // Standard ID (Not extended)
        
        myCAN.MCAN_WriteTXBuffer(0, &txHeader, txData);
        bool success = myCAN.MCAN_TransmitBufferContents(0);

        if(success) {
            Serial.println("CAN-FD Packet queued for transmission.");
        } else {
            Serial.println("TX Buffer out of range error.");
        }
    }
}