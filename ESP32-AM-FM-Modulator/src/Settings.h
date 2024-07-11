#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdint.h>
#include <Wire.h>
#include <SD.h>

//=======================================================
// EEPROM specification
//=======================================================

#define BT_DEVICE_NAME   "ESP32 AM/FM Transmitter"

#define EEPROM_TYPE      64 // 24Cxx where xx is EEPROM_SIZE
#define EEPROM_PAGE_SIZE 32 // in bytes, check datasheet for value

#if EEPROM_TYPE <= 16
  #define EEPROM_I2C_ADDRESS_SIZE     1  // 1 for 24C02 through 24C16
#else
  #define EEPROM_I2C_ADDRESS_SIZE     2  // 2 for 24C32 and larger
#endif

#if EEPROM_TYPE == 0
  #define EEPROM_SIZE        64
  #define EEPROM_BLOCK_SIZE  64
#elsif EEPROM_TYPE == 1
  #define EEPROM_SIZE       128
  #define EEPROM_BLOCK_SIZE 128
#else
  #define EEPROM_SIZE (256 * EEPROM_TYPE / 2)
  #if EEPROM_I2C_ADDRESS_SIZE == 1
    #define EEPROM_BLOCK_SIZE 256
  #else
    #define EEPROM_BLOCK_SIZE EEPROM_SIZE
  #endif

#endif

#define EEPROM_I2C_DEVICE         0x50
#define EEPROM_I2C_CLOCK_SPEED  100000  // Most EEPROMs work only at 100kHz

#define EEPROM_SETTINGS_PAGE         0
#define EEPROM_SETTINGS_ADDR         0

//=======================================================
// Main settings
//   - NV stored in EEPROM page 0 @ 0x00-0x7F
//   - Make sure sizeof(NV) stays below 128
//=======================================================

#define SET_SOURCE_MIN            0
#define SET_SOURCE_MAX            3

#define SET_SOURCE_SD_CARD        0
#define SET_SOURCE_WEB_RADIO      1
#define SET_SOURCE_WAVE_GEN       2
#define SET_SOURCE_BLUETOOTH      3
#define SET_SOURCE_DEFAULT        SET_SOURCE_SD_CARD


#define SET_WAVE_FORM_MIN         0
#define SET_WAVE_FORM_MAX         6   //3
#define SET_WAVE_SINE_440HZ       0
#define SET_WAVE_SINE_1KHZ        1
#define SET_WAVE_TRI_440HZ        2
#define SET_WAVE_TRI_1KHZ         3
#define SET_WAVE_DEFAULT          SET_WAVE_SINE_440HZ

#define SET_OUTPUT_MAX            2 // With FM chip mounted
#define SET_OUTPUT_AM_ONLY        0
#define SET_OUTPUT_FM_ONLY        1
#define SET_OUTPUT_AM_FM          2
#define SET_OUTPUT_DEFAULT        SET_OUTPUT_AM_ONLY

#define SET_AMGRID_MAX            2
#define SET_AMGRID_EUR            0
#define SET_AMGRID_AUS            1
#define SET_AMGRID_USA            2
#define SET_AMGRID_DEFAULT        SET_AMGRID_EUR

#define SET_AM_MOD_LEVEL_100      0
#define SET_AM_MOD_LEVEL_50       1
#define SET_AM_MOD_LEVEL_DEFAULT  SET_AM_MOD_LEVEL_100

#define SET_AM_MOD_TRIM_MIN       0
#define SET_AM_MOD_TRIM_MAX       20
#define SET_AM_MOD_TRIM_DEFAULT   7

#define SET_FM_MOD_STEREO         0
#define SET_FM_MOD_MONO           1
#define SET_FM_MOD_DEFAULT        SET_FM_MOD_STEREO

#define SET_FM_PGA_MIN            0
#define SET_FM_PGA_MAX            31
#define SET_FM_PGA_DEFAULT        0

#define SET_AM_FREQ_MIN_EUR       531  // European BC band
#define SET_AM_FREQ_MAX_EUR      1602
#define SET_AM_FREQ_STEP_EUR        9  // European grid = 9 kHz

#define SET_AM_FREQ_MIN_AUS       531  // Australia/New Zealand BC band
#define SET_AM_FREQ_MAX_AUS      1701
#define SET_AM_FREQ_STEP_AUS        9

#define SET_AM_FREQ_MIN_USA       530  // American BC Band
#define SET_AM_FREQ_MAX_USA      1700
#define SET_AM_FREQ_STEP_USA       10

#define SET_AM_FREQ_DEFAULT       990  // 990 kHz (valid in all grids)

#define SET_FM_FREQ_MIN           875  // 87.5 to 108.0 MHz
#define SET_FM_FREQ_MAX          1080
#define SET_FM_FREQ_STEP            1
#define SET_FM_FREQ_DEFAULT       999  // 99.9 MHz

#define SET_SSID_MAX               3  //max 4 networks

#define SET_WEBRADIO_MAX         199  // By far enough
//#define SET_SD_TRACKS_MAX        299 // should be by far enough
//#define SET_SD_TRACKS_MAX        99  // should be by far enough
#define SET_SD_TRACKS_MAX        5000  // should be by far enough
#define SET_SD_TRACKS_SIZE      5000//10000  // should be enough for about 200 tracks, depending on the lengths of the file names
#define SET_SD_TRACKS_PS_SIZE  128000  // should be enough for about 2500 tracks, depending on the lengths of the file names

#define SET_SOFT_RESET_MAGIC     0x5A    //  If this is set, the reset procedure will be different.

// Use the same values as Audio class, see:
// https://github.com/schreibfaul1/ESP32-audioI2S/wiki#how-to-adjust-the-balance-and-volume
#define SET_VOLUME_MIN         0
#define SET_VOLUME_MAX         21
#define SET_VOLUME_DEFAULT     4

typedef struct {
#pragma pack(1) // place at 8 bit boundaries
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
#pragma pack()
} NvSettings_t;

class SettingsClass {

  public:

    friend TwoWire & EEPROM_I2C_PORT(void);
    friend class SsidSettingsClass;
    friend class UrlSettingsClass;
    //friend class TrackSettingsClass;

    SettingsClass(TwoWire &wire = Wire, uint32_t current_speed = 400000);

    uint8_t      GetCurrentSourceId(void) { return NV.SourceAF; }
    const char * GetSourceName(void);
    const char * GetSourceName(uint8_t n);
    void AmFreqToString(char * s, int freq = 0) { AmFreqToString(s, "", freq); }
    void FmFreqToString(char * s, int freq = 0) { FmFreqToString(s, "", freq); }
    void AmFreqToString(char * s, const char * prefix, int freq = 0);
    void FmFreqToString(char * s, const char * prefix, int freq = 0);

    void SetLoopFunction(void (*f)(void)) { _loop = f; }
    void EepromLoad(void);
    void EepromStore(void);
    void EepromErase(void);
    bool LoadFromCard(const char * filename);

    void Print(void);
    uint8_t GridStep(void);
    int  CalcNearestAmChannel(int dir, int cur_freq);

    void doSoftRestart(void);
    bool isSoftRestart(void)    { return NV.MagicKey == SET_SOFT_RESET_MAGIC; }
    void clearSoftRestart(void) { NV.MagicKey = 0; EepromStore(); }

    void NextPlayer(void);
    void PrevPlayer(void);

    void PrevDiskTrack(void);
    void NextDiskTrack(void);
    void PrevRadioStation(void);
    void NextRadioStation(void);

    void NextWaveform(void);
    void PrevWaveform(void);

    void UpdateCurrentTrackTime(int adj = 1);
    
    uint8_t GetLogVolume(uint8_t range) { return GetLogVolume(NV.Volume, range); }
    uint8_t GetLogVolume(uint8_t v, uint8_t range);

    //NvSettings_t & NV() { return NV; }

    // non-volatiles start here
    NvSettings_t NV;
    // volatiles start here
    uint32_t CurrentTrackTime;
    uint32_t CurrentTrackTimeOffset;
    uint32_t TotalTrackTime;
    uint32_t SeekTrackTime;
    uint8_t  BootKeys;
    uint8_t  MenuId;  // 0 = player, > 0 = menu id
    uint8_t  AudioEnded;
    uint8_t  NoCard;
    uint8_t  Play;
    uint8_t  FirstTime;
    uint8_t  FirstSong;
    uint8_t  FmPresent;
    //uint8_t  FmUpdate;
    //uint8_t  FmReinit;
    uint16_t AmOffset50;
    uint16_t AmOffset100;
     //uint8_t  WifiConnected;
    //uint8_t  BtConnected;
    uint8_t  NewSourceAF;
    uint8_t  InitDAC;

  protected:

    void SetWireClock(void)     { if(_prevspeed != EEPROM_I2C_CLOCK_SPEED) Wire.setClock(EEPROM_I2C_CLOCK_SPEED); }
    void RestoreWireClock(void) { if(_prevspeed != EEPROM_I2C_CLOCK_SPEED) Wire.setClock(_prevspeed); }
    void loop(void)             { if(_loop) _loop(); }

    TwoWire * _wire;
    uint32_t _prevspeed;
    void (*_loop)(void);
    NvSettings_t NvMirror;
};

extern SettingsClass Settings;

//==========================================================
// SSID settings for webradio
//   - ID       : stored in EEPROM page 0 @ 0x80 (128)
//   - Password : stored in EEPROM page 1
//   - SettingsClass Settings instance must also be present
//==========================================================

#define EEPROM_SSID_PAGE       0
#define EEPROM_SSID_ADDRESS  128
#define EEPROM_PASSWORD_PAGE   1
#define EEPROM_SSID_SIZE      32
#define EEPROM_PASSWORD_SIZE  63

#pragma pack(1) // place at 8 bit boundaries

typedef struct {
  uint8_t ssid[EEPROM_SSID_SIZE+1];         // +1 for terminating zero
  uint8_t password[EEPROM_PASSWORD_SIZE+1]; // +1 for terminating zero
} wifi_credentials_t;

#pragma pack()

class SsidSettingsClass {

  public:

    SsidSettingsClass();
    void EepromLoad(void);
    void EepromStore(int n, wifi_credentials_t &);
    bool LoadFromCard(const char * filename);

    void Print(bool showpassword = false);
    const char * GetSsid(int n, bool use_zero_length = true);
    const char * GetPassword(int n) { return (const char *)_credentials[n].password; }

  protected:
    uint8_t   _valid;
    wifi_credentials_t _credentials[SET_SSID_MAX+1];
};

extern SsidSettingsClass SsidSettings;

//==========================================================
// URL settings for webradio
//   - stored in EEPROM page 2+
//   - SettingsClass Settings instance must also be present
//==========================================================

#define EEPROM_WEB_ADDRESS    (2 * 256)  // location of first entry
#define EEPROM_WEB_LIST_SIZE  (EEPROM_SIZE - EEPROM_WEB_ADDRESS)
#define EEPROM_WEB_NAME_SIZE  16  // Your personal station name
#define EEPROM_WEB_URL_SIZE   256 // The web address of the station

typedef struct {
  char name[EEPROM_WEB_NAME_SIZE+1];
  uint8_t namesize;
  const char * url;
  uint16_t  urlsize;
} web_station_t;

class UrlSettingsClass {

  public:

    UrlSettingsClass();
    void EepromLoad(void);
    bool LoadFromCard(const char * filename);

    void Print(bool raw = false);
    void GetStation(uint16_t idx, web_station_t & web_station);

  protected:
    void EepromStore(void);
    void GetStation(web_station_t & web_station);
    void NextStation(void);

  protected:

    uint8_t  _valid;
    uint16_t _ptr;
    uint16_t _index;
    uint8_t  _data[EEPROM_WEB_LIST_SIZE];
};

extern UrlSettingsClass UrlSettings;

//==========================================================
// MP3 Track list
//   - SettingsClass Settings instance must also be present
//==========================================================

class TrackSettingsClass {
  public:
    TrackSettingsClass()  { _tracks = NULL; _loop = NULL; }
    ~TrackSettingsClass() { if(_tracks != NULL) delete _tracks; }
    void AddLoopFunction(void (*f)(void)) { _loop = f; }

    bool LoadFromCard(bool keep_mounted = false);
    void Print(void);
    const char * GetTrackName(uint16_t n); // Note: n = 1 .. _number_of_tracks
    bool IsValid(void) { return _tracks != NULL && _number_of_tracks > 0; }

  protected:

    uint8_t * _tracks;
    uint32_t  _number_of_tracks;
    uint32_t  _list_size;
    uint32_t  _list_max_size;
    void    (*_loop)(void);
};

extern TrackSettingsClass TrackSettings;

#endif
