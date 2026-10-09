/*
 * TCAN4x5x High Priority GPIO1 Example
 * 
 * Demonstrates how to route specific CAN IDs so they trigger an independent 
 * hardware interrupt on the GPIO1 pin using the High Priority Message (HPM) system.
 */

#include <TCAN4x5x.h>
#include <SPI.h>

const uint8_t CS_PIN = 10;
const uint8_t GPIO1_PIN = 3; // Connect this to the TCAN GPIO1 pin

TCAN4x5x myCAN(CS_PIN);

uint16_t priorityIDs[] = {0x055, 0x123, 0x7FF};
const uint8_t numPriorityIDs = 3;

volatile bool priorityHit = false;
TCAN_RX_Packet priorityPacket;

// Independent Hardware Interrupt for Priority Messages attached strictly to GPIO1
void priorityISR() {
    // Extract data using the GPO2/HPM handler
    TCAN_RX_Packet tempPacket = myCAN.handleGPOISR();
    if (tempPacket.valid) {
        priorityPacket = tempPacket;
        priorityHit = true;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Configure the internal chip routing to map MCAN_INT1 to GPIO1
    myCAN.setGPIO1Interrupt();

    // Attach to the correct GPIO1 pin, NOT the nINT pin
    myCAN.attachInterruptPin(GPIO1_PIN, priorityISR);
    
    // Initialize the CAN controller
    myCAN.begin(&SPI);
    
    // Apply the priority list to the hardware filters (Must be done after begin)
    myCAN.setPriorityCANIDList(priorityIDs, numPriorityIDs, INT_1);// INT_1 is the default for GPIO1 routing
    
    Serial.println("CAN Controller configured for GPIO1 priority filtering.");
}

void loop() {
    if (priorityHit) {
        priorityHit = false;
        Serial.print("URGENT Priority ID Received: 0x");
        Serial.println(priorityPacket.header.ID, HEX);
    }
}