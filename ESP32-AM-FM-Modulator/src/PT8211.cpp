// 
// Simple access to PT8211 DAC by using bit banging interface
//

#include "Board.h"
#include "PT8211.h"
#include <Arduino.h>

#ifndef _BOARD_H_
  #define PIN_I2S_BCK    27
  #define PIN_I2S_WS     33
  #define PIN_I2S_DOUT   12
#endif

#if 0  // Standard "digitalWrite" is rather slow

  #define PT8211_SET_WS()   digitalWrite(PIN_I2S_WS, HIGH)
  #define PT8211_CLR_WS()   digitalWrite(PIN_I2S_WS, LOW)

  #define PT8211_SET_BCK()  digitalWrite(PIN_I2S_BCK, HIGH)
  #define PT8211_CLR_BCK()  digitalWrite(PIN_I2S_BCK, LOW)

  #define PT8211_SET_DOUT() digitalWrite(PIN_I2S_DOUT, HIGH)
  #define PT8211_CLR_DOUT() digitalWrite(PIN_I2S_DOUT, LOW)

  #define PT8211_DELAY() 

#else // Direct register writes is much faster
  #if PIN_I2S_WS < 32
    #define PT8211_SET_WS()   REG_WRITE(GPIO_OUT_W1TS_REG, (uint32_t)1 << PIN_I2S_WS)
    #define PT8211_CLR_WS()   REG_WRITE(GPIO_OUT_W1TC_REG, (uint32_t)1 << PIN_I2S_WS)
  #else
    #define PT8211_SET_WS()   REG_WRITE(GPIO_OUT1_W1TS_REG, (uint32_t)1 << (PIN_I2S_WS - 32))
    #define PT8211_CLR_WS()   REG_WRITE(GPIO_OUT1_W1TC_REG, (uint32_t)1 << (PIN_I2S_WS - 32))
  #endif

  #if PIN_I2S_BCK < 32
    #define PT8211_SET_BCK()  REG_WRITE(GPIO_OUT_W1TS_REG, (uint32_t)1 << PIN_I2S_BCK)
    #define PT8211_CLR_BCK()  REG_WRITE(GPIO_OUT_W1TC_REG, (uint32_t)1 << PIN_I2S_BCK)
  #else
    #define PT8211_SET_BCK()  REG_WRITE(GPIO_OUT1_W1TS_REG, (uint32_t)1 << (PIN_I2S_BCK - 32))
    #define PT8211_CLR_BCK()  REG_WRITE(GPIO_OUT1_W1TC_REG, (uint32_t)1 << (PIN_I2S_BCK - 32))
  #endif

  #if PIN_I2S_DOUT < 32
    #define PT8211_SET_DOUT() REG_WRITE(GPIO_OUT_W1TS_REG, (uint32_t)1 << PIN_I2S_DOUT)
    #define PT8211_CLR_DOUT() REG_WRITE(GPIO_OUT_W1TC_REG, (uint32_t)1 << PIN_I2S_DOUT)
  #else
    #define PT8211_SET_DOUT() REG_WRITE(GPIO_OUT1_W1TS_REG, (uint32_t)1 << (PIN_I2S_DOUT - 32))
    #define PT8211_CLR_DOUT() REG_WRITE(GPIO_OUT1_W1TC_REG, (uint32_t)1 << (PIN_I2S_DOUT - 32))
  #endif

//  #define PT8211_DELAY() asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; ")
  #define PT8211_DELAY()   // no nops necessary on ESP32
 
#endif

void PT8211_Init(void) {
  pinMode(PIN_I2S_WS, OUTPUT);
  pinMode(PIN_I2S_BCK, OUTPUT);
  pinMode(PIN_I2S_DOUT, OUTPUT);
  PT8211_CLR_WS();
  PT8211_CLR_BCK();
  PT8211_CLR_DOUT();
}

void /*IRAM_ATTR*/ PT8211_Write(int16_t left,int16_t right) {
  PT8211_CLR_WS();
  for(int i = 0; i < 16; i++) {
    PT8211_CLR_BCK();
    (right & 0x8000) ? PT8211_SET_DOUT() : PT8211_CLR_DOUT();
    PT8211_DELAY();
    PT8211_SET_BCK();
    PT8211_DELAY();
    right <<= 1;
  }
  PT8211_SET_WS();
  for(int i = 0; i < 16; i++) {
    PT8211_CLR_BCK();
    (left & 0x8000) ? PT8211_SET_DOUT() : PT8211_CLR_DOUT();
    PT8211_DELAY();
    PT8211_SET_BCK();
    PT8211_DELAY();
    left <<= 1;
  }
  PT8211_CLR_WS();
  PT8211_CLR_BCK();
  PT8211_CLR_DOUT();
}
