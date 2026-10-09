/*
 * TCAN4x5x Struct Read & Write Example
 * 
 * Demonstrates how to pack a C++ struct containing a character array 
 * into a CAN-FD packet using an Extended CAN ID, safely unpacking it on receive.
 */

#include <TCAN4x5x.h>
#include <SPI.h>
    
const uint8_t CS_PIN = 10;
const uint8_t NINT_PIN = 2; 

TCAN4x5x myCAN(CS_PIN);

unsigned long lastTransmitTime = 0;
const unsigned long transmitInterval = 1000; 

volatile bool structReady = false;
TCAN_RX_Packet structPacket;

// __attribute__((packed)) ensures the compiler doesn't add hidden padding bytes
struct __attribute__((packed)) CustomPayload {
    char message[12]; 
};

void canISR() {
    TCAN_RX_Packet tempPacket = myCAN.handleReceiveISR();
    if (tempPacket.valid) {
        structPacket = tempPacket;
        structReady = true;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Attach the ISR to the correct pin (nINT)
    myCAN.attachInterruptPin(NINT_PIN, canISR);

    Serial.println("Initializing TCAN4x5x...");
    myCAN.begin(&SPI);
}

void loop() {
    // --- TX LOGIC ---
    if (millis() - lastTransmitTime >= transmitInterval) {
        lastTransmitTime = millis();

        TCAN4x5x_MCAN_TX_Header txHeader = {0};       
        CustomPayload txData;
        strcpy(txData.message, "helloCANFD"); 

        txHeader.DLC = MCAN_DLC_12B; // Next valid CAN-FD size up from 11 bytes
        txHeader.ID = 0x1500;        // Extended ID
        txHeader.XTD = 1;            // MUST be 1 for IDs larger than 0x7FF
        txHeader.FDF = 1;            // CAN-FD frame format enabled
        txHeader.BRS = 1;            // Bit Rate Switching enabled
        
        // Cast the struct address to a (uint8_t*) pointer so the library can read its raw bytes
        myCAN.MCAN_WriteTXBuffer(0, &txHeader, (uint8_t*)&txData);
        myCAN.MCAN_TransmitBufferContents(0);

        Serial.println("TX - Sent 'helloCANFD' on Extended ID 0x1500");
    }

    // --- RX LOGIC ---
    if (structReady) {
        structReady = false;
        
        Serial.print("RX - ID: 0x");
        Serial.print(structPacket.header.ID, HEX);
        
        if (structPacket.header.XTD == 1 && structPacket.header.ID == 0x1500) {
            // Cast the raw byte array back into our custom struct
            CustomPayload* receivedData = (CustomPayload*)structPacket.data;
            Serial.print(" | Message: ");
            Serial.println(receivedData->message);
        } else {
            Serial.println(" | (Other message received)");
        }
    }
}