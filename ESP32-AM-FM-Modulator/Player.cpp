
#include "src/ChipInfo.h"
#include "Player.h"
#include "Display.h"
#include "SimpleWaveGenerator.h"

#define BT_ACTIVITY_TIMER   10  // in 10 ms slots

static void AudioDieOut(Audio * _audio, int loops = 100) {
  if(_audio == NULL) return;
  for(int i = 0; i < loops; i++) {
    uint64_t timestamp_us = esp_timer_get_time();
    while(esp_timer_get_time() < timestamp_us + 1000) { // every millisecond
      _audio->loop();
    }
  }
}

//******************************************************//
// Constructor                                          //
//******************************************************//

PlayerClass::PlayerClass() {
  _loop             = NULL;
  _audio            = NULL;
  _a2dp_sink        = NULL;
  _waveform_player  = NULL;
  _activity_counter = 0;
  _activity_sum     = 1;
  _connected        = false;
}

//******************************************************//
// SD Card Player                                       //
//******************************************************//

void PlayerClass::SD_PrintDebug(void) {
  if(_audio != NULL) {
    Serial.printf("getAudioCurrentTime : %d\n", _audio->getAudioCurrentTime());
    Serial.printf("getAudioFileDuration: %d\n", _audio->getAudioFileDuration());
    Serial.printf("getSampleRate       : %d\ngetBitsPerSample    : %d\n", _audio->getSampleRate(), _audio->getBitsPerSample());
    Serial.printf("getFileSize         : %d\ngetFilePos          : %d\n", _audio->getFileSize(), _audio->getFilePos());
    Serial.printf("getBitRate          : %d\n", _audio->getBitRate());
    Serial.printf("getChannels         : %d\n", _audio->getChannels());
  }
}

bool PlayerClass::PlayTrackFromSD(int n, uint32_t resume_pos, uint32_t resume_time) {
  _audio->stopSong();
  if(n == 0 || n > Settings.NV.DiskTotalTracks) {
    Serial.printf("Illegal track number: 0 < n < %d\n", Settings.NV.DiskTotalTracks + 1);
    return false;
  }
  String track(TrackSettings.GetTrackName(n));
  Serial.printf("Starting [%d]: \"%s\" at %d\n", n, track.c_str(), resume_pos);
  Display.ShowTrackTitle(track.c_str());
  //_audio->connecttoSD(track.c_str(), resume_pos);  // Appears to be no longer in the library
  _audio->connecttoFS(SD, track.c_str(), resume_pos);
  Settings.AudioEnded             = 0;
  Settings.NV.DiskCurrentTrack    = n;
  Settings.NV.DiskTrackResumePos  = resume_pos;
  Settings.NV.DiskTrackResumeTime = resume_time;
  Settings.CurrentTrackTimeOffset = resume_time;
  Settings.CurrentTrackTime       = resume_time; 
  Settings.TotalTrackTime         = 0;   // Will become available later, see loop()
  return true;
}

//******************************************************//
// All Players                                          //
//******************************************************//

static void avrc_metadata_callback(uint8_t attribute, const uint8_t *data) {
  switch(attribute) {
    case 0x01: Serial.printf("AVRC metadata: title \"%s\"\n", data);
               Settings.CurrentTrackTime = -1; 
               Display.ShowTrackTitle(strlen((const char *)data) > 0 ? (const char *)data : (const char *)Settings.NV.BtName);
               break;
    default  : Serial.printf("AVRC metadata: 0x20  rsp: attribute id 0x%x, %s\n", attribute, data); break;
  }
}

static void bt_volumechange(int v) { 
  Serial.printf("bt_volumechange: %d\n",v);
  Player.CheckBluetoothVolume(v);
}

//static bool address_validator(esp_bd_addr_t remote_bda) { return true; }
//static void sample_rate_callback(uint16_t rate) { Serial.printf("sample_rate_callback: %d\n", rate); }
//static void rssi_callback(esp_bt_gap_cb_param_t::read_rssi_delta_param &rssi) { Serial.printf("rssi_callback\n"); }

void bt_raw_stream_reader(const uint8_t* pdata, uint32_t len) { 
  Player._activity_counter = BT_ACTIVITY_TIMER;  
  static int i = 0;
  if(len < 100) return; // Ignore small packets
  if(!(++i & 0xF)) { // We examine the data only once in 16 times, to save some processor performance
    const uint16_t *p = (const uint16_t *)pdata;
    uint32_t sum = 0; 
    for(int j = 0; j < len/2; j++) sum += *p++;
    Player._activity_sum = sum;
    #if 0
    //Serial.printf("%d ", len);
    //Serial.printf("%-2d", Player._activity_sum != 0);
    Serial.printf("%8d ", Player._activity_sum);
    if(!(i & 0xFF))
      Serial.println();
    #endif
  }
}

static void bt_on_data_received(void) {
 // _activity_counter = BT_ACTIVITY_TIMER; 
  /*
  static int i = 0;
  Serial.print('d');
  if(++i == 64) {
    Serial.println();
    i = 0;
  }*/
}

void PlayerClass::Stop(void) { // Stop() must be followed by a reboot (or in the future perhaps Start())
  Serial.printf("Stopping player \"%s\"\n", Settings.GetSourceName(Settings.NV.SourceAF));
  if(_audio != NULL) { _audio->stopSong(); AudioDieOut(_audio); delete _audio; _audio = NULL; }
  if(_a2dp_sink != NULL) { _a2dp_sink->pause(); }  // Still need to pause BT, because it's a separate thread
  if(_waveform_player != NULL) _waveform_player->Pause();;
}

void PlayerClass::Start(bool autoplay) {
  Serial.printf("Initialize player \"%s\"\n", Settings.GetSourceName(Settings.NV.SourceAF));
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : if(Settings.NoCard || !TrackSettings.IsValid()) { 
                                   Display.ShowHelpLine("Loading Track List...", true);
                                   TrackSettings.LoadFromCard(true); // If previous session had no card, or after a soft restart.
                                 }
                                 else {
                                   if (!SD.begin(PIN_VSPI_SS, SPI)) {
                                     Serial.println("Card Mount Failed, Cannot play track");
                                     Display.ShowHelpLine(DISPLAY_HELP_NO_CARD, true);
                                     Settings.NoCard = 1;
                                     break;
                                   }
                                   Serial.println("Card Mount Succeeded");
                                   Settings.NoCard    = 0;
                                 }
                                 if(Settings.NoCard) { //
                                   Display.ShowHelpLine(DISPLAY_HELP_NO_CARD, true);
                                   Settings.Play = 0;
                                 }
                                 else {
                                   Display.ShowHelpLine(Settings.Play ? DISPLAY_HELP_NONE : DISPLAY_HELP_PRESS_PLAY_TO_START, true);
                                   Settings.FirstSong = 1;
                                   Settings.CurrentTrackTime = Settings.NV.DiskTrackResumeTime;
                                   Settings.TotalTrackTime   = Settings.NV.DiskTrackTotalTime;
                                 }
                                 _audio = new Audio;
                                 _audio->setPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
                                 #if DAC_ID == DAC_ID_PT8211
                                   _audio->setI2SCommFMT_LSB(true);
                                   _audio->setVolume(SET_VOLUME_MAX);
                                 #else
                                   _audio->setVolume(SET_VOLUME_DEFAULT);
                                 #endif
                                 Settings.InitDAC = 1;
                                 break;
    case SET_SOURCE_BLUETOOTH  : Display.ShowHelpLine("Setting up Bluetooth");
                                 _a2dp_sink = new BluetoothA2DPSink;
                                 if(_a2dp_sink == NULL)
                                   Serial.println("Error creating BluetoothA2DPSink");
                                 else {
                                   Serial.println("Initializing created");
                                   i2s_pin_config_t my_pin_config = {
                                     .mck_io_num   = I2S_PIN_NO_CHANGE,
                                     .bck_io_num   = PIN_I2S_BCK,
                                     .ws_io_num    = PIN_I2S_WS,
                                     .data_out_num = PIN_I2S_DOUT,
                                     .data_in_num  = I2S_PIN_NO_CHANGE
                                   };
                                   _a2dp_sink->set_pin_config(my_pin_config);
                                   #if DAC_ID == DAC_ID_PT8211
                                     i2s_config_t my_i2s_config = {
                                       .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
                                       .sample_rate = 44100, // updated automatically by A2DP
                                       .bits_per_sample = (i2s_bits_per_sample_t)16,
                                       .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
                                       .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_MSB),
                                       .intr_alloc_flags = 0, // default interrupt priority
                                       .dma_buf_count = 8,
                                       .dma_buf_len = 64,
                                       .use_apll = true,
                                       .tx_desc_auto_clear = true // avoiding noise in case of data unavailability
                                     };
                                     _a2dp_sink->set_i2s_config(my_i2s_config);
                                   #endif                                   
                                   _a2dp_sink->set_avrc_metadata_callback(avrc_metadata_callback);
                                   //_a2dp_sink->set_sample_rate_callback(sample_rate_callback);
                                   _a2dp_sink->set_on_volumechange(bt_volumechange);
                                   //_a2dp_sink->set_rssi_callback(rssi_callback);
                                   _a2dp_sink->set_raw_stream_reader(bt_raw_stream_reader);
                                   //_a2dp_sink->set_on_data_received(bt_on_data_received);
                                   _a2dp_sink->start((const char *)Settings.NV.BtName, true);
                                   _a2dp_sink->reconnect(); // OMT new
                                   Display.ShowHelpLine((const char *)Settings.NV.BtName);
                                 }
                                 _activity_counter = 0;
                                 break;
    case SET_SOURCE_WEB_RADIO  :{uint64_t timestamp_us = esp_timer_get_time();
                                 uint8_t  timeout = 25;
                                 Display.ShowHelpLine(DISPLAY_HELP_INITIALIZING_WEBRADIO, true);
                                 Serial.printf("Connecting to %s ", SsidSettings.GetSsid(Settings.NV.CurrentSsid), false);
                                 WiFi.begin(SsidSettings.GetSsid(Settings.NV.CurrentSsid), SsidSettings.GetPassword(Settings.NV.CurrentSsid));
                                 while (WiFi.status() != WL_CONNECTED  && timeout > 0) {
                                   if( esp_timer_get_time() >= timestamp_us + 500000) {
                                      timestamp_us = esp_timer_get_time();
                                      timeout--;
                                      Serial.print(".");
                                   }
                                   // OMT: to do: Keep display running
                                 }
                                 if(WiFi.status() != WL_CONNECTED) {
                                   Serial.println(" CONNECTION FAILED");
                                   //Display.ShowHelpLine(DISPLAY_HELP_WIFI_CONNECT_FAILED, true);
                                   Display.ShowWebRadioConnect(false);
                                   //Settings.WifiConnected = 0;
                                 }
                                 else {
                                   Serial.println(" CONNECTED");
                                   if(Settings.Play)
                                     Display.ShowHelpLine(DISPLAY_HELP_WIFI_CONNECTED, true);
                                   else
                                     Display.ShowHelpLine(DISPLAY_HELP_PRESS_PLAY_TO_START, true);
                                     //Display.ShowHelpLine("PRESS_PLAY_TO_START", true);
                                   //Settings.WifiConnected = 1;
                                 }
                                 _audio = new Audio;
                                 _audio->setPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
                                 #if DAC_ID == DAC_ID_PT8211
                                   _audio->setI2SCommFMT_LSB(true);
                                   _audio->setVolume(SET_VOLUME_MAX);
                                 #else
                                   _audio->setVolume(SET_VOLUME_DEFAULT);
                                 #endif
                                 Settings.InitDAC = 1;
                                }break;
    
    case SET_SOURCE_WAVE_GEN   : _waveform_player = new SimpleWaveGeneratorClass;
                                 _waveform_player->Init();
                                 _waveform_player->Volume(Settings.GetLogVolume(WAVEFORM_VOLUME_MAX),false);
                                 break;
    default: break;
  }
  Display.ShowPlayer();
  //SetVolume(Settings.NV.Volume);
  if(autoplay)
    Play();
//PrintChipInfo(0xC);
}

void PlayerClass::Play(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : if(Settings.NoCard)
                                   break;
                                 else {
                                   if(Settings.FirstSong) {
                                     PlayTrackFromSD(Settings.NV.DiskCurrentTrack, Settings.NV.DiskTrackResumePos, Settings.NV.DiskTrackResumeTime);
                                     Settings.FirstSong = 0;
                                   }
                                   else
                                     PlayTrackFromSD(Settings.NV.DiskCurrentTrack);
                                 }
                                 Settings.Play = 1;
                                 Display.ShowPlayPause(Settings.Play);
                                 Display.ShowPlayTime(true);
                                 Display.ShowTrackNumber();
                                 break;
    case SET_SOURCE_BLUETOOTH  : Settings.CurrentTrackTime = 0;
                                 //Display.ShowBluetoothId();
                                 Display.ShowBluetoothName();
                                 Settings.Play = 0; // omt
                                 Display.ShowPlayPause(0);
                                 break;
    case SET_SOURCE_WEB_RADIO  : if(WiFi.isConnected()) {
                                   web_station_t web_station;
                                   UrlSettings.GetStation(Settings.NV.WebRadioCurrentStation, web_station);
                                   Serial.printf("**********start a new radio: %s\n",web_station.url);
                                   _audio->connecttohost(web_station.url);
                                   Serial.println("**********start a new radio************");
                                   Settings.AudioEnded = 0;
                                   Settings.Play = 1;
                                 }
                                 else
                                   Settings.Play = 0;
                                 Settings.CurrentTrackTime = 0;
                                 Display.ShowWebRadioNumber();
                                 Display.ShowPlayPause(Settings.Play);
                                 Serial.println("**********new radio started************");
                                 break;
    case SET_SOURCE_WAVE_GEN   : _waveform_player->Volume(SET_VOLUME_DEFAULT,false); 
                                 _waveform_player->Start(Settings.NV.WaveformId);
                                 Settings.Play = 1;
                                 Settings.CurrentTrackTime = 0;
                                 _waveform_player->Resume();
                                 Display.ShowPlayPause();
                                 Display.ShowPlayTime();
                                 Display.ShowWaveformName(_waveform_player->GetWaveformName(Settings.NV.WaveformId));
                                 //Display.ShowTrackTitle("_waveform_player->GetWaveformName(Settings.NV.WaveformId)");  // OMT: ticker test
                                 Display.ShowWaveformId();
                                 break;
    default:                     break;
  }
}

IRAM_ATTR void PlayerClass::loop(bool ten_ms_tick, bool seconds_tick) {
  if(_audio != NULL)  _audio->loop();
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD   :{  uint32_t t = _audio->getAudioCurrentTime() + Settings.CurrentTrackTimeOffset;
                                  if(Settings.CurrentTrackTime != t) {
                                    if(t)
                                      Settings.CurrentTrackTime = t;
                                    t = _audio->getAudioFileDuration(); 
                                    if(t && Settings.TotalTrackTime != t) {
                                      Settings.TotalTrackTime = t; // Sometimes the total time updates after start
                                    }
                                    Display.ShowPlayTime(); //GuiUpdatePlayingTime();
                                    if(Settings.CurrentTrackTime > Settings.TotalTrackTime + 6) { // +6 for some tolerance
                                     Settings.AudioEnded = 1;
                                     _audio->stopSong(); // Somtimes the "audio_eof_mp3" is not generated, so force a stop
                                     audio_eof_mp3("time out on audio_eof_mp3");
                                   } 
                                 }
                               }
                                 if(Settings.AudioEnded)
                                   PlayNext();
                                 break;
    case SET_SOURCE_BLUETOOTH  : if(ten_ms_tick &&_activity_counter > 0) {
                                   _activity_counter--;
                                 }
                                 if(Settings.Play) {
                                   if(_activity_counter == 0 || _activity_sum == 0) {
                                     //Serial.printf("\n*** STOP %d,%d***\n", _activity_counter, _activity_sum);
                                     Settings.Play = 0; 
                                     Display.ShowPlayPause();
                                   }
                                 }
                                 else {
                                   if(_activity_counter > 0 && _activity_sum != 0) {
                                     //Serial.printf("\n*** PLAY %d,%d***\n", _activity_counter, _activity_sum);
                                     Settings.Play = 1; 
                                     Display.ShowPlayPause();
                                   }
                                 }
                                 if(seconds_tick) {
                                   CheckBluetoothVolume(_a2dp_sink->get_volume()); // a minimum volume is needed to run properly
                                   if(Settings.Play) {
                                     Settings.UpdateCurrentTrackTime();
                                     Display.ShowPlayTime();
                                   }
                                   if(!_a2dp_sink->is_connected() && _connected) {
                                      Display.ShowTrackTitle((const char *)Settings.NV.BtName);
                                   }
                                   _connected = _a2dp_sink->is_connected();
                                 }
                                 break;
    case SET_SOURCE_WEB_RADIO  : if(seconds_tick && _audio->isRunning()) {
                                   Settings.UpdateCurrentTrackTime();
                                   Display.ShowPlayTime();
                                   if(Settings.CurrentTrackTime == 2) { // Show the station name from list. 
                                     web_station_t web_station;
                                     UrlSettings.GetStation(Settings.NV.WebRadioCurrentStation, web_station);
                                     Display.ShowStationName(web_station.name);
                                   }
                                 }
                                 if(Settings.AudioEnded)
                                   PlayNext();
                                 break;
    case SET_SOURCE_WAVE_GEN   : _waveform_player->loop();
                                 if(seconds_tick && Settings.Play) {
                                   Settings.UpdateCurrentTrackTime();
                                   Display.ShowPlayTime();
                                 }
                                 break;
    default:                     break;
  }
  Display.loop(ten_ms_tick);
}

void PlayerClass::PlayNext(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : Settings.NextDiskTrack();    Play(); break;
    case SET_SOURCE_WEB_RADIO  : Settings.NextRadioStation(); Play(); break;
    case SET_SOURCE_BLUETOOTH  : _a2dp_sink->next();
                                 Settings.CurrentTrackTime = 0; 
                                 break; 
    case SET_SOURCE_WAVE_GEN   : Settings.NextWaveform();
                                 Play();
                                 break;
    default: break;
  }  
}

void PlayerClass::PlayPrevious(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : Settings.PrevDiskTrack();    Play(); break;
    case SET_SOURCE_WEB_RADIO  : Settings.PrevRadioStation(); Play(); break;
    case SET_SOURCE_BLUETOOTH  : if(Settings.CurrentTrackTime >= 5) 
                                   _a2dp_sink->rewind();
                                 else
                                   _a2dp_sink->previous();
                                 Settings.CurrentTrackTime = 0;
                                 break;  
    case SET_SOURCE_WAVE_GEN   : Settings.PrevWaveform();
                                 Play();
                                 break;
    default: break;
  }
}

void PlayerClass::PauseResume(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : Settings.Play = !_audio->isRunning();
                                 Serial.printf("PLAY/RESUME %d\n", (int)Settings.Play); 
                                 _audio->pauseResume();
                                 Display.ShowPlayPause(Settings.Play);
                                 if(!Settings.Play) {
                                   if(!Settings.NoCard && Settings.NV.DiskTotalTracks > 0) {
                                     Serial.println("Save card track position");
                                     Settings.NV.DiskTrackResumeTime = _audio->getAudioCurrentTime() + Settings.CurrentTrackTimeOffset;
                                     Settings.NV.DiskTrackResumePos  = _audio->getFilePos();
                                     Settings.NV.DiskTrackTotalTime  = Settings.TotalTrackTime;
                                     Settings.EepromStore();
                                   }
                                 }
                                 break;
    case SET_SOURCE_BLUETOOTH  : if(!Settings.Play) {
                                   _a2dp_sink->play();
                                   Display.ShowPlayPause(1);                                   
                                 }
                                 else {
                                   _a2dp_sink->pause();
                                   Display.ShowPlayPause(0);
                                 }
                                 break;
    case SET_SOURCE_WEB_RADIO  : Settings.Play = !_audio->isRunning();
                                 _audio->pauseResume();
                                 Display.ShowPlayPause(Settings.Play);
                                 break;
    case SET_SOURCE_WAVE_GEN   : if(Settings.Play) {
                                   _waveform_player->Pause();
                                   Serial.println("--- WG pause ---");
                                 }
                                 else {
                                   _waveform_player->Resume();
                                   Serial.println("--- WG resume ---");
                                 }
                                 Settings.Play = !Settings.Play;
                                 Display.ShowPlayPause();
                                 break;
    default: break;
  }
}

#define BLUETOOTH_VOLUME_MIN  24

void PlayerClass::CheckBluetoothVolume(int v) {
  if(_a2dp_sink != NULL)  { 
    if(v < BLUETOOTH_VOLUME_MIN) { // a minimum volume is needed to run properly
      _a2dp_sink->set_volume(BLUETOOTH_VOLUME_MIN);
      Serial.println("Bluetooth zero or low volume detected");
    }
  }
}

//******************************************************//
// optional functions                                   //
//******************************************************//

// optional (weak function override)

void audio_showstation(const char *info)     { Display.ShowStation(info);     }
void audio_showstreamtitle(const char *info) { Display.ShowStreamTitle(info); }
void audio_lasthost(const char *info)        { Display.ShowLastHost(info);    }
void audio_eof_mp3(const char *info)         { Display.ShowTrackEnded(info);  Settings.AudioEnded = 1; }
void audio_eof_stream(const char* info)      { Display.ShowStreamEnded(info); Settings.AudioEnded = 1; }

//#define SHOW_MORE_INFO

#ifdef SHOW_MORE_INFO
void audio_info(const char *info){
  Serial.print("info        "); Serial.println(info);
}

void audio_id3data(const char *info){  //id3 metadata
  Serial.print("id3data     ");Serial.println(info);
}

void audio_id3image(File& file, const size_t pos, const size_t size) { //ID3 metadata image
  Serial.println("icydescription");
}

void audio_icydescription(const char* info) {
  Serial.print("audio_icydescription");Serial.println(info);
}

#endif

void audio_commercial(const char *info){  //duration in sec
  Serial.print("commercial  ");Serial.println(info);
}

void audio_showstreaminfo(const char *info) {
  Serial.print("streaminfo  "); Serial.println(info);
}

void audio_bitrate(const char *info) {
  Serial.print("bitrate     "); Serial.println(info);
}

void audio_icyurl(const char *info){  //homepage
  Serial.print("icyurl      ");Serial.println(info);
}

void audio_eof_speech(const char *info){
  Serial.print("eof_speech  ");Serial.println(info);
}
