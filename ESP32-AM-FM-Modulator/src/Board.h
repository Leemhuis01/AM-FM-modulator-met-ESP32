#ifndef _BOARD_H_
#define _BOARD_H_

#define ESP32_DEMO_TARGET   0    // Using PCBA for ESP32-QVGA-DS-v3
#define ESP32_WROOM_TARGET  1    // Usig final PCBA
#define ESP32_WROVER_TARGET 2    // Usig final PCBA
#define TARGET_BOARD        ESP32_WROVER_TARGET

#define DAC_ID_PT8211       0
#define DAC_ID_MAX98357A    1
#define DAC_ID              DAC_ID_PT8211

#ifndef NO_SERIAL_DEBUG
#define DEBUG_MSG(s)                  Serial.printf(s)
#define DEBUG_MSG_VAL(s,v)            Serial.printf(s,v)
#define DEBUG_MSG_VAL2(s,v1,v2)       Serial.printf(s,v1,v2)
#define DEBUG_MSG_VAL3(s,v1,v2,v3)    Serial.printf(s,v1,v2,v3)
#define DEBUG_MSG_VAL4(s,v1,v2,v3,v4) Serial.printf(s,v1,v2,v3,v4)
#else
#define DEBUG_MSG(s)               
#define DEBUG_MSG_VAL(s,v)         
#define DEBUG_MSG_VAL2(s,v1,v2)    
#define DEBUG_MSG_VAL3(s,v1,v2,v3)
#define DEBUG_MSG_VAL4(s,v1,v2,v3,v4) 
#endif

#define SERIAL_BAUD_RATE      115200

#define I2C_HIGH_CLOCK_SPEED  400000
#define I2C_LOW_CLOCK_SPEED   100000

//=====================
//== Pin definitions ==
//=====================  

#if TARGET_BOARD == ESP32_DEMO_TARGET
#else

#define PIN_ADC         35

// Buttons

#if TARGET_BOARD == ESP32_WROOM_TARGET
  #define PIN_SW1       16
  #define PIN_SW2       17
  #define PIN_DAC2      26
#else
  #define PIN_SW1        0
  #define PIN_SW2       26
#endif

#define PIN_SW3         34
#define PIN_SW4         39
#define PIN_SW5         36

#define PIN_AM_DEPTH    32
#define PIN_DAC1        25
 
// I2S is used for sound 
#define PIN_I2S_DOUT    12
#define PIN_I2S_WS      33
#define PIN_I2S_BCK     27

// VSPI is used for SD card
#define PIN_VSPI_MOSI   23
#define PIN_VSPI_MISO   19
#define PIN_VSPI_SCK    18
#define PIN_VSPI_SS      5

// I2C port 1 is used for OLED
#define PIN_SCL1        22
#define PIN_SDA1        21

// I2C port 2 is used for EEPROM & KT0803
#define PIN_SCL2        13 
#define PIN_SDA2        14

// CD4015B shift register for the AM PLL divider
#define PIN_CD4015B_DAT  2  // Needs also be defined inside CD4015B.h
#define PIN_CD4015B_CLK  4  // Needs also be defined inside CD4015B.h
#define PIN_AM_OFF       PIN_CD4015B_DAT // Shared function
#define PIN_FM_SW        15

// Switch IDs
#define KEY_SPARE        0
#define KEY_DN           1
#define KEY_UP           2
#define KEY_OK           3
#define KEY_MENU         4
#define KEY_NONE         0xFF

#define BOOT_KEY1_MASK   0x01
#define BOOT_KEY2_MASK   0x02
#define BOOT_KEY3_MASK   0x04
#define BOOT_KEY4_MASK   0x08
#define BOOT_KEY5_MASK   0x10

#endif

//=====================
//== Other Constants ==
//=====================  

#define TIMESTAMP_BASE_RATE  1000000   // Microseconds rate (1 MHz)
//#define TIMESTAMP_RATE       8000      // for 8 kHz rate
#define TIMESTAMP_RATE_10MS  100       // for 100 Hz (10 ms period) rate
//#define TIMESTAMP_DIV       (TIMESTAMP_BASE_RATE / TIMESTAMP_RATE)
#define TIMESTAMP_10MS_DIV  (TIMESTAMP_BASE_RATE / TIMESTAMP_RATE_10MS) 

//=====================
//==  Useful Macros  ==
//=====================

#define USE_ESP32_FAST_IO
// Use PIN_CLR  & PIN_SET  for GPIO0  - GPIO31
// Use PIN_CLR1 & PIN_SET1 for GPIO32 & GPIO33
#ifdef USE_ESP32_FAST_IO
  #define PIN_CLR(p)   REG_WRITE(GPIO_OUT_W1TC_REG, (uint32_t)1 << p)
  #define PIN_SET(p)   REG_WRITE(GPIO_OUT_W1TS_REG, (uint32_t)1 << p)
  #define PIN_CLR1(p)  REG_WRITE(GPIO_OUT1_W1TS_REG, (uint32_t)1 << (p - 32))
  #define PIN_SET1(p)  REG_WRITE(GPIO_OUT1_W1TC_REG, (uint32_t)1 << (p - 32))
#else
  #define PIN_CLR(p ) digitalWrite(p, LOW)
  #define PIN_SET(p)  digitalWrite(p, HIGH)
  #define PIN_CLR1    PIN_CLR
  #define PIN_SET1    PIN_SET
#endif

//===============================
//==  Global variables/classes ==
//=============================== 

//extern SPIClass QVGA_SPI;  // SPI port for QVGA Display
//extern SPIClass SD_SPI;    // SPI port for micro SD Card

//#define TSC_SPI  QVGA_SPI

#endif // _BOARD_H_