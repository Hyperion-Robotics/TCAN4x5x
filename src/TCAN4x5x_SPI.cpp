/*
 * TCAN4x5x_SPI.c
 * Description: This file is responsible for abstracting the lower-level microcontroller SPI read and write functions
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


#include "TCAN4x5x.h"

/*
 * @brief Single word write
 *
 * @param address A 16-bit address of the destination register
 * @param data A 32-bit word of data to write to the destination register
 */
void TCAN4x5x::AHB_WRITE_32(uint16_t address, uint32_t data){
    AHB_WRITE_BURST_START(address, 1);
    AHB_WRITE_BURST_WRITE(data);
    AHB_WRITE_BURST_END();
}


/*
 * @brief Single word read
 *
 * @param address A 16-bit address of the source register
 *
 * @return Returns 32-bit word of data from source register
 */
uint32_t TCAN4x5x::AHB_READ_32(uint16_t address) {
    uint32_t returnData;

    AHB_READ_BURST_START(address, 1);
    returnData = AHB_READ_BURST_READ();
    AHB_READ_BURST_END();

    return returnData;
}


/*
 * @brief Burst write start
 *
 * The SPI transaction contains 3 parts: the header (start), the payload, and the end of data (end)
 * This function is the start, where the register address and number of words are transmitted
 *
 * @param address A 16-bit address of the destination register
 * @param words The number of 4-byte words that will be transferred. 0 = 256 words
 */
void TCAN4x5x::AHB_WRITE_BURST_START(uint16_t address, uint8_t words){
    // Initialize SPI transaction parameters
    tcan_port->beginTransaction(tcan_port_settings);

    // Set CS pin low to wake up the TCAN chip and START the transaction
    digitalWrite(nCS, LOW);

    // Send the Write Opcode 
    // (Assuming AHB_WRITE_OPCODE is defined in your header, just like READ)
    tcan_port->transfer(AHB_WRITE_OPCODE);

    // Send the 16-bit address
    tcan_port->transfer((address >> 8) & 0xFF); // Send High Byte
    tcan_port->transfer(address & 0xFF);        // Send Low Byte
   
    // Send the number of words to write
    tcan_port->transfer(words);
}

/*
 * @brief Burst write
 *
 * The SPI transaction contains 3 parts: the header (start), the payload, and the end of data (end)
 * This function writes a single word at a time
 *
 * @param data A 32-bit word of data to write to the destination register
 */
void TCAN4x5x::AHB_WRITE_BURST_WRITE(uint32_t data) {
    // Arduino transfer() is blocking, so no WAIT_FOR_TRANSMIT is needed.
    
    // Extract and send the 4 bytes from highest (MSB) to lowest (LSB)
    tcan_port->transfer((data >> 24) & 0xFF); // Send Byte 3
    tcan_port->transfer((data >> 16) & 0xFF); // Send Byte 2
    tcan_port->transfer((data >> 8) & 0xFF);  // Send Byte 1
    tcan_port->transfer(data & 0xFF);         // Send Byte 0
}


/*
 * @brief Burst write end
 *
 * The SPI transaction contains 3 parts: the header (start), the payload, and the end of data (end)
 * This function ends the burst transaction by pulling nCS high
 */
void TCAN4x5x::AHB_WRITE_BURST_END(void){
    // Set CS high to end the SPI transaction
    digitalWrite(nCS, HIGH); 
    
    // Release the SPI bus so other devices can use it
    tcan_port->endTransaction();
}


/*
 * @brief Burst read start
 *
 * The SPI transaction contains 3 parts: the header (start), the payload, and the end of data (end)
 * This function is the start, where the register address and number of words are transmitted
 *
 * @param address A 16-bit start address to begin the burst read
 * @param words The number of 4-byte words that will be transferred. 0 = 256 words
 */
void TCAN4x5x::AHB_READ_BURST_START(uint16_t address, uint8_t words){
    // Initialize SPI transaction parameters
    tcan_port->beginTransaction(tcan_port_settings);

    // Set CS pin low to wake up the TCAN chip and START the transaction
    digitalWrite(nCS, LOW);

    // Send the Read Opcode
    tcan_port->transfer(AHB_READ_OPCODE);

    // Send the 16-bit address (Standard bit-shifting is 100% safe)
    tcan_port->transfer((address >> 8) & 0xFF); // Send High Byte
    tcan_port->transfer(address & 0xFF);        // Send Low Byte
   
    // Send the number of words to read
    tcan_port->transfer(words);

}


/*
 * @brief Burst read start
 *
 * The SPI transaction contains 3 parts: the header (start), the payload, and the end of data (end)
 * This function where each word of data is read from the TCAN4x5x
 *
 * @return A 32-bit single data word that is read at a time
 */
uint32_t TCAN4x5x::AHB_READ_BURST_READ(void){
    uint8_t readData;
    uint8_t readData1;
    uint8_t readData2;
    uint8_t readData3;
    uint32_t returnData;

    // Send dummy bytes (0x00) to keep the clock running.
    // The target chip will send back the actual data during these clocks.
    readData  = tcan_port->transfer(0x00);
    readData1 = tcan_port->transfer(0x00);
    readData2 = tcan_port->transfer(0x00);
    readData3 = tcan_port->transfer(0x00);

    // Combine the 4 separate bytes into a single 32-bit word
    returnData = (((uint32_t)readData) << 24) | 
                 (((uint32_t)readData1) << 16) | 
                 (((uint32_t)readData2) << 8) | 
                 ((uint32_t)readData3);


    return returnData;
}


/*
 * @brief Burst write end
 *
 * The SPI transaction contains 3 parts: the header (start), the payload, and the end of data (end)
 * This function ends the burst transaction by pulling nCS high
 */
void TCAN4x5x::AHB_READ_BURST_END(void){
    // Set CS high to end the SPI transaction
    digitalWrite(nCS, HIGH); 
    
    // Release the SPI bus so other devices can use it
    tcan_port->endTransaction();
}
