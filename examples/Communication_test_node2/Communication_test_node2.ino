#include <Arduino.h>
#include <SPI.h>
#include <TCAN4x5x.h>

const uint8_t CS_PIN   = 21;
const uint8_t MISO_PIN = 19;
const uint8_t MOSI_PIN = 17;
const uint8_t SCK_PIN  = 18;

const uint8_t NINT_PIN = 4;
const uint8_t GPO2_PIN = 5; 

TCAN4x5x myCAN(CS_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);

volatile bool standardHit = false;
volatile bool priorityHit = false;
TCAN_RX_Packet standardRx;
TCAN_RX_Packet priorityRx;

void IRAM_ATTR standardISR() {
    TCAN_RX_Packet temp = myCAN.handleReceiveISR(); 
    if (temp.valid) {
        standardRx = temp;
        standardHit = true;
    }
}

void IRAM_ATTR priorityISR() {
    TCAN4x5x_MCAN_Interrupts ir = {0};
    myCAN.MCAN_ReadInterrupts(&ir);
    
    // Check and clear the RX FIFO 1 New Message (RF1N) flag
    if (ir.word & (1UL << 4)) {
        ir.word = (1UL << 4); 
        myCAN.MCAN_ClearInterrupts(&ir);
        
        priorityRx.length = myCAN.MCAN_ReadNextFIFO(RXFIFO1, &priorityRx.header, priorityRx.data);
        priorityRx.valid = true;
        priorityHit = true;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    myCAN.attachInterruptPin(NINT_PIN, standardISR);
    myCAN.attachInterruptPin(GPO2_PIN, priorityISR);

    // Expand MRAM allocation for 6 custom filters and both FIFOs
    TCAN4x5x_MRAM_Config customMRAM = {0};
    customMRAM.SIDNumElements = 6;  
    customMRAM.Rx0NumElements = 5;  
    customMRAM.Rx0ElementSize = MRAM_64_Byte_Data;  
    customMRAM.Rx1NumElements = 5;  
    customMRAM.Rx1ElementSize = MRAM_64_Byte_Data;  
    customMRAM.TxBufferNumElements = 2;
    customMRAM.TxBufferElementSize = MRAM_64_Byte_Data;
    myCAN.setMRAMConfig(&customMRAM);

    myCAN.begin(&SPI);
    
    // Hardware Blacklist (Instantly dropped, MCU never interrupted)
    myCAN.MCAN_AddBlacklistID(0x666, 0); 
    myCAN.MCAN_AddBlacklistID(0x777, 1); 
    
    // Hardware Priority Routing (Sent exclusively to GPO2 / RX FIFO 1)
    myCAN.MCAN_RoutePriorityID(0x101, 2); 
    myCAN.MCAN_RoutePriorityID(0x102, 3); 
    myCAN.MCAN_RoutePriorityID(0x103, 4); 
    myCAN.MCAN_RoutePriorityID(0x104, 5); 
    
    Serial.println("ESP32 Advanced Node Ready.");
}

void loop() {
    static unsigned long lastSend = 0;
    static uint8_t idx = 0;
    
    uint16_t txIDs[10] = {0x201, 0x101, 0x666, 0x202, 0x102, 0x777, 0x203, 0x103, 0x204, 0x104};

    if (millis() - lastSend > 1000) {
        lastSend = millis();
        
        TCAN4x5x_MCAN_TX_Header txHdr = {0};
        txHdr.DLC = 4; // Exactly 4 bytes (No padding)
        txHdr.FDF = 1; // CAN-FD format
        txHdr.BRS = 1; // Bit-rate switching enabled
        txHdr.ID = txIDs[idx];

        uint8_t payload[4] = {0xA0, 0xB0, 0xC0, idx};
        myCAN.MCAN_WriteTXBuffer(0, &txHdr, payload);
        myCAN.MCAN_TransmitBufferContents(0);
        
        Serial.print(">>> Transmitted ID: 0x");
        Serial.println(txIDs[idx], HEX);

        idx = (idx + 1) % 10; 
    }

    if (standardHit) {
        standardHit = false;
        Serial.print("    [nINT - FIFO0] Standard ID: 0x");
        Serial.print(standardRx.header.ID, HEX);
        Serial.print(" | Data: ");
        for (int i = 0; i < standardRx.length; i++) Serial.print(standardRx.data[i], HEX);
        Serial.println();
    }

    if (priorityHit) {
        priorityHit = false;
        Serial.print("    [GPO2 - FIFO1] Priority ID: 0x");
        Serial.print(priorityRx.header.ID, HEX);
        Serial.print(" | Data: ");
        for (int i = 0; i < priorityRx.length; i++) Serial.print(priorityRx.data[i], HEX);
        Serial.println();
    }
}