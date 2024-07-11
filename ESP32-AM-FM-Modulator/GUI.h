#ifndef _GUI_H_
#define _GUI_H_

#define GUI_RESULT_NOP                  0
#define GUI_RESULT_TEMP_PAUSE_PLAY    101
#define GUI_RESULT_PAUSE_PLAY         102
#define GUI_RESULT_NEXT_PLAY          103
#define GUI_RESULT_PREV_PLAY          104
#define GUI_RESULT_SEEK_PLAY          105
#define GUI_RESULT_TIMEOUT            106
#define GUI_RESULT_NEW_AM_FREQ        107
#define GUI_RESULT_NEW_FM_FREQ        108
#define GUI_RESULT_NEW_AF_SOURCE      109
#define GUI_RESULT_NEW_OUTPUT_SEL     110
#define GUI_RESULT_NEW_AM_TRIM        111
#define GUI_RESULT_NEW_AM_DEPTH       112
#define GUI_RESULT_NEW_FM_MODULATION  113
#define GUI_RESULT_NEW_FM_MOD_TYPE    114
#define GUI_RESULT_NEW_SSID           115
#define GUI_RESULT_NEW_FM_PGA         116

void GuiInit();
int  GuiCallback(int key_press, unsigned timestamp_10ms);

#endif // _GUI_H_
