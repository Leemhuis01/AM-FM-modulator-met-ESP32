#include <arduino.h>
#include <Wire.h>
#include "Settings.h"
#include "board.h"

#define NvData        ((unsigned char *)&NV)
#define NvMirrorData  ((unsigned char *)&NvMirror)


#if EEPROM_I2C_ADDRESS_SIZE == 1
  #error THIS APPLICATION ACCEPS ONLY EEPROMS >= 32K
#endif

const char * const s_not_printable = "[Not Printable]";
const char * const s_none          = "[None]";

//=======================================================
// Helper functions
//=======================================================

static int SD_read_line(File &f, char * dest, int n) {
  memset(dest, 0, n);

  // See: https://stackoverflow.com/questions/65621607/read-line-by-line-in-file-and-store-into-string-flash-data-saving-spiffs
  int len = f.readBytesUntil('\n', dest, n);
  if (len && dest[len - 1] == '\r')
    dest[--len] = '\0'; // If a '\r' is present, remove it!
  return len;
}

#if 1
static void HEXR(uint8_t s[], int n) {
  for(int i = 0; i < n; i++)
    Serial.printf("%02X",s[i]);
  Serial.println();
}
#else
  #define HEXR(a,b)
#endif

#if 0
static void HEXP(uint8_t s[], int n) {
  for(int i = 0; i < n; i++)
    Serial.printf("%02X",s[i]);
  Serial.println();
}
#else
  #define HEXP(a,b)
#endif

inline static int Validate(int val, int min, int max, int def) {
  if(val < min || val > max)
    return def;
  return val;
}
inline static int Validate(int val, int min, int max) {
  if(val < min)
    return max;
  if (val > max)
    return min;
  return val;
}

TwoWire & EEPROM_I2C_PORT(void) { return *Settings._wire; }

#define WIRE EEPROM_I2C_PORT()

//#define WIRE (*Settings.GetWire())

static void Read24Cxx(int addr, uint8_t * dat, int len) {
#if EEPROM_I2C_ADDRESS_SIZE == 2 // Only for 24C32 and larger
  WIRE.beginTransmission(EEPROM_I2C_DEVICE);
  WIRE.write(addr >> 8);
  WIRE.write(addr & 0xFF);
  if( WIRE.endTransmission() != 0) return;
  for(int i = 0; i < len; i+= 64) {
    if(len-i >= 64) {
       WIRE.requestFrom(EEPROM_I2C_DEVICE, 64, (int)(len-i == 64));
       WIRE.readBytes(&dat[i], 64);
    }
    else {
      WIRE.requestFrom(EEPROM_I2C_DEVICE, len-i, 1);
      WIRE.readBytes(&dat[i], len-i);
    }
  }
#endif
}

static void Read24Cxx(int page, int addr, uint8_t * dat, int len) {
#if EEPROM_I2C_ADDRESS_SIZE == 1
  WIRE.beginTransmission(EEPROM_I2C_DEVICE | (page & 7));
  WIRE.write(addr);
  if(WIRE.endTransmission() != 0) return;
  WIRE.requestFrom(EEPROM_I2C_DEVICE| (page & 7), len, 1);
  WIRE.readBytes(dat, len);
#else
  WIRE.beginTransmission(EEPROM_I2C_DEVICE);
  WIRE.write(page);
  WIRE.write(addr);
  if(WIRE.endTransmission() != 0) return;
  WIRE.requestFrom(EEPROM_I2C_DEVICE, len, 1);
  WIRE.readBytes(dat, len);
#endif
}

static void Write24Cxx(int page, int addr, uint8_t dat) {
  #if EEPROM_I2C_ADDRESS_SIZE == 1
    WIRE.beginTransmission(EEPROM_I2C_DEVICE | (page & 7));
  #else
    WIRE.beginTransmission(EEPROM_I2C_DEVICE);
    WIRE.write(page);
  #endif
  WIRE.write(addr);
  WIRE.write(dat);
  if(WIRE.endTransmission() != 0) return;
  delay(5);
}

static void Write24CxxPage(uint16_t addr, uint8_t * dat) {
  addr /= EEPROM_PAGE_SIZE; // Place on page boundary
  addr *= EEPROM_PAGE_SIZE;
  DEBUG_MSG_VAL("Writing EEPROM page at 0x%04X : ", addr);
  HEXR(dat, EEPROM_PAGE_SIZE);

  #if EEPROM_I2C_ADDRESS_SIZE == 1
    WIRE.beginTransmission(EEPROM_I2C_DEVICE | ((addr >> 8) & 7));
  #else
    WIRE.beginTransmission(EEPROM_I2C_DEVICE);
    WIRE.write(addr >> 8);
  #endif
  WIRE.write(addr & 0xFF);
  WIRE.write(dat, EEPROM_PAGE_SIZE);
  if(WIRE.endTransmission() != 0) return;
  delay(5);
}

// Write each byte individually. Optimizing is possible, but then you must consider the
// page size of the EEPROM. Page size could be 8,16,32,64,128 bytes. See datasheet for info.
static void Write24Cxx(int page, int addr, uint8_t * dat, int len) {
  for(int i = 0; i < len; i++)
    Write24Cxx(page, addr+i, dat[i]);
}

//==============================================================================
// Main settings
//   - NV stored in EEPROM page 0 @ 0x00-0x7F
//============================================================================== 

uint8_t SettingsClass::GridStep(void) {
  return NV.GridAM == SET_AMGRID_USA ? SET_AM_FREQ_STEP_USA : SET_AM_FREQ_STEP_EUR;
}

int SettingsClass::CalcNearestAmChannel(int dir, int cur_freq) {
  int new_freq = cur_freq;
  switch(NV.GridAM) {
    case SET_AMGRID_EUR: new_freq = new_freq - new_freq % SET_AM_FREQ_STEP_EUR;
                         if(dir > 0)
                           new_freq += SET_AM_FREQ_STEP_EUR;
                         else if(dir < 0)
                           new_freq -= SET_AM_FREQ_STEP_EUR;
                           new_freq = Validate(new_freq, SET_AM_FREQ_MIN_EUR,SET_AM_FREQ_MAX_EUR);
                         Serial.printf("EUR AM frequency calculated to %u kHz\n", new_freq);
                         break;
    case SET_AMGRID_AUS: new_freq = new_freq - new_freq % SET_AM_FREQ_STEP_AUS;
                         if(dir > 0)
                           new_freq += SET_AM_FREQ_STEP_AUS;
                         else if(dir < 0)
                           new_freq -= SET_AM_FREQ_STEP_AUS;
                           new_freq = Validate(new_freq, SET_AM_FREQ_MIN_AUS,SET_AM_FREQ_MAX_AUS);
                         Serial.printf("AUS AM frequency calculated to %u kHz\n", new_freq);
                         break;
    case SET_AMGRID_USA: new_freq = new_freq - new_freq % SET_AM_FREQ_STEP_USA;
                         if(dir > 0)
                           new_freq += SET_AM_FREQ_STEP_USA;
                         else if(dir < 0)
                           new_freq -= SET_AM_FREQ_STEP_USA;
                           new_freq = Validate(new_freq, SET_AM_FREQ_MIN_USA,SET_AM_FREQ_MAX_USA);
                         Serial.printf("USA AM frequency calculated to %u kHz\n", new_freq);
                         break;
  }
  return new_freq;
}

// Public fuctions

SettingsClass::SettingsClass(TwoWire &wire, uint32_t current_speed) {
  memset(this, 0, sizeof(SettingsClass));
  _loop      = NULL;
  _prevspeed = current_speed;
  _wire      = &wire;
}

const char * SettingsClass::GetSourceName(uint8_t n) {
  static const char * player_name[SET_SOURCE_MAX + 1] =
    { "SD Card Player", "Webradio Player",  "Waveform Player",  "Bluetooth Player" };
  if(n == 0xFF) return(s_none);
  return player_name[n];
}

const char * SettingsClass::GetSourceName(void) { return GetSourceName(NV.SourceAF); }

void SettingsClass::AmFreqToString(char * s, const char * prefix, int freq) { //max size "1602kHz(187m)" => 13
  int wave;
  if(freq == 0) freq = NV.FreqAM;
  wave = (299792 * 2) / freq;      // fixed point with one bit fraction
  wave = (wave >> 1) + (wave & 1); // round to nearest integer
  sprintf(s, "%s%dkHz(%dm)", prefix, freq, wave);
}

void SettingsClass::FmFreqToString(char * s, const char * prefix, int freq) { //max size "100.0MHz(C43+)" => 14
  const char *sub = "";
  if(freq == 0) freq = NV.FreqFM;
  int chan    = (freq - 870) / 3;
  int subchan = (freq - 870) % 3;  
  if(subchan == 1) sub = "+";
  if(subchan == 2) sub = "-", chan++;
  sprintf(s, "%s%d.%dMHz(C%d%s)", prefix, freq/10, freq%10, chan, sub);
}

void SettingsClass::doSoftRestart(void) {
  NV.MagicKey = SET_SOFT_RESET_MAGIC;
  EepromStore();
  esp_restart();
}
#if 0
  uint8_t  SourceAF;       // 0 = SD-Card, 1 = web radio, 2 = Tone generator, 3 = Bluetooth audio
  uint8_t  OutputSel;      // 0 = AM; 1 = FM; 2 = AM + FM;
  uint16_t FreqAM;         // In kHz
  uint16_t FreqFM;         // In steps of 100 kHz (875 - 1080)
  uint8_t  GridAM;         // 0 = Eur:531-1602@9kHz, 1 = Aus:531-1701@9kHz, 2 = USA:530-1700@10kHz
  uint8_t  AmModLevel;     // 0 = full (max 100%), 1 = half (max 50%)
  uint8_t  AmTrim;         // AM modulation trimming for 100% depth
  uint8_t  FmModType;      // 0 = stereo, 1 = mono
uint8_t  FmPga;          // KT0803L audio gain
uint8_t  Reserved1;
  uint8_t  CurrentSsid;    // 0,1,2,3:  We can connect to up to four wifi access points
  uint8_t  TotalSsid;      // Valid number of SSISs
  uint8_t  MagicKey;       // When set to SET_SOFT_RESET value, the ESP32 wil perform a restart.
  uint8_t  WaveformId;     // See "SimpleWaveGenerator.h" for the list of wave forms
  uint16_t DiskTotalTracks;
  uint16_t DiskCurrentTrack;
  uint32_t DiskTrackResumeTime;
  uint32_t DiskTrackResumePos;
  uint32_t DiskTrackTotalTime;
  uint16_t WebRadioCurrentStation;
  uint16_t WebRadioTotalStations;
  uint8_t  Volume;         // Volume settingsd only for debugging puposes;
  uint8_t  VolumeSteps;    // The AM/FM application plays always on max volume
  uint8_t  BtName[32];
#endif

void SettingsClass::EepromLoad(void) {
  DEBUG_MSG_VAL2("EEPROM type 24C%02d (size = %d bytes)\n", EEPROM_TYPE, EEPROM_SIZE);
  // Read from EEPROM
  SetWireClock();
  Read24Cxx(0, 0, NvData, sizeof(NvSettings_t));
  RestoreWireClock();
  memcpy(NvMirrorData, NvData, sizeof(NvSettings_t));
#if 1
  // Check for valid entries
  NV.SourceAF         = Validate(NV.SourceAF,    0,                    SET_SOURCE_MAX,      SET_SOURCE_DEFAULT);
  NV.OutputSel        = Validate(NV.OutputSel,   0,                    SET_OUTPUT_MAX,      SET_OUTPUT_DEFAULT);
  NV.GridAM           = Validate(NV.GridAM,      0,                    SET_AMGRID_MAX,      SET_AMGRID_DEFAULT);
  NV.FreqAM           = CalcNearestAmChannel(0,  NV.FreqAM);
  NV.FreqFM           = Validate(NV.FreqFM,      SET_FM_FREQ_MIN,      SET_FM_FREQ_MAX,     SET_FM_FREQ_DEFAULT);
  NV.AmModLevel       = Validate(NV.AmModLevel,  SET_AM_MOD_LEVEL_100, SET_AM_MOD_LEVEL_50, SET_AM_MOD_LEVEL_100);
  NV.AmTrim           = Validate(NV.AmTrim,      SET_AM_MOD_TRIM_MIN,  SET_AM_MOD_TRIM_MAX, SET_AM_MOD_TRIM_DEFAULT);
  NV.FmModType        = Validate(NV.FmModType,   SET_FM_MOD_STEREO,    SET_FM_MOD_MONO,     SET_FM_MOD_STEREO);
  NV.FmPga            = Validate(NV.FmPga,       SET_FM_PGA_MIN,       SET_FM_PGA_MAX,      SET_FM_PGA_DEFAULT);
  NV.WaveformId       = Validate(NV.WaveformId,  SET_WAVE_FORM_MIN,    SET_WAVE_FORM_MAX,   SET_WAVE_DEFAULT);
  NV.VolumeSteps      = Validate(NV.VolumeSteps, SET_VOLUME_MIN,       SET_VOLUME_MAX,      SET_VOLUME_MAX);
  #if DAC_ID == DAC_ID_PT8211
    NV.Volume         = Validate(NV.Volume,      SET_VOLUME_MIN,       NV.VolumeSteps,      SET_VOLUME_DEFAULT);
  #else
    NV.Volume         = Validate(NV.Volume,      SET_VOLUME_MIN,       NV.VolumeSteps,      NV.VolumeSteps);
  #endif
  NV.DiskTotalTracks  = Validate(NV.DiskTotalTracks, 0, SET_SD_TRACKS_MAX + 1, 0); 
  NV.DiskCurrentTrack = Validate(NV.DiskCurrentTrack, 1, NV.DiskTotalTracks, 1);
  NV.CurrentSsid      = Validate(NV.CurrentSsid, 0, SET_SSID_MAX, 0);
  NV.TotalSsid        = Validate(NV.TotalSsid,   0, SET_SSID_MAX + 1, 0);
  NV.WebRadioCurrentStation = Validate(NV.WebRadioCurrentStation, 1, SET_WEBRADIO_MAX, 1);
  NV.WebRadioTotalStations  = Validate(NV.WebRadioTotalStations,  0, SET_WEBRADIO_MAX, 0);

  if(NV.MagicKey != SET_SOFT_RESET_MAGIC)  NV.MagicKey = 0;
  if(NV.DiskTrackResumeTime == UINT32_MAX) NV.DiskTrackResumeTime = 0;
  if(NV.DiskTrackResumePos  == UINT32_MAX) NV.DiskTrackResumePos  = 0;
  if(NV.DiskTrackTotalTime  == UINT32_MAX) NV.DiskTrackTotalTime  = 0;
  if(NV.BtName[0] < 0x21 || NV.BtName[0] >= 127) {
  strcpy((char*)NV.BtName, BT_DEVICE_NAME);
  }
#endif
  // Save corrected entries if necessary
  EepromStore();
  // Init voltatiles
  _prevspeed       = 0;
  CurrentTrackTime = 0;
  CurrentTrackTimeOffset = 0;
  TotalTrackTime   = 0;
  SeekTrackTime    = 0;
  AudioEnded       = 0;
  NoCard           = 1;
  Play             = 0;
  FirstTime        = 1;
  FmPresent        = 1; // assume the FM chip is present
  //FmUpdate         = 0;
  //FmReinit         = 0;
  AmOffset50       = 0;
  AmOffset100      = 0;
  NewSourceAF      = NV.SourceAF;

}

void SettingsClass::EepromStore(void) {
  for(int i = 0; i < sizeof(NvSettings_t); i++) {
    if(NvMirrorData[i] != NvData[i]) {
      NvMirrorData[i] = NvData[i];
      SetWireClock();
      Write24Cxx(0, i, NvMirrorData[i]);
      RestoreWireClock();
      Serial.printf("EEPROM write %2i:%5u (%02X)\n", i, NvMirrorData[i], NvMirrorData[i]);
    }
  }
}

void SettingsClass::EepromErase(void) {
  uint8_t FF[EEPROM_PAGE_SIZE];
  memset(FF, 0xFF, EEPROM_PAGE_SIZE);
  SetWireClock();
  for(int i = 0; i < sizeof(NvSettings_t); i += EEPROM_PAGE_SIZE) {
    Write24CxxPage(i, FF); // Erase the contents
  }
  RestoreWireClock();
}

bool SettingsClass::LoadFromCard(const char * filename) {
  char fn[32];
  
  Serial.printf("Reading \"%s\"", filename);
  if(strlen(filename) > sizeof(fn) - 1) {
    Serial.println(" failed, file name too long");
    return false;
  }
  fn[0] = '/';
  strcpy(&fn[1], filename);
  File f = SD.open(fn, FILE_READ);
  if (f == 0) {
    Serial.println(" failed to open file");
    return false;
  }
  if(!SD_read_line(f, (char *)NV.BtName, sizeof(NV.BtName))) 
  strcpy((char*)NV.BtName, BT_DEVICE_NAME); // failed: use default name  
  Settings.EepromStore();
  f.close();
  return true;
}

#define PRINT_SETTINGS_VALUE_DEC(v) Serial.printf("  " #v " = %d\n", v), loop()
#define PRINT_SETTINGS_VALUE_HEX(v) Serial.printf("  " #v " = 0x%02X\n", v), loop()
#define PRINT_SETTINGS_VALUE(v,f)   Serial.printf("  " #v " = %" #f "\n", v), loop()

void SettingsClass::Print(void) {
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.SourceAF);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.OutputSel);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.FreqAM);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.FreqFM);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.GridAM);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.AmModLevel);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.AmTrim);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.FmModType);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.FmPga);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.CurrentSsid);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.TotalSsid);
  PRINT_SETTINGS_VALUE_HEX(Settings.NV.MagicKey);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.WaveformId);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.DiskTotalTracks);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.DiskCurrentTrack);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.DiskTrackResumeTime);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.DiskTrackResumePos);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.DiskTrackTotalTime);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.WebRadioCurrentStation);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.WebRadioTotalStations);
  PRINT_SETTINGS_VALUE(Settings.NV.BtName,s);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.Volume);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV.VolumeSteps);
  PRINT_SETTINGS_VALUE(Settings.CurrentTrackTime,d);
  PRINT_SETTINGS_VALUE(Settings.CurrentTrackTimeOffset,d);
  PRINT_SETTINGS_VALUE(Settings.TotalTrackTime,d);
  PRINT_SETTINGS_VALUE(Settings.SeekTrackTime,d);
  PRINT_SETTINGS_VALUE(Settings.BootKeys,d);
  PRINT_SETTINGS_VALUE(Settings.MenuId,d);
  PRINT_SETTINGS_VALUE(Settings.AudioEnded,d);
  PRINT_SETTINGS_VALUE(Settings.NoCard,d);
  PRINT_SETTINGS_VALUE(Settings.Play,d);
  PRINT_SETTINGS_VALUE(Settings.FirstTime,d);
  PRINT_SETTINGS_VALUE(Settings.FirstSong,d);
  PRINT_SETTINGS_VALUE(Settings.FmPresent,d);
  //PRINT_SETTINGS_VALUE(Settings.FmUpdate,d);
  //PRINT_SETTINGS_VALUE(Settings.FmReinit,d);
  PRINT_SETTINGS_VALUE(Settings.AmOffset50,d);
  PRINT_SETTINGS_VALUE(Settings.AmOffset100,d);
  //PRINT_SETTINGS_VALUE(Settings.WifiConnected,d);
  //PRINT_SETTINGS_VALUE(Settings.BtConnected,d);
  PRINT_SETTINGS_VALUE(Settings.InitDAC,d);
}

void SettingsClass::NextPlayer(void) {
  if(Settings.NV.SourceAF >= SET_SOURCE_MAX)
    Settings.NV.SourceAF = SET_SOURCE_MIN;
  else 
    Settings.NV.SourceAF++;
}

void SettingsClass::PrevPlayer(void) {
  if(Settings.NV.SourceAF == SET_SOURCE_MIN)
    Settings.NV.SourceAF = SET_SOURCE_MAX;
  else 
    Settings.NV.SourceAF--;
}

void SettingsClass::PrevDiskTrack(void) {
  if(NV.DiskTotalTracks > 0) {
    if(NV.DiskCurrentTrack > 1)
      NV.DiskCurrentTrack--;
    else
      NV.DiskCurrentTrack = NV.DiskTotalTracks;
  }
}

void SettingsClass::NextDiskTrack(void) {
  if(NV.DiskTotalTracks > 0) {
    if (Settings.NV.DiskCurrentTrack < Settings.NV.DiskTotalTracks)
      Settings.NV.DiskCurrentTrack++;
    else
      Settings.NV.DiskCurrentTrack = 1;
  }
}

void SettingsClass::PrevRadioStation(void) {
  if(NV.WebRadioTotalStations > 0) {
    if(NV.WebRadioCurrentStation > 1)
      NV.WebRadioCurrentStation--;
    else
      NV.WebRadioCurrentStation = NV.WebRadioTotalStations;
  }
}

void SettingsClass::NextRadioStation(void) {
  if(NV.WebRadioTotalStations > 0) {
    if (Settings.NV.WebRadioCurrentStation < Settings.NV.WebRadioTotalStations)
      Settings.NV.WebRadioCurrentStation++;
    else
      Settings.NV.WebRadioCurrentStation = 1;
  }
}

void SettingsClass::NextWaveform(void) {
  NV.WaveformId = Validate(NV.WaveformId + 1, SET_WAVE_FORM_MIN, SET_WAVE_FORM_MAX);
}

void SettingsClass::PrevWaveform(void) {
  NV.WaveformId = Validate(NV.WaveformId - 1, SET_WAVE_FORM_MIN, SET_WAVE_FORM_MAX);
}

void SettingsClass::UpdateCurrentTrackTime(int adj) {
  CurrentTrackTime = (int32_t)CurrentTrackTime + adj;
  if(CurrentTrackTime >= 99 * 3600)
    CurrentTrackTime = 0;
}

uint8_t SettingsClass::GetLogVolume(uint8_t v, uint8_t range) {
  if(NV.VolumeSteps == 0) return range;
  if(v > NV.VolumeSteps) v = NV.VolumeSteps;
  int logvol = (int)range * (int)v * (int)v / (int)NV.VolumeSteps / (int)NV.VolumeSteps;
  return logvol > v ? logvol : v;
}

//==========================================================
// SSID settings for webradio 
//   - ID       : stored in EEPROM page 0 @ 0x80 (128)
//   - Password : stored in EEPROM page 1
//   - SettingsClass Settings instance must also be present
//==========================================================

SsidSettingsClass::SsidSettingsClass() {
  memset(this, 0, sizeof(SsidSettingsClass));
}

void SsidSettingsClass::EepromLoad(void) {
  memset(this, 0, sizeof(SsidSettingsClass));
  Settings.SetWireClock();
  for(int i = 0; i < Settings.NV.TotalSsid; i++) {
    Read24Cxx(EEPROM_SSID_PAGE,     i * EEPROM_SSID_SIZE + EEPROM_SSID_ADDRESS, _credentials[i].ssid, EEPROM_SSID_SIZE);
    Read24Cxx(EEPROM_PASSWORD_PAGE, i * EEPROM_PASSWORD_SIZE, _credentials[i].password, EEPROM_PASSWORD_SIZE);
  }
  Settings.RestoreWireClock();
  _valid = _credentials[0].ssid[0] != 0xFF;
}

void SsidSettingsClass::EepromStore(int n, wifi_credentials_t &c) {
  Settings.SetWireClock();
  if(memcmp(_credentials[n].ssid, c.ssid, EEPROM_SSID_SIZE) != 0) {
    HEXP(_credentials[n].ssid, EEPROM_SSID_SIZE);
    HEXP(c.ssid, EEPROM_SSID_SIZE);
    memcpy(_credentials[n].ssid, c.ssid, EEPROM_SSID_SIZE);
    Write24Cxx(EEPROM_SSID_PAGE, n * EEPROM_SSID_SIZE + EEPROM_SSID_ADDRESS, c.ssid, EEPROM_SSID_SIZE);
    Serial.printf("Saving SSID[%d]: %s \n", n, c.ssid);
  }
  if(memcmp(_credentials[n].password, c.password, EEPROM_PASSWORD_SIZE) != 0) {
    memcpy(_credentials[n].password, c.password, EEPROM_PASSWORD_SIZE);
    Write24Cxx(EEPROM_PASSWORD_PAGE, n * EEPROM_PASSWORD_SIZE, c.password, EEPROM_PASSWORD_SIZE);
    Serial.printf("Saving PASS[%d]: %s \n", n, "********"/*c.password*/);
  }
  Settings.RestoreWireClock();
}

bool SsidSettingsClass::LoadFromCard(const char * filename) {
  char fn[32];
  
  Serial.printf("Reading \"%s\"", filename);
  if(strlen(filename) > sizeof(fn) - 1) {
    Serial.println(" failed, file name too long");
    return false;
  }
  fn[0] = '/';
  strcpy(&fn[1], filename);
  File f = SD.open(fn, FILE_READ);
  if (f == 0) {
    Serial.println(" failed to open file");
    return false;
  }
  int n1, n2;
  wifi_credentials_t wifi_credentials;
  Settings.NV.TotalSsid = 0;
  memset(&wifi_credentials, 0, sizeof(wifi_credentials_t));
  Serial.print(": ");
  for (int i = 0; i <= SET_SSID_MAX; i++) {
    n1 = SD_read_line(f, (char *)wifi_credentials.ssid, sizeof(wifi_credentials.ssid));
    n2 = SD_read_line(f, (char *)wifi_credentials.password, sizeof(wifi_credentials.password));
    if (n1 && n2) {
      EepromStore(Settings.NV.TotalSsid++, wifi_credentials);
    }
  }
  Serial.printf("Found %d SSIDs\n", Settings.NV.TotalSsid);
  if (Settings.NV.CurrentSsid >= Settings.NV.TotalSsid)
    Settings.NV.CurrentSsid = 0;
  Settings.EepromStore();
  f.close();
  return true;
}

void SsidSettingsClass::Print(bool showpassword) { // For debug purposes
  if(Settings.NV.TotalSsid == 0) {
    Serial.println("No SSIDs Defined");
    return;
  }
  for(int i = 0; i < Settings.NV.TotalSsid; i++) {
    bool printable = isPrintable(_credentials[i].ssid[0]);
    Serial.printf("SSID: \"%s\", PASS: \"", printable ? (char *)_credentials[i].ssid : s_not_printable);
    if(showpassword) {
      printable = isPrintable(_credentials[i].password[0]);
      Serial.print(printable ? (char *)_credentials[i].password : s_not_printable);
    }
    else
      Serial.print("********");
    Serial.println("\"");
    Settings.loop(); // Keep critical function running
  }
}

const char * SsidSettingsClass::GetSsid(int n, bool use_zero_length) { 
  const char *ssid = (const char *)_credentials[n].ssid;
  return use_zero_length || ssid[0] != '\0' ? ssid : s_none;
}


//==========================================================
// URL settings for webradio 
//   - stored in EEPROM page 2+
//   - SettingsClass Settings instance must also be present
//==========================================================

UrlSettingsClass::UrlSettingsClass() {
  _valid = 0;
  _ptr   = 0;
  _index = 0;;
  memset(_data, 0xFF, EEPROM_WEB_LIST_SIZE);
}

void UrlSettingsClass::EepromLoad(void) {
  Settings.SetWireClock();
  memset(_data, 0xFF, EEPROM_WEB_LIST_SIZE);
  Read24Cxx(EEPROM_WEB_ADDRESS, _data, EEPROM_WEB_LIST_SIZE);
  _valid = 1;
  Settings.RestoreWireClock();
}

void UrlSettingsClass::EepromStore(void) {
  // We must use page writes here.
  // Otherwise writing 7680 (when using 24C64) individual bytes,
  // will take at least 7680 * 5ms = 38 seconds
  uint8_t buf[EEPROM_PAGE_SIZE];

  Settings.SetWireClock();
  for(int i = 0; i < EEPROM_WEB_LIST_SIZE; i += EEPROM_PAGE_SIZE) {
    int addr = i + EEPROM_WEB_ADDRESS;
    Read24Cxx(addr, buf, EEPROM_PAGE_SIZE); // Read the current contents
    if(memcmp(buf, &_data[i], EEPROM_PAGE_SIZE) != 0) {
      Write24CxxPage(addr, &_data[i]); // Write the (new) contents
    }
  }
  Settings.RestoreWireClock();
 }
 
bool UrlSettingsClass::LoadFromCard(const char * filename) {
  char fn[32];
  
  Serial.printf("Reading \"%s\"", filename);
  if(strlen(filename) > sizeof(fn) - 1) {
      Serial.println(" failed, file name too long");
      return false;
  }
  fn[0] = '/';
  strcpy(&fn[1], filename);
  File f = SD.open(fn, FILE_READ);
  if (f == 0) {
    Serial.println(" failed to open file");
    return false;
  }
  Serial.print(": ");
  int n;
  uint8_t buff[256];
  Settings.NV.WebRadioTotalStations  = 0;
  _ptr = _index = 0;
  while((n = SD_read_line(f, (char *)buff, sizeof(buff))) != 0) {
    n++; // We include the terminating zero
    if(_ptr + n <= EEPROM_WEB_LIST_SIZE) { // Check if it fits in the list
      memcpy(&_data[_ptr], buff, n);
      _ptr += n;
      Settings.NV.WebRadioTotalStations++;
    }
  }
  Serial.printf("Found %d stations, using %d bytes of %d in list.\n", Settings.NV.WebRadioTotalStations, _ptr, EEPROM_WEB_LIST_SIZE);
  if (Settings.NV.WebRadioCurrentStation > Settings.NV.WebRadioTotalStations)
    Settings.NV.WebRadioCurrentStation = 1;
  EepromStore();
  Settings.EepromStore();
  _valid = 1;
  f.close();
  return true;
}

void UrlSettingsClass::Print(bool raw) {
  uint16_t old_ptr   = _ptr;
  uint16_t old_index = _index;
  web_station_t stat;

  if(Settings.NV.WebRadioTotalStations == 0 || !_valid) {
    Serial.println("No Radio URLs Defined");
    return;
  }
  _ptr = _index = 0;
  for(int i = 0; i < Settings.NV.WebRadioTotalStations; i++) {
    if(raw) {
      Serial.println(isPrintable(_data[_ptr]) ? (char *)&_data[_ptr] : s_not_printable);
    }
    else {
      GetStation(stat);
      Settings.loop(); // Keep critical function running
      Serial.printf("[%d] = %-16s: %s\n", _index + 1, stat.name,stat.url);
    }
    NextStation();
    Settings.loop();   // Keep critical function running
  }
  _ptr   = old_ptr;
  _index = old_index;
}

void UrlSettingsClass::NextStation(void) {
  if(isPrintable(_data[_ptr])) {
    _ptr += strlen((char *)&_data[_ptr]) + 1;
  }
  _index++;
}

void UrlSettingsClass::GetStation(web_station_t & web_station) {
  if(!isPrintable(_data[_ptr])) {
    sprintf(web_station.name, "[%d]", _index + 1);
    web_station.namesize = strlen(web_station.name);
    web_station.url = s_not_printable;
    web_station.urlsize = strlen(web_station.url);
    return;
  }

  char * p1 = (char *)&_data[_ptr];
  char * p2 = strchr(p1, '|');

  if(p2 != NULL) {
    web_station.namesize = p2 - p1;
    if(web_station.namesize > EEPROM_WEB_NAME_SIZE)
       web_station.namesize = EEPROM_WEB_NAME_SIZE;
    strncpy(web_station.name, p1, web_station.namesize);
    web_station.name[web_station.namesize] = 0; // terminate with 0
    web_station.url = p2 + 1; // skip the '|
    web_station.urlsize = strlen(web_station.url);
  }
  else {
    sprintf(web_station.name, "[%d]", _index + 1);
    web_station.namesize = strlen(web_station.name);
    web_station.url = p1;
    web_station.urlsize = strlen(p1);
  }
}

void UrlSettingsClass::GetStation(uint16_t idx, web_station_t & web_station) {
  if(Settings.NV.WebRadioTotalStations > 0) {
    _ptr = _index = 0;
    for(int i = 1; i < idx && i <= Settings.NV.WebRadioTotalStations; i++) {
      NextStation();
      Settings.loop(); // Keep critical function running
    }
    GetStation(web_station);
  }
}

//==========================================================
// MP3 Track list 
//   - SettingsClass Settings instance must also be present
//==========================================================

bool TrackSettingsClass::LoadFromCard(bool keep_mounted) {
  if(_tracks == NULL) { 
    if(psramFound()) {
      _list_max_size = SET_SD_TRACKS_PS_SIZE;
      _tracks = (uint8_t *)ps_malloc(SET_SD_TRACKS_PS_SIZE);
    }
    else {
      _list_max_size = SET_SD_TRACKS_SIZE;
      _tracks = (uint8_t *)malloc(SET_SD_TRACKS_SIZE);
    }
  }
  memset(_tracks, 0, _list_max_size);
  _number_of_tracks = 0;
  _list_size        = 0;
   
  if (!SD.begin(PIN_VSPI_SS, SPI)) {
    Serial.println("Card Mount Failed, Cannot read track list");
    Settings.NoCard = 1;
    return false;
  }
  Settings.NoCard = 0;
  File _root;
  File _file;
  Serial.printf("Counting audio files ... ");
//for(int ff = 0; ff <10; ff++) // test by 10x number of files
{
  _root = SD.open("/");
  if (!_root || !_root.isDirectory()) {
    Serial.println("failed to open root directory");
    return false;
  }
  _file = _root.openNextFile();
  while (_file && _list_size < _list_max_size) {
    if (!_file.isDirectory()) {
      String temp = _file.name();
      temp.toLowerCase();
      if (temp.endsWith(".wav") || temp.endsWith(".mp3")) {
        int f_len = temp.length() + 1; // include the terminal 0
        if(_list_size + f_len < _list_max_size) {
          _number_of_tracks++;
          strcpy((char *)_tracks + _list_size, _file.name());
          _list_size += f_len;
        }   
      }
    }
    _file = _root.openNextFile();
  }
  _root.close();
}
  Serial.printf("found %d audio files, (list memory usage %d of %d, average file name size %d)\n", _number_of_tracks, _list_size, _list_max_size, _list_size / _number_of_tracks);
  Settings.NV.DiskTotalTracks = _number_of_tracks;
  if (Settings.NV.DiskCurrentTrack > Settings.NV.DiskTotalTracks) {
    Settings.NV.DiskCurrentTrack = 1;
    Serial.println("Reset DiskCurrentTrack");
  }
  Settings.EepromStore();
  if(!keep_mounted)
    SD.end(); // webradio doesn't like a mounted SD Card
  return true;  
}

void TrackSettingsClass::Print(void) {
  if(Settings.NoCard) {
    Serial.println("No card found");
    return;
  }
  if(_number_of_tracks == 0) {
    Serial.println("Track list is empty");
    return;
  }
  char * p = (char *)_tracks;
  for(int i = 0; i < _number_of_tracks; i++) {
    Serial.printf("[%d] = %s\n", i + 1, p);
    p += strlen(p) + 1;
    if(_loop != NULL) _loop();
  }
}

const char * TrackSettingsClass::GetTrackName(uint16_t  n) {
  if(n > _number_of_tracks) return "";
  char * p = (char *)_tracks;
  for(int i = 1; i < n; i++) {
    p += strlen(p) + 1;
  }
  return p;
}

 