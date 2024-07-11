
#include <Arduino.h>
#include "CD4015B.h"

#define CD4015B_CONNECT_LM358  0  // Opamp level shifter (non inverting)
#define CD4015B_CONNECT_BC547  1  // Transistor level shifter (inverting)
#define CD4015B_CONNECT_TTL    2  // Direct connect (no level shifting)

#define CD4015B_CONNECT        CD4015B_CONNECT_BC547

#define CD4015B_DATA_NORMAL  0
#define CD4015B_DATA_HASH1   1  // PCB 1.1, 1.2, 1.5+
#define CD4015B_DATA_HASH2   2  // PCB 1.3 & 1.4

#define CD4015B_DATA_FORMAT  CD4015B_DATA_HASH1

//#define CD4025B_TEST        // Undef for normal operation

#if CD4015B_CONNECT == CD4015B_CONNECT_BC547
  #define CD4015B_DELAY_US      10    // For BC457 / 2N7000 level shifter
  #define CD4015B_PIN_LOW       HIGH  // Inverting
  #define CD4015B_PIN_HIGH      LOW
#else
  #if 0
  #if CD4015B_CONNECT == CD4015B_CONNECT_LM358
    #define CD4015B_DELAY_US    75    // For (slow) LM358 level shifter
  #else /* CD4015B_CONNECT_TTL */
    #define CD4015B_DELAY       0     // No delay needed
  #endif
  #define CD4015B_PIN_LOW       LOW   // Non inverting
  #define CD4015B_PIN_HIGH      HIGH
  #endif
#endif

#define CD4015B_P7  0x80
#define CD4015B_P6  0x40
#define CD4015B_P5  0x20
#define CD4015B_P4  0x10
#define CD4015B_P3  0x08
#define CD4015B_P2  0x04
#define CD4015B_P1  0x02
#define CD4015B_P0  0x01

#if CD4015B_DATA_FORMAT == CD4015B_DATA_NORMAL
  #define CD4015B_Q7(q) (q & CD4015B_P7)
  #define CD4015B_Q6(q) (q & CD4015B_P6)
  #define CD4015B_Q5(q) (q & CD4015B_P5)
  #define CD4015B_Q4(q) (q & CD4015B_P4)
  #define CD4015B_Q3(q) (q & CD4015B_P3)
  #define CD4015B_Q2(q) (q & CD4015B_P2)
  #define CD4015B_Q1(q) (q & CD4015B_P1)
  #define CD4015B_Q0(q) (q & CD4015B_P0)
#elif CD4015B_DATA_FORMAT == CD4015B_DATA_HASH1
  #define CD4015B_Q7(q) (q & CD4015B_P3) // Change according to your schematics
  #define CD4015B_Q6(q) (q & CD4015B_P7) // . 
  #define CD4015B_Q5(q) (q & CD4015B_P6) // .
  #define CD4015B_Q4(q) (q & CD4015B_P5) // .
  #define CD4015B_Q3(q) (q & CD4015B_P4) // .
  #define CD4015B_Q2(q) (q & CD4015B_P2) // .
  #define CD4015B_Q1(q) (q & CD4015B_P1) // .
  #define CD4015B_Q0(q) (q & CD4015B_P0) // Change according to your schematics
#elif CD4015B_DATA_FORMAT == CD4015B_DATA_HASH2
  #define CD4015B_Q7(q) (q & CD4015B_P7) // Change according to your schematics
  #define CD4015B_Q6(q) (q & CD4015B_P3) // . 
  #define CD4015B_Q5(q) (q & CD4015B_P4) // . 
  #define CD4015B_Q4(q) (q & CD4015B_P5) // . 
  #define CD4015B_Q3(q) (q & CD4015B_P0) // . 
  #define CD4015B_Q2(q) (q & CD4015B_P6) // . 
  #define CD4015B_Q1(q) (q & CD4015B_P1) // . 
  #define CD4015B_Q0(q) (q & CD4015B_P2) // Change according to your schematics
#else
  #error CD4015B: No data format defined
#endif

inline void CD4015B_WriteBit(uint8_t b) {
  digitalWrite(PIN_CD4015B_DAT, b ? CD4015B_PIN_HIGH : CD4015B_PIN_LOW);
  digitalWrite(PIN_CD4015B_CLK, CD4015B_PIN_LOW);
  delayMicroseconds(CD4015B_DELAY_US);
  digitalWrite(PIN_CD4015B_CLK, CD4015B_PIN_HIGH);
  delayMicroseconds(CD4015B_DELAY_US);
}

void CD4015B_WriteByte(uint8_t q, uint8_t shared_data, uint8_t shared_clk) {
  #ifndef CD4025B_TEST
    for(int i = 0; i < 8; i++) CD4015B_WriteBit(0); // Need to clear all bits first
    CD4015B_WriteBit(CD4015B_Q7(q)); 
    CD4015B_WriteBit(CD4015B_Q6(q));
    CD4015B_WriteBit(CD4015B_Q5(q));
    CD4015B_WriteBit(CD4015B_Q4(q));
    CD4015B_WriteBit(CD4015B_Q3(q));
    CD4015B_WriteBit(CD4015B_Q2(q));
    CD4015B_WriteBit(CD4015B_Q1(q));
    CD4015B_WriteBit(CD4015B_Q0(q));
    // Write/restore the shared pin states
    digitalWrite(PIN_CD4015B_DAT, shared_data);
    digitalWrite(PIN_CD4015B_CLK, shared_clk);
    delayMicroseconds(CD4015B_DELAY_US);
  #else
    while(1) {
      digitalWrite(PIN_CD4015B_DAT, HIGH);
      delayMicroseconds(CD4015B_DELAY_US);
      digitalWrite(PIN_CD4015B_CLK, HIGH);
      delayMicroseconds(CD4015B_DELAY_US);
      digitalWrite(PIN_CD4015B_DAT, LOW);
      delayMicroseconds(CD4015B_DELAY_US);
      digitalWrite(PIN_CD4015B_CLK, LOW);
      delayMicroseconds(CD4015B_DELAY_US);
    }
  #endif
}
