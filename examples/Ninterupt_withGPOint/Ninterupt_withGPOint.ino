#include <TCAN4x5x.h>
#include <SPI.h>

const uint8_t CS_PIN = 10;
const uint8_t NINT_PIN = 2;   // Default interrupt pin (Standard Messages)
const uint8_t GPIO1_PIN = 3;  // Priority interrupt pin (Priority Messages)

TCAN4x5x myCAN(CS_PIN);

uint16_t priorityIDs[] = {0x123, 0x456};
volatile bool standardHit = false;
volatile bool priorityHit = false;
TCAN_RX_Packet standardPacket;
TCAN_RX_Packet priorityPacket;

// 1. ISR for Standard Messages (nINT)
void standardISR() {
    TCAN_RX_Packet temp = myCAN.handleReceiveISR(); // Reads FIFO 0
    if (temp.valid) {
        standardPacket = temp;
        standardHit = true;
    }
}

// 2. ISR for Priority Messages (GPIO1)
void priorityISR() {
    // handleGPO2ISR() checks the HPM hardware flag, which is what 
    // setGPIO1CANIDList relies on, making it the correct handler here
    TCAN_RX_Packet temp = myCAN.handleGPOISR(); 
    if (temp.valid) {
        priorityPacket = temp;
        priorityHit = true;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // 3. Configure routing for GPIO1 priority
    myCAN.setGPIO1Interrupt(); // Configures hardware pin

    // 4. Attach MCU interrupts to BOTH pins
    myCAN.attachInterruptPin(NINT_PIN, standardISR); // Standard Messages
    myCAN.attachInterruptPin(GPIO1_PIN, priorityISR); // Priority Messages

    myCAN.begin(&SPI);
    
    // 5. Apply the priority list (routes HPM to GPIO1)
    myCAN.setPriorityCANIDList(priorityIDs, numPriorityIDs, INT_1);// INT_1 is the default for GPIO1 routing 
}

void loop() {
    if (standardHit) {
        standardHit = false;
        Serial.print("Standard ID: 0x");
        Serial.println(standardPacket.header.ID, HEX);
    }
    
    if (priorityHit) {
        priorityHit = false;
        Serial.print("PRIORITY ID: 0x");
        Serial.println(priorityPacket.header.ID, HEX);
    }
}