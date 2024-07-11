#ifndef _CD4015B_H_
#define _CD4015B_H_

#include "Board.h"

#ifndef PIN_CD4015B_DAT
  #define PIN_CD4015B_DAT  2
#endif

#ifndef PIN_CD4015B_CLK
  #define PIN_CD4015B_CLK   4
#endif

#include <stdint.h>

void CD4015B_WriteByte(uint8_t q, uint8_t shared_data = 1, uint8_t shared_clk = 1);

// REM: shared_pin is the level of the pin after q has been written. Used to share
//      the pin with another function. PIN_CD4015B_DATA may also be used outside
//      this function. However, toggling PIN_CD4015B_CLK outside this functon will
//      corrupt the contents of the CD4015B. Also make sure, that the shared
//      function of the pin is not corrupted by the toggling in this function.

#endif
