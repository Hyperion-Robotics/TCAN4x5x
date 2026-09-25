/*
 * TCAN4550.c
 * Description: This file contains TCAN4550 functions, and relies on the SPI abstraction functions
 * Additional Feature Sets of TCAN4550 vs TCAN4x5x:
 *  - Watchdog Timer Functions
 *
 *  Version: 1.2.2
 *  Date: 6/8/2019
 *
 *  Change Log
 *
 *  1.2.2(6/8/2020)
 *      - Corrected incorrect mask value for REG_BITS_DEVICE_IE_MASK (bit 31 is now 0)
 *  1.2.1 (9/19/2019)
 *      - Added a missing AHB_BURST_READ_END() to the MCAN_ReadXIDFilter function, which caused the next read or write to fail
 *
 *  1.2.0 (5/1/2019)
 *      - Added the MCAN_ConfigureGlobalFilter function for changing default packet behavior
 *	    - Added a define to allow caching of MCAN configuration registers to reduce the number of SPI reads
 *	    - Added a FIFO fill level checker to the MCAN_ReadNextFIFO method to exit if there is no new element to read
 *	    - Added a read function for SID and XID filters
 *	    - Added a SPIERR clear function
 *	    
 *  1.1.1 (6/12/2018)
 *      - Minor typo correction for the ConfigureNominalTiming_Simple() function
 *
 *  1.1 (6/6/2018)
 *      - Updates to code for readability, and consistency
 *      - Some function names updated for consistency
 *      - Added functionality and abstraction for interrupts
 *      - Bit fields updated for final silicon
 *
 *
 * Copyright (c) 2019 Texas Instruments Incorporated.  All rights reserved.
 * Software License Agreement
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of Texas Instruments Incorporated nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TCAN4x5x.h"


#ifdef MCAN_CACHE_CONFIGURATION
// If caching is enabled, define the necessary variables
uint32_t MCAN_CACHE[9];
#endif


// ---------------------------------------------------------------------------
// Constructors & Defaults
// ---------------------------------------------------------------------------
TCAN4x5x::TCAN4x5x(uint8_t csPin) {
    nCS = csPin;
    loadDefaultSettings();
}

TCAN4x5x::TCAN4x5x(uint8_t csPin, uint8_t MISO_Pin, uint8_t MOSI_Pin, uint8_t SCK_Pin) {
    nCS = csPin;
    MISO = MISO_Pin;
    MOSI = MOSI_Pin;
    SCK = SCK_Pin;
    loadDefaultSettings();
}

// ---> ADD THIS MISSING DESTRUCTOR <---
TCAN4x5x::~TCAN4x5x() {
    // Empty destructor to satisfy the C++ linker
}

// Pre-loads the exact safe defaults from the Texas Instruments main.c example
void TCAN4x5x::loadDefaultSettings(void) {
    // 500k arbitration with a 40 MHz crystal
    _nomTiming.NominalBitRatePrescaler = 2;
    _nomTiming.NominalTqBeforeSamplePoint = 32;
    _nomTiming.NominalTqAfterSamplePoint = 8;

    // 2 Mbps CAN FD with a 40 MHz crystal
    _dataTiming.DataBitRatePrescaler = 1;
    _dataTiming.DataTqBeforeSamplePoint = 15;
    _dataTiming.DataTqAfterSamplePoint = 5;

    // Enable FD Mode and Bit Rate Switching
    _cccrConfig.FDOE = 1;
    _cccrConfig.BRSE = 1;

    // Default Filter: Accept everything into RX FIFO 0
    _gfc.RRFE = 1; 
    _gfc.RRFS = 1;
    _gfc.ANFE = TCAN4x5x_GFC_ACCEPT_INTO_RXFIFO0;
    _gfc.ANFS = TCAN4x5x_GFC_ACCEPT_INTO_RXFIFO0;

    // Default MRAM (Memory) Allocation
    _mramConfig.SIDNumElements = 1;
    _mramConfig.XIDNumElements = 1;
    _mramConfig.Rx0NumElements = 5;
    _mramConfig.Rx0ElementSize = MRAM_64_Byte_Data;
    _mramConfig.TxBufferNumElements = 2;
    _mramConfig.TxBufferElementSize = MRAM_64_Byte_Data;

    // Default Device Config
    _devConfig.SWE_DIS = 0;
    _devConfig.GPIO1_GPO_CONFIG = TCAN4x5x_DEV_CONFIG_GPO1_MCAN_INT1;
    _devConfig.GPIO1_CONFIG = TCAN4x5x_DEV_CONFIG_GPIO1_CONFIG_GPO;
    _devConfig.WD_ACTION = TCAN4x5x_DEV_CONFIG_WDT_ACTION_nINT;
    _devConfig.CLK_REF = 1; // 40 MHz crystal
    _devConfig.WAKE_CONFIG = TCAN4x5x_DEV_CONFIG_WAKE_BOTH_EDGES;
    
    // Enable New Message Interrupt for RX FIFO 0
    _mcanIE.RF0NE = 1;
}

// ---------------------------------------------------------------------------
// Pre-begin() Configuration Setters (User customization)
// ---------------------------------------------------------------------------
void TCAN4x5x::setNominalBitRate(uint16_t prescaler, uint8_t tqBefore, uint8_t tqAfter) {
    _nomTiming.NominalBitRatePrescaler = prescaler;
    _nomTiming.NominalTqBeforeSamplePoint = tqBefore;
    _nomTiming.NominalTqAfterSamplePoint = tqAfter;
}

void TCAN4x5x::setDataBitRate(uint16_t prescaler, uint8_t tqBefore, uint8_t tqAfter) {
    _dataTiming.DataBitRatePrescaler = prescaler;
    _dataTiming.DataTqBeforeSamplePoint = tqBefore;
    _dataTiming.DataTqAfterSamplePoint = tqAfter;
}

void TCAN4x5x::setMRAMConfig(TCAN4x5x_MRAM_Config *config) {
    _mramConfig = *config; // Copy the user's struct into our internal state
}

void TCAN4x5x::setDeviceConfig(TCAN4x5x_DEV_CONFIG *config) {
    _devConfig = *config;
}

void TCAN4x5x::setGlobalFilterConfig(TCAN4x5x_MCAN_Global_Filter_Configuration *config) {
    _gfc = *config;
}

// ---------------------------------------------------------------------------
// The Library Begin Function
// ---------------------------------------------------------------------------
void TCAN4x5x::begin(SPIClass *spiPort, SPISettings spiSettings) {
    tcan_port = spiPort;
    tcan_port_settings = spiSettings;

    // 1. Initialize SPI Pins (Your existing multiplexer logic goes here)
    pinMode(nCS, OUTPUT);
    digitalWrite(nCS, HIGH);
    
    // [Insert your existing ESP32/RP2040/Teensy custom SPI pin logic here]
    // tcan_port->begin();

    // 2. Hardware initialization sequence using the stored structs
    Device_ClearSPIERR();

    // Clear and configure device interrupts
    TCAN4x5x_Device_Interrupt_Enable dev_ie = {0};
    Device_ConfigureInterruptEnable(&dev_ie);

    TCAN4x5x_Device_Interrupts dev_ir = {0};
    Device_ReadInterrupts(&dev_ir);
    if (dev_ir.PWRON) {
        Device_ClearInterrupts(&dev_ir);
    }

    // Enter protected mode and apply the internal structs to the hardware registers
    MCAN_EnableProtectedRegisters();
    MCAN_ConfigureCCCRRegister(&_cccrConfig);
    MCAN_ConfigureGlobalFilter(&_gfc);
    MCAN_ConfigureNominalTiming_Simple(&_nomTiming);
    MCAN_ConfigureDataTiming_Simple(&_dataTiming);
    
    MRAM_Clear();
    MRAM_Configure(&_mramConfig);
    
    MCAN_DisableProtectedRegisters();

    // Apply the interrupt routing and device behaviors
    MCAN_ConfigureInterruptEnable(&_mcanIE);
    Device_Configure(&_devConfig);

    // Boot the transceiver!
    Device_SetMode(TCAN4x5x_DEVICE_MODE_NORMAL);
    MCAN_ClearInterruptsAll();
}

void TCAN4x5x::setSPISettings(SPISettings spiSettings){
    tcan_port_settings = spiSettings;
}

void TCAN4x5x::setCS(uint8_t csPin){
    //make th old pin floating again
    pinMode(nCS, INPUT);

    nCS = csPin;
    pinMode(nCS, OUTPUT);
    digitalWrite(nCS, HIGH);
}

void TCAN4x5x::setSPIPins(uint8_t MISO_Pin, uint8_t MOSI_Pin, uint8_t SCK_Pin){
    MISO = MISO_Pin;
    MOSI = MOSI_Pin;
    SCK = SCK_Pin;

    tcan_port->begin(SCK, MISO, MOSI, nCS);
}

void TCAN4x5x::resetSPISettings(void){
    tcan_port_settings = SPI_DEFAULT_SETTINGS;
}

// ---------------------------------------------------------------------------
// Hardware Pin Interrupt Routing
// ---------------------------------------------------------------------------

void TCAN4x5x::setGPO2Interrupt(void) {
    // Read the Modes of Operation and Pin Configuration register (0x0800)
    uint32_t reg0800 = AHB_READ_32(REG_DEV_MODES_AND_PINS);

    // Clear GPO2_CONFIG bits (23:22) and set to 01 to output M_CAN_INT 0
    reg0800 &= ~(3UL << 22);
    reg0800 |=  (1UL << 22);

    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, reg0800);
}

void TCAN4x5x::setGPIO1Interrupt(void) {
    // Read the Modes of Operation and Pin Configuration register (0x0800)
    uint32_t reg0800 = AHB_READ_32(REG_DEV_MODES_AND_PINS);

    // Clear GPIO1_CONFIG bits (15:14) to 00 to configure as GPO
    reg0800 &= ~(3UL << 14); 
    
    // Clear GPIO1_GPO_CONFIG bits (11:10) and set to 01 to output M_CAN_INT 1
    reg0800 &= ~(3UL << 10);
    reg0800 |=  (1UL << 10); 

    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, reg0800);
}

// ---------------------------------------------------------------------------
// High Priority Message Filter List Generator
// ---------------------------------------------------------------------------

void TCAN4x5x::setGPO2CANIDList(uint16_t idList[], uint8_t listSize) {
    
    // 1. Map the High Priority Message (HPM) interrupt to M_CAN_INT 0
    // Read the Interrupt Line Select register (0x1058)
    uint32_t ils = AHB_READ_32(REG_MCAN_ILS);
    ils &= ~(1UL << 8); // Clear bit 8 (HPML) to route HPM to m_can_int0
    AHB_WRITE_32(REG_MCAN_ILS, ils);

    // 2. Enable the High Priority Message (HPM) interrupt
    // Read the Interrupt Enable register (0x1054)
    uint32_t ie = AHB_READ_32(REG_MCAN_IE);
    ie |= (1UL << 8); // Set bit 8 (HPME) to enable the HPM interrupt
    AHB_WRITE_32(REG_MCAN_IE, ie);

    // 3. Enable the physical Interrupt Line 0
    // Read the Interrupt Line Enable register (0x105C)
    uint32_t ile = AHB_READ_32(REG_MCAN_ILE);
    ile |= (1UL << 0); // Set bit 0 (EINT0) to enable m_can_int0
    AHB_WRITE_32(REG_MCAN_ILE, ile);

    // 4. Unlock protected registers to allow writing to the MRAM filter list
    MCAN_EnableProtectedRegisters();

    // 5. Iterate through the array and write each CAN ID to the MRAM filter list
    for (uint8_t i = 0; i < listSize; i++) {
        TCAN4x5x_MCAN_SID_Filter filter = {0};
        
        // Configure as a classic filter matching an exact ID
        filter.SFT = TCAN4x5x_SID_SFT_CLASSIC;
        
        // Mark this specific filter element as a Priority Message 
        // and automatically store the payload in RX FIFO 0
        filter.SFEC = TCAN4x5x_SID_SFEC_PRIORITYSTORERX0; 
        
        filter.SFID1 = idList[i]; // The exact CAN ID to match
        filter.SFID2 = 0x7FF;     // 0x7FF mask ensures an exact 11-bit match

        // Write the configured filter into the MRAM at index 'i'
        MCAN_WriteSIDFilter(i, &filter);
    }

    // 6. Lock protected registers to resume normal operation
    MCAN_DisableProtectedRegisters();
}

void TCAN4x5x::setGPIO1CANIDList(uint16_t idList[], uint8_t listSize) {
    
    // 1. Map the High Priority Message (HPM) interrupt to M_CAN_INT 1 (for GPIO1)
    // Read the Interrupt Line Select register (0x1058)
    uint32_t ils = AHB_READ_32(REG_MCAN_ILS);
    ils |= (1UL << 8); // SET bit 8 (HPML) to route HPM to m_can_int1
    AHB_WRITE_32(REG_MCAN_ILS, ils);

    // 2. Enable the High Priority Message (HPM) interrupt
    // Read the Interrupt Enable register (0x1054)
    uint32_t ie = AHB_READ_32(REG_MCAN_IE);
    ie |= (1UL << 8); // Set bit 8 (HPME) to enable the HPM interrupt
    AHB_WRITE_32(REG_MCAN_IE, ie);

    // 3. Enable the physical Interrupt Line 1 (EINT1)
    // Read the Interrupt Line Enable register (0x105C)
    uint32_t ile = AHB_READ_32(REG_MCAN_ILE);
    ile |= (1UL << 1); // Set bit 1 (EINT1) to enable m_can_int1
    AHB_WRITE_32(REG_MCAN_ILE, ile);

    // 4. Unlock protected registers to allow writing to the MRAM filter list
    MCAN_EnableProtectedRegisters();

    // 5. Iterate through the array and write each CAN ID to the MRAM filter list
    for (uint8_t i = 0; i < listSize; i++) {
        TCAN4x5x_MCAN_SID_Filter filter = {0};
        
        // Configure as a classic filter matching an exact ID
        filter.SFT = TCAN4x5x_SID_SFT_CLASSIC;
        
        // Mark this specific filter element as a Priority Message 
        // and automatically store the payload in RX FIFO 0
        filter.SFEC = TCAN4x5x_SID_SFEC_PRIORITYSTORERX0; 
        
        filter.SFID1 = idList[i]; // The exact CAN ID to match
        filter.SFID2 = 0x7FF;     // 0x7FF mask ensures an exact 11-bit match

        // Write the configured filter into the MRAM at index 'i'
        MCAN_WriteSIDFilter(i, &filter);
    }

    // 6. Lock protected registers to resume normal operation
    MCAN_DisableProtectedRegisters();
}


// ---------------------------------------------------------------------------
// Direct ISR Handlers
// ---------------------------------------------------------------------------

TCAN_RX_Packet TCAN4x5x::handleReceiveISR(void) {
    TCAN_RX_Packet packet; // Automatically initializes valid to false
    TCAN4x5x_MCAN_Interrupts mcan_ir = {0};
    
    MCAN_ReadInterrupts(&mcan_ir); 

    if (mcan_ir.RF0N) {
        MCAN_ClearInterrupts(&mcan_ir);
        
        // Read the FIFO directly into the struct's header and data arrays
        packet.length = MCAN_ReadNextFIFO(RXFIFO0, &packet.header, packet.data);
        packet.valid = true; 
    }
    
    return packet;
}

TCAN_RX_Packet TCAN4x5x::handleGPOISR(void) {
    TCAN_RX_Packet packet;
    TCAN4x5x_MCAN_Interrupts mcan_ir = {0};
    
    MCAN_ReadInterrupts(&mcan_ir); 

    // Trigger on High Priority Message (HPM) or standard FIFO0 match
    if (mcan_ir.HPM || mcan_ir.RF0N) {
        MCAN_ClearInterrupts(&mcan_ir);
        packet.length = MCAN_ReadNextFIFO(RXFIFO0, &packet.header, packet.data);
        packet.valid = true;
    }
    
    return packet;
}
    
/**
 * @brief Handle GPIO1 Interrupt Service Routine
 *
 * This function is called when the GPIO1 interrupt is triggered.
 *
 * @return The device interrupts
 */
TCAN4x5x_Device_Interrupts TCAN4x5x::handleGPIO1ISR(void) {
    TCAN4x5x_Device_Interrupts dev_ir = {0};
    
    // Read the non-CAN device interrupts
    Device_ReadInterrupts(&dev_ir);
    
    // Clear them to release the physical hardware pin
    Device_ClearInterrupts(&dev_ir);

    return dev_ir; // Return the struct directly to the user
}


/**
 * @brief Enable Protected MCAN Registers
 *
 * Attempts to enable CCCR.CCE and CCCR.INIT to allow writes to protected registers, needed for MCAN configuration
 *
 * @return @c true if successfully enabled, otherwise return @c false
 */
bool TCAN4x5x::MCAN_EnableProtectedRegisters(void) {
    uint8_t i;
    uint32_t readValue, firstRead;

    firstRead = AHB_READ_32(REG_MCAN_CCCR);
    if ((firstRead & (REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT)) == (REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT))
        return true;

    // Unset the CSA and CSR bits since those will be set if we're in standby mode. Writing a 1 to these bits will force a clock stop event and prevent the return to normal mode
    firstRead &= ~(REG_BITS_MCAN_CCCR_CSA | REG_BITS_MCAN_CCCR_CSR | REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT);
    // Try up to 5 times to set the CCCR register, if not, then fail config, since we need these bits set to configure the device.
    for (i = 5; i > 0; i--)
    {
        AHB_WRITE_32(REG_MCAN_CCCR, firstRead | REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT);
        readValue = AHB_READ_32(REG_MCAN_CCCR);

        if ((readValue & (REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT)) == (REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT))
            return true;
        else if (i == 1)		// Ran out of tries, give up
            return false;
    }
    return true;
}


/**
 * @brief Disable Protected MCAN Registers
 *
 * Attempts to disable CCCR.CCE and CCCR.INIT to disallow writes to protected registers
 *
 * @return @c true if successfully enabled, otherwise return @c false
 */
bool TCAN4x5x::MCAN_DisableProtectedRegisters(void) {
    uint8_t i;
    uint32_t readValue;

    readValue = AHB_READ_32(REG_MCAN_CCCR);
    if ((readValue & REG_BITS_MCAN_CCCR_CCE) == 0)
        return true;

    // Try up to 5 times to unset the CCCR register, if not, then fail config, since we need these bits set to configure the device.
    for (i = 5; i > 0; i--)
    {
        AHB_WRITE_32(REG_MCAN_CCCR, (readValue & ~(REG_BITS_MCAN_CCCR_CSA | REG_BITS_MCAN_CCCR_CSR | REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT)));	// Unset these bits
        readValue = AHB_READ_32(REG_MCAN_CCCR);

        if ((readValue & REG_BITS_MCAN_CCCR_CCE) == 0)
            return true;
        else if (i == 1)
            return false;
    }
    return true;
}


/**
 * @brief Configure the MCAN CCCR Register
 *
 * Configures the bits of the CCCR register to match the CCCR config struct
 *
 * @warning This function writes to protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *cccrConfig is a pointer to a @c MCAN_CCCR_Config struct containing the configuration bits
 *
 * @return @c true if successfully enabled, otherwise return @c false
 */
bool TCAN4x5x::MCAN_ConfigureCCCRRegister(TCAN4x5x_MCAN_CCCR_Config *cccrConfig){
    uint32_t value, readValue;


    value = cccrConfig->word;
    value &= ~(REG_BITS_MCAN_CCCR_RESERVED_MASK | REG_BITS_MCAN_CCCR_CSA | REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT);		// Bitwise AND to get the valid bits (ignore reserved bits and the CCE and INIT)

    // If we made it here, we can update the value so that our protected write stays enabled
    value |= (REG_BITS_MCAN_CCCR_INIT | REG_BITS_MCAN_CCCR_CCE);


    AHB_WRITE_32(REG_MCAN_CCCR, value);

#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    readValue = AHB_READ_32(REG_MCAN_CCCR);

    // Need to do these bitwise ANDs to make this work for clock stop requests and not trigger a false failure when comparing read back value
    if ((readValue & ~(REG_BITS_MCAN_CCCR_RESERVED_MASK | REG_BITS_MCAN_CCCR_CSA | REG_BITS_MCAN_CCCR_CSR | REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT))
            != (value & ~(REG_BITS_MCAN_CCCR_RESERVED_MASK | REG_BITS_MCAN_CCCR_CSA | REG_BITS_MCAN_CCCR_CSR | REG_BITS_MCAN_CCCR_CCE | REG_BITS_MCAN_CCCR_INIT)))
    {
        // If our written value and read back value aren't the same, then we return a failure.
        return false;
    }

    // Check to see if the CSR bits are not as expected, since this can be set by the hardware.
    if ((readValue & REG_BITS_MCAN_CCCR_CSR) != cccrConfig->CSR)
    {
        // Then there's a difference in the CSR bits, which may not be a failure.
        if (Device_ReadMode() == DEVICE_MODE_STANDBY)
        {
            // CSR bit is set due to being in standby mode. Not a failure.
            return true;
        } else {
            // It's not matching for some other reason, we've got a real failure.
            return false;
        }
    }
#endif
    return true;
}


/**
 * @brief Read the MCAN CCCR configuration register
 *
 * Reads the MCAN CCCR configuration register and updates the passed @c TCAN4x5x_MCAN_CCCR_Config struct
 *
 * @param *cccrConfig is a pointer to a @c TCAN4x5x_MCAN_CCCR_Config struct containing the CCCR bit fields that will be updated
 */
void TCAN4x5x::MCAN_ReadCCCRRegister(TCAN4x5x_MCAN_CCCR_Config *cccrConfig) {
    cccrConfig->word = AHB_READ_32(REG_MCAN_CCCR);
}


/**
 * @brief Reads the MCAN data time settings, using the simple struct
 *
 * Reads the MCAN data timing registers and updates the @c *dataTiming struct
 *
 * @warning This function writes to protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *dataTiming is a pointer of a @c TCAN4x5x_MCAN_Data_Timing_Simple struct containing the simplified data timing information
 */
void TCAN4x5x::MCAN_ReadDataTimingFD_Simple(TCAN4x5x_MCAN_Data_Timing_Simple *dataTiming){
    uint32_t regData;

    // Read the data timing register
    regData = AHB_READ_32(REG_MCAN_DBTP);

    // These registers are only writable if CCE and INIT are both set. Sets the nominal bit timing and prescaler information
    dataTiming->DataBitRatePrescaler = ((regData >> 16) & 0x1F) + 1;
    dataTiming->DataTqBeforeSamplePoint = ((regData >> 8) & 0x1F) + 2;
    dataTiming->DataTqAfterSamplePoint = ((regData >> 4) & 0xF) + 1;
}


/**
 * @brief Reads the MCAN data time settings, using the raw MCAN struct
 *
 * Reads the MCAN data timing registers and updates the @c *dataTiming struct
 *
 * @warning This function reads from protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *dataTiming is a pointer of a @c TCAN4x5x_MCAN_Data_Timing_Raw struct containing the raw data timing information
 */
void TCAN4x5x::MCAN_ReadDataTimingFD_Raw(TCAN4x5x_MCAN_Data_Timing_Raw *dataTiming){
    uint32_t regData;

    // Read the data timing register
    regData = AHB_READ_32(REG_MCAN_DBTP);

    // These registers are only writable if CCE and INIT are both set. Sets the nominal bit timing and prescaler information
    dataTiming->DataBitRatePrescaler = ((regData >> 16) & 0x1F);
    dataTiming->DataTimeSeg1andProp = ((regData >> 8) & 0x1F);
    dataTiming->DataTimeSeg2 = ((regData >> 4) & 0xF);
    dataTiming->DataSyncJumpWidth = (regData & 0xF);

    if (regData & REG_BITS_MCAN_DBTP_TDC_EN)
    {
        // If TDC is set, then read the TDC register
        regData = AHB_READ_32(REG_MCAN_TDCR);
        dataTiming->TDCOffset = ((regData >> 8) & 0x7F);
        dataTiming->TDCFilter = (regData & 0x7F);
    } else {
        dataTiming->TDCOffset = 0;
        dataTiming->TDCFilter = 0;
    }
}


/**
 * @brief Writes the MCAN data time settings, using the simple data timing struct
 *
 * Writes the data timing information to MCAN using the input from the @c *dataTiming pointer
 *
 * @warning This function writes to protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *dataTiming is a pointer of a @c TCAN4x5x_MCAN_Data_Timing_Simple struct containing the simplified data timing information
 * @return @c true if successfully enabled, otherwise return @c false
 */
bool TCAN4x5x::MCAN_ConfigureDataTiming_Simple(TCAN4x5x_MCAN_Data_Timing_Simple *dataTiming) {
    uint32_t writeValue, TDCOWriteValue;
    uint32_t tempValue;

    // These registers are only writable if CCE and INIT are both set. Sets the nominal bit timing and prescaler information
    // Check to make sure prescaler is in range 1-32
    tempValue = dataTiming->DataBitRatePrescaler;
    if (tempValue > 32)
        tempValue = 32;
    else if (tempValue == 0)
        tempValue = 1;

    writeValue = ((uint32_t)(tempValue - 1)) << 16;		// Subtract 1 because MCAN expects 1 less than actual value

    // Check Tq before sample point is within valid range of 2-33
    tempValue = dataTiming->DataTqBeforeSamplePoint;
    if (tempValue > 33)
        tempValue = 33;
    else if (tempValue < 2)
        tempValue = 2;

    writeValue |= ((uint32_t)(tempValue - 2)) << 8;		// Subtract 2 for the Sync bit and because MCAN expects 1 less than actual
    TDCOWriteValue = (uint32_t)(tempValue - 1) << 8;	// Subtract 1 to make secondary sample point match primary. We take the sync bit out. See below note as to why
    // Check Tq after the sample point is within valid range of 1-16
    tempValue = dataTiming->DataTqAfterSamplePoint;
    if (tempValue > 16)
        tempValue = 16;
    else if (tempValue == 0)
        tempValue = 1;

    writeValue |= ((uint32_t)(tempValue - 1)) << 4;		// Subtract 1 because MCAN expects 1 less than actual value

    //Copy SJW from tq after sample point in most cases
    writeValue |= ((uint32_t)(tempValue - 1));			// Subtract 1 because MCAN expects 1 less than actual value

    // NOTE: In most cases, you want to enable Transceiver Delay Compensation Offset and set it to 1 more than what's in the DTSEG1 register in MCAN.
    // Doing this ensures that the secondary sample point is the same as the primary sample point
    writeValue |= REG_BITS_MCAN_DBTP_TDC_EN;
    AHB_WRITE_32(REG_MCAN_DBTP, writeValue);				// Write the value to the DBTP register

#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    tempValue = AHB_READ_32(REG_MCAN_DBTP);
    if (tempValue != writeValue)
        return false;
#endif

    AHB_WRITE_32(REG_MCAN_TDCR, TDCOWriteValue);
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    tempValue = AHB_READ_32(REG_MCAN_TDCR);
    if (tempValue != TDCOWriteValue)
        return false;
#endif

    // Configure the Timestamp counter to use an external time stamp value. This is required to use time stamps with CAN FD
    AHB_WRITE_32(REG_MCAN_TSCC, REG_BITS_MCAN_TSCC_COUNTER_EXTERNAL);
    return true;
}


/**
 * @brief Writes the MCAN data time settings, using the raw MCAN data timing struct
 *
 * Writes the data timing information to MCAN using the input from the @c *dataTiming pointer
 *
 * @warning This function writes to protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *dataTiming is a pointer of a @c TCAN4x5x_MCAN_Data_Timing_Raw struct containing the raw data timing information
 *
 * @return @c true if successfully enabled, otherwise return @c false
 */
bool TCAN4x5x::MCAN_ConfigureDataTiming_Raw(TCAN4x5x_MCAN_Data_Timing_Raw *dataTiming){
    uint32_t writeValue;
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    uint32_t tempValue;
#endif

    // These registers are only writable if CCE and INIT are both set. Sets the nominal bit timing and prescaler information
    writeValue = ((uint32_t)(dataTiming->DataBitRatePrescaler & 0x1F)) << 16;
    writeValue |= ((uint32_t)(dataTiming->DataTimeSeg1andProp & 0x1F)) << 8;
    writeValue |= ((uint32_t)(dataTiming->DataTimeSeg2 & 0x0F)) << 4;
    writeValue |= ((uint32_t)(dataTiming->DataSyncJumpWidth & 0x0F));
    if ((dataTiming->TDCOffset > 0) || (dataTiming->TDCFilter > 0))
    {
        // If either of these are set, then enable transmitter delay compensation
        writeValue |= REG_BITS_MCAN_DBTP_TDC_EN;
        AHB_WRITE_32(REG_MCAN_DBTP, writeValue);
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
        // Check to see if the write was successful.
        tempValue = AHB_READ_32(REG_MCAN_DBTP);
        if (tempValue != writeValue)
            return false;
#endif

        writeValue = (uint32_t)(dataTiming->TDCOffset & 0x7F) << 8;
        writeValue |= (uint32_t)(dataTiming->TDCFilter & 0x7F);
        AHB_WRITE_32(REG_MCAN_TDCR, writeValue);
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
        // Check to see if the write was successful.
        tempValue = AHB_READ_32(REG_MCAN_TDCR);
        if (tempValue != writeValue)
            return false;
#endif
    } else {
        AHB_WRITE_32(REG_MCAN_DBTP, writeValue);
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
        // Check to see if the write was successful.
        tempValue = AHB_READ_32(REG_MCAN_DBTP);
        if (tempValue != writeValue)
            return false;
#endif
    }

    // Configure the Timestamp counter to use an external time stamp value. This is required to use time stamps with CAN FD
    AHB_WRITE_32(REG_MCAN_TSCC, REG_BITS_MCAN_TSCC_COUNTER_EXTERNAL);

    return true;
}


/**
 * @brief Reads the MCAN nominal/arbitration time settings, using the simple timing struct
 *
 * Reads the MCAN nominal timing registers and updates the @c *nomTiming struct
 *
 * @warning This function reads from protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *nomTiming is a pointer of a @c TCAN4x5x_MCAN_Nominal_Timing_Simple struct containing the simplified nominal timing information
 */
void TCAN4x5x::MCAN_ReadNominalTiming_Simple(TCAN4x5x_MCAN_Nominal_Timing_Simple *nomTiming){
    uint32_t readValue;

    readValue = AHB_READ_32(REG_MCAN_NBTP);

    // These registers are only writable if CCE and INIT are both set. Sets the nominal bit timing and prescaler information
    nomTiming->NominalBitRatePrescaler = ((readValue >> 16) & 0x1FF) + 1;
    nomTiming->NominalTqBeforeSamplePoint = ((readValue >> 8) & 0xFF) + 2;
    nomTiming->NominalTqAfterSamplePoint = (readValue & 0x7F) + 1;
}


/**
 * @brief Reads the MCAN nominal/arbitration time settings, using the raw MCAN timing struct
 *
 * Reads the MCAN nominal timing registers and updates the @c *nomTiming struct
 *
 * @warning This function reads from protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *nomTiming is a pointer of a @c TCAN4x5x_MCAN_Nominal_Timing_Raw struct containing the raw MCAN nominal timing information
 */
void TCAN4x5x::MCAN_ReadNominalTiming_Raw(TCAN4x5x_MCAN_Nominal_Timing_Raw *nomTiming){
    uint32_t readValue;

    readValue = AHB_READ_32(REG_MCAN_NBTP);

    // These registers are only writable if CCE and INIT are both set. Sets the nominal bit timing and prescaler information
    nomTiming->NominalSyncJumpWidth = ((readValue >> 25) & 0x7F);
    nomTiming->NominalBitRatePrescaler = ((readValue >> 16) & 0x1FF);
    nomTiming->NominalTimeSeg1andProp = ((readValue >> 8) & 0xFF);
    nomTiming->NominalTimeSeg2 = (readValue & 0x7F);
}


/**
 * @brief Writes the MCAN nominal timing settings, using the simple nominal timing struct
 *
 * Writes the data timing information to MCAN using the input from the @c *nomTiming pointer
 *
 * @warning This function writes to protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *nomTiming is a pointer of a @c TCAN4x5x_MCAN_Nominal_Timing_Simple struct containing the simplified nominal timing information
 * @return @c true if successfully enabled, otherwise return @c false
 */
bool TCAN4x5x::MCAN_ConfigureNominalTiming_Simple(TCAN4x5x_MCAN_Nominal_Timing_Simple *nomTiming){
    uint32_t writeValue, tempValue;


    // These registers are only writable if CCE and INIT are both set. Sets the nominal bit timing and prescaler information
    // Check that prescaler is in valid range of 1-512
    tempValue = nomTiming->NominalBitRatePrescaler;
    if (tempValue > 512)
        tempValue = 512;
    else if (tempValue == 0)
        tempValue = 1;
    writeValue = ((uint32_t)(tempValue - 1)) << 16;		// Subtract 1 because MCAN expects 1 less than actual value


    // Check that prescaler is in valid range of 2-257
    tempValue = nomTiming->NominalTqBeforeSamplePoint;
    if (tempValue > 257)
        tempValue = 257;
    else if (tempValue < 2)
        tempValue = 2;
    writeValue |= ((uint32_t)(tempValue - 2)) << 8;		// Subtract 2, 1 for sync, and 1 because MCAN expects 1 less than actual value

    // Check that prescaler is in valid range of 2-257
    tempValue = nomTiming->NominalTqAfterSamplePoint;
    if (tempValue > 128)
        tempValue = 128;
    else if (tempValue < 2)
        tempValue = 2;
    writeValue |= ((uint32_t)(tempValue - 1));			// Subtract 1 because MCAN expects 1 less than actual value
    writeValue |= ((uint32_t)(tempValue - 1)) << 25; 	// NSJW is made to match the MCAN after bit time value

    // Write value to the NBTP register
    AHB_WRITE_32(REG_MCAN_NBTP, writeValue);
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Check the write was successful
    tempValue = AHB_READ_32(REG_MCAN_NBTP);
    if (tempValue != writeValue)
        return false;
#endif

    return true;
}


/**
 * @brief Writes the MCAN nominal timing settings, using the raw MCAN nominal timing struct
 *
 * Writes the data timing information to MCAN using the input from the @c *nomTiming pointer
 *
 * @warning This function writes to protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *nomTiming is a pointer of a @c TCAN4x5x_MCAN_Nominal_Timing_Raw struct containing the raw MCAN nominal timing information
 * @return @c true if successfully enabled, otherwise return @c false
 */
bool TCAN4x5x::MCAN_ConfigureNominalTiming_Raw(TCAN4x5x_MCAN_Nominal_Timing_Raw *nomTiming){
    uint32_t writeValue;
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    uint32_t tempValue;
#endif
    // These registers are only writable if CCE and INIT are both set. Sets the nominal bit timing and prescaler information
    writeValue = ((uint32_t)(nomTiming->NominalSyncJumpWidth & 0x7F)) << 25;
    writeValue |= ((uint32_t)(nomTiming->NominalBitRatePrescaler & 0x1FF)) << 16;
    writeValue |= ((uint32_t)(nomTiming->NominalTimeSeg1andProp)) << 8;
    writeValue |= ((uint32_t)(nomTiming->NominalTimeSeg2 & 0x7F));
    AHB_WRITE_32(REG_MCAN_NBTP, writeValue);
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Check that the write was successful
    tempValue = AHB_READ_32(REG_MCAN_NBTP);
    if (tempValue != writeValue)
        return false;
#endif
    return true;
}

/**
 * @brief Configures the MCAN global filter configuration register, using the passed Global Filter Configuration struct.
 *
 * Configures the default behavior of the MCAN controller when receiving messages. This can include accepting or rejecting CAN messages by default.
 *
 * @warning This function writes to protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *gfc is a pointer of a @c TCAN4x5x_MCAN_Global_Filter_Configuration struct containing the register values
 * @return @c true if successfully enabled, otherwise return @c false
 */
bool TCAN4x5x::MCAN_ConfigureGlobalFilter(TCAN4x5x_MCAN_Global_Filter_Configuration *gfc){
    uint32_t writeValue, readValue;


    writeValue = (gfc->word & REG_BITS_MCAN_GFC_MASK);
    AHB_WRITE_32(REG_MCAN_GFC, writeValue);

#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    readValue = AHB_READ_32(REG_MCAN_GFC);

    // Need to do these bitwise ANDs to make this work for clock stop requests and not trigger a false failure when comparing read back value
    if (readValue != writeValue)
    {
        // If our written value and read back value aren't the same, then we return a failure.
        return false;
    }
#endif
    return true;
}


/**
 * @brief Configures the MRAM registers
 *
 * Uses the @c *MRAMConfig pointer to set up the various sections of the MRAM memory space.
 * There are several different elements that may be configured in the MRAM, including their number of elements, as well as size of elements.
 * This function will automatically generate the start addresses for each of the appropriate MRAM sections, attempting to place them immediately back-to-back.
 * This function will check for over allocated memory conditions, and return @c false if this is found to be the case.
 *
 * @warning This function writes to protected MCAN registers
 * @note Requires that protected registers have been unlocked using @c MCAN_EnableProtectedRegisters() and @c MCAN_DisableProtectedRegisters() be used to lock the registers after configuration
 *
 * @param *MRAMConfig is a pointer of a @c TCAN4x5x_MRAM_Config struct containing the desired MRAM configuration
 * @return @c true if successful, otherwise return @c false
 */
bool TCAN4x5x::MRAM_Configure(TCAN4x5x_MRAM_Config *MRAMConfig){
    uint16_t startAddress = 0x0000;         // Used to hold the start and end addresses for each section as we write them into the appropriate registers
    uint32_t registerValue = 0;             // Used to create the 32-bit word to write to each register
    uint32_t readValue = 0;
    uint8_t MRAMValue;


    // First the 11-bit filter section can be setup.
    MRAMValue = MRAMConfig->SIDNumElements;
    if (MRAMValue > 128)
        MRAMValue = 128;

    registerValue = 0;
    if (MRAMValue > 0)
    {
        registerValue = ((uint32_t)(MRAMValue) << 16) | ((uint32_t)startAddress);
    }
    startAddress += (4 * (uint16_t)MRAMValue);
    AHB_WRITE_32(REG_MCAN_SIDFC, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_SIDFC] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_SIDFC);
    if (readValue != registerValue)
        return false;
#endif


    // The 29-bit extended filter section
    MRAMValue = MRAMConfig->XIDNumElements;
    if (MRAMValue > 64)
        MRAMValue = 64;

    registerValue = 0;
    if (MRAMValue > 0)
    {
        registerValue = ((uint32_t)(MRAMValue) << 16) | ((uint32_t)startAddress);
    }
    startAddress += (8 * (uint16_t)MRAMValue);
    AHB_WRITE_32(REG_MCAN_XIDFC, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_XIDFC] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_XIDFC);
    if (readValue != registerValue)
        return false;
#endif


    // RX FIFO 0
    MRAMValue = MRAMConfig->Rx0NumElements;
    if (MRAMValue > 64)
        MRAMValue = 64;

    registerValue = 0;
    if (MRAMValue > 0)
    {
        registerValue = ((uint32_t)(MRAMValue) << 16) | ((uint32_t)startAddress);       // Write start address and the number of elements
        registerValue |= REG_BITS_MCAN_RXF0C_F0OM_OVERWRITE;                            // Also enable overwrite mode when FIFO is full
    }
    startAddress += (((uint32_t)MCAN_TXRXESC_DataByteValue((uint8_t)MRAMConfig->Rx0ElementSize) + 8) * (uint16_t)MRAMValue);
    AHB_WRITE_32(REG_MCAN_RXF0C, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_RXF0C] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_RXF0C);
    if (readValue != registerValue)
        return false;
#endif

    // RX FIFO 1
    MRAMValue = MRAMConfig->Rx1NumElements;
    if (MRAMValue > 64)
        MRAMValue = 64;

    registerValue = 0;
    if (MRAMValue > 0)
    {
        registerValue = ((uint32_t)(MRAMValue) << 16) | ((uint32_t)startAddress);
    }
    startAddress += (((uint32_t)MCAN_TXRXESC_DataByteValue((uint8_t)MRAMConfig->Rx1ElementSize) + 8) * (uint16_t)MRAMValue);
    AHB_WRITE_32(REG_MCAN_RXF1C, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_RXF1C] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_RXF1C);
    if (readValue != registerValue)
        return false;
#endif

    // RX Buffers
    // Since RXBuffers are a little weird, you don't actually tell MCAN how many elements you have. Instead, you tell it indirectly through filters.
    // For example, you would have to setup a filter to tell it which value to go to
    MRAMValue = MRAMConfig->RxBufNumElements;
    if (MRAMValue > 64)
        MRAMValue = 64;

    registerValue = 0;
    if (MRAMValue > 0)
    {
        registerValue = ((uint32_t)startAddress);
    }
    startAddress += (((uint32_t)MCAN_TXRXESC_DataByteValue((uint8_t)MRAMConfig->RxBufElementSize) + 8) * (uint16_t)MRAMValue);
    AHB_WRITE_32(REG_MCAN_RXBC, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_RXBC] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_RXBC);

    if (readValue != registerValue)
        return false;
#endif

    // TX Event FIFO
    MRAMValue = MRAMConfig->TxEventFIFONumElements;
    if (MRAMValue > 32)
        MRAMValue = 32;

    registerValue = 0;
    if (MRAMValue > 0)
    {
        registerValue = ((uint32_t)(MRAMValue) << 16) | ((uint32_t)startAddress);
    }
    startAddress += (8 * (uint16_t)MRAMValue);
    AHB_WRITE_32(REG_MCAN_TXEFC, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_TXEFC] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_TXEFC);
    if (readValue != registerValue)
        return false;
#endif

    // TX Buffer
    MRAMValue = MRAMConfig->TxBufferNumElements;
    if (MRAMValue > 32)
        MRAMValue = 32;


    registerValue = 0;
    if (MRAMValue > 0)
    {
        registerValue = ((uint32_t)(MRAMValue) << 24) | ((uint32_t)startAddress);
        registerValue |= REG_BITS_MCAN_TXBC_TFQM;               // Sets TFQM to 1 (Queue mode), and sets all registers to be generic non-dedicated buffers.
    }
    startAddress += (((uint32_t)MCAN_TXRXESC_DataByteValue((uint8_t)MRAMConfig->TxBufferElementSize) + 8) * (uint16_t)MRAMValue);
    AHB_WRITE_32(REG_MCAN_TXBC, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_TXBC] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_TXBC);
    if (readValue != registerValue)
        return false;
#endif


    // Check and make sure we did not go out of memory bounds. If it is, return fail
    if ((startAddress - 1) > (MRAM_SIZE + REG_MRAM))
        return false;

    // Set the RX Element Size Register
    registerValue = ((uint32_t)(MRAMConfig->RxBufElementSize) << 8) | ((uint32_t)(MRAMConfig->Rx1ElementSize) << 4) | (uint32_t)(MRAMConfig->Rx0ElementSize);
    AHB_WRITE_32(REG_MCAN_RXESC, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_RXESC] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_RXESC);
    if (readValue != registerValue)
        return false;
#endif


    // Set the TX Element Size Register
    registerValue = (uint32_t)(MRAMConfig->TxBufferElementSize);
    AHB_WRITE_32(REG_MCAN_TXESC, registerValue);
#ifdef MCAN_CACHE_CONFIGURATION
    MCAN_CACHE[MCAN_CACHE_TXESC] = registerValue;
#endif
#ifdef MCAN_VERIFY_CONFIGURATION_WRITES
    // Verify content of register
    readValue = AHB_READ_32(REG_MCAN_TXESC);
    if (readValue != registerValue)
        return false;
#endif

    return true;
}


/**
 * @brief Clear (Zero-fill) the contents of MRAM
 *
 * Write 0s to every address in MRAM. Useful for initializing the MRAM to known values during initial configuration so that accidental ECC errors do not happen
 */
void TCAN4x5x::MRAM_Clear(void){
    uint16_t curAddr;
    const uint16_t endAddr = REG_MRAM + MRAM_SIZE;

    // Need to write 0's to the entire MRAM
    curAddr = REG_MRAM;

    while (curAddr < endAddr)
    {
        AHB_WRITE_32(curAddr, 0);
        curAddr += 4;
    }

}


/**
 * @brief Read the next MCAN FIFO element
 *
 * This function will read the next MCAN FIFO element specified and return the corresponding header information and data payload.
 * The start address of the elment is automatically calculated by looking at the MCAN's register that says where the next element to read exists.
 *
 * @param FIFODefine is an @c TCAN4x5x_MCAN_FIFO_Enum enum corresponding to either RXFIFO0 or RXFIFO1
 * @param *header is a pointer to a @c MCAN_RX_Header struct containing the CAN-specific header information
 * @param dataPayload[] is a byte array that will be updated with the read data
 *
 * @warning @c dataPayload[] must be at least as big as the largest possible data payload, otherwise writing to out of bounds memory may occur
 *
 * @return the number of bytes that were read from the TCAN4x5x and stored into @c dataPayload[]
 */
uint8_t TCAN4x5x::MCAN_ReadNextFIFO(TCAN4x5x_MCAN_FIFO_Enum FIFODefine, TCAN4x5x_MCAN_RX_Header *header, uint8_t dataPayload[]){
    uint32_t readData;
    uint16_t startAddress;
    uint8_t i, getIndex, elementSize;

    // Get the get buffer location and size, depending on the source type
    switch (FIFODefine)
    {
        default: // RXFIFO0 is default
        {
            readData = AHB_READ_32(REG_MCAN_RXF0S);
            if ((readData & 0x0000007F) == 0)
                return 0;
            getIndex = (uint8_t) ((readData & 0x3F00) >> 8);
            // Get the RX 0 Start location and size...
#ifdef MCAN_CACHE_CONFIGURATION
            readData = MCAN_CACHE[MCAN_CACHE_RXF0C];
#else
            readData = AHB_READ_32(REG_MCAN_RXF0C);
#endif
            startAddress = (uint16_t)(readData & 0x0000FFFF) + REG_MRAM;
#ifdef MCAN_CACHE_CONFIGURATION
            readData = MCAN_CACHE[MCAN_CACHE_RXESC];
#else
            readData = AHB_READ_32(REG_MCAN_RXESC);
#endif
            readData &= 0x07;
            elementSize = MCAN_TXRXESC_DataByteValue(readData); // Maximum theoretical data payload supported by this MCAN configuration
            // Calculate the actual start address for the latest index
            startAddress += (((uint32_t)elementSize + 8) * getIndex);
            break;
        }

        case RXFIFO1:
        {
            readData = AHB_READ_32(REG_MCAN_RXF1S);
            if ((readData & 0x0000007F) == 0)
                return 0;
            getIndex = (uint8_t) ((readData & 0x3F00) >> 8);
            // Get the RX 1 Start location and size...
#ifdef MCAN_CACHE_CONFIGURATION
            readData = MCAN_CACHE[MCAN_CACHE_RXF1C];
#else
            readData = AHB_READ_32(REG_MCAN_RXF1C);
#endif
            startAddress = (uint16_t)(readData & 0x0000FFFF) + REG_MRAM;
#ifdef MCAN_CACHE_CONFIGURATION
            readData = MCAN_CACHE[MCAN_CACHE_RXESC];
#else
            readData = AHB_READ_32(REG_MCAN_RXESC);
#endif
            readData = (readData & 0x70) >> 4;
            elementSize = MCAN_TXRXESC_DataByteValue(readData); // Maximum theoretical data payload supported by this MCAN configuration
            // Calculate the actual start address for the latest index
            startAddress += (((uint32_t)elementSize + 8) * getIndex);
            break;
        }
    }


    // Read the data, start with a burst read
    AHB_READ_BURST_START(startAddress, 2);
    readData = AHB_READ_BURST_READ(); // First header
    header->ESI = (readData & 0x80000000) >> 31;
    header->XTD = (readData & 0x40000000) >> 30;
    header->RTR = (readData & 0x20000000) >> 29;

    if (header->XTD)
        header->ID  = (readData & 0x1FFFFFFF);
    else
        header->ID  = (readData & 0x1FFC0000) >> 18;

    readData = AHB_READ_BURST_READ();   // Second header
    AHB_READ_BURST_END();               // Terminate the burst read
    header->RXTS    = (readData & 0x0000FFFF);
    header->DLC     = (readData & 0x000F0000) >> 16;
    header->BRS     = (readData & 0x00100000) >> 20;
    header->FDF     = (readData & 0x00200000) >> 21;
    header->FIDX    = (readData & 0x7F000000) >> 24;
    header->ANMF    = (readData & 0x80000000) >> 31;

    // Get the actual data
    // If the data payload size of the header is smaller than the maximum we can store, then update the new element size to read only what we need to (prevents accidental overflow reading)
    if (MCAN_DLCtoBytes(header->DLC) < elementSize )
        elementSize = MCAN_DLCtoBytes(header->DLC); // Returns the number of data bytes

    // Start a burst read for the number of data bytes we require at the data payload area of the MRAM
    // The equation below ensures that we will always read the correct number of words since the divide truncates any remainders, and we need a ceil()-like function
    if (elementSize > 0) {
        AHB_READ_BURST_START(startAddress + 8, (elementSize + 3) >> 2);
        i = 0;  // Used to count the number of bytes we have read.
        while (i < elementSize) {
            if ((i % 4) == 0) {
                readData = AHB_READ_BURST_READ();
            }

            dataPayload[i] = (uint8_t)((readData >> ((i % 4) * 8)) & 0xFF);
            i++;
            if (i > elementSize)
                i = elementSize;
        }
        AHB_READ_BURST_END(); // Terminate the burst read
    }
    // Acknowledge the FIFO read
    switch (FIFODefine)
    {
    default: // RXFIFO0
        AHB_WRITE_32(REG_MCAN_RXF0A, getIndex);
        break;

    case RXFIFO1:
        AHB_WRITE_32(REG_MCAN_RXF1A, getIndex);
        break;
    }


    return i;   // Return the number of bytes retrieved
}


/**
 * @brief Read the specified RX buffer element
 *
 * This function will read the specified MCAN buffer element and return the corresponding header information and data payload.
 * The start address of the element is automatically calculated.
 *
 * @param bufIndex is the RX buffer index to read from (starts at 0)
 * @param *header is a pointer to a @c TCAN4x5x_MCAN_RX_Header struct containing the CAN-specific header information
 * @param dataPayload[] is a byte array that will be updated with the read data
 *
 * @warning @c dataPayload[] must be at least as big as the largest possible data payload, otherwise writing to out of bounds memory may occur
 *
 * @return the number of bytes that were read from the TCAN4x5x and stored into @c dataPayload[]
 */
uint8_t TCAN4x5x::MCAN_ReadRXBuffer(uint8_t bufIndex, TCAN4x5x_MCAN_RX_Header *header, uint8_t dataPayload[]){
    uint32_t readData;
    uint16_t startAddress;
    uint8_t i, getIndex, elementSize;

    // Get the get buffer location and size
    getIndex = bufIndex;
    if (getIndex > 64)
        getIndex = 64;

    // Get the RX Buffer Start location and size...
#ifdef MCAN_CACHE_CONFIGURATION
    readData = MCAN_CACHE[MCAN_CACHE_RXBC];
#else
    readData = AHB_READ_32(REG_MCAN_RXBC);
#endif
    startAddress = (uint16_t)(readData & 0x0000FFFF) + REG_MRAM;
#ifdef MCAN_CACHE_CONFIGURATION
    readData = MCAN_CACHE[MCAN_CACHE_RXESC];
#else
    readData = AHB_READ_32(REG_MCAN_RXESC);
#endif
    readData = (readData & 0x0700) >> 8;
    elementSize = MCAN_TXRXESC_DataByteValue(readData); // Maximum theoretical data payload supported by this MCAN configuration
    // Calculate the actual start address for the latest index
    startAddress += (((uint32_t)elementSize + 8) * getIndex);



    // Read the data, start with a burst read
    AHB_READ_BURST_START(startAddress, 2);
    readData = AHB_READ_BURST_READ(); // First header
    header->ESI = (readData & 0x80000000) >> 31;
    header->XTD = (readData & 0x40000000) >> 30;
    header->RTR = (readData & 0x20000000) >> 29;

    if (header->XTD)
        header->ID  = (readData & 0x1FFFFFFF);
    else
        header->ID  = (readData & 0x1FFC0000) >> 18;

    readData = AHB_READ_BURST_READ();   // Second header
    AHB_READ_BURST_END();               // Terminate the burst read
    header->RXTS    = (readData & 0x0000FFFF);
    header->DLC     = (readData & 0x000F0000) >> 16;
    header->BRS     = (readData & 0x00100000) >> 20;
    header->FDF     = (readData & 0x00200000) >> 21;
    header->FIDX    = (readData & 0x7F000000) >> 24;
    header->ANMF    = (readData & 0x80000000) >> 31;

    // Get the actual data
    // If the data payload size of the header is smaller than the maximum we can store, then update the new element size to read only what we need to (prevents accidentical overflow reading)
    if (MCAN_DLCtoBytes(header->DLC) < elementSize )
        elementSize = MCAN_DLCtoBytes(header->DLC); // Returns the number of data bytes

    // Start a burst read for the number of data bytes we require at the data payload area of the MRAM
    // The equation below ensures that we will always read the correct number of words since the divide truncates any remainders, and we need a ceil()-like function
    if (elementSize > 0) {
        AHB_READ_BURST_START(startAddress + 8, (elementSize + 3) >> 2);
        i = 0;  // Used to count the number of bytes we have read.
        while (i < elementSize) {
            if ((i % 4) == 0) {
                readData = AHB_READ_BURST_READ();
            }

            dataPayload[i] = (uint8_t)((readData >> ((i % 4) * 8)) & 0xFF);
            i++;
            if (i > elementSize)
                i = elementSize;
        }
        AHB_READ_BURST_END(); // Terminate the burst read
    }
    // Acknowledge the FIFO read
    if (getIndex < 32)
    {
        AHB_WRITE_32(REG_MCAN_NDAT1, 1 << getIndex);
    } else {
        AHB_WRITE_32(REG_MCAN_NDAT2, 1 << (getIndex-32));
    }


    return i;   // Return the number of bytes retrieved
}


/**
 * @brief Write CAN message to the specified TX buffer
 *
 * This function will write a CAN message to a specified TX buffer that can be transmitted at a later time with the @c MCAN_TransmitBufferContents() function
 *
 * @param bufIndex is the TX buffer index to write to (starts at 0)
 * @param *header is a pointer to a @c TCAN4x5x_MCAN_TX_Header struct containing the CAN-specific header information
 * @param dataPayload[] is a byte array that contains the data payload
 *
 * @warning @c dataPayload[] must be at least as big as the specified DLC size inside the @c *header struct
 *
 * @return the number of bytes that were read from the TCAN4x5x and stored into @c dataPayload[]
 */
uint32_t TCAN4x5x::MCAN_WriteTXBuffer(uint8_t bufIndex, TCAN4x5x_MCAN_TX_Header *header, uint8_t dataPayload[]){
    // Step 1: Get the start address of the
    uint32_t SPIData;
    uint16_t startAddress;
    uint8_t i, elementSize, temp;


    // Get the TX Start location and size...
#ifdef MCAN_CACHE_CONFIGURATION
    SPIData = MCAN_CACHE[MCAN_CACHE_TXBC];
#else
    SPIData = AHB_READ_32(REG_MCAN_TXBC);
#endif
    startAddress = (uint16_t)(SPIData & 0x0000FFFF) + 0x8000;
    // Transmit FIFO and queue numbers
    temp = (uint8_t)((SPIData >> 24) & 0x3F);
    elementSize = temp > 32 ? 32 : temp;
    // Dedicated transmit buffers
    temp = (uint8_t)((SPIData >> 16) & 0x3F);
    elementSize += temp > 32 ? 32 : temp;

    if (bufIndex > (elementSize-1)) {
        return 0;
    }

    // Get the actual element size of each TX element
#ifdef MCAN_CACHE_CONFIGURATION
    SPIData = MCAN_CACHE[MCAN_CACHE_TXESC];
#else
    SPIData = AHB_READ_32(REG_MCAN_TXESC);
#endif
    elementSize = MCAN_TXRXESC_DataByteValue(SPIData & 0x07) + 8;

    // Calculate the actual start address for the latest index
    startAddress += ((uint32_t)elementSize * bufIndex);

    // Now we need to actually check how much data we are writing (because we don't need to fill a 64-byte FIFO if we are sending an 8 byte can packet)
    elementSize = (MCAN_DLCtoBytes(header->DLC & 0x0F) + 8) >> 2; // Convert it to words for easier reading by dividing by 4, and only look at data payload
    if (MCAN_DLCtoBytes(header->DLC & 0x0F) % 4) { // If we don't have a whole word worth of data... We need to round up to the nearest word (by default it truncates). Can be done by simply adding another word.
        elementSize += 1;
    }
    // Read the data, start with a burst read
    AHB_WRITE_BURST_START(startAddress, elementSize);
    SPIData = 0;

    SPIData         |= ((uint32_t)header->ESI & 0x01) << 31;
    SPIData         |= ((uint32_t)header->XTD & 0x01) << 30;
    SPIData         |= ((uint32_t)header->RTR & 0x01) << 29;

    if (header->XTD)
        SPIData     |= ((uint32_t)header->ID & 0x1FFFFFFF);
    else
        SPIData     |= ((uint32_t)header->ID & 0x07FF) << 18;

    AHB_WRITE_BURST_WRITE(SPIData);

    SPIData = 0;
    SPIData         |= ((uint32_t)header->DLC & 0x0F) << 16;
    SPIData         |= ((uint32_t)header->BRS & 0x01) << 20;
    SPIData         |= ((uint32_t)header->FDF & 0x01) << 21;
    SPIData         |= ((uint32_t)header->EFC & 0x01) << 23;
    SPIData         |= ((uint32_t)header->MM & 0xFF) << 24;
    AHB_WRITE_BURST_WRITE(SPIData);

    // Get the actual data
    elementSize = MCAN_DLCtoBytes(header->DLC & 0x0F); // Returns the number of data bytes
    i = 0;  // Used to count the number of bytes we have read.
    while (i < elementSize) {
        SPIData = 0;
        // If elementSize - i < 4, then this means we are on our last word, with a word that is less than 4 bytes long
        if ((elementSize - i) < 4) {
            while (i < elementSize)
            {
                SPIData |= ((uint32_t)dataPayload[i] << ((i % 4) * 8));
                i++;
            }

            AHB_WRITE_BURST_WRITE(SPIData);
        } else {
            SPIData |= ((uint32_t)dataPayload[i++]);
            SPIData |= ((uint32_t)dataPayload[i++]) << 8;
            SPIData |= ((uint32_t)dataPayload[i++]) << 16;
            SPIData |= ((uint32_t)dataPayload[i++]) << 24;

            AHB_WRITE_BURST_WRITE(SPIData);
        }

        if (i > elementSize)
            i = elementSize;
    }
    AHB_WRITE_BURST_END();              // Terminate the burst read

    return (uint32_t)1 << bufIndex; // Return the number of bytes retrieved
}


/**
 * @brief Transmit TX buffer contents of the specified tx buffer
 *
 * Writes the specified buffer index bit value into the TXBAR register to request a message to send
 *
 * @param bufIndex is the TX buffer index to write to (starts at 0)
 *
 * @warning Function does NOT check if the buffer contents are valid
 *
 * @return @c true if the request was queued, @c false if the buffer value was invalid (out of range)
 */
bool TCAN4x5x::MCAN_TransmitBufferContents(uint8_t bufIndex){
    uint32_t writeValue;
    uint8_t requestedBuf = bufIndex;

    if (requestedBuf > 31)
        return false;

    writeValue = 1 << requestedBuf;

    AHB_WRITE_32(REG_MCAN_TXBAR, writeValue);
    return true;
}


/**
 * @brief Write MCAN Standard ID filter into MRAM
 *
 * This function will write a standard ID MCAN filter to a specified filter element
 *
 * @param filterIndex is the SID filter index in MRAM to write to (starts at 0)
 * @param *filter is a pointer to a @c TCAN4x5x_MCAN_SID_Filter struct containing the MCAN filter information
 *
 * @return @c true if write was successful, @c false if not
 */
bool TCAN4x5x::MCAN_WriteSIDFilter(uint8_t filterIndex, TCAN4x5x_MCAN_SID_Filter *filter){
    uint32_t readData;
    uint16_t startAddress;
    uint8_t getIndex;
#ifdef MCAN_CACHE_CONFIGURATION
    readData = MCAN_CACHE[MCAN_CACHE_SIDFC];
#else
    readData = AHB_READ_32(REG_MCAN_SIDFC);
#endif
    getIndex = (readData & 0x00FF0000) >> 16;
    if (filterIndex > getIndex) // Check if the fifo number is valid and within range. If not, then fail
        return false;
    else
        getIndex = filterIndex;

    startAddress = (uint16_t)(readData & 0x0000FFFF) + REG_MRAM;
    // Calculate the actual start address for the latest index
    startAddress += (getIndex << 2);                // Multiply by 4 and add to start address

    AHB_WRITE_32(startAddress, filter->word);       // Write the value to the register
#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Verify that write was successful
    readData = AHB_READ_32(startAddress);
    if (readData != filter->word)
        return false;
#endif
    return true;
}


/**
 * @brief Read a MCAN Standard ID filter from MRAM
 *
 * This function will read a standard ID MCAN filter from a specified filter element
 *
 * @param filterIndex is the SID filter index in MRAM to read from (starts at 0)
 * @param *filter is a pointer to a @c TCAN4x5x_MCAN_SID_Filter struct that will be updated with the read MCAN filter
 *
 * @return @c true if read was successful, @c false if not
 */
bool TCAN4x5x::MCAN_ReadSIDFilter(uint8_t filterIndex, TCAN4x5x_MCAN_SID_Filter *filter){
    uint32_t readData;
    uint16_t startAddress;
    uint8_t getIndex;
#ifdef MCAN_CACHE_CONFIGURATION
    readData = MCAN_CACHE[MCAN_CACHE_SIDFC];
#else
    readData = AHB_READ_32(REG_MCAN_SIDFC);
#endif
    getIndex = (readData & 0x00FF0000) >> 16;
    if (filterIndex > getIndex) // Check if the fifo number is valid and within range. If not, then fail
        return false;
    else
        getIndex = filterIndex;

    startAddress = (uint16_t)(readData & 0x0000FFFF) + REG_MRAM;
    // Calculate the actual start address for the latest index
    startAddress += (getIndex << 2);                // Multiply by 4 and add to start address

    filter->word = AHB_READ_32(startAddress);       // Read the value from the MRAM
    return true;
}


/**
 * @brief Write MCAN Extended ID filter into MRAM
 *
 * This function will write an extended ID MCAN filter to a specified filter element
 *
 * @param filterIndex is the XID filter index in MRAM to write to (starts at 0)
 * @param *filter is a pointer to a @c TCAN4x5x_MCAN_XID_Filter struct containing the MCAN filter information
 *
 * @return @c true if write was successful, @c false if not
 */
bool TCAN4x5x::MCAN_WriteXIDFilter(uint8_t filterIndex, TCAN4x5x_MCAN_XID_Filter *filter){
    uint32_t readData, writeData;
    uint16_t startAddress;
    uint8_t getIndex;
#ifdef MCAN_CACHE_CONFIGURATION
    readData = MCAN_CACHE[MCAN_CACHE_XIDFC];
#else
    readData = AHB_READ_32(REG_MCAN_XIDFC);
#endif
    getIndex = (readData & 0x00FF0000) >> 16;
    if (filterIndex > getIndex) // Check if the fifo number is valid and within range. If not, then fail
        return false;
    else
        getIndex = filterIndex;

    startAddress = (uint16_t)(readData & 0x0000FFFF) + REG_MRAM;
    // Calculate the actual start address for the latest index
    startAddress += (getIndex << 3);    // Multiply by 4 and add to start address

    // Write the 2 words to memory
    writeData = (uint32_t)(filter->EFEC) << 29;
    writeData |= (uint32_t)(filter->EFID1);
    AHB_WRITE_32(startAddress, writeData);      // Write the value to the register
#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    readData = AHB_READ_32(startAddress);
    if (readData != writeData)
        return false;
#endif

    startAddress += 4;
    writeData = (uint32_t)(filter->EFT) << 30;
    writeData |= (uint32_t)(filter->EFID2);
    AHB_WRITE_32(startAddress, writeData);      // Write the value to the register
#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    readData = AHB_READ_32(startAddress);
    if (readData != writeData)
        return false;
#endif

    return true;
}


/**
 * @brief Read MCAN Extended ID filter from MRAM
 *
 * This function will read an extended ID MCAN filter from a specified filter element
 *
 * @param filterIndex is the XID filter index in MRAM to read from (starts at 0)
 * @param *filter is a pointer to a @c TCAN4x5x_MCAN_XID_Filter struct that will be updated with information from MRAM
 *
 * @return @c true if read was successful, @c false if not
 */
bool TCAN4x5x::MCAN_ReadXIDFilter(uint8_t filterIndex, TCAN4x5x_MCAN_XID_Filter *filter){
    uint32_t readData;
    uint16_t startAddress;
    uint8_t getIndex;
#ifdef MCAN_CACHE_CONFIGURATION
    readData = MCAN_CACHE[MCAN_CACHE_XIDFC];
#else
    readData = AHB_READ_32(REG_MCAN_XIDFC);
#endif
    getIndex = (readData & 0x00FF0000) >> 16;
    if (filterIndex > getIndex) // Check if the fifo number is valid and within range. If not, then fail
        return false;
    else
        getIndex = filterIndex;

    startAddress = (uint16_t)(readData & 0x0000FFFF) + REG_MRAM;
    // Calculate the actual start address for the latest index
    startAddress += (getIndex << 3);    // Multiply by 4 and add to start address

    AHB_READ_BURST_START(startAddress, 2);      // Send SPI header for a burst SPI read of 2 words
    readData = AHB_READ_BURST_READ();            // Read first word from MRAM

    filter->EFEC = (TCAN4x5x_XID_EFEC_Values)((readData >> 29) & 0x07);
    filter->EFID1 = readData & 0x1FFFFFFF;

    readData = AHB_READ_BURST_READ();           // Read second word from MRAM
    AHB_READ_BURST_END();                       // Terminate the SPI transaction
    filter->EFT = (TCAN4x5x_XID_EFT_Values)((readData >> 30) & 0x03);
    filter->EFID2 = readData & 0x1FFFFFFF;

    return true;
}


/**
 * @brief Read the MCAN interrupts
 *
 * Reads the MCAN interrupts and updates a @c TCAN4x5x_MCAN_Interrupts struct that is passed to the function
 *
 * @param *ir is a pointer to a @c TCAN4x5x_MCAN_Interrupts struct containing the interrupt bit fields that will be updated
 */
void TCAN4x5x::MCAN_ReadInterrupts(TCAN4x5x_MCAN_Interrupts *ir){
    ir->word = AHB_READ_32(REG_MCAN_IR);
}


/**
 * @brief Clear the MCAN interrupts
 *
 * Will attempt to clear any interrupts that are marked as a '1' in the passed @c TCAN4x5x_MCAN_Interrupts struct
 *
 * @param *ir is a pointer to a @c TCAN4x5x_MCAN_Interrupts struct containing the interrupt bit fields that will be updated
 */
void TCAN4x5x::MCAN_ClearInterrupts(TCAN4x5x_MCAN_Interrupts *ir){
    AHB_WRITE_32(REG_MCAN_IR, ir->word);
}


/**
 * @brief Clear all MCAN interrupts
 *
 * Clears all MCAN interrupts
 */
void TCAN4x5x::MCAN_ClearInterruptsAll(void){
    AHB_WRITE_32(REG_MCAN_IR, 0xFFFFFFFF);
}


/**
 * @brief Read the MCAN interrupt enable register
 *
 * Reads the MCAN interrupt enable register and updates the passed @c TCAN4x5x_MCAN_Interrupt_Enable struct
 *
 * @param *ie is a pointer to a @c TCAN4x5x_MCAN_Interrupt_Enable struct containing the interrupt bit fields that will be updated
 */
void TCAN4x5x::MCAN_ReadInterruptEnable(TCAN4x5x_MCAN_Interrupt_Enable *ie){
    ie->word = AHB_READ_32(REG_MCAN_IE);
}


/**
 * @brief Configures the MCAN interrupt enable register
 *
 * Configures the MCAN interrupt enable register based on the passed @c TCAN4x5x_MCAN_Interrupt_Enable struct
 * Also enables MCAN interrupts out to the INT1 pin.
 *
 * @param *ie is a pointer to a @c TCAN4x5x_MCAN_Interrupt_Enable struct containing the desired enabled interrupt bits
 */
void TCAN4x5x::MCAN_ConfigureInterruptEnable(TCAN4x5x_MCAN_Interrupt_Enable *ie){
    AHB_WRITE_32(REG_MCAN_IE, ie->word);
    AHB_WRITE_32(REG_MCAN_ILE, REG_BITS_MCAN_ILE_EINT0);		// This is necessary to enable the MCAN Int mux to the output nINT pin
}


/**
 * @brief Converts the CAN message DLC hex value to the number of bytes it corresponds to
 *
 * @param inputDLC is the DLC value from/to a CAN message struct
 * @return The number of bytes of data (0-64 bytes)
 */
uint8_t TCAN4x5x::MCAN_DLCtoBytes(uint8_t inputDLC){
    static const uint8_t lookup[7] = {12, 16, 20, 24, 32, 48, 64};

    if (inputDLC < 9)
        return inputDLC;

    if (inputDLC < 16)
        return lookup[(unsigned int)(inputDLC-9)];

    return 0;

}


/**
 * @brief Converts the MCAN ESC (Element Size) value to number of bytes that it corresponds to
 *
 * @param inputESCValue is the value from an element size configuration register
 * @return The number of bytes of data (8-64 bytes)
 */
uint8_t TCAN4x5x::MCAN_TXRXESC_DataByteValue(uint8_t inputESCValue){
    static const uint8_t lookup[8] = {8, 12, 16, 20, 24, 32, 48, 64};
    return lookup[(unsigned int)(inputESCValue & 0x07)];
}





/* ************************************** *
 *  Start of Device (Non-MCAN) Functions  *
 * ************************************** */

/**
 * @brief Read the TCAN4x5x device version register
 *
 * @return The register value for the device version register
 */
uint16_t TCAN4x5x::Device_ReadDeviceVersion(void){
    uint32_t readValue;

    readValue = AHB_READ_32(REG_SPI_REVISION);

    return (uint16_t)(readValue & 0xFFFF);
}


/**
 * @brief Configures the device mode and pin register
 *
 * Configures the device mode and pin register based on the passed @c DEV_CONFIG struct, but will mask out the reserved bits on a write
 *
 * @param *devCfg is a pointer to a @c DEV_CONFIG struct containing the desired device mode and pin register values
 *
 * @return @c true if configuration successfully done, @c false if not
 */
bool TCAN4x5x::Device_Configure(TCAN4x5x_DEV_CONFIG *devCfg){
    // First we must read the register
    uint32_t readDevice = AHB_READ_32(REG_DEV_MODES_AND_PINS);

    // Then mask the bits that will be set by the struct
    readDevice &= ~(REG_BITS_DEVICE_MODE_SWE_MASK  | REG_BITS_DEVICE_MODE_DEVICE_RESET    | REG_BITS_DEVICE_MODE_WDT_MASK        |
            REG_BITS_DEVICE_MODE_NWKRQ_CONFIG_MASK | REG_BITS_DEVICE_MODE_INH_MASK        | REG_BITS_DEVICE_MODE_GPO1_FUNC_MASK  |
            REG_BITS_DEVICE_MODE_FAIL_SAFE_MASK    | REG_BITS_DEVICE_MODE_GPO1_MODE_MASK  | REG_BITS_DEVICE_MODE_WDT_ACTION_MASK |
            REG_BITS_DEVICE_MODE_WDT_RESET_BIT     | REG_BITS_DEVICE_MODE_NWKRQ_VOLT_MASK | REG_BITS_DEVICE_MODE_TESTMODE_ENMASK |
            REG_BITS_DEVICE_MODE_GPO2_MASK         | REG_BITS_DEVICE_MODE_WD_CLK_MASK     | REG_BITS_DEVICE_MODE_WAKE_PIN_MASK);

    // Copy to a temporary location in memory, so we don't modify the incoming struct
    TCAN4x5x_DEV_CONFIG tempCfg;
    tempCfg.word = devCfg->word;

    // Clear the reserved flags.
    tempCfg.RESERVED0 = 0;
    tempCfg.RESERVED1 = 0;
    tempCfg.RESERVED2 = 0;
    tempCfg.RESERVED3 = 0;
    tempCfg.RESERVED4 = 0;
    tempCfg.RESERVED5 = 0;


    // Set the bits according to the incoming struct
    readDevice |= (REG_BITS_DEVICE_MODE_FORCED_SET_BITS | tempCfg.word);

    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, readDevice);

#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    uint32_t readValue = AHB_READ_32(REG_DEV_MODES_AND_PINS);       // Read value
    if (readValue != readDevice)
        return false;
#endif
    return true;
}


/**
 * @brief Reads the device mode and pin register
 *
 * Reads the device mode and pin register and updates the passed @c TCAN4x5x_DEV_CONFIG struct
 *
 * @param *devCfg is a pointer to a @c TCAN4x5x_DEV_CONFIG struct to be updated with the current mode and pin register values
 */
void TCAN4x5x::Device_ReadConfig(TCAN4x5x_DEV_CONFIG *devCfg){
    devCfg->word = AHB_READ_32(REG_DEV_MODES_AND_PINS);
}


/**
 * @brief Read the device interrupts
 *
 * Reads the device interrupts and updates a @c TCAN4x5x_Device_Interrupts struct that is passed to the function
 *
 * @param *ir is a pointer to a @c TCAN4x5x_Device_Interrupts struct containing the interrupt bit fields that will be updated
 */
void TCAN4x5x::Device_ReadInterrupts(TCAN4x5x_Device_Interrupts *ir){
    ir->word = AHB_READ_32(REG_DEV_IR);
}


/**
 * @brief Clear the device interrupts
 *
 * Will attempt to clear any interrupts that are marked as a '1' in the passed @c TCAN4x5x_Device_Interrupts struct
 *
 * @param *ir is a pointer to a @c TCAN4x5x_Device_Interrupts struct containing the interrupt bit fields that will be updated
 */
void TCAN4x5x::Device_ClearInterrupts(TCAN4x5x_Device_Interrupts *ir){
    AHB_WRITE_32(REG_DEV_IR, ir->word);
}


/**
 * @brief Clear all device interrupts
 *
 * Clears all device interrupts
 */
void TCAN4x5x::Device_ClearInterruptsAll(void){
    AHB_WRITE_32(REG_DEV_IR, 0xFFFFFFFF);
}


/**
 * @brief Clears a SPIERR flag that may be set
 */
void TCAN4x5x::Device_ClearSPIERR(void){
    AHB_WRITE_32(REG_SPI_STATUS, 0xFFFFFFFF);       // Simply write all 1s to attempt to clear a SPIERR that was set
}


/**
 * @brief Read the device interrupt enable register
 *
 * Reads the device interrupt enable register and updates the passed @c TCAN4x5x_Device_Interrupt_Enable struct
 *
 * @param *ie is a pointer to a @c TCAN4x5x_Device_Interrupt_Enable struct containing the interrupt bit fields that will be updated
 */
void TCAN4x5x::Device_ReadInterruptEnable(TCAN4x5x_Device_Interrupt_Enable *ie){
    ie->word = AHB_READ_32(REG_DEV_IE);
}


/**
 * @brief Configures the device interrupt enable register
 *
 * Configures the device interrupt enable register based on the passed @c Device_Interrupt_Enable struct
 *
 * @param *ie is a pointer to a @c Device_Interrupt_Enable struct containing the desired enabled interrupt bits
 *
 * @return @c true if configuration successfully done, @c false if not
 */
bool TCAN4x5x::Device_ConfigureInterruptEnable(TCAN4x5x_Device_Interrupt_Enable *ie){
    AHB_WRITE_32(REG_DEV_IE, ie->word);
#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    uint32_t readValue = AHB_READ_32(REG_DEV_IE);       // Read value
    readValue &= REG_BITS_DEVICE_IE_MASK;               // Apply mask to ignore reserved
    if (readValue != (ie->word & REG_BITS_DEVICE_IE_MASK))
        return false;
#endif
    return true;
}


/**
 * @brief Sets the TCAN4x5x device mode
 *
 * Sets the TCAN4x5x device mode based on the input @c modeDefine enum
 *
 * @param modeDefine is an @c Device_Mode_Enum enum
 *
 * @return @c true if configuration successfully done, @c false if not
 */
bool TCAN4x5x::Device_SetMode(TCAN4x5x_Device_Mode_Enum modeDefine){
    uint32_t writeValue = (AHB_READ_32(REG_DEV_MODES_AND_PINS) & ~REG_BITS_DEVICE_MODE_DEVICEMODE_MASK);

    switch (modeDefine) {
        case TCAN4x5x_DEVICE_MODE_NORMAL:
            writeValue |= REG_BITS_DEVICE_MODE_DEVICEMODE_NORMAL;
            break;

        case TCAN4x5x_DEVICE_MODE_SLEEP:
            writeValue |= REG_BITS_DEVICE_MODE_DEVICEMODE_SLEEP;
            break;

        case TCAN4x5x_DEVICE_MODE_STANDBY:
            writeValue |= REG_BITS_DEVICE_MODE_DEVICEMODE_STANDBY;
            break;

        default:
            return false;
    }

    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, writeValue);

#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    writeValue &= REG_BITS_DEVICE_MODE_DEVICEMODE_MASK; // Mask out the part we care about verifying
    if ((AHB_READ_32(REG_DEV_MODES_AND_PINS) & REG_BITS_DEVICE_MODE_DEVICEMODE_MASK) != writeValue)
        return false;
#endif
    return true;
}


/**
 * @brief Reads the TCAN4x5x device mode
 *
 * Reads the TCAN4x5x device mode and returns a @c modeDefine enum
 *
 * @return A @c Device_Mode_Enum enum of the current state
 */
TCAN4x5x_Device_Mode_Enum TCAN4x5x::Device_ReadMode(void){
    uint32_t readValue = (AHB_READ_32(REG_DEV_MODES_AND_PINS) & REG_BITS_DEVICE_MODE_DEVICEMODE_MASK);

    switch (readValue) {
        case REG_BITS_DEVICE_MODE_DEVICEMODE_NORMAL:
            return TCAN4x5x_DEVICE_MODE_NORMAL;

        case REG_BITS_DEVICE_MODE_DEVICEMODE_SLEEP:
            return TCAN4x5x_DEVICE_MODE_SLEEP;

        case REG_BITS_DEVICE_MODE_DEVICEMODE_STANDBY:
            return TCAN4x5x_DEVICE_MODE_STANDBY;

        default:
            return TCAN4x5x_DEVICE_MODE_STANDBY;
    }
}


/**
 * @brief Sets the TCAN4x5x device test mode
 *
 * Sets the TCAN4x5x device test mode based on the input @c modeDefine enum
 *
 * @param modeDefine is an @c Device_Test_Mode_Enum enum
 *
 * @return @c true if configuration successfully done, @c false if not
 */
bool TCAN4x5x::Device_EnableTestMode(TCAN4x5x_Device_Test_Mode_Enum modeDefine){
    uint32_t readWriteValue = AHB_READ_32(REG_DEV_MODES_AND_PINS);
    readWriteValue &= ~REG_BITS_DEVICE_MODE_TESTMODE_MASK;					// Clear the bits that we are setting

    // Set the appropriate bits depending on the passed in value
    switch (modeDefine)
    {
        case TCAN4x5x_DEVICE_TEST_MODE_NORMAL:
            Device_DisableTestMode();
            break;

        case TCAN4x5x_DEVICE_TEST_MODE_CONTROLLER:
            readWriteValue |= REG_BITS_DEVICE_MODE_TESTMODE_CONTROLLER | REG_BITS_DEVICE_MODE_TESTMODE_EN;
            break;

        case TCAN4x5x_DEVICE_TEST_MODE_PHY:
            readWriteValue |= REG_BITS_DEVICE_MODE_TESTMODE_PHY | REG_BITS_DEVICE_MODE_TESTMODE_EN;
            break;

        default: return false;	    									// If an invalid value was passed, then we will return fail
    }
    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, readWriteValue);				// Write the updated values

#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    if (AHB_READ_32(REG_DEV_MODES_AND_PINS) != readWriteValue)
        return false;
#endif
    return true;
}


/**
 * @brief Disables the TCAN4x5x device test mode
 *
 * @return @c true if disabling test mode was successful, @c false if not
 */
bool TCAN4x5x::Device_DisableTestMode(void){
    uint32_t readWriteValue = AHB_READ_32(REG_DEV_MODES_AND_PINS);
    readWriteValue &= ~(REG_BITS_DEVICE_MODE_TESTMODE_MASK | REG_BITS_DEVICE_MODE_TESTMODE_ENMASK);	// Clear the bits
    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, readWriteValue);				// Write the updated values

#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    if (AHB_READ_32(REG_DEV_MODES_AND_PINS) != readWriteValue)
        return false;
#endif
    return true;
}


/**
 * @brief Reads the TCAN4x5x device test mode
 *
 * @return an @c TCAN4x5x_Device_Test_Mode_Enum of the current device test mode
 */
TCAN4x5x_Device_Test_Mode_Enum TCAN4x5x::Device_ReadTestMode(void){
    uint32_t readValue = AHB_READ_32(REG_DEV_MODES_AND_PINS);

    // If Test mode is enabled...
    if (readValue & REG_BITS_DEVICE_MODE_TESTMODE_ENMASK)
    {
        if (readValue & REG_BITS_DEVICE_MODE_TESTMODE_CONTROLLER)
        {
            return TCAN4x5x_DEVICE_TEST_MODE_CONTROLLER;
        } else {
            return TCAN4x5x_DEVICE_TEST_MODE_PHY;
        }
    }
    return TCAN4x5x_DEVICE_TEST_MODE_NORMAL;
}


/**
 * @brief Configure the watchdog
 *
 * @param WDTtimeout is an @c WDT_Timer_Enum enum of different times for the watch dog window
 *
 * @return @c true if successfully configured, or @c false otherwise
 */
bool TCAN4x5x::WDT_Configure(TCAN4x5x_WDT_Timer_Enum WDTtimeout){
    uint32_t readWriteValue = AHB_READ_32(REG_DEV_MODES_AND_PINS);
    readWriteValue &= ~REG_BITS_DEVICE_MODE_WD_TIMER_MASK;					// Clear the bits that we are setting

    // Set the appropriate bits depending on the passed in value
    switch (WDTtimeout)
    {
        case TCAN4x5x_WDT_60MS:
            readWriteValue |= REG_BITS_DEVICE_MODE_WD_TIMER_60MS;
            break;

        case TCAN4x5x_WDT_600MS:
            readWriteValue |= REG_BITS_DEVICE_MODE_WD_TIMER_600MS;
            break;

        case TCAN4x5x_WDT_3S:
            readWriteValue |= REG_BITS_DEVICE_MODE_WD_TIMER_3S;
            break;

        case TCAN4x5x_WDT_6S:
            readWriteValue |= REG_BITS_DEVICE_MODE_WD_TIMER_6S;
            break;

        default: return false;									// If an invalid value was passed, then we will return fail
    }
    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, readWriteValue);				// Write the updated values

#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    if (AHB_READ_32(REG_DEV_MODES_AND_PINS) != readWriteValue)
        return false;
#endif
    return true;
}


/**
 * @brief Read the watchdog configuration
 *
 * @return an @c TCAN4x5x_WDT_Timer_Enum enum of the currently configured time window
 */
TCAN4x5x_WDT_Timer_Enum TCAN4x5x::WDT_Read(void){
    uint32_t readValue = AHB_READ_32(REG_DEV_MODES_AND_PINS);
    readValue &= REG_BITS_DEVICE_MODE_WD_TIMER_MASK;

    switch (readValue)
    {
        case REG_BITS_DEVICE_MODE_WD_TIMER_60MS:
            return TCAN4x5x_WDT_60MS;

        case REG_BITS_DEVICE_MODE_WD_TIMER_600MS:
            return TCAN4x5x_WDT_600MS;

        case REG_BITS_DEVICE_MODE_WD_TIMER_3S:
            return TCAN4x5x_WDT_3S;

        case REG_BITS_DEVICE_MODE_WD_TIMER_6S:
            return TCAN4x5x_WDT_6S;

        default: return TCAN4x5x_WDT_60MS;									// If an invalid value was passed, then we will return the POR default
    }
}


/**
 * @brief Enable the watchdog timer
 *
 * @return @c true if successfully enabled, or @c false otherwise
 */
bool TCAN4x5x::WDT_Enable(void){
    uint32_t readWriteValue = AHB_READ_32(REG_DEV_MODES_AND_PINS) | REG_BITS_DEVICE_MODE_WDT_EN;
    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, readWriteValue);		// Enable the watch dog timer

#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    if (AHB_READ_32(REG_DEV_MODES_AND_PINS) != readWriteValue)
        return false;
#endif

    return true;
}


/**
 * @brief Disable the watchdog timer
 *
 * @return @c true if successfully disabled, or @c false otherwise
 */
bool TCAN4x5x::WDT_Disable(void){
    uint32_t writeValue = AHB_READ_32(REG_DEV_MODES_AND_PINS);
    writeValue &= ~REG_BITS_DEVICE_MODE_WDT_EN;					// Clear the EN bit
    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, writeValue);			// Disable the watch dog timer

#ifdef DEVICE_VERIFY_CONFIGURATION_WRITES
    // Check to see if the write was successful.
    if (AHB_READ_32(REG_DEV_MODES_AND_PINS) != writeValue)
        return false;
#endif
        return true;
}


/**
 * @brief Reset the watchdog timer
 */
void TCAN4x5x::WDT_Reset(void){
    uint32_t writeValue = AHB_READ_32(REG_DEV_MODES_AND_PINS);
    writeValue |= REG_BITS_DEVICE_MODE_WDT_RESET_BIT;
    AHB_WRITE_32(REG_DEV_MODES_AND_PINS, writeValue);		// Reset the watch dog timer
}