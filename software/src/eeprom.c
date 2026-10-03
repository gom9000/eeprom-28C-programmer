/*
 * eeprom.c
 *
 *  ___ ___ ___ ___  ___  __  __                                                     
 * | __| __| _ \ _ \/ _ \|  \/  |___ _ __ _ _ ___  __ _ _ _ __ _ _ __  _ __  ___ _ _ 
 * | _|| _||  _/   / (_) | |\/| |___| '_ \ '_/ _ \/ _` | '_/ _` | '  \| '  \/ -_) '_|
 * |___|___|_| |_|_\\___/|_|  |_|   | .__/_| \___/\__, |_| \__,_|_|_|_|_|_|_\___|_|  
 *                                  |_|           |___/                              
 *
 * Author.....: Alessandro Fraschetti (mail: gos95@gommagomma.net)
 * Target.....: RaspberryPI
 * Version....: 1.2 2026/10/03
 * Description: EEPROM 28C-family programmer utility
 * URL........: https://github.com/gom9000/xp-eeprom-28C-programmer
 * License....: this program is under the terms of MIT License
 * Module.....: eeprom operations library
 * Notes......: ADDR BUS : A0-A14 = 8,9,7  0,2,3  12,13,14  30,21,22,23,24,25 (wiringPi pin numbers)
 *              DATA BUS : D0-D7  = 15,16,1  4,5  6,10,11
 *              CTRL BUS : CE=27, OE=28, WE=29
 *
 */


#include <stdio.h>
#include <wiringPi.h>
#include "eeprom.h"


uint8_t D[] = {15,16,1,4,5,6,10,11}; // D0-D7
uint8_t A[] = {8,9,7,0,2,3,12,13,14,30,21,22,23,24,25}; //A0-A14
uint8_t CE = 27;
uint8_t OE = 28;
uint8_t WE = 29;


void setup()
{
    for (unsigned char ii=0; ii<BUS_SIZE(A); ii++)
    {
        pinMode(A[ii], OUTPUT);
        digitalWrite(A[ii], LOW);
    }

    setDataBusDirection(INPUT);

    pinMode(CE, OUTPUT);
    pinMode(OE, OUTPUT);
    pinMode(WE, OUTPUT);

    digitalWrite(CE, HIGH);
    digitalWrite(OE, HIGH);
    digitalWrite(WE, HIGH);
}

void setAddress(address_t address)
{
    for (unsigned char ii=0; ii<BUS_SIZE(A); ii++)
    {
        digitalWrite(A[ii], address & 1);
        address >>= 1;
    }
}

void setData(data_t data)
{
    for (unsigned char ii=0; ii<BUS_SIZE(D); ii++)
    {
        digitalWrite(D[ii], data & 1);
        data >>= 1;
    }
}

data_t getData()
{
    data_t data = 0;

    for (unsigned char ii=BUS_SIZE(D); ii>0; ii--)
        data = (data << 1) | digitalRead(D[ii-1]);

    return data;
}

void setDataBusDirection(unsigned char dir)
{
    for (unsigned char ii=0; ii<BUS_SIZE(D); ii++)
        pinMode(D[ii], dir);
}

data_t readROM(address_t address)
{
    data_t data = 0;
    setDataBusDirection(INPUT);
    setAddress(address);
    digitalWrite(CE, LOW);
    digitalWrite(OE, LOW);
    delayMicroseconds(1);
    data = getData();
    digitalWrite(OE, HIGH);
    digitalWrite(CE, HIGH);

    return data;
}

void writeROM(address_t address, data_t data)
{
    setAddress(address);
    setDataBusDirection(OUTPUT);
    setData(data);
    digitalWrite(CE, LOW);
    digitalWrite(WE, LOW);
    delayMicroseconds(1);
    digitalWrite(WE, HIGH);
    digitalWrite(CE, HIGH);
}

/*
 * Wait for the end of the internal write cycle using DATA polling: while the
 * cycle is in progress, a read returns the complement of bit 7 of the last
 * written byte; at the end it returns the true data.
 * Returns 0 if the location holds the expected data, -1 on timeout/mismatch
 * (e.g. Software Data Protection enabled: the write is ignored).
 */
int waitForWriteCycle(address_t address, data_t data)
{
    unsigned int t0;

    /* the write cycle starts only when the byte/page load window (tBLC, 150us)
     * expires after the last WE rising edge: polling earlier reads old data */
    delayMicroseconds(TBLC_WAIT_US);
    t0 = micros();

    while ((readROM(address) & 0x80) != (data & 0x80))
        if (micros() - t0 > WRITE_TIMEOUT_US) return -1;

    /* D7 may become valid slightly before D0-D6: read again the whole byte */
    return (readROM(address) == data)? 0 : -1;
}

/*
 * Write length bytes of buf starting from address start.
 * pagesize <= 1: byte mode, one write cycle per byte (all 28C devices).
 * pagesize > 1: page mode (28C64/28C256: 64 bytes), up to pagesize bytes
 *               loaded within tBLC (150us) and written in one cycle.
 * Each block is verified; failed bytes are retried once in byte mode.
 * Returns the number of locations that could not be written; stops early
 * after MAX_WRITE_ERRORS failures (e.g. Software Data Protection enabled).
 */
length_t programROM(address_t start, const data_t *buf, length_t length, length_t pagesize)
{
    length_t errors = 0;
    length_t ii = 0;

    if (pagesize < 1) pagesize = 1;
    while (ii < length)
    {
        address_t address = start + ii;
        length_t n = pagesize - (address % pagesize); /* stay within the page */
        if (n > length - ii) n = length - ii;

        for (length_t kk=0; kk<n; kk++)
            writeROM(address+kk, buf[ii+kk]);
        if (waitForWriteCycle(address+n-1, buf[ii+n-1]))
            delay(10); /* timeout: be sure the cycle is over before verifying */

        for (length_t kk=0; kk<n; kk++)
        {
            if (readROM(address+kk) == buf[ii+kk]) continue;
            writeROM(address+kk, buf[ii+kk]);
            if (waitForWriteCycle(address+kk, buf[ii+kk]) && ++errors >= MAX_WRITE_ERRORS)
                return errors;
        }
        ii += n;
    }
    return errors;
}

length_t eraseROM(address_t start, length_t length, data_t blank, length_t pagesize)
{
    data_t buf[MAX_PAGE_SIZE];
    length_t errors = 0;
    length_t ii = 0;

    for (unsigned int kk=0; kk<MAX_PAGE_SIZE; kk++) buf[kk] = blank;
    while (ii < length)
    {
        /* chunks aligned to MAX_PAGE_SIZE, so they never cross a page */
        length_t n = MAX_PAGE_SIZE - ((start + ii) % MAX_PAGE_SIZE);
        if (n > length - ii) n = length - ii;
        errors += programROM(start+ii, buf, n, pagesize);
        if (errors >= MAX_WRITE_ERRORS) break;
        ii += n;
    }
    return errors;
}

length_t testROM(address_t start, length_t length, data_t data)
{
    length_t count = 0;
    for (address_t address=start; address<start+length; address++)
       if (readROM(address) != data) count++;

    return count;
}

void dumpROM(address_t start, length_t length, FILE *outstream, unsigned char ascii)
{
    if (!ascii)
    {
        for (address_t address=start; address<start+length; address++)
            fputc(readROM(address), outstream);
        return;
    }

    if (outstream == stdout && start%16)
    {
        printf("%.5X:  ", start-start%16);
        for (unsigned char ii=0; ii<start%16; ii++)
            printf("   ");
    }

    for (address_t address=start; address<start+length; address++)
    {
        if (outstream == stdout && !(address%16)) printf("%.5X:  ", address);
        fprintf(outstream, "%.2X", readROM(address));
        if (outstream == stdout) printf(" ");
        if (address%16 == 15) fprintf(outstream, "\r\n");
    }
    if ((start+length)%16) fprintf(outstream, (outstream == stdout)? "\n" : "\r\n");
}

void setSDPMode(unsigned char mode)
{
    if (mode)
    {
        writeROM(0x5555, 0xAA);
        writeROM(0x2AAA, 0x55);
        writeROM(0x5555, 0xA0);
    } else {
        writeROM(0x5555, 0xAA);
        writeROM(0x2AAA, 0x55);
        writeROM(0x5555, 0x80);
        writeROM(0x5555, 0xAA);
        writeROM(0x2AAA, 0x55);
        writeROM(0x5555, 0x20);
    }
    delay(10); /* wait for the internal write cycle to complete */
}
