/*
 * TCAN4x5x Robust Receive Example
 * 
 * Demonstrates safe hardware interrupt handling by keeping the ISR incredibly 
 * short and deferring all Serial printing to the main loop.
 */

#include <TCAN4x5x.h>
#include <SPI.h>

const uint8_t CS_PIN = 10;
const uint8_t NINT_PIN = 2;

TCAN4x5x myCAN(CS_PIN);

// Volatile variables for safe ISR-to-Loop data transfer
volatile bool packetReady = false;
TCAN_RX_Packet rxPacket; 

// True Hardware Interrupt Service Routine
void canISR() {
    // Extract the bundled data packet directly from the library
    TCAN_RX_Packet tempPacket = myCAN.handleReceiveISR();
    
    if (tempPacket.valid) {
        rxPacket = tempPacket; // Copy to global struct
        packetReady = true;    // Flag the main loop
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Attach the ISR to the correct pin (nINT)
    myCAN.attachInterruptPin(NINT_PIN, canISR);

    Serial.println("Starting CAN Controller...");
    myCAN.begin(&SPI);
    
    // Optional: Verify hardware communication
    uint16_t devVersion = myCAN.Device_ReadDeviceVersion();
    Serial.print("Device Version: 0x");
    Serial.println(devVersion, HEX);
}

void loop() {
    if (packetReady) {
        // Clear flag immediately to allow next interrupt
        packetReady = false; 

        Serial.print("Message Received! ID: 0x");
        Serial.print(rxPacket.header.ID, HEX);
        Serial.print(" | DLC Bytes: ");
        Serial.print(rxPacket.length);
        
        Serial.print(" | Data: ");
        for (int i = 0; i < rxPacket.length; i++) {
            Serial.print("0x");
            Serial.print(rxPacket.data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}