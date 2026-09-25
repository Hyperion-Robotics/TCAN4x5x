# TCAN4x5x CAN-FD Library for Arduino

A robust, hardware-agnostic C++ library for the Texas Instruments **TCAN4550** and **TCAN4551** CAN-FD controllers. 

This library takes Texas Instruments' low-level ANSI-C driver and wraps it into a modern, flexible Arduino interface. It is built specifically for bare-metal performance, bypassing slow polling loops in favor of true hardware Interrupt Service Routines (ISRs) and advanced internal hardware routing.

## Key Features
* **True Hardware ISRs:** Utilizes a direct-return architecture (`TCAN_RX_Packet`) allowing you to write your own non-blocking `attachInterrupt` routines without rigid static wrappers.
* **Advanced Hardware Routing:** Natively map High Priority Messages (HPM) to physically trigger independent hardware pins like `GPO1` or `GPO2`.
* **Hardware-Agnostic SPI:** Works out-of-the-box with standard 8-bit AVRs, while fully supporting custom SPI pin multiplexing for modern 32-bit architectures (ESP32, RP2040, Teensy).
* **Payload Struct Packing:** Send and receive complex C++ data structs directly over the CAN-FD data phase.

## Hardware & Wiring

The TCAN4x5x uses a 20-pin VQFN package. The library relies on standard SPI communication alongside dedicated hardware interrupt pins.

### Pin Configuration
Below is the official pinout and description for the TCAN4x5x family:

![Pin Configuration](pin_table.png)

| TCAN Pin | Name | Description | Arduino Connection |
| :--- | :--- | :--- | :--- |
| **4** | SCLK | SPI Clock Input | Standard SCK |
| **5** | SDI | SPI Data Input | Standard MOSI |
| **6** | SDO | SPI Data Output | Standard MISO |
| **7** | nCS | SPI Chip Select | Any Digital Pin (Passed to constructor) |
| **8** | nINT | Main Interrupt | External Interrupt Pin (e.g., Pin 2) |
| **3** | GPO1 | Configurable Output | Optional: Secondary Interrupt Pin |
| **9** | GPO2 | Configurable Output | Optional: Secondary Interrupt Pin |

### Example Board Layout
For custom PCB designs, ensure traces for the 40 MHz crystal (Pins 1 and 20) are length-matched, and appropriate bypass capacitors are placed near the supply pins.

![Example Layout](pcb_excample.png)

## Installation

1. Download this repository as a `.zip` file.
2. Open the Arduino IDE.
3. Navigate to **Sketch** -> **Include Library** -> **Add .ZIP Library...**
4. Select the downloaded `.zip` file.

## Quick Start: Basic Receive

This minimal example demonstrates how to initialize the controller and read incoming standard CAN traffic using a hardware interrupt.

```cpp
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

    pinMode(NINT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(NINT_PIN), canISR, FALLING);

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
```

## Advanced Configuration

The library pre-loads safe Texas Instruments default settings (500k/2M bit rates, standard filters). Advanced users can easily override these by calling the configuration setters before calling begin().

```cpp
void setup() {
    // 1. Set custom bit rates (e.g., 1 Mbps Arbitration)
    myCAN.setNominalBitRate(1, 32, 8); 
    
    // 2. Configure internal routing (e.g., Route interrupts to GPO1)
    myCAN.setGPIO1Interrupt();

    // 3. Boot the chip to commit settings to hardware
    myCAN.begin(&SPI);
    
    // 4. Set dynamic filters (e.g., Route specific IDs to trigger GPO1)
    uint16_t priorityIDs[] = {0x055, 0x123};
    myCAN.setGPIO1CANIDList(priorityIDs, 2);
}
```

## License
This library is released under the BSD 3-Clause License. It is built upon the foundational register maps and ANSI-C hardware abstraction layer originally developed and copyrighted by Texas Instruments Incorporated.
