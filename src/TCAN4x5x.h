/*
 * TCAN4550.h
 * Description: This file contains TCAN4550 functions, and relies on the TCAN4x5x_SPI abstraction functions
 *
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

#ifndef TCAN4550_H_
#define TCAN4550_H_

// Arduino IDE SPI library
#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>

#include "TCAN4x5x_Reg.h"
#include "TCAN4x5x_Data_Structs.h"

//! If TCAN4x5x_MCAN_CACHE_CONFIGURATION is defined, then the read and write to MRAM functions will cache certain values to reduce the number of SPI reads necessary to send or receive a packet
#define TCAN4x5x_MCAN_CACHE_CONFIGURATION

//! If TCAN4x5x_MCAN_VERIFY_CONFIGURATION_WRITES is defined, then each MCAN configuration write will be read and verified for correctness
#define TCAN4x5x_MCAN_VERIFY_CONFIGURATION_WRITES

//! If TCAN4x5x_DEVICE_VERIFY_CONFIGURATION_WRITES is defined, then each device configuration write will be read and verified for correctness
#define TCAN4x5x_DEVICE_VERIFY_CONFIGURATION_WRITES

// Recreates the TI hardware register macros for Arduino
#define HWREG8(x)  (*((volatile uint8_t *)(x)))
#define HWREG16(x) (*((volatile uint16_t *)(x)))
#define HWREG32(x) (*((volatile uint32_t *)(x)))

// Defines for caching the configuration values to save on reads and writes
#ifdef TCAN4x5x_MCAN_CACHE_CONFIGURATION
#define TCAN4x5x_MCAN_CACHE_SIDFC           0
#define TCAN4x5x_MCAN_CACHE_XIDFC           1
#define TCAN4x5x_MCAN_CACHE_RXF0C           2
#define TCAN4x5x_MCAN_CACHE_RXF1C           3
#define TCAN4x5x_MCAN_CACHE_RXBC            4
#define TCAN4x5x_MCAN_CACHE_TXEFC           5
#define TCAN4x5x_MCAN_CACHE_TXBC            6
#define TCAN4x5x_MCAN_CACHE_RXESC           7
#define TCAN4x5x_MCAN_CACHE_TXESC           8
#endif

//------------------------------------------------------------------------
// AHB Access Op Codes
//------------------------------------------------------------------------
#define AHB_WRITE_OPCODE                            0x61
#define AHB_READ_OPCODE                             0x41

//------------------------------------------------------------------------
// SPI Default settings
//------------------------------------------------------------------------
#define SPI_DEFAULT_CLOCK_SPEED_HZ                        10000000  // min 8MHz max 18MHz
#define SPI_DEFAULT_BIT_ORDER                             MSBFIRST
#define SPI_DEFAULT_DATA_MODE                             SPI_MODE0
#define SPI_DEFAULT_SETTINGS                              SPISettings(SPI_DEFAULT_CLOCK_SPEED_HZ, SPI_DEFAULT_BIT_ORDER, SPI_DEFAULT_DATA_MODE)

typedef enum { RXFIFO0, RXFIFO1 } TCAN4x5x_MCAN_FIFO_Enum;
typedef enum { TCAN4x5x_WDT_60MS, TCAN4x5x_WDT_600MS, TCAN4x5x_WDT_3S, TCAN4x5x_WDT_6S } TCAN4x5x_WDT_Timer_Enum;
typedef enum { TCAN4x5x_DEVICE_TEST_MODE_NORMAL, TCAN4x5x_DEVICE_TEST_MODE_PHY, TCAN4x5x_DEVICE_TEST_MODE_CONTROLLER } TCAN4x5x_Device_Test_Mode_Enum;
typedef enum { TCAN4x5x_DEVICE_MODE_NORMAL, TCAN4x5x_DEVICE_MODE_STANDBY, TCAN4x5x_DEVICE_MODE_SLEEP } TCAN4x5x_Device_Mode_Enum;


class TCAN4x5x {
public:
    // ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
    //                            Constructors & SPI Setup
    // ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
    TCAN4x5x(uint8_t csPin);
    TCAN4x5x(uint8_t csPin, uint8_t MISO_Pin, uint8_t MOSI_Pin, uint8_t SCK_Pin);
    ~TCAN4x5x();

    // Pre-begin() Configuration Methods
    void setNominalBitRate(uint16_t prescaler, uint8_t tqBefore, uint8_t tqAfter);
    void setDataBitRate(uint16_t prescaler, uint8_t tqBefore, uint8_t tqAfter);
    void setMRAMConfig(TCAN4x5x_MRAM_Config *config);
    void setDeviceConfig(TCAN4x5x_DEV_CONFIG *config);
    void setGlobalFilterConfig(TCAN4x5x_MCAN_Global_Filter_Configuration *config);

    void begin(SPIClass *spiPort, SPISettings spiSettings = SPI_DEFAULT_SETTINGS);
    void setSPISettings(SPISettings spiSettings);
    void resetSPISettings(void);
    void setCS(uint8_t csPin);
    void setSPIPins(uint8_t MISO_Pin, uint8_t MOSI_Pin, uint8_t SCK_Pin);

    // ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
    //                         Hardware Interrupt Routing
    // ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
    void setGPO2CANIDList(uint16_t idList[], uint8_t listSize);
    void setGPIO1CANIDList(uint16_t idList[], uint8_t listSize);
    
    void setGPO2Interrupt(void);
    void setGPIO1Interrupt(void);
    

    // Direct ISR Data Extractors
    TCAN_RX_Packet handleReceiveISR(void);
    TCAN_RX_Packet handleGPOISR(void);
    TCAN4x5x_Device_Interrupts handleGPIO1ISR(void);

    // ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
    //                            MCAN Device Functions
    // ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
    bool MCAN_EnableProtectedRegisters(void);
    bool MCAN_DisableProtectedRegisters(void);
    bool MCAN_ConfigureCCCRRegister(TCAN4x5x_MCAN_CCCR_Config *cccr);
    bool MCAN_ConfigureDataTiming_Simple(TCAN4x5x_MCAN_Data_Timing_Simple *dataTiming);
    bool MCAN_ConfigureDataTiming_Raw(TCAN4x5x_MCAN_Data_Timing_Raw *dataTiming);
    bool MCAN_ConfigureNominalTiming_Simple(TCAN4x5x_MCAN_Nominal_Timing_Simple *nomTiming);
    bool MCAN_ConfigureNominalTiming_Raw(TCAN4x5x_MCAN_Nominal_Timing_Raw *nomTiming);
    bool MCAN_ConfigureGlobalFilter(TCAN4x5x_MCAN_Global_Filter_Configuration *gfc);

    void MCAN_ReadCCCRRegister(TCAN4x5x_MCAN_CCCR_Config *cccrConfig);
    void MCAN_ReadDataTimingFD_Simple(TCAN4x5x_MCAN_Data_Timing_Simple *dataTiming);
    void MCAN_ReadDataTimingFD_Raw(TCAN4x5x_MCAN_Data_Timing_Raw *dataTiming);
    void MCAN_ReadNominalTiming_Simple(TCAN4x5x_MCAN_Nominal_Timing_Simple *nomTiming);
    void MCAN_ReadNominalTiming_Raw(TCAN4x5x_MCAN_Nominal_Timing_Raw *nomTiming);

    void MRAM_Clear(void);
    bool MRAM_Configure(TCAN4x5x_MRAM_Config *MRAMConfig);
    
    void MCAN_ReadInterrupts(TCAN4x5x_MCAN_Interrupts *ir);
    void MCAN_ClearInterrupts(TCAN4x5x_MCAN_Interrupts *ir);
    void MCAN_ClearInterruptsAll(void);
    void MCAN_ReadInterruptEnable(TCAN4x5x_MCAN_Interrupt_Enable *ie);
    void MCAN_ConfigureInterruptEnable(TCAN4x5x_MCAN_Interrupt_Enable *ie);
    
    bool MCAN_TransmitBufferContents(uint8_t bufIndex);
    bool MCAN_WriteSIDFilter(uint8_t filterIndex, TCAN4x5x_MCAN_SID_Filter *filter);
    bool MCAN_ReadSIDFilter(uint8_t filterIndex, TCAN4x5x_MCAN_SID_Filter *filter);
    bool MCAN_WriteXIDFilter(uint8_t fifoIndex, TCAN4x5x_MCAN_XID_Filter *filter);
    bool MCAN_ReadXIDFilter(uint8_t filterIndex, TCAN4x5x_MCAN_XID_Filter *filter);

    
    uint8_t MCAN_ReadNextFIFO(TCAN4x5x_MCAN_FIFO_Enum FIFODefine, TCAN4x5x_MCAN_RX_Header *header, uint8_t dataPayload[]);
    uint8_t MCAN_ReadRXBuffer(uint8_t bufIndex, TCAN4x5x_MCAN_RX_Header *header, uint8_t dataPayload[]);
    uint8_t MCAN_DLCtoBytes(uint8_t inputDLC);
    uint8_t MCAN_TXRXESC_DataByteValue(uint8_t inputESCValue);
    uint32_t MCAN_WriteTXBuffer(uint8_t bufIndex, TCAN4x5x_MCAN_TX_Header *header, uint8_t dataPayload[]);


// ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
    //                            Non-MCAN Device Functions
    // ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
    void Device_ReadConfig(TCAN4x5x_DEV_CONFIG *devCfg);
    void Device_ReadInterrupts(TCAN4x5x_Device_Interrupts *ir);
    void Device_ClearInterrupts(TCAN4x5x_Device_Interrupts *ir);
    void Device_ClearInterruptsAll(void);
    void Device_ClearSPIERR(void);
    void Device_ReadInterruptEnable(TCAN4x5x_Device_Interrupt_Enable *ie);
    void WDT_Reset(void);
    
    bool Device_Configure(TCAN4x5x_DEV_CONFIG *devCfg);
    bool Device_ConfigureInterruptEnable(TCAN4x5x_Device_Interrupt_Enable *ie);
    bool Device_SetMode(TCAN4x5x_Device_Mode_Enum modeDefine);
    bool Device_EnableTestMode(TCAN4x5x_Device_Test_Mode_Enum modeDefine);
    bool Device_DisableTestMode(void);
    bool WDT_Configure(TCAN4x5x_WDT_Timer_Enum WDTtimeout);
    bool WDT_Enable(void);
    bool WDT_Disable(void);

    uint16_t Device_ReadDeviceVersion(void);
    TCAN4x5x_WDT_Timer_Enum WDT_Read(void);
    TCAN4x5x_Device_Mode_Enum Device_ReadMode(void);
    TCAN4x5x_Device_Test_Mode_Enum Device_ReadTestMode(void);


private:
    uint8_t nCS = 0xFF;
    uint8_t MOSI = 0xFF;
    uint8_t MISO = 0xFF;
    uint8_t SCK = 0xFF;

    SPISettings tcan_port_settings;
    SPIClass* tcan_port;


    // Internal state variables that store the configuration before begin() is called
    TCAN4x5x_MCAN_Nominal_Timing_Simple _nomTiming = {0};
    TCAN4x5x_MCAN_Data_Timing_Simple _dataTiming = {0};
    TCAN4x5x_MCAN_CCCR_Config _cccrConfig = {0};
    TCAN4x5x_MCAN_Global_Filter_Configuration _gfc = {0};
    TCAN4x5x_MRAM_Config _mramConfig = {0};
    TCAN4x5x_DEV_CONFIG _devConfig = {0};
    TCAN4x5x_MCAN_Interrupt_Enable _mcanIE = {0};
    
    // Internal helper to load default TI settings
    void loadDefaultSettings(void);

    //------------------------------------------------------------------------
    //							Write Functions
    //------------------------------------------------------------------------
    void AHB_WRITE_32(uint16_t address, uint32_t data);
    void AHB_WRITE_BURST_START(uint16_t address, uint8_t words);
    void AHB_WRITE_BURST_WRITE(uint32_t data);
    void AHB_WRITE_BURST_END(void);

    //--------------------------------------------------------------------------
    //							Read Functions
    //--------------------------------------------------------------------------
    uint32_t AHB_READ_32(uint16_t address);
    void AHB_READ_BURST_START(uint16_t address, uint8_t words);
    uint32_t AHB_READ_BURST_READ(void);
    void AHB_READ_BURST_END(void);

}; 

#endif