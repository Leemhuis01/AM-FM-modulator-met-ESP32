#ifndef _PT8211_H_
#define _PT8211_H_

#include <stdint.h>

void PT8211_Init(void);
void PT8211_Write(int16_t left,int16_t right);

#endif //_PT8211_H_
