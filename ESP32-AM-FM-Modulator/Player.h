#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <BluetoothA2DPSink.h>
#include <Audio.h>
#include "src/board.h"
#include "src/settings.h"
#include "SimpleWaveGenerator.h"

#if 1
class PlayerBaseClass {
  friend void bt_raw_stream_reader(const uint8_t*, uint32_t);
  public:
    PlayerBaseClass();
    virtual ~PlayerBaseClass();
    virtual void Start(bool autoplay = false) = 0;
    virtual void Stop(void) = 0;
    virtual void Play(void) = 0;
    
    virtual void PlayNext(void) = 0 ;
    virtual void PlayPrevious(void) = 0;
    virtual void loop(bool ten_ms_tick = false, bool seconds_tick = false) = 0;
    virtual void PauseResume(void) = 0;
    virtual void SetVolume(int vol) = 0;
    virtual void PrintDebug(bool b = false) = 0;
    
    void AddLoopFunction(void (*f)(void)) { _loop = f; }
  private:
    void (*_loop)(void);
};

class MP3_PlayerClass : public PlayerBaseClass {
  friend void bt_raw_stream_reader(const uint8_t*, uint32_t);
  public:
    MP3_PlayerClass();
    ~MP3_PlayerClass();
    void Start(bool autoplay = false);
    void Stop(void);
    void Play(void);
    
    void PlayNext(void);
    void PlayPrevious(void);
    void loop(bool ten_ms_tick = false, bool seconds_tick = false);
    void PauseResume(void);
  private:
};

class WR_PlayerClass : public PlayerBaseClass {
  friend void bt_raw_stream_reader(const uint8_t*, uint32_t);
  public:
    WR_PlayerClass();
    ~WR_PlayerClass();
    void Start(bool autoplay = false);
    void Stop(void);
    void Play(void);
    
    void PlayNext(void);
    void PlayPrevious(void);
    void loop(bool ten_ms_tick = false, bool seconds_tick = false);
    void PauseResume(void);
  private:
};

class BT_PlayerClass : public PlayerBaseClass {
  friend void bt_raw_stream_reader(const uint8_t*, uint32_t);
  public:
    BT_PlayerClass();
    ~BT_PlayerClass();
    void Start(bool autoplay = false);
    void Stop(void);
    void Play(void);
    
    void PlayNext(void);
    void PlayPrevious(void);
    void loop(bool ten_ms_tick = false, bool seconds_tick = false);
    void PauseResume(void);
  private:
};

class WF_PlayerClass : public PlayerBaseClass {
  friend void bt_raw_stream_reader(const uint8_t*, uint32_t);
  public:
    WF_PlayerClass();
    ~WF_PlayerClass();
    void Start(bool autoplay = false);
    void Stop(void);
    void Play(void);
    
    void PlayNext(void);
    void PlayPrevious(void);
    void loop(bool ten_ms_tick = false, bool seconds_tick = false);
    void PauseResume(void);
  private:
};

#endif

class PlayerClass {
  friend void bt_raw_stream_reader(const uint8_t*, uint32_t);
  public:
    PlayerClass();
    void Start(bool autoplay = false);
    void Stop(void);
    void Play(void);
    
    void PlayNext(void);
    void PlayPrevious(void);
    void loop(bool ten_ms_tick = false, bool seconds_tick = false);
    void PauseResume(void);
    void AddLoopFunction(void (*f)(void)) { _loop = f; }
#ifdef SET_VOLUME_DEFAULT
    void SetVolume(int vol) {
      if(vol > Settings.NV.VolumeSteps) vol = Settings.NV.VolumeSteps;
      if(_audio != NULL)           _audio->setVolume(vol);
      if(_waveform_player != NULL) _waveform_player->Volume(vol,true);
      if(_a2dp_sink != NULL)       _a2dp_sink->set_volume(vol);
      Settings.NV.Volume = vol;
    }
#endif 
    void CheckBluetoothVolume(int v); 
    void SD_PrintDebug(void);
    void WP_PrintDebug(bool hex = false) { if(_waveform_player != NULL) _waveform_player->Print(hex); }

    //void SetupPlayerDisplay(bool no_queue = false);
    //void PositionEntry(void);
    
  protected:
    //void StopPlayer(void);
    //void StartPlayer(void);
    bool PlayTrackFromSD(int n, uint32_t resume_pos = 0, uint32_t resume_time = 0);
 
    void (*_loop)(void);
    Audio * _audio;
    BluetoothA2DPSink * _a2dp_sink;
    SimpleWaveGeneratorClass * _waveform_player;
    uint16_t _activity_counter;
    uint32_t _activity_sum;
    bool     _connected;
};

extern PlayerClass Player;

#endif //_PLAYER_H_
