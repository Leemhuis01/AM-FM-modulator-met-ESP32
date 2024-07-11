//------------------------------------------------------------------------------------------------------------------------
// Includes

#include <Arduino.h> // Only for Serial.prinf functions
#include <math.h>
#include "SimpleWaveGenerator.h"
#include "src/board.h" // Comment out for stand alone version

#ifndef _BOARD_H_
 #error _BOARD_H_ not defined ()uncomment this line for stand alone version
 #define PIN_I2S_BCK         26
 #define PIN_I2S_WS          25
 #define PIN_I2S_DOUT        27
 #define DAC_ID_PT8211       0
 #define DAC_ID_MAX98357A    1
 #define DAC_ID              DAC_ID_MAX98357A // Choose your DAC type here
#endif

#ifdef USE_INTERNAL_DACS
  #define I2S_MODE         i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN)
  #define I2S_COMM_FORMAT  i2s_comm_format_t(I2S_COMM_FORMAT_STAND_MSB)
  #define DAC_ADJUST       (int16_t)INT16_MIN 
  #error
#else
  #define I2S_MODE         i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX)
  #if DAC_ID == DAC_ID_MAX98357A
    #define I2S_COMM_FORMAT  i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S)    // Standard Philips format 
    #define DAC_ADJUST       (int16_t)0   
  #else
    //#define I2S_COMM_FORMAT  i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S)    // Standard Philips format 
    #define I2S_COMM_FORMAT  i2s_comm_format_t(I2S_COMM_FORMAT_STAND_MSB)    // PT8211 format
    //#define DAC_ADJUST       (int16_t)INT16_MIN 
    #define DAC_ADJUST       (int16_t)0 
  #endif
#endif

// MAX98357A: LRCLK ONLY supports 8kHz, 16kHz, 32kHz, 44.1kHz, 48kHz, 88.2kHz and 96kHz frequencies. 
#if DAC_ID != DAC_ID_MAX98357A 
  #define SAMPLE_RATE_440HZ          96000
  #define SAMPLE_FREQ_440HZ          440
  #define NUMBER_OF_SAMPLES_440HZ    (SAMPLE_RATE_440HZ / SAMPLE_FREQ_440HZ) 

  #define SAMPLE_RATE_1KHZ           96000
  #define SAMPLE_FREQ_1KHZ           1000
  #define NUMBER_OF_SAMPLES_1KHZ     (SAMPLE_RATE_1KHZ / SAMPLE_FREQ_1KHZ) 

  #define SAMPLE_RATE_DAC_TEST       96000
#else // PT8211 can use any frequency
  #define SAMPLE_RATE_440HZ          44000
  #define SAMPLE_FREQ_440HZ          440
  #define NUMBER_OF_SAMPLES_440HZ    (SAMPLE_RATE_440HZ / SAMPLE_FREQ_440HZ) 

  #define SAMPLE_RATE_1KHZ           64000
  #define SAMPLE_FREQ_1KHZ           1000
  #define NUMBER_OF_SAMPLES_1KHZ     (SAMPLE_RATE_1KHZ / SAMPLE_FREQ_1KHZ) 

  #define SAMPLE_RATE_DAC_TEST       0x10000  // for 1 Hz signal
#endif

#if NUMBER_OF_SAMPLES_440HZ > NUMBER_OF_SAMPLES_1KHZ
  #define NUMBER_OF_SAMPLES_MAX     NUMBER_OF_SAMPLES_440HZ
#else
  #define NUMBER_OF_SAMPLES_MAX     NUMBER_OF_SAMPLES_1KHZ
#endif

#define NUMBER_OF_SAMPLES_DAC_TEST NUMBER_OF_SAMPLES_MAX

//====================================== local constants ==============================================

// I2S configuration structures (find in: C:\Users\[user]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.3\tools\sdk\esp32\include\driver\include\driver
static const i2s_config_t i2s_config = {
    .mode                 = I2S_MODE,
    .sample_rate          = 44100,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,  // the internal DAC module will only take the 8bits from MSB
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT, 
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,       // high interrupt priority
    .dma_buf_count        = 8,                          // 8 buffers
    .dma_buf_len          = 1024,                       // 1K per buffer, so 8K of buffer space
    .use_apll             = 0,
    .tx_desc_auto_clear   = true, 
    .fixed_mclk           = -1    
};
 
// These are the physical wiring connections to our I2S decoder board/chip from the esp32, there are other connections
// required for the chips mentioned at the top (but not to the ESP32), please visit the page mentioned at the top for
// further information regarding these other connections.

#ifdef USE_INTERNAL_DACS
  #define I2S_PIN_CONFIG  NULL
#else
  static const i2s_pin_config_t pin_config = {
    .bck_io_num   = PIN_I2S_BCK,         // The bit clock connectiom, goes to the ESP32
    .ws_io_num    = PIN_I2S_WS,          // Word select, also known as word select or left right clock
    .data_out_num = PIN_I2S_DOUT,        // Data out from the ESP32, connect to DIN on 38357A
    .data_in_num  = I2S_PIN_NO_CHANGE    // we are not interested in I2S data into the ESP32
  };
  #define I2S_PIN_CONFIG  &pin_config
#endif

//====================================== local variables ==============================================

static SimpleWaveGeneratorClass::samples_t samples[NUMBER_OF_SAMPLES_MAX];

//====================================== local functions ===============================================

void SimpleWaveGeneratorClass::CreateSineWave(samples_t * dest, int samples) {
  float scale = (float)INT16_MAX * _volume / WAVEFORM_VOLUME_MAX;
  float step = M_TWOPI / samples;
  for(int alpha = 0; alpha < samples; alpha++){
    dest[alpha].left = dest[alpha].right = int(sin(step*alpha) * scale) + DAC_ADJUST;
  }
  _current_number_of_samples = samples;
} 

void SimpleWaveGeneratorClass::CreateTriangleWave(samples_t * dest, int samples) {
  float scale = (float)_volume / WAVEFORM_VOLUME_MAX;
  float step  = scale * (float)UINT16_MAX / (samples / 2);
  float val   = scale * (float)INT16_MIN;

  for(int i = 0; i < samples; i++) {
    dest[i].left = dest[i].right = (int16_t)val + DAC_ADJUST;
    val += i < samples / 2 ? step : -step;
  }
  _current_number_of_samples = samples;
}

void SimpleWaveGeneratorClass::CreateDacTestWave(samples_t * dest, int samples) {
  static uint16_t val = INT16_MIN;
  float scale = (float)_volume / WAVEFORM_VOLUME_MAX;
 
  for(int i = 0; i < samples; i++) {
    dest[i].left = dest[i].right = uint16_t(scale * val) + DAC_ADJUST;
    val += 64;
  }
  _current_number_of_samples = samples;
}

void SimpleWaveGeneratorClass::CreateDacTestMin(samples_t * dest, int samples) {
  for(int i = 0; i < samples; i++) {
    dest[i].left = dest[i].right = (uint16_t)(INT16_MIN + DAC_ADJUST);
  }
  _current_number_of_samples = samples;
}

void SimpleWaveGeneratorClass::CreateDacTestMax(samples_t * dest, int samples) {
  for(int i = 0; i < samples; i++) {
    dest[i].left = dest[i].right = (uint16_t)(INT16_MAX + DAC_ADJUST);
  }
  _current_number_of_samples = samples;
}

//====================================== public functions ===============================================

SimpleWaveGeneratorClass::SimpleWaveGeneratorClass() {
  _current_wave_form = WAVEFORM_NONE;
  _volume   = WAVEFORM_VOLUME_MAX;
  _i2s_port = i2s_port_t(I2S_NUM_0);
  _pause    = true;
  _samples  = NULL;
  _i2s_installed = false;
}

SimpleWaveGeneratorClass::~SimpleWaveGeneratorClass() {
  _pause = true;
  if(_i2s_installed) {
    i2s_stop(_i2s_port);
    delay(100);
    i2s_driver_uninstall(_i2s_port);
  }
  if(_samples != NULL) free(_samples);
  Serial.println("~SimpleWaveGeneratorClass");
}

void SimpleWaveGeneratorClass::Init(i2s_port_t port) {
  _i2s_port = port;
  if(_samples != NULL) {
    _samples = (samples_t *)calloc(NUMBER_OF_SAMPLES_MAX, sizeof(samples_t));
  }
  if(!_i2s_installed) {
    i2s_driver_install(_i2s_port, &i2s_config, 0, NULL); // ESP32 will allocated resources to run I2S
    i2s_set_pin(_i2s_port, I2S_PIN_CONFIG);              // Tell it the pins you will be using 
    _i2s_installed = true;
  }
  Serial.println("SimpleWaveGeneratorClass::Init(i2s_port_t port)");
  _pause = true;
}

void SimpleWaveGeneratorClass::Volume(int vol, bool start) { 
  Serial.printf("SimpleWaveGeneratorClass::Volume %d/%d\n", vol, WAVEFORM_VOLUME_MAX);
  if(vol < 0) vol = 0;
  if(vol > WAVEFORM_VOLUME_MAX) vol = WAVEFORM_VOLUME_MAX;
  _volume = WAVEFORM_VOLUME_MAX;//vol;
  Serial.printf("SimpleWaveGeneratorClass::Volume %d/%d\n", _volume, WAVEFORM_VOLUME_MAX);
  if(start) Start(_current_wave_form); // Recalculate waveform...
}

void SimpleWaveGeneratorClass::Start(int waveform) {
  bool cur_pause = _pause;
  _pause = false;
  switch(waveform) {
    case WAVEFORM_SINE_440HZ: CreateSineWave(samples, _current_number_of_samples = NUMBER_OF_SAMPLES_440HZ); 
                              _current_sample_rate = SAMPLE_RATE_440HZ;
                               break;
    case WAVEFORM_SINE_1KHZ : CreateSineWave(samples, _current_number_of_samples = NUMBER_OF_SAMPLES_1KHZ); 
                              _current_sample_rate = SAMPLE_RATE_1KHZ;
                              break;
    case WAVEFORM_TRI_440HZ : CreateTriangleWave(samples, _current_number_of_samples = NUMBER_OF_SAMPLES_440HZ);
                              _current_sample_rate = SAMPLE_RATE_440HZ;
                              break;
    case WAVEFORM_TRI_1KHZ  : CreateTriangleWave(samples, _current_number_of_samples = NUMBER_OF_SAMPLES_1KHZ);
                              _current_sample_rate = SAMPLE_RATE_1KHZ;
                              break;
    case WAVEFORM_DAC_TEST  : CreateDacTestWave(samples, _current_number_of_samples = NUMBER_OF_SAMPLES_MAX);
                              _current_sample_rate = SAMPLE_RATE_DAC_TEST;
                              break;
    case WAVEFORM_DAC_MIN   : CreateDacTestMin(samples, _current_number_of_samples = NUMBER_OF_SAMPLES_MAX);
                              _current_sample_rate = SAMPLE_RATE_DAC_TEST;
                              break;
    case WAVEFORM_DAC_MAX   : CreateDacTestMax(samples, _current_number_of_samples = NUMBER_OF_SAMPLES_MAX);
                              _current_sample_rate = SAMPLE_RATE_DAC_TEST;
                              break;
    default                 : _current_wave_form = WAVEFORM_NONE; // Not a valid waveform, so exit without install.
                              return; 
  }
  i2s_set_sample_rates(_i2s_port, _current_sample_rate);
  _current_wave_form = waveform;
  _pause = cur_pause;
}

void SimpleWaveGeneratorClass::Pause(void)   { _pause = true; }
void SimpleWaveGeneratorClass::Resume(void)  { _pause = false; }

void SimpleWaveGeneratorClass::loop(void) {
  if(_current_wave_form >= 0 && !_pause) { 
    size_t BytesWritten; 
    i2s_write(_i2s_port, samples, _current_number_of_samples * sizeof(samples_t), &BytesWritten, portMAX_DELAY );
    if(_current_wave_form == WAVEFORM_DAC_TEST)
      CreateDacTestWave(samples, _current_number_of_samples); // update the buffer with new values
  }
}

#ifndef NO_WAVE_INFO

const char *SimpleWaveGeneratorClass::GetWaveformName(int id) {
  const char * WaveNames[8] = { "Sine 440Hz", "Sine 1000Hz", "Triangle 440Hz", "Triangle 1000Hz",
                                "DAC Test",   "DAC Minimum", "DAC Maximum",    "Invalid Waveform" }; 
  if(id < WAVEFORM_SINE_440HZ || id > WAVEFORM_COUNT)
    return WaveNames[WAVEFORM_COUNT];
  return WaveNames[id];
}

#endif

void SimpleWaveGeneratorClass::Print(bool hex) {
  Serial.printf("Waveform=%s, Buffer Valid=%d, Volume=%d, Rate=%d, Samples=%d, Pause=%d\n",GetWaveformName(), _samples != NULL, _volume, _current_sample_rate, _current_number_of_samples, _pause);
  for(int i = 0; i < _current_number_of_samples; i++) {
    if(hex)
        Serial.printf("%04X ", ((uint32_t)samples[i].left) & 0xFFFF);
    else
      #if DAC_ID == DAC_ID_MAX98357A
        Serial.printf("%6d ", (int)samples[i].left);
      #else
        Serial.printf("%5d ", ((uint32_t)samples[i].left) & 0xFFFF);
      #endif
    if((i & 0x1F) == 0x1F)
      Serial.println();
    loop(); // Keep sound runnig while printing...
  }
}
