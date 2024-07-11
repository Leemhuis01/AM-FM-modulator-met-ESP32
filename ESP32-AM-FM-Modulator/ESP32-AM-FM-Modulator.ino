
// Increase size:
// https://github.com/espressif/arduino-esp32/issues/1906

#include <Audio.h>
#include <OneButton.h>
#include <driver/dac.h>

#include "src/board.h"
#include "src/settings.h"
#include "src/ChipInfo.h" 
#include "src/NFOR_SSD1306.h"
#include "src/CD4015B.h"
#include "src/PT8211.h"
#include "src/KT0803X.h"
#include "Player.h"
#include "Display.h"
#include "SimpleWaveGenerator.h"
#include "Gui.h"

SettingsClass      Settings(Wire1,I2C_LOW_CLOCK_SPEED);
KT0803X_Class      KT0803X(Wire1, PIN_FM_SW, I2C_LOW_CLOCK_SPEED);

SsidSettingsClass  SsidSettings;
UrlSettingsClass   UrlSettings;
TrackSettingsClass TrackSettings;

PlayerClass        Player;
DisplayClass       Display;

SimpleWaveGeneratorClass WaveGenerator;

#define BLUETOOTH_FILE_NAME "bluetooth.ini"
#define SSID_FILE_NAME      "ssid.ini"
#define WEBRADIO_FILE_NAME  "webradio.ini"

OneButton SW1(PIN_SW1);
OneButton SW2(PIN_SW2);
OneButton SW3(PIN_SW3);
OneButton SW4(PIN_SW4);
OneButton SW5(PIN_SW5);

//=====================================================================
//== Locals: Set AM Modulator
//=====================================================================

static void SetAmModLevel(bool update_pin = true) {
  dac_output_enable(DAC_CHANNEL_1); // Need to re-init DAC every time alfer I2S change
  dac_output_voltage(DAC_CHANNEL_1, Settings.NV.AmModLevel ? Settings.AmOffset50 : Settings.AmOffset100 - Settings.NV.AmTrim);
  if(update_pin) {
    digitalWrite(PIN_AM_DEPTH, Settings.NV.AmModLevel ? 1 : 0);
    DEBUG_MSG_VAL("AM modulation depth set to %d%%\n", Settings.NV.AmModLevel ? 50 : 100);
  }
}

static void SetAmDivider(void) {
  uint8_t AM_ON = Settings.NV.OutputSel != SET_OUTPUT_FM_ONLY;
  int divider = Settings.NV.FreqAM / Settings.GridStep() - 1;

  CD4015B_WriteByte(divider, AM_ON, 0); 
  DEBUG_MSG_VAL4("AM PLL Divider %d (0x%02X), frequency %d, inh %d\n", divider, divider, Settings.NV.FreqAM, AM_ON);
}

static void SetOutputSel(void) {
  SetAmDivider();
  if(Settings.NV.OutputSel != SET_OUTPUT_AM_ONLY) {
    digitalWrite(PIN_FM_SW, 1);
    if(!KT0803X.Start(0)) {
      Settings.NV.OutputSel = SET_OUTPUT_AM_ONLY;
      digitalWrite(PIN_FM_SW, 0);
    }
  }
  else {
    digitalWrite(PIN_FM_SW, 0);
  }
}

//#define DEBUG_CALCULATE_AM_MOD_OFFSETS

#define MOD_OFFSET_LOW_THRESHOLD     16  // Note: ADC kicks in on at about 0.1 volts, so we need to tune a bit, with Settings.NV.AmTrim
//#define MOD_OFFSET_HIGH_THRESHOLD  3300  // see: https://randomnerdtutorials.com/esp32-adc-analog-read-arduino-ide/
#define MOD_OFFSET_HIGH_THRESHOLD  2700  // temp threshold, needs different value on final PCBA
#define DELAY_WAVE()  delayMicroseconds(80)  

static void CalculateAmModOffsets(void) {
  int adc  = 0;
  #ifdef DEBUG_CALCULATE_AM_MOD_OFFSETS
    int a[256];
  #endif 
  int16_t dac;
    
  PT8211_Init(); // Reassign the pins for bit banged I2S
  Settings.AmOffset100 = UINT16_MAX;
  // Calculate for 100% depth
  digitalWrite(PIN_AM_OFF, LOW); 
  digitalWrite(PIN_AM_DEPTH, LOW); 
  PT8211_Write(INT16_MAX, INT16_MAX);
  dac_output_enable(DAC_CHANNEL_1);   // Need to re-init DAC every time alfer I2S change
  dac_output_voltage(DAC_CHANNEL_1, 0);
  delay(1); // Check C24 if delay doesn't work (C24 must be 10nF and not 100nF)
  #ifdef DEBUG_CALCULATE_AM_MOD_OFFSETS
  for(dac = 0; dac < 256; dac++) {
  #else
  for(dac = 0; dac < 256 && Settings.AmOffset100 == UINT16_MAX; dac++) {
  #endif
    dac_output_voltage(DAC_CHANNEL_1, dac);
    DELAY_WAVE(); 
    #ifdef DEBUG_CALCULATE_AM_MOD_OFFSETS
      a[dac] = 
    #endif
    adc = analogRead(PIN_ADC);
    if(Settings.AmOffset100 == UINT16_MAX && adc >= MOD_OFFSET_LOW_THRESHOLD)
      Settings.AmOffset100 = dac;// - Settings.NV.AmTrim;
  }
  #ifdef DEBUG_CALCULATE_AM_MOD_OFFSETS
  Serial.println("");
  for(dac = 0; dac < 256; dac += 8) {
    Serial.printf("Seed: %3i, ADC: ", dac);
    for(int i = 0; i < 8; i++)
      Serial.printf("%4i  ", a[dac+i]);
    Serial.println("");
  }
  #endif
  Serial.printf("\nOffset for 100%% depth: %i\n", Settings.AmOffset100);
   
  // Calculate for to 50% depth
  Settings.AmOffset50 = UINT16_MAX;
  digitalWrite(PIN_AM_DEPTH, HIGH);  
  PT8211_Write(INT16_MIN, INT16_MIN);
  dac_output_voltage(DAC_CHANNEL_1, 0);
  delay(1); 
  #ifdef DEBUG_CALCULATE_AM_MOD_OFFSETS
  for(dac = 0; dac < 256; dac++) {
  #else
  for(dac = 0; dac < 256 && Settings.AmOffset50 == UINT16_MAX; dac++) {
  #endif
    dac_output_voltage(DAC_CHANNEL_1, dac);
    DELAY_WAVE(); 
    #ifdef DEBUG_CALCULATE_AM_MOD_OFFSETS
      a[dac] = 
    #endif
    adc = analogRead(PIN_ADC);
    if(Settings.AmOffset50 == UINT16_MAX && adc >= MOD_OFFSET_HIGH_THRESHOLD)
      Settings.AmOffset50 = dac;
  }
  digitalWrite(PIN_AM_DEPTH, 0);
  #ifdef DEBUG_CALCULATE_AM_MOD_OFFSETS
  Serial.println("");
  for(dac = 0; dac < 256; dac += 8) {
    Serial.printf("Seed: %3i, ADC: ", dac);
    for(int i = 0; i < 8; i++)
      Serial.printf("%4i  ", a[dac+i]);
    Serial.println("");
  }
  #endif
  Serial.printf("Offset for  50%% depth: %i\n", Settings.AmOffset50);
}

static void SetFmGainAndFreq(void) {
  KT0803X.SetFreq(Settings.NV.FreqFM, false);
  KT0803X.SetPga(Settings.NV.FmPga, false); 
  KT0803X.SetMono(Settings.NV.FmModType, true);
}

//=====================================================================//
//== Local: serial_command parser (for debugging purposes)           ==//
//=====================================================================//

static struct {
  const char * shortname;
  const char * longname;
  bool suppress_help;
} serial_command_list[] = {
  { "?",      "help"            , false }, // 0
  { "s",      "settings"        , false }, // 1
  { "rl",     "radiolist"       , false }, // 2
  { "rlr",    "radiolistraw"    , false }, // 3
  { "ssid",   "ssid"            , false }, // 4
  { "ssidpw", "ssidpassword"    , true  }, // 5
  { "tl",     "tracklist"       , false }, // 6
  { "pi",     "playinfo"        , false }, // 7
  { "sd",     "sd-card"         , false }, // 8
  { "wr",     "webradio"        , false }, // 9
  { "wf",     "waveform"        , false }, // 10
  { "bt",     "bluetooth"       , false }, // 11
  { "res",    "softreset"       , false }, // 12
  { "pw",     "printwaveform"   , false }, // 13
  { "pwh",    "printwaveformhex", false }, // 14
  { "sys",    "systeminfo"      , false }, // 15
  { "oled",   "oled-display"    , false }, // 16
};

static void serial_command_help(void) {
  Serial.println("Short   | Long");
  Serial.println("--------|-------------");
  for (int i = 0; i < sizeof(serial_command_list) / sizeof(serial_command_list[0]); i++)
    if (!serial_command_list[i].suppress_help)
      Serial.printf("%-8s| %s\n", serial_command_list[i].shortname, serial_command_list[i].longname);
}

static int find_command_match(String &r) {
  for(int i = 0; i < sizeof(serial_command_list) / sizeof(serial_command_list[0]); i++) {
    if(r.equals(serial_command_list[i].shortname) || r.equals(serial_command_list[i].longname)) return i;
  }
  return -1;
}

static void change_player(uint8_t old_id, uint8_t new_id);

static void parse_command(String &r) { 
  r.trim();
  r.toLowerCase();
  Serial.print("SERIAL COMMAND: ");
  Serial.println(r);
  switch(find_command_match(r)) {
    case  0: serial_command_help();     break;
    case  1: Settings.Print();          break;
    case  2: UrlSettings.Print(false);  break;
    case  3: UrlSettings.Print(true);   break;
    case  4: SsidSettings.Print(false); break;
    case  5: SsidSettings.Print(true);  break;
    case  6: TrackSettings.Print();     break;
    case  7: Player.SD_PrintDebug();    break;
    case  8: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_SD_CARD);   break;
    case  9: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_WEB_RADIO); break;
    case 10: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_WAVE_GEN);  break;
    case 11: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_BLUETOOTH); break;
    case 12: Settings.doSoftRestart();    break;
    case 13: Player.WP_PrintDebug(false); break;
    case 14: Player.WP_PrintDebug(true);  break;
    case 15: PrintChipInfo(0x0F); break;
    case 16: OLED.Print(); break;
    default: int vol = r.toInt();
             if (vol > 0) {
               Player.SetVolume(vol);
             }
  }
}

//=====================================================================
//== Local: Timestamps
//=====================================================================

static uint32_t timestamp_10ms = 0;
static uint32_t timestamp_1sec = 0;
static bool     ten_ms_tick    = false;
static bool     second_tick    = false;

static void UpdateTimestamps(void) {
  static uint64_t timestamp_us_prev  = 0;
  static uint16_t timestamp_1sec_cnt = 0;

  uint64_t timestamp_us = esp_timer_get_time();
  ten_ms_tick = false;
  second_tick = false;
  if(timestamp_us - timestamp_us_prev >= TIMESTAMP_10MS_DIV) {
    timestamp_10ms++;
    timestamp_1sec_cnt++;
    timestamp_us_prev += TIMESTAMP_10MS_DIV;
    if(timestamp_us - timestamp_us_prev < TIMESTAMP_10MS_DIV) // supress multiple ticks 
      ten_ms_tick = true; 
    if(timestamp_1sec_cnt == 100) {
      timestamp_1sec++;
      timestamp_1sec_cnt = 0;
      second_tick = true;
    }
  }
}

//=====================================================================//
//== Local: Read setup and track data from SD Card                   ==//
//=====================================================================//

static bool ReadSetupFromCard(bool get_init_data = true) {
  if(Settings.NV.SourceAF == SET_SOURCE_WAVE_GEN) return true; // Nothing to load for waveform player
  if (!SD.begin(PIN_VSPI_SS, SPI)) {
    Serial.println("Card Mount Failed, using SSID & Radio lists from EEPROM");
    Settings.NoCard = 1;
    return false;
  }
  Serial.println("Card Mount Succeeded");
  Settings.NoCard = 0;
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD   : TrackSettings.LoadFromCard();
                                break;
    case SET_SOURCE_WEB_RADIO : if(!SsidSettings.LoadFromCard(SSID_FILE_NAME))
                                  Serial.println("Using SSID list from EEPROM");
                                if(!UrlSettings.LoadFromCard(WEBRADIO_FILE_NAME))
                                  Serial.println("Using web station list from EEPROM");
                                break;
    case SET_SOURCE_BLUETOOTH : if(!Settings.LoadFromCard(BLUETOOTH_FILE_NAME))
                                  Serial.println("Using Bluetooth ID from EEPROM");
                                break;
  }
  SD.end(); // webradio doesn't like a mounted SD Card
  return true;
}

//=====================================================================//
//== Local: change player                                            ==//
//=====================================================================//

static void change_player(uint8_t old_id, uint8_t new_id) {
  if(new_id != old_id) {
    Serial.print("Change Player from \"");
    Serial.print(Settings.GetSourceName(old_id));
    Serial.print("\" to \"");
    Serial.print(Settings.GetSourceName(new_id));
    Serial.println("\"");
    Player.Stop();
    Settings.Play = 0;
    Settings.NV.SourceAF = new_id;
    Display.ShowBaseScreen(DISPLAY_HELP_PLEASE_WAIT);
    delay(100); // Give threads in Stop() some time to die out properly
    Settings.doSoftRestart();
  }
}

static void change_player(bool prev) {
  uint8_t CurrentSourceId = Settings.GetCurrentSourceId();
  if(prev) Settings.PrevPlayer();
  else     Settings.NextPlayer();
  change_player(CurrentSourceId, Settings.GetCurrentSourceId());
}

//=====================================================================//
//== Local: Loop function for Settings (keeps audio running)         ==//
//=====================================================================//

void SettingsLoopFunction(void) { Player.loop(); }

//=====================================================================//
//== Local: Button handlers                                          ==//
//=====================================================================//

static void GUI_Handler(uint8_t key) {
  if(key != 0xFF) Serial.printf("Button %d pressed: ", key);
  switch(GuiCallback(key, timestamp_10ms)) {
    case GUI_RESULT_NEXT_PLAY      : Player.PlayNext();     break;
    case GUI_RESULT_PREV_PLAY      : Player.PlayPrevious(); break;
    case GUI_RESULT_PAUSE_PLAY     : if(Settings.FirstTime) {
                                       Serial.println("FIRST TIME");    
                                       Settings.FirstSong = 1;
                                       Player.Play();
                                       Settings.FirstTime = 0;
                                     }
                                     else
                                       Player.PauseResume();
                                     break;
    //case GUI_RESULT_SEEK_PLAY    : Player.PositionEntry(); break;
    //case GUI_RESULT_STOP_PLAY    : break;
    //case GUI_RESULT_START_PLAY   : break;
    case GUI_RESULT_NEW_AM_FREQ    : SetAmDivider();     Display.ShowFrequencies(); break;
    case GUI_RESULT_NEW_FM_FREQ    : SetFmGainAndFreq(); Display.ShowFrequencies(); break;
    case GUI_RESULT_NEW_OUTPUT_SEL : SetOutputSel();     Display.ShowFrequencies(); break;
    case GUI_RESULT_NEW_AF_SOURCE  : change_player(Settings.NV.SourceAF, Settings.NewSourceAF); break;
    case GUI_RESULT_NEW_AM_TRIM    : SetAmModLevel(false); break;
    case GUI_RESULT_NEW_AM_DEPTH   : SetAmModLevel(true);  break;
    case GUI_RESULT_NEW_FM_MOD_TYPE:
    case GUI_RESULT_NEW_FM_PGA     : SetFmGainAndFreq(); break;
    default: break;
  }
  if(key != 0xFF) Serial.println();
}

static void SW1_HandleClick(void) { GUI_Handler(KEY_MENU); } // GUI_Handler(KEY_SPARE); }
static void SW2_HandleClick(void) { GUI_Handler(KEY_DN);   }
static void SW3_HandleClick(void) { GUI_Handler(KEY_UP);   }
static void SW4_HandleClick(void) { GUI_Handler(KEY_OK);   }
static void SW5_HandleClick(void) { GUI_Handler(KEY_MENU); }

//=====================================================================//
//== Main Set-up                                                     ==//
//=====================================================================//
//
// It appears that switching to another player cannot be done dynamically.
// Some parts of the player seems not to be cleaned up properly, so the new
// player will not work, or is unstable. Therefore, the solution is to do a
// soft restart to assure the player get a fresh end clean environment.
// It appears that switching to another player cannot be done dynamically.
// Some parts of the player seems not to be cleaned up properly, so the new
// player will not work, or is unstable. Therefore, the solution is to do a
// soft restart to assure the player get a fresh end clean environment.

void setup(void) {
  //Serial
  Serial.begin(115200);

  pinMode(PIN_CD4015B_DAT, OUTPUT); // Shared with PIN_AM_OFF
  pinMode(PIN_CD4015B_CLK, OUTPUT);
  pinMode(PIN_AM_DEPTH, OUTPUT);
  pinMode(PIN_FM_SW, OUTPUT);

  //SD(SPI)
  pinMode(PIN_VSPI_SS, OUTPUT);
  digitalWrite(PIN_VSPI_SS, HIGH);
  SPI.begin(PIN_VSPI_SCK, PIN_VSPI_MISO, PIN_VSPI_MOSI);
  SPI.setFrequency(1000000);

  // I2C
  Wire.begin(PIN_SDA1, PIN_SCL1, I2C_HIGH_CLOCK_SPEED); // For OLED
  Wire.setBufferSize(256 + 8);
  Wire1.begin(PIN_SDA2, PIN_SCL2, I2C_LOW_CLOCK_SPEED); // For EEPROM
  Wire1.setBufferSize(256 + 8);
  Wire1.setTimeout(10); // Prevents long time-out when KT0803L is not responding

  // Buttons
  // uint8_t boot_keys = 0;
  Settings.BootKeys = 0;
  if(digitalRead(PIN_SW1) == LOW) Settings.BootKeys |= BOOT_KEY1_MASK;//boot_keys++;
  if(digitalRead(PIN_SW2) == LOW) Settings.BootKeys |= BOOT_KEY2_MASK;//boot_keys++;
  if(digitalRead(PIN_SW3) == LOW) Settings.BootKeys |= BOOT_KEY3_MASK;//boot_keys++;
  if(digitalRead(PIN_SW4) == LOW) Settings.BootKeys |= BOOT_KEY4_MASK;//boot_keys++;
  if(digitalRead(PIN_SW5) == LOW) Settings.BootKeys |= BOOT_KEY5_MASK;//boot_keys++;
  Serial.printf("Button boot keys pressed at startup 0x%02X \n", Settings.BootKeys);

  SW1.attachClick(SW1_HandleClick);
  SW2.attachClick(SW2_HandleClick);
  SW3.attachClick(SW3_HandleClick);
  SW4.attachClick(SW4_HandleClick);
  SW5.attachClick(SW5_HandleClick);

  //if (boot_keys >= 2) {
  if (Settings.BootKeys == 0x12) {
    Settings.EepromErase();       // Use this when settings have got corrupted
  }
  Settings.EepromLoad();          // Get the primary settings form EEPROM
  Display.Initialize(Settings.isSoftRestart());
  if(Settings.isSoftRestart()) {   // A soft restart does not show the welcome screen, so it
    Settings.clearSoftRestart();   // simulates an impression of a dynamically change of player.
    Settings.Play = 1;             // After a soft restart, we start playing immediately (Note: BT always starts immediately)
    if(Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO) {
      SsidSettings.EepromLoad();
      UrlSettings.EepromLoad();
      if(Settings.NV.TotalSsid == 0 || Settings.NV.WebRadioTotalStations == 0)
        ReadSetupFromCard();
    }
    if(Settings.NV.SourceAF == SET_SOURCE_SD_CARD)
      ReadSetupFromCard(); // (Re)load the track list
  }
  else {
    UpdateTimestamps();
    uint32_t timestamp_now = timestamp_10ms;
    Display.ShowWelcome(); 
    if(Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO) {
      SsidSettings.EepromLoad();
      UrlSettings.EepromLoad();
    }
    ReadSetupFromCard();
    while(timestamp_10ms < timestamp_now + 300)  // Show welcome screen at least 3 seconds
      UpdateTimestamps();
    Settings.Play = Settings.NV.SourceAF == SET_SOURCE_BLUETOOTH; // Bluetooth is alwayf "autoplay"
  }
  Settings.SetLoopFunction(SettingsLoopFunction);
  CalculateAmModOffsets(); 
  SetOutputSel();
  SetAmModLevel();
  Settings.FmPresent = KT0803X.CheckPresent();
  if(Settings.FmPresent) {
    if(Settings.NV.OutputSel != SET_OUTPUT_AM_ONLY) {
      KT0803X.SetFreq(Settings.NV.FreqFM, false);
      KT0803X.SetPga(Settings.NV.FmPga, false); 
      KT0803X.SetMono(Settings.NV.FmModType, false);
      if(!KT0803X.Start(0)) { // Error starting FM Tranmitter (KT0803L)
        Settings.FmPresent = 0;
        Serial.println("Error starting FM Tranmitter (KT0803L), selecting AM only output");
        Settings.NV.OutputSel = SET_OUTPUT_AM_ONLY;
      }
    }
  }
  else {
    Serial.println("FM Tranmitter (KT0803L) not found, selecting AM only output");
    Settings.NV.OutputSel = SET_OUTPUT_AM_ONLY;
  }
  //KT0803X.Init();
  Display.ShowBaseScreen(Settings.Play ? DISPLAY_HELP_PLEASE_WAIT : DISPLAY_HELP_PRESS_PLAY_TO_START);
  Serial.printf("Current player is \"%s\"\n", Settings.GetSourceName());
  Player.Start(Settings.Play);

  if(Settings.Play == 1 || Settings.NV.SourceAF == SET_SOURCE_BLUETOOTH) {
    //Player.Play();
    //Player.Start(true);
    Settings.FirstTime = 0;
  }
  else {
    //Player.Start(false);
    Serial.println("Press \"Play\" to start");
    Display.ShowPlayPause(Settings.Play);
  }
  GuiInit();
  // OMT: test
  //for(int i = 0; i <= Settings.NV.VolumeSteps; i++)
  //  Serial.printf("Volume test: %d of %d => %d of 63; %d => %d of 127\n", i, Settings.NV.VolumeSteps,Settings.GetLogVolume(i, 63), Settings.NV.VolumeSteps, Settings.GetLogVolume(i, 127)); 
  // /OMT
}

//=====================================================================//
//== Main Loop                                                       ==//
//=====================================================================//

void loop(void) {
  static unsigned run_time = 0;

  UpdateTimestamps();
  if(second_tick) {
    Settings.EepromStore();
    if(Settings.InitDAC) { // Some audio drivers destroys the DAC, so reint if necessary
      SetAmModLevel();
      Settings.InitDAC = 0;
    }
  }
  Player.loop(ten_ms_tick, second_tick); // runs also audio.loop() if applicable

  //Button logic
  SW1.tick();
  SW2.tick();
  SW3.tick();
  SW4.tick();
  SW5.tick();
  GUI_Handler(KEY_NONE);
  //Serial port commands
  if (Serial.available()) {
    String r = Serial.readStringUntil('\n');
    parse_command(r);
  }
}
