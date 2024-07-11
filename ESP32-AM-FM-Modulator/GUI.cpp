#include <string.h>
#include <stdio.h>
#include "src/board.h"
#include "GUI.h"
#include "display.h"
#include "src/Settings.h"
#include "src/NFOR_SSD1306.h"
#include "src/KT0803X.h"

#include "Player.h"

#define MENU_DEBUG(s) Serial.printf(s)

/*****************************************************************************/
/*  Menu texts                                                               */
/*****************************************************************************/

#define GUI_STRING static const char * const

#define NEW_SD_CARD "SD Card"

        // Ruler for 16 characters "1234567890123456" , "1234567890123456"; 

GUI_STRING MENU_TEXT_SOURCE0   =   "Audio Source"     ;

GUI_STRING MENU_TEXT_SSID0     =   "Select Wifi SSID";

GUI_STRING MENU_TEXT_AM_GRID0  =   "AM Spectrum"      ;
GUI_STRING MENU_TEXT_AM_GRID1[]= { "Europe,Asia,Afr." , "Australia/New Z.",
                                   "N and S America " };

GUI_STRING MENU_TEXT_FM_PGA    =   "FM Modul. Gain"   ;

GUI_STRING MENU_TEXT_OUTPUT0   =   "Output select"    ; 
GUI_STRING MENU_TEXT_OUTPUT1[] = { "AM Only"          , "FM Only", "AM & FM" };

GUI_STRING MENU_TEXT_AM        =   "AM Frequency"     ;
GUI_STRING MENU_TEXT_FM        =   "FM Frequency"     ;

GUI_STRING MENU_TEXT_TIME      =   "Go to Track Time" ;

GUI_STRING MENU_TEXT_AM_MOD    =   "AM Mod. Depth"    ;
GUI_STRING MENU_TEXT_FM_MOD    =   "FM Mod. Mode"     ;

GUI_STRING MENU_TRIM_AM        =   "AM Depth Trim"    ;

GUI_STRING MENU_TEXT_SETUP0       = "**** Setup **** ";
GUI_STRING MENU_TEXT_SETUP1_ENTER = "OK key to enter";
GUI_STRING MENU_TEXT_SETUP1_EXIT  = "OK key to exit";

GUI_STRING MENU_OK_CONFIRM        = "Press OK to confirm";

GUI_STRING MENU_AM_FREQ_WAVE      = "%dkHz(%dm)";
GUI_STRING MENU_FM_FREQ_CHAN      = "%d.%dMHz(C%d%c)";

//************************************************************************//
// Important constant                                                     //
//************************************************************************//

#define S2QS(qs) ((qs)<<2)  // convert seconds to quarters of a second

#define TICKER_INFO_DELAY  S2QS(5) // five seconds

/*****************************************************************************/
/*  Playing time variables and functions                                     */
/*    - For efficient processing speed we maintain both the integer and an   */
/*      ASCII version for CurrentTrackTime                                   */
/*****************************************************************************/

static char CurrentTrackTimeString[10]; // first char holds offset
static char TotalTrackTimeString[10];

static void CurrentTrackTimeToString() {
    unsigned t = Settings.CurrentTrackTime;
    CurrentTrackTimeString[0] = 5; // index to first valid char
    CurrentTrackTimeString[9] = 0;
    CurrentTrackTimeString[8] = t % 10 + '0'; t = t / 10;
    CurrentTrackTimeString[7] = t %  6 + '0'; t = t /  6;
    CurrentTrackTimeString[6] = ':';
    CurrentTrackTimeString[5] = t % 10 + '0'; t = t / 10;
    if(t) CurrentTrackTimeString[0] = 4;
    CurrentTrackTimeString[4] = t %  6 + '0'; t = t /  6;
    CurrentTrackTimeString[3] = ':';
    if(t) CurrentTrackTimeString[0] = 2;
    CurrentTrackTimeString[2] = t % 10 + '0'; t = t / 10;
    if(t) CurrentTrackTimeString[0] = 1;
    CurrentTrackTimeString[1] = t % 10 + '0'; t = t / 10;
}

static void PlayTimeToString(unsigned sec, char * s) {
  unsigned hrs = sec / 3600;
  sec          = sec - hrs * 3600;
  unsigned min = sec / 60;
  sec          = sec - min * 60;
  if(hrs)
    sprintf(s, "%d:%02d:%02d", (int)hrs, (int)min, (int)sec);
  else
    sprintf(s, "%d:%02d", (int)min, (int)sec);
}

/*static*/ void CurrentTimeUpdatex() {
#if 1
  CurrentTrackTimeToString(); 
#else
  if(Settings.Play) {
    //Settings.CurrentTrackTime++;
    if(++CurrentTrackTimeString[8] <= '9') return; // Seconds
    CurrentTrackTimeString[8] = '0';
    if(++CurrentTrackTimeString[7] <= '5') return;
    CurrentTrackTimeString[7] = '0';
    if(++CurrentTrackTimeString[5] <= '9') return; // Minutes
    CurrentTrackTimeString[5] = '0';
    if(CurrentTrackTimeString[0] == 5) CurrentTrackTimeString[0] = 4; // Index to time string "10:00"
    if(++CurrentTrackTimeString[4] <= '5') return;
    CurrentTrackTimeString[4] = '0';
    if(CurrentTrackTimeString[0] == 4) CurrentTrackTimeString[0] = 2; // Index to time string "1:00:00"
    if(++CurrentTrackTimeString[2] <= '9') return;
    CurrentTrackTimeString[2] = '0';
    CurrentTrackTimeString[0] = 1;                       // Index to time string "10:00:00"
    if(++CurrentTrackTimeString[1] <= '9') return;
    CurrentTrackTimeString[1] = '0';
  }
#endif
}
/*
static void ShowTrackAndTime() {
  char s[12];
  // Display track info
  sprintf(s, "%d/%d", Settings.NV.DiskCurrentTrack, Settings.NV.DiskTotalTracks);
  Display.ShowTrack(s);
  // Display time info
//Serial.printf("ShowTrackAndTime: %d, %d\n", Settings.CurrentTrackTime, Settings.TotalTrackTime);
  CurrentTrackTimeToString();
  PlayTimeToString(Settings.TotalTrackTime, TotalTrackTimeString);
  Display.ShowTimes(&CurrentTrackTimeString[(int)CurrentTrackTimeString[0]], TotalTrackTimeString);
}
*/

/*****************************************************************************/
/*  Menu defines and variables                                               */
/*****************************************************************************/

#define MENU_DELAY_PLAYER      8  // seconds
#define MENU_DELAY_SEEK        10 // seconds

#define MENU_FUNC_INIT         0
#define MENU_FUNC_CALLBACK     1
#define MENU_FUNC_TICK         2  // Occurs every quarter of a second
#define MENU_FUNC_SECOND       3  // Occurs every second (only for player)
#define MENU_FUNC_KEY_UP       4
#define MENU_FUNC_KEY_DN       5
#define MENU_FUNC_KEY_OK       6
#define MENU_FUNC_KEY_MENU     7
#define MENU_FUNC_EXIT         8

#define MENU_RESULT_EXIT       50
#define MENU_RESULT_SETUP      51

static int menu_delay_qs = 0; // Menu counter for delay in quarters of a second
static int menu_edit_val = 0; // Value will be copied to settings when confirmed

#define MENU_RESET_TIMEOUT(T) menu_delay_qs = S2QS(T)

/*****************************************************************************/
/*  Some useful menu routines                                                */
/*****************************************************************************/

static void MenuValueAdjust(int adj, int min, int max, int roll) {
  if( min == max)
    menu_edit_val = min;
  else {
    menu_edit_val += adj;
    if(menu_edit_val < min) menu_edit_val = roll ? max : min;
    if(menu_edit_val > max) menu_edit_val = roll ? min : max;
  }
  MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
}

/*****************************************************************************/
/*  Player menu                                                              */
/*****************************************************************************/

static int MenuPlayer(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : MENU_DEBUG("MenuPlayer: INIT\n");
                              Display.PrepareMenuScreen(false);
                              break;
    case MENU_FUNC_KEY_DN   : MENU_DEBUG("MenuPlayer: KEY_DN\n");
                              return GUI_RESULT_PREV_PLAY;
    case MENU_FUNC_KEY_UP   : MENU_DEBUG("MenuPlayer: KEY_UP\n");
                              return GUI_RESULT_NEXT_PLAY;

    case MENU_FUNC_KEY_OK   : MENU_DEBUG("Menu Player: KEY_OK\n");
                              return GUI_RESULT_PAUSE_PLAY;
    case MENU_FUNC_KEY_MENU : MENU_DEBUG("MenuPlayer: KEY_MENU\n");
                              Display.PrepareMenuScreen(true);
                              return MENU_RESULT_EXIT;
                              break;
  }
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Source menu                                                              */
/*****************************************************************************/

static void MenuSourceAdjust(int adj) {
  MenuValueAdjust(adj, 0, SET_SOURCE_MAX, 1);
  Display.ShowMenuStrings(NULL, Settings.GetSourceName(menu_edit_val), NULL);
}

static int MenuSource(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : menu_edit_val = Settings.NV.SourceAF;
                              Display.ShowMenuStrings(MENU_TEXT_SOURCE0, NULL, MENU_OK_CONFIRM);
                              MenuSourceAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuSourceAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuSourceAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NewSourceAF = menu_edit_val; 
                              return GUI_RESULT_NEW_AF_SOURCE;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select SSID                                                         */
/*****************************************************************************/

static void MenuSelectSsidAdjust(int adj) {
  if(Settings.NV.TotalSsid == 0) {
    Display.ShowMenuStrings(NULL, "No Network found", NULL);
    //Display.StartTicker("Insert SD Card with \"ssid.ini\", and restart the board.");
    MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
  }
  else {
    MenuValueAdjust(adj, 0, Settings.NV.TotalSsid - 1, 1);
    Display.ShowMenuStrings(NULL, SsidSettings.GetSsid(menu_edit_val), NULL);
  }
}

static int MenuSelectSsid(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : if(Settings.NV.SourceAF != SET_SOURCE_WEB_RADIO)
                                return MENU_RESULT_EXIT; // go to next menu item
                              menu_edit_val = Settings.NV.CurrentSsid;
                              Display.ShowMenuStrings(MENU_TEXT_SSID0, NULL, MENU_OK_CONFIRM);
                              MenuSelectSsidAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuSelectSsidAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuSelectSsidAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NV.CurrentSsid = menu_edit_val;   
                              return GUI_RESULT_NEW_SSID;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : //if(Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO && Settings.NV.TotalSsid == 0)
                              //  Display.StartTicker("");
                              break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Output menu                                                              */
/*****************************************************************************/

static void MenuOutputAdjust(int adj) {
  MenuValueAdjust(adj, 0, SET_OUTPUT_MAX, 1);
  Display.ShowMenuStrings(NULL, MENU_TEXT_OUTPUT1[menu_edit_val], NULL);
}

static int MenuOutput(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : if(!Settings.FmPresent) // AM only, skip this menu
                                return MENU_RESULT_EXIT;
                              menu_edit_val = Settings.NV.OutputSel;
                              Display.ShowMenuStrings(MENU_TEXT_OUTPUT0, NULL, MENU_OK_CONFIRM);
                              MenuOutputAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuOutputAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuOutputAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NV.OutputSel = menu_edit_val;
                              return GUI_RESULT_NEW_OUTPUT_SEL;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Track Time menu                                                          */
/*****************************************************************************/

static struct {
   unsigned char flag_play   : 1;
   unsigned char flag_select : 1;
   unsigned char sec         : 6;
   unsigned      max;
} menu_time_data;

static void MenuTimeAdjust(int adj) {
  char text[32];
  if(menu_time_data.sec) {
    if(adj > 0)                                     // if(prev) round down to current minute
      MenuValueAdjust(1, 0, menu_time_data.max, 0); // else round up to next minute
    menu_time_data.sec = 0;
  }
  else
    MenuValueAdjust(adj, 0, menu_time_data.max, 0);
  PlayTimeToString(menu_edit_val * 60, text);
  strcat(text,"/");
  strcat(text, TotalTrackTimeString);
  if(strlen(text) <= 16) {
    strcat(text,"        ");
    text[16] = '\0';
  }
  Display.ShowMenuStrings(NULL, text, NULL);
  MENU_RESET_TIMEOUT(MENU_DELAY_SEEK);
}

static int MenuTime(int function) {
  char text[24];
  if(menu_time_data.flag_select != 0) { // Already sought in a track?
    menu_time_data.flag_select = 0;
    return GUI_RESULT_TIMEOUT;
  }
  switch(function) {
    case MENU_FUNC_INIT     : MENU_DEBUG("MenuTime: INIT\n");
                              if(Settings.NV.SourceAF != SET_SOURCE_SD_CARD || Settings.NoCard)
                                return MENU_RESULT_EXIT; // go to next menu item
                              menu_time_data.flag_play   = Settings.Play;
                              Settings.Play              = 0; // (temporally) stop playing
                              menu_time_data.flag_select = 0;
                              menu_time_data.sec         = Settings.CurrentTrackTime % 60; // keep seconds
                              menu_edit_val              = Settings.CurrentTrackTime / 60; // only minutes
                              menu_time_data.max         = Settings.TotalTrackTime / 60;
                              strcpy(text, &CurrentTrackTimeString[(int)CurrentTrackTimeString[0]]);
                              strcat(text, "/");
                              strcat(text, TotalTrackTimeString);
                              Display.ShowMenuStrings(MENU_TEXT_TIME, text, MENU_OK_CONFIRM);
                              MENU_RESET_TIMEOUT(MENU_DELAY_SEEK);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0) {
                                Settings.Play = menu_time_data.flag_play;
                                return GUI_RESULT_TIMEOUT;
                              }
                              break;
    case MENU_FUNC_KEY_UP   : MenuTimeAdjust(1); break;
    case MENU_FUNC_KEY_DN   : MenuTimeAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : //Settings.CurrentTrackTime = menu_edit_val * 60;
                              Settings.SeekTrackTime = menu_edit_val * 60;
                              //CurrentTrackTimeToString();
                              menu_time_data.flag_select = 1;
                              Settings.Play = menu_time_data.flag_play;
                              return GUI_RESULT_SEEK_PLAY;
    case MENU_FUNC_KEY_MENU : Settings.Play = menu_time_data.flag_play;
                              return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select AM grid                                                           */
/*****************************************************************************/

static void MenuSetupAmGridAdjust(int adj) {
  MenuValueAdjust(adj, 0, SET_AMGRID_MAX, 1); 
  Display.ShowMenuStrings(NULL, MENU_TEXT_AM_GRID1[menu_edit_val], NULL);
}

static int MenuSetupAmGrid(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : menu_edit_val = Settings.NV.GridAM;
                              Display.ShowMenuStrings(MENU_TEXT_AM_GRID0, NULL, MENU_OK_CONFIRM);
                              MenuSetupAmGridAdjust(0);
                              break;
    case MENU_FUNC_KEY_UP   : MenuSetupAmGridAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuSetupAmGridAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NV.GridAM = menu_edit_val;   
                              Settings.NV.FreqAM = Settings.CalcNearestAmChannel(0, Settings.NV.FreqAM); 
                              return GUI_RESULT_NEW_AM_FREQ;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select FM PGA                                                            */
/*****************************************************************************/

static void MenuSetupFmPgaAdjust(int adj) {
  char text[32];

  MenuValueAdjust(adj, SET_FM_PGA_MIN, SET_FM_PGA_MAX, 1); 
  sprintf(text, "%d (%d dB)", menu_edit_val, KT0803X.PgaInDb(menu_edit_val));
  Display.ShowMenuStrings(NULL, text, NULL);
}

static int MenuSetupFmPga(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : menu_edit_val = Settings.NV.FmPga;
                              Display.ShowMenuStrings(MENU_TEXT_FM_PGA, NULL, NULL);
                              MenuSetupFmPgaAdjust(0);
                              break;
    case MENU_FUNC_KEY_UP   : MenuSetupFmPgaAdjust(1);  Settings.NV.FmPga = menu_edit_val; return GUI_RESULT_NEW_FM_PGA;
    case MENU_FUNC_KEY_DN   : MenuSetupFmPgaAdjust(-1); Settings.NV.FmPga = menu_edit_val; return GUI_RESULT_NEW_FM_PGA;
    case MENU_FUNC_KEY_OK   : break;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select AM Modulation Level                                               */
/*****************************************************************************/

static void MenuSetupFmModTypeValueAdjust(int toggle) {
  Settings.NV.FmModType ^= toggle;
  MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
  Display.ShowMenuStrings(NULL, Settings.NV.FmModType ? "Mono" : "Stereo", NULL);
}

static int MenuSetupFmModType(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : if(!Settings.FmPresent) // AM only, skip this menu
                                return MENU_RESULT_EXIT;
                              Display.ShowMenuStrings(MENU_TEXT_FM_MOD, NULL,"");
                              MenuSetupFmModTypeValueAdjust(0);
                              break;
    case MENU_FUNC_KEY_UP   : 
    case MENU_FUNC_KEY_DN   : MenuSetupFmModTypeValueAdjust(1);
                              return GUI_RESULT_NEW_FM_MOD_TYPE;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select AM Modulation Offset (trimming)                                   */
/*****************************************************************************/

static void MenuTrimAmOffsetAdjust(int adj) {
  char text[32];
  MenuValueAdjust(adj, SET_AM_MOD_TRIM_MIN, SET_AM_MOD_TRIM_MAX, 0); 
  itoa(-menu_edit_val, text, 10);
  Settings.NV.AmTrim = menu_edit_val;
  Display.ShowMenuStrings(NULL, text, NULL);
}

static int MenuTrimAmOffset(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : menu_edit_val = Settings.NV.AmTrim;
                              Display.ShowMenuStrings(MENU_TRIM_AM, NULL, "");
                              MenuTrimAmOffsetAdjust(0);
                              break;
    case MENU_FUNC_KEY_UP   : MenuTrimAmOffsetAdjust(1);  return GUI_RESULT_NEW_AM_TRIM;
    case MENU_FUNC_KEY_DN   : MenuTrimAmOffsetAdjust(-1); return GUI_RESULT_NEW_AM_TRIM;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

#if 0

static void MenuSetupAmModLevelValueAdjust(int toggle) {
  Settings.NV.AmModLevel ^= toggle;
  MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
  Display.ShowMenuStrings(NULL, Settings.NV.AmModLevel ? "50%" : "100% ", NULL);
}

static int MenuSetupAmModLevel(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : Display.ShowMenuStrings(MENU_TEXT_AM_MOD, NULL,"");
                              MenuSetupAmModLevelValueAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : 
    case MENU_FUNC_KEY_DN   : MenuSetupAmModLevelValueAdjust(1);
                              return GUI_RESULT_NEW_AM_DEPTH;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

static int SpeakerVolumeSelect; // 0 Volume, 1 = Balance

static void MenuSpeakerVolumeShowSelect() {
  char * s = SpeakerVolumeSelect ? MENU_TEXT_VOLUME0_BAL : MENU_TEXT_VOLUME0_AUX;
  Display_ShowMenuStrings(s, NULL, NULL);
}

static void MenuSpeakerVolumeShowLevel() {
  char text[32];
  sprintf(text, MENU_TEXT_VOLUME1_VOL, Settings.NV.VolAux1, Settings.NV.VolAux2);
  Display_ShowMenuStrings(NULL, text, NULL);
  MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
}

static void MenuSpeakerVolumeAdjustVal(unsigned char *val, int adj) {
  menu_edit_val = *val;
  MenuValueAdjust(adj, SET_VOLUME_MIN, SET_VOLUME_MAX, 0);
  *val = menu_edit_val;
}

static void MenuSpeakeVolumeAdjustVol(int adj) {
  unsigned char *v1,*v2;
  if(SpeakerVolumeSelect == 0) {
    if(Settings.NV.VolAux1 <= Settings.NV.VolAux2)
      { v1 = &Settings.NV.VolAux1; v2 = &Settings.NV.VolAux2; }
    else
      { v1 = &Settings.NV.VolAux2; v2 = &Settings.NV.VolAux1; }
    if(*v1 + adj < SET_VOLUME_MIN)
      { *v2 = SET_VOLUME_MIN + *v2 - *v1; *v1 = SET_VOLUME_MIN; }
    else if(*v2 + adj > SET_VOLUME_MAX)
      { *v1 = SET_VOLUME_MAX - *v2 + *v1; *v2 = SET_VOLUME_MAX; }
    else
      { *v1 += adj; *v2 += adj; }
  }
  else {
    MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux1, -adj);
    MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux2,  adj);
    // balance cannot be odd; adjust if necessary
    if(Settings.NV.VolAux1 == SET_VOLUME_MAX && Settings.NV.VolAux2 & 1)
      MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux2, -1);
    if(Settings.NV.VolAux2 == SET_VOLUME_MAX && Settings.NV.VolAux1 & 1)
      MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux1, -1);
    if(Settings.NV.VolAux1 == SET_VOLUME_MIN && Settings.NV.VolAux2 & 1)
      MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux2, 1);
    if(Settings.NV.VolAux2 == SET_VOLUME_MIN && Settings.NV.VolAux1 & 1)
      MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux1, 1);
  }
  MenuSpeakerVolumeShowLevel();
}

static int MenuTrimAmOffset(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : if(Settings.NV.OutputSel != 0) // output not to speakers?
                                return MENU_RESULT_EXIT;     // then go to next menu item
                              SpeakerVolumeSelect = 0;
                              MenuSpeakerVolumeShowSelect();
                              MenuSpeakerVolumeShowLevel();
                              TFT_ICONS(MENUFONT_MINUS,MENUFONT_PLUS,MENUFONT_BALANCE,MENUFONT_MENU);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuSpeakeVolumeAdjustVol(4);  return MENU_RESULT_NEW_AF_VOLUME;
    case MENU_FUNC_KEY_DN   : MenuSpeakeVolumeAdjustVol(-4); return MENU_RESULT_NEW_AF_VOLUME;
    case MENU_FUNC_ENC_SW   :
    case MENU_FUNC_KEY_OK   : SpeakerVolumeSelect = !SpeakerVolumeSelect;
                              MenuSpeakerVolumeShowSelect();
                              MenuSpeakerVolumeShowLevel();
                              if(SpeakerVolumeSelect) TFT_ICONS(0,0,MENUFONT_VOLUME,0);
                              else                    TFT_ICONS(0,0,MENUFONT_BALANCE,0);
                              break;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_ENC_CCW  : MenuSpeakeVolumeAdjustVol(-1); return MENU_RESULT_NEW_AF_VOLUME;
    case MENU_FUNC_ENC_CW   : MenuSpeakeVolumeAdjustVol(1);  return MENU_RESULT_NEW_AF_VOLUME;
    case MENU_FUNC_EXIT     : AuxVolToString(); break;
  };
  return GUI_RESULT_NOP;
}
#endif
/*****************************************************************************/
/*  Select FM Modulation Type                                                */
/*****************************************************************************/

static void MenuSetupAmModLevelValueAdjust(int toggle) {
  Settings.NV.AmModLevel ^= toggle;
  MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
  Display.ShowMenuStrings(NULL, Settings.NV.AmModLevel ? "50%" : "100% ", NULL);
}

static int MenuSetupAmModLevel(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : Display.ShowMenuStrings(MENU_TEXT_AM_MOD, NULL,"");
                              MenuSetupAmModLevelValueAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : 
    case MENU_FUNC_KEY_DN   : MenuSetupAmModLevelValueAdjust(1);
                              return GUI_RESULT_NEW_AM_DEPTH;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select AM frequency                                                      */
/*****************************************************************************/

static void MenuAmFreqAdjust(int adj) {
  char text[32];
  menu_edit_val = Settings.CalcNearestAmChannel(adj, menu_edit_val);
  MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
  Settings.AmFreqToString(text, "", menu_edit_val);
  Display.ShowMenuStrings(NULL, text, NULL);
}

static int MenuSetupAmFreq(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : menu_edit_val = Settings.NV.FreqAM;
                              Display.ShowMenuStrings(MENU_TEXT_AM, NULL, MENU_OK_CONFIRM);
                              MenuAmFreqAdjust(0);
                              break;
    case MENU_FUNC_KEY_UP   : MenuAmFreqAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuAmFreqAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NV.FreqAM = menu_edit_val;
                              return GUI_RESULT_NEW_AM_FREQ;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select FM frequency                                                      */
/*****************************************************************************/

static void MenuFmFreqAdjust(int adj) {
  char text[32];
  MenuValueAdjust(adj, SET_FM_FREQ_MIN, SET_FM_FREQ_MAX, 1);
  //FmFreqToString(menu_edit_val, text);
  Settings.FmFreqToString(text, "", menu_edit_val);
  Display.ShowMenuStrings(NULL, text, NULL);
}

static int MenuSetupFmFreq(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : if(!Settings.FmPresent) // AM only, skip this menu
                                return MENU_RESULT_EXIT;
                              menu_edit_val = Settings.NV.FreqFM;
                              Display.ShowMenuStrings(MENU_TEXT_FM, NULL, MENU_OK_CONFIRM);
                              MenuFmFreqAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuFmFreqAdjust(SET_FM_FREQ_STEP);  break;
    case MENU_FUNC_KEY_DN   : MenuFmFreqAdjust(-SET_FM_FREQ_STEP); break;
    case MENU_FUNC_KEY_OK   : Settings.NV.FreqFM = menu_edit_val;
                              return GUI_RESULT_NEW_FM_FREQ;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Enter/Exit setup menu                                                    */
/*****************************************************************************/

static int MenuEnterSetup(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : Display.ShowMenuStrings(MENU_TEXT_SETUP0, MENU_TEXT_SETUP1_ENTER, "");
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_qs == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_OK   : return MENU_RESULT_SETUP;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
  };
  return GUI_RESULT_NOP;
}

static int MenuExitSetup(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : Display.ShowMenuStrings(MENU_TEXT_SETUP0, MENU_TEXT_SETUP1_EXIT, "");
                              break;
    case MENU_FUNC_KEY_OK   : return MENU_RESULT_SETUP;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Menu function selector                                                   */
/*****************************************************************************/

static int current_menu;

typedef struct {
  int (* f)(int);
  int next;
  int next_alt;
  int use_timeout;
} MenuType;

#define MENU_INDEX_PLAYER          0
#define MENU_INDEX_TIME            1
#define MENU_INDEX_SSID            2
#define MENU_INDEX_SOURCE          3
#define MENU_INDEX_OUTPUT          4
#define MENU_INDEX_SETUP_FM_FREQ   5
#define MENU_INDEX_SETUP_FM_MOD    6
#define MENU_INDEX_SETUP_AM_LVL    7
#define MENU_INDEX_ENTER_SETUP     8
#define MENU_INDEX_SETUP_AM_FREQ   9
#define MENU_INDEX_TRIM_AM_OFFSET  10
#define MENU_INDEX_SETUP_AM_GRID   11
#define MENU_INDEX_SETUP_FM_PGA    12
#define MENU_INDEX_EXIT_SETUP      13
#define MENU_INDEX_COUNT           14

#define MENU_INDEX_DEFAULT         MENU_INDEX_PLAYER

static MenuType MenuEntries[MENU_INDEX_COUNT] = {
  { MenuPlayer,          MENU_INDEX_TIME,           0,0 },
  { MenuTime,            MENU_INDEX_SSID,           0,1 },
  { MenuSelectSsid,      MENU_INDEX_SOURCE,         0,1 },
  { MenuSource,          MENU_INDEX_OUTPUT,         0,1 },
  { MenuOutput,          MENU_INDEX_SETUP_FM_FREQ,  0,1 },
  { MenuSetupFmFreq,     MENU_INDEX_SETUP_FM_MOD,   0,1 },
  { MenuSetupFmModType,  MENU_INDEX_SETUP_AM_LVL,   0,1 },
  { MenuSetupAmModLevel, MENU_INDEX_ENTER_SETUP,    0,1 },
  { MenuEnterSetup,      MENU_INDEX_DEFAULT,        MENU_INDEX_SETUP_AM_FREQ, 1 },
  { MenuSetupAmFreq,     MENU_INDEX_TRIM_AM_OFFSET, 0,0 },
  { MenuTrimAmOffset,    MENU_INDEX_SETUP_AM_GRID,  0,0 },
  { MenuSetupAmGrid,     MENU_INDEX_SETUP_FM_PGA,   0,0 },
  { MenuSetupFmPga,      MENU_INDEX_EXIT_SETUP,     0,0 },
  { MenuExitSetup,       MENU_INDEX_SETUP_AM_FREQ,  MENU_INDEX_DEFAULT, 0 }
};

#define MenuFunction(func) MenuEntries[current_menu].f(func)

/*****************************************************************************/
/*  GUI: Init                                                                */
/*****************************************************************************/

void GuiInit() {
  // Init some strings
  CurrentTrackTimeToString();

  menu_time_data.flag_select = 0;
  // Init menu
  current_menu = MENU_INDEX_PLAYER;
  MenuFunction(MENU_FUNC_INIT);
}

/*****************************************************************************/
/* GUI: Main routine (called every 20.8 to 125 microseconds)                 */
/*****************************************************************************/

int GuiCallback(int key_press, uint32_t timestamp_10ms) {
  static uint8_t  key              = 0xff;
  static uint32_t prev_time_stamp  = 0;
  static uint8_t  second_counter   = 0;
  static uint8_t  menu_counter     = 0;
  static uint32_t timestamp_second = 0;
  int result = GUI_RESULT_NOP;

  if(key_press != 0xff)
    key = key_press;
  
  if(timestamp_10ms == prev_time_stamp)
    return GUI_RESULT_NOP;
    
  prev_time_stamp = timestamp_10ms;
  if(++second_counter >= 100) { 
    timestamp_second++;
    second_counter = 0;
    #if 0                               
      MenuFunction(MENU_FUNC_SECOND);
    #endif
  }
  if(++menu_counter >= 25) { // Quarter second time slices
    menu_counter = 0;
    result = MenuFunction(MENU_FUNC_TICK);
  }
  if(result == GUI_RESULT_NOP) {
    if(key != 0xff) {
      switch(key) {
        case KEY_SPARE : break;
        case KEY_DN    : result = MenuFunction(MENU_FUNC_KEY_DN); break;
        case KEY_UP    : result = MenuFunction(MENU_FUNC_KEY_UP); break;
        case KEY_OK    : result = MenuFunction(MENU_FUNC_KEY_OK); break;
        case KEY_MENU  : result = MenuFunction(MENU_FUNC_KEY_MENU); break;
      }
      key = 0xff;
    }
  }
  do {
    switch(result) {
      case MENU_RESULT_SETUP:   MenuFunction(MENU_FUNC_EXIT);
                                current_menu = MenuEntries[current_menu].next_alt;
                                MenuFunction(MENU_FUNC_INIT);
                                break;
      case MENU_RESULT_EXIT:    MenuFunction(MENU_FUNC_EXIT);
                                Settings.EepromStore();
                                current_menu = MenuEntries[current_menu].next;
                                result = MenuFunction(MENU_FUNC_INIT);
                                break;
      case GUI_RESULT_TIMEOUT:  MenuFunction(MENU_FUNC_EXIT);
                                Settings.EepromStore();
                                current_menu = MENU_INDEX_DEFAULT;
                                MenuFunction(MENU_FUNC_INIT);
                                break;
    }
  } while(result == MENU_RESULT_EXIT);
  return result;
}
