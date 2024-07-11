//****************************************************************************
// Driver for FM Transmitter KT0803L (should also work with KT0803K & KT0803M)
//
// It is recommented to decrease the Wire time out significatly. I tested with
// 10 ms, so after "Wire.begin();" call "Wire.setTimeout(10);"
//****************************************************************************

#include <arduino.h>

#include "KT0803X.h"

// I monitored the I2C communicaton of some car MP3 players:
// On Renkforce FM tranmitter with KT0803M (set to 87.5 MHz)
//   Init:       Reg 0x02 = 0b00000101 (Eur pre-emph, pilot tone high)
//               Reg 0x13 = 0b10000000 (RFGain[2])
//               Reg 0x00 = 0b01101011 (channel[8:1])
//               Reg 0x01 = 0b11100011 (channel[11:9] + RFGain[1:0] + PGA[2:0])
//
//  new channel: Reg 0x00 = 0b01101011 (channel[8:1]
//               Reg 0x01 = 0b11100011 (channel[11:9] + RFGain[1:0] + PGA[2:0])
//
// On no-name FM "CAR MP3 PLAYER" with KT0803L (set to 87.5 MHz)
//   Init:       Reg 0x1E = 0x60 = 0b01100000 (External clock, clock = 32768 Hz)
//               Reg 0x17 = 0x40 = 0b01000000 (Frequency deviation 112.5 kHz)
//               Reg 0x04 = 0x34 = 0b00110100 (PGA_LSB = 3, old FDEV = 112.5 kHz)
//               Reg 0x13 = 0x80 = 0b10000000 (RFGain[2])
//               Reg 0x00 = 0x6B = 0b01101011 (CHSEL[8:1])
//               Reg 0x01 = 0xEB = 0b11101011 (CHSEL[11:9] + RFGain[1:0] + PGA[2:0])
//               Reg 0x02 = 0x40 = 0b00000100 (USA pre-emph, pilot tone high)
//
//  new channel: Reg 0x00 = 0x38 = 0b00111000 (CHSEL[8:1] to 108.0 MHz
//               Reg 0x01 = 0xEC = 0b11101100 (CHSEL[11:9] + RFGain[1:0] + PGA[2:0])
//               Reg 0x02 = 0x40 = 0b00000100 (USA pre-emph, pilot tone high)

#define KT803X_SLAVE_ADDRESS 0x3E  // Slave adress is 0x3E 

#define KT0803_DEBUG_PRINT(s)   Serial.print(s)
#define KT0803_DEBUG_PRINTLN(s) Serial.println(s)

#define PGA_LSB   ((_pga & 0x03) << 4)
#define PGA       ((_pga & 0x1C) << 1)
#define FREQ_LSB  (_freq & 0xFF)
#define FREQ_MSB  ((_freq >> 8) & 7)
#define MONO      _mono

#define KT803X_REG_0X00     0x00,  FREQ_LSB     /* CHSEL[8:1]              */

#define KT803X_REG_0X01     0x01,( 0xC0 |       /* RFGAIN[1:0]             */ \
                                   PGA  |       /* PGA[2:0]                */ \
                                   FREQ_MSB)    /* CHSEL[11:9]             */

#define KT803X_REG_0X02     0x02,( 0x00 |       /* CHSEL[0] (always 0)     */ \
                                   0x40 |       /* RFGAIN[3]               */ \
                                   0x00 |       /* MUTE (always off)       */ \
                                   0x04 |       /* PLTADJ (set to high)    */ \
                                   0x01 )       /* PHTCNST (50us = Europe) */

#define KT803X_REG_0X04     0x04,( 0x00 |       /* ALC_EN (disabled, KT0803L only)       */ \
                                   MONO |       /* MONO / STEREO                         */ \
                                   PGA_LSB |    /* PGA_LSB[1:0]                          */ \
                                   0x04 |       /* FDEV[1:0] (112.5 kHz, not in KT0803L) */ \
                                   0x00 )       /* BASS[1:0] (no bass enhance)           */

#define KT803X_REG_0X0B_ON  0x0B,  0x00         /* Standy off, PDPA on,       */ 
#define KT803X_REG_0X0B_OFF 0x0B,  0xA0         /* Standy on, PDPA off,       */ 

#define KT803X_REG_0X10     0x10,( 0x18 |       /* LMTLVL[1:0] (0.9625, not in KT0803L)  */ \
                                   0x01 )       /* Use PGA 1 dB steps                    */

#define KT803X_REG_0X13     0x13,( 0x80 |       /* RFGAIN[2]               */ \
                                   0x00 )       /* PA_CTRL (internal power)*/
#if 1
#define KT803L_REG_0X17     0x17,( 0x40 |       /* FDEV (112.5 kHz)           */ \
     /* KT0803L only) */           0x00 |       /* AU_ENHANCE (disabled)      */ \
                                   0x00 )       /* XTAL_SEL (32786 Hz)        */
#else
#define KT803L_REG_0X17     0x17,( 0x00 |       /* FDEV (75 kHz)              */ \
     /* KT0803L only) */           0x00 |       /* AU_ENHANCE (disabled)      */ \
                                   0x00 )       /* XTAL_SEL (32786 Hz)        */
#endif
/////////////////////////////////////////////////////////////////////////////////////////////
// Protected functions
/////////////////////////////////////////////////////////////////////////////////////////////

int KT0803X_Class::ReadReg(int reg) {
  uint8_t val = 0;
  _wire->beginTransmission(KT803X_SLAVE_ADDRESS);
  _wire->write(reg);
  _wire->endTransmission(false);
  _wire->requestFrom(KT803X_SLAVE_ADDRESS, 1);
  return _wire->readBytes(&val, 1) == 1 ? val : -1;
}

int KT0803X_Class::WriteReg(int reg, int val) {
  _wire->beginTransmission(KT803X_SLAVE_ADDRESS);
  _wire->write(reg);
  _wire->write(val);
  return _wire->endTransmission();
}

void KT0803X_Class::WriteFreqPga(void) {
  if(_chip_present) {
    WriteReg(KT803X_REG_0X00);  // CHANSEL[8:1]
    WriteReg(KT803X_REG_0X01);  // CHANSEL[11:9] + PGA
    WriteReg(KT803X_REG_0X04);  // PGA_LSB + MONO
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Public functions
/////////////////////////////////////////////////////////////////////////////////////////////

#ifdef PIN_KT0803X_ENABLE
KT0803X_Class::KT0803X_Class(TwoWire &wire, uint8_t pin_power_on, uint32_t current_speed) {
  _pin_power_on = pin_power_on;
#else
KT0803X_Class::KT0803X_Class(TwoWire &wire, uint32_t current_speed) {
#endif
  _prevspeed    = current_speed;
  _wire         = &wire;
  _mono         = 0x00; // Stereo
  _pga          = KT0803X_DEFAULT_PGA;
  _freq         = KT0803X_DEFAULT_FREQ;
}

void KT0803X_Class::PrintRegs(void) {
  const uint8_t regs_nr[] = { 0x0,0x1,0x2,0x4,0xB,0xC,0xE,0xF,0x10,0x12,0x13,0x14,0x15,0x16,0x17,0x1E,0x26,0x27 };
  uint16_t freq; 
  uint8_t  reg, pga, rfgain, pilot, preemp;
  //int8_t   pga_db;
  const char *ms;
  Serial.println("KT0803L register dump (see datasheet at http://radio-z.ucoz.lv/kt_0803/KT0803L_V1.3.pdf)");
  if(_chip_present) {
    Serial.print("Register: ");
    for(int i = 0; i <sizeof(regs_nr); i++) Serial.printf("0x%02X ",regs_nr[i]);
    Serial.println();
    Serial.print("Contents: ");
    for(int i = 0; i <sizeof(regs_nr); i++) {
      reg = ReadReg(regs_nr[i]);
      Serial.printf("0x%02X ",reg);
      switch(regs_nr[i]) {
        case 0x00: freq = reg;
                   break;
        case 0x01: freq    += ((uint16_t)reg & 7) << 8;
                   rfgain  = reg >> 6;
                   pga     = (reg >> 3) & 7;
                   break;
        case 0x02: freq    = (freq << 1) + (reg >> 7);
                   rfgain += (reg >> 3) & 8;
                   freq   *= 5; 
                   pilot   = reg & 4;
                   preemp  = reg & 1;
                   break;
        case 0x04: pga     = (pga << 2) + ((reg >> 4) & 3);
                   ms      = (reg & 0x40) ? "mono" : "stereo";
                   break;
        case 0x13: rfgain += ((reg >> 5) & 4);
                   break;
        defaut   : break;
      }
    }
    Serial.println();
    Serial.printf("Key values: freq: %d.%02d (%s), RFGain: %d, PGA: %d (%d dB), Pilot: %s, Pre-emph: %d microseconds\n", 
                  freq / 100, freq % 100, ms, rfgain, pga, PgaInDb(pga), pilot ? "high" : "low", preemp ? 50 : 75);
  }
  else {
    Serial.println("Device not present");
  }
}

bool KT0803X_Class::CheckPresent(void) {
  #ifdef PIN_KT0803X_ENABLE
    pinMode(_pin_power_on, OUTPUT);
    digitalWrite(_pin_power_on, HIGH);
    delay(50);
  #endif
  _wire->beginTransmission(KT803X_SLAVE_ADDRESS);
  _chip_present = _wire->endTransmission() == 0;
  return _chip_present;
}

bool KT0803X_Class::Start(uint8_t debug_flags) {
  int count = 0;
  KT0803_DEBUG_PRINT("Start KT0803X ... ");
  // Check if chip present
  if(!CheckPresent()) {
    KT0803_DEBUG_PRINTLN("Device not found");
    return false; 
  }
  WriteReg(KT803X_REG_0X0B_ON); // Wake up first if in Standby mode (KT0803L only)
  // Wait until chip started up and power good bit set
  while(!(ReadReg(0x0F) & 0x10)) {
    if(++count > 1000) {      // Note: I measured a count of 189 before bit was set
      _chip_present = false;  // FM chip did not power up, report and abort init
      KT0803_DEBUG_PRINTLN("Device not ready");
      return false; 
    }
  }
  KT0803_DEBUG_PRINTLN("Ready");
  if(debug_flags & 2) PrintRegs();
  WriteReg(KT803X_REG_0X10);
  WriteReg(KT803L_REG_0X17);
  WriteReg(KT803X_REG_0X13);
  WriteReg(KT803X_REG_0X02);
  WriteFreqPga();
  if(debug_flags & 1) PrintRegs();
  return true;
}

void KT0803X_Class::Stop(void) {
  #ifdef PIN_KT0803X_ENABLE
    digitalWrite(_pin_power_on, LOW);
  #else
    WriteReg(KT803X_REG_0X0B_OFF); // Go to Standby mode (KT0803L only), and switch off PDPA (needed for KT0803K/M)
  #endif
  _chip_present = false;
}

int KT0803X_Class::PgaInDb(uint8_t pga) { // See datasheet KT0803L register 0x04
  if(pga < 16)  return -(int8_t)pga;
  if(pga >= 19) return pga - 19;
  return 0;
}
