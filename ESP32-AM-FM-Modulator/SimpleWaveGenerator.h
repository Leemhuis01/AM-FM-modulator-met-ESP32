#ifndef _SIMPLEWAVEGENERATOR_H_
#define _SIMPLEWAVEGENERATOR_H_

#include <driver/i2s.h>  // Library of I2S routines, comes with ESP32 standard install

//#define USE_INTERNAL_DACS
//#define NO_WAVE_INFO

#define WAVEFORM_NONE       -1
#define WAVEFORM_SINE_440HZ  0
#define WAVEFORM_SINE_1KHZ   1
#define WAVEFORM_TRI_440HZ   2
#define WAVEFORM_TRI_1KHZ    3
#define WAVEFORM_DAC_TEST    4
#define WAVEFORM_DAC_MIN     5
#define WAVEFORM_DAC_MAX     6
#define WAVEFORM_COUNT       7

#define WAVEFORM_VOLUME_MAX  127

class SimpleWaveGeneratorClass {
  public:
    SimpleWaveGeneratorClass();
    ~SimpleWaveGeneratorClass();
    void Init(i2s_port_t port = i2s_port_t(I2S_NUM_0));
    void Start(int waveform);
    void Pause(void);
    void Resume(void);
    void loop(void);
    void Volume(int vol, bool start); // 0 .. WAVEFORM_VOLUME_MAX
    #ifndef NO_WAVE_INFO
      const char *GetWaveformName(int id);
      const char *GetWaveformName() {return GetWaveformName(_current_wave_form); }
    #endif
    typedef struct { int16_t left, right; } samples_t;
    void Print(bool hex = false); // for debug purposes
    
  protected:
    void CreateSineWave(samples_t * dest, int samples);
    void CreateTriangleWave(samples_t * dest, int samples);
    void CreateDacTestWave(samples_t * dest, int samples);
    void CreateDacTestMin(samples_t * dest, int samples);
    void CreateDacTestMax(samples_t * dest, int samples);
    
    i2s_port_t _i2s_port;
    int  _volume; 
    int  _current_wave_form;
    int  _current_sample_rate;
    int  _current_number_of_samples;
    bool _pause;
    bool _i2s_installed;
    samples_t * _samples;
};

extern SimpleWaveGeneratorClass WaveGenerator;

#endif // _SIMPLEWAVEGENERATOR_H_
