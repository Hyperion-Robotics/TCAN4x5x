/*
 * TCAN4x5x Hardware Status & Self-Diagnostic Example
 * 
 * Performs a 5-point hardware verification of the TCAN4x5x controller without 
 * requiring an active CAN bus or a second transceiver node.
 * 
 * Tests Performed:
 * 1. SPI Bus Communication & Device Revision Read
 * 2. High-Speed Crystal Oscillator & Normal Mode Transition
 * 3. Power Supply Rail (VSUP/VIO) & Hardware Fault Register Check
 * 4. Internal 2KB MRAM Read/Write Verification
 * 5. Bosch M_CAN Internal Loopback (TX Buffer -> RX FIFO 0 & nINT Pin Check)
 */

#include <Arduino.h>
#include <SPI.h>
#include <TCAN4x5x.h>

// Define SPI and Interrupt pins (Adjust for your target microcontroller)
const uint8_t CS_PIN   = 21;
const uint8_t MISO_PIN = 19;
const uint8_t MOSI_PIN = 17;
const uint8_t SCK_PIN  = 18;
const uint8_t NINT_PIN = 4; 

TCAN4x5x myCAN(CS_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);

volatile bool isrTriggered = false;

#if defined(ESP32)
void IRAM_ATTR diagnosticISR() {
    isrTriggered = true;
}
#else
void diagnosticISR() {
    isrTriggered = true;
}
#endif

// Helper function to write a 32-bit register directly over SPI for loopback testing
void writeTCANReg(uint16_t addr, uint32_t data) {
    SPI.beginTransaction(SPI_DEFAULT_SETTINGS);
    digitalWrite(CS_PIN, LOW);
    SPI.transfer(AHB_WRITE_OPCODE);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);
    SPI.transfer(1);
    SPI.transfer((data >> 24) & 0xFF);
    SPI.transfer((data >> 16) & 0xFF);
    SPI.transfer((data >> 8) & 0xFF);
    SPI.transfer(data & 0xFF);
    digitalWrite(CS_PIN, HIGH);
    SPI.endTransaction();
}

// Configures the Bosch M_CAN core for silent internal loopback testing
void setInternalLoopback(bool enable) {
    TCAN4x5x_MCAN_CCCR_Config cccr = {0};
    cccr.FDOE = 1; // Enable CAN-FD
    cccr.BRSE = 1; // Enable Bit Rate Switching
    
    myCAN.MCAN_EnableProtectedRegisters();

    if (enable) {
        cccr.TEST = 1; // Unlock REG_MCAN_TEST (0x1010)
        cccr.MON  = 1; // Enable Bus Monitoring Mode (Silent Loopback)
        myCAN.MCAN_ConfigureCCCRRegister(&cccr);
        writeTCANReg(REG_MCAN_TEST, REG_BITS_MCAN_TEST_LOOP_BACK);
    } else {
        writeTCANReg(REG_MCAN_TEST, 0x00000000);
        myCAN.MCAN_ConfigureCCCRRegister(&cccr);
    }

    myCAN.MCAN_DisableProtectedRegisters();
    myCAN.Device_SetMode(TCAN4x5x_DEVICE_MODE_NORMAL);
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);

    Serial.println("\n=============================================");
    Serial.println("      TCAN4x5x HARDWARE SELF-DIAGNOSTIC      ");
    Serial.println("=============================================\n");

    // Configure external interrupt pin
    pinMode(NINT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(NINT_PIN), diagnosticISR, FALLING);

    // Initialize SPI bus and TCAN4x5x controller
    // Automatically configures custom SPI pins and starts the SPI bus!
    myCAN.begin(&SPI);

    // Clear any latched power-on or undervoltage flags and transition to Normal Mode
    myCAN.Device_ClearInterruptsAll();
    myCAN.Device_SetMode(TCAN4x5x_DEVICE_MODE_NORMAL);

    // -------------------------------------------------------------------------
    // TEST 1: SPI Bus & Silicon Revision
    // -------------------------------------------------------------------------
    uint16_t version = myCAN.Device_ReadDeviceVersion();
    Serial.print("[TEST 1] SPI Revision Read (0x0008): 0x");
    Serial.print(version, HEX);
    if (version != 0x0000 && version != 0xFFFF) {
        Serial.println(" -> PASS");
    } else {
        Serial.println(" -> FAIL (Check SPI wiring and VIO/VSUP power)");
        while (1);
    }

    // -------------------------------------------------------------------------
    // TEST 2: Crystal Oscillator & Operating Mode Transition
    // -------------------------------------------------------------------------
    TCAN4x5x_Device_Mode_Enum mode = myCAN.Device_ReadMode();
    Serial.print("[TEST 2] Crystal & Normal Mode Transition: ");
    if (mode == TCAN4x5x_DEVICE_MODE_NORMAL) {
        Serial.println("NORMAL MODE -> PASS");
    } else {
        Serial.println("STUCK IN STANDBY -> FAIL (Check VSUP >= 5.5V and Crystal)");
    }

    // -------------------------------------------------------------------------
    // TEST 3: Power Supply & Thermal Fault Register Check
    // -------------------------------------------------------------------------
    TCAN4x5x_Device_Interrupts dev_ir = {0};
    myCAN.Device_ReadInterrupts(&dev_ir);
    Serial.print("[TEST 3] Hardware Fault Register Check: ");
    if (!dev_ir.UVSUP && !dev_ir.UVIO && !dev_ir.TSD && !dev_ir.SPIERR && !dev_ir.ECCERR) {
        Serial.println("0 Faults -> PASS");
    } else {
        Serial.print("FAULT DETECTED (Register: 0x");
        Serial.print(dev_ir.word, HEX);
        Serial.println(") -> FAIL (Check VSUP/VIO power rails)");
    }

    // -------------------------------------------------------------------------
    // TEST 4: MRAM Read/Write Integrity (2KB Internal Memory)
    // -------------------------------------------------------------------------
    Serial.print("[TEST 4] MRAM Filter Write & Readback: ");
    TCAN4x5x_MCAN_SID_Filter writeFilter = {0};
    writeFilter.SFT = TCAN4x5x_SID_SFT_CLASSIC;
    writeFilter.SFEC = TCAN4x5x_SID_SFEC_STORERX0;
    writeFilter.SFID1 = 0x123; 
    writeFilter.SFID2 = 0x7FF;

    myCAN.MCAN_EnableProtectedRegisters();
    myCAN.MCAN_WriteSIDFilter(0, &writeFilter);
    myCAN.MCAN_DisableProtectedRegisters();

    TCAN4x5x_MCAN_SID_Filter readFilter = {0};
    myCAN.MCAN_ReadSIDFilter(0, &readFilter);

    if (readFilter.SFID1 == 0x123 && readFilter.SFID2 == 0x7FF) {
        Serial.println("Matched 0x123 -> PASS");
    } else {
        Serial.println("Mismatch -> FAIL");
    }

    // -------------------------------------------------------------------------
    // TEST 5: Internal Controller Loopback (TX -> MCAN Core -> RX FIFO 0)
    // -------------------------------------------------------------------------
    Serial.print("[TEST 5] CAN-FD Internal Loopback (TX to RX): ");
    
    setInternalLoopback(true);

    TCAN4x5x_MCAN_TX_Header txHeader = {0};
    txHeader.ID  = 0x123;
    txHeader.DLC = MCAN_DLC_8B;
    txHeader.FDF = 1; // CAN-FD frame
    txHeader.BRS = 1; // Bit Rate Switch enabled
    uint8_t txData[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0x20, 0x26};

    isrTriggered = false;
    myCAN.MCAN_WriteTXBuffer(0, &txHeader, txData);
    myCAN.MCAN_TransmitBufferContents(0);

    delay(50);

    TCAN_RX_Packet rxPacket = myCAN.handleReceiveISR();

    if (rxPacket.valid && rxPacket.header.ID == 0x123 && rxPacket.data[0] == 0xDE && rxPacket.data[7] == 0x26) {
        Serial.println("PASS");
        Serial.print("         -> Loopback ID: 0x");
        Serial.print(rxPacket.header.ID, HEX);
        Serial.print(" | DLC Bytes: ");
        Serial.print(rxPacket.length);
        Serial.print(" | Payload: ");
        for (int i = 0; i < rxPacket.length; i++) {
            Serial.print("0x");
            Serial.print(rxPacket.data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.print("         -> Hardware Interrupt Triggered: ");
        Serial.println(isrTriggered ? "YES" : "NO (Check nINT pin connection)");
    } else {
        Serial.println("FAIL (Loopback packet did not arrive in RX FIFO 0)");
    }

    // Restore normal controller operation
    setInternalLoopback(false);

    Serial.println("\n=============================================");
    Serial.println("             DIAGNOSTIC COMPLETE             ");
    Serial.println("=============================================");
}

void loop() {
    delay(5000);
}