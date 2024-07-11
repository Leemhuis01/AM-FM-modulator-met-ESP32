#ifndef _KT0803X_H_
#define _KT0803X_H_

#include <Wire.h>
//#include "src/Board.h" 

#define I2C_KT0803X_CLOCK_SPEED   100000

#define PIN_KT0803X_ENABLE     15  // default pin, can be omitted (undef) or overridden in constructor
#define KT0803X_DEFAULT_FREQ  999  // = 99.9 MHz
#define KT0803X_DEFAULT_PGA     0  // = 0 dB, see datasheet page 9 at: http://radio-z.ucoz.lv/kt_0803/KT0803L_V1.3.pdf

class KT0803X_Class {
  public:
    #ifdef PIN_KT0803X_ENABLE
      KT0803X_Class(TwoWire &wire, uint8_t pin_power_on = PIN_KT0803X_ENABLE, uint32_t current_speed = I2C_KT0803X_CLOCK_SPEED);
    #else
      KT0803X_Class(TwoWire &wire, uint32_t current_speed = I2C_KT0803X_CLOCK_SPEED);
    #endif
    bool CheckPresent(void);
    bool Start(uint8_t debug_flags = 0);
    void Stop(void);
    void PrintRegs(void);
    void SetFreq(uint16_t f, bool write_regs = true) { _freq = f & 0x7FF;  if(write_regs) WriteFreqPga(); }
    void SetPga(uint8_t pga, bool write_regs = true) { _pga  = pga & 0x1F; if(write_regs) WriteFreqPga(); }
    void SetMono(bool mono,  bool write_regs = true) { _mono = mono ? 0x40: 0x00; if(write_regs) WriteFreqPga(); } // Bit 6 of register 4
    bool IsPresent(void) { return _chip_present; }
    int  PgaInDb(void) { return PgaInDb(_pga); }
    int  PgaInDb(uint8_t pga);
  protected:
    void SetWireClock(void)     { if(_prevspeed != I2C_KT0803X_CLOCK_SPEED) Wire.setClock(I2C_KT0803X_CLOCK_SPEED); }
    void RestoreWireClock(void) { if(_prevspeed != I2C_KT0803X_CLOCK_SPEED) Wire.setClock(_prevspeed); }
    void WriteFreqPga(void);
    int  WriteReg(int reg, int val);
    int  ReadReg(int reg);

    TwoWire * _wire;
    #ifdef PIN_KT0803X_ENABLE
      uint8_t _pin_power_on;
    #endif
    uint8_t   _mono;
    uint8_t   _pga;  // 0 - 31
    uint16_t  _freq; // in multiples of 100kHz, so 1008 = 100.8 MHz
    uint32_t  _prevspeed;
    bool      _chip_present;
};

extern KT0803X_Class KT0803X;

#endif // _KT0803X_H_
