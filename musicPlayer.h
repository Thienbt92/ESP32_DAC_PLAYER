#include <Arduino.h>

class WAV_Class
{
private:
    #define   Size_Header_WAV    44
    #define   Size_Buffer_WAV    2048
    #define   Frequency_Timer    1000000
public:
  struct WAV_Header
  {
    // The canonical WAVE format starts with the RIFF header:
    uint32_t          Chunk_ID;             //Contains the letters "RIFF" in ASCII form (0x52494646 big-endian form)
    uint32_t          chunkSize;
    uint32_t          Format;               //Contains the letters "WAVE" (0x57415645 big-endian form)
    // The "fmt " subchunk describes the sound data's format:
    uint32_t          Subchunk1ID;          // Contains the letters "fmt " (0x666d7420 big-endian form)
    uint32_t          Subchunk1Size;
    uint16_t          AudioFormat;          // PCM = 1 (i.e. Linear quantization) Values other than 1 indicate some form of compression.
    uint16_t          NumChannels;          // Mono = 1, Stereo = 2, etc.
    uint32_t          SampleRate;           // 8000, 44100, etc.
    uint32_t          ByteRate;             // == SampleRate * NumChannels * BitsPerSample/8
    uint16_t          BlockAlign;           // == NumChannels * BitsPerSample/8. The number of bytes for one sample including all channels. I wonder what happens when this number isn't an integer?
    uint16_t          BitsPerSample;        // 8 bits = 8, 16 bits = 16, etc.
    // The "data" subchunk contains the size of the data and the actual sound:
    uint32_t          Subchunk2ID;          // Contains the letters "data" (0x64617461 big-endian form).
    uint32_t          Subchunk2Size;        // == NumSamples * NumChannels * BitsPerSample/8. This is the number of bytes in the data. You can also think of this as the size of the read of the subchunk following this number.
  };

  volatile uint8_t DacPin;
  uint8_t Buffer_Main[Size_Buffer_WAV];        // 
  uint32_t Freq_Timer=0;                       // The amount to increase the counter by per call to "onTimer"
  float Count=0;                               // The counter counting up, we check this to see if we need to send
  uint16_t LastIntCount=0;                     // The last integer part of count
  uint32_t Count_Byte=0;
  uint32_t Count_Frame=0;
  uint16_t Pointer_Update=0;

  bool WAV_Playing=false;
  bool HalfTransfer=false;
  bool Play_Finish=false;
  bool Play_Repeat=false;
  /******************** Funtion ****************************/
  bool  WAV_Init(void);
  bool  WAV_UpdateHeader(uint8_t *buffer_header,uint16_t _size);
  uint8_t WAV_getSizeHeader(void);
  void WAV_UpdateBuffer(uint8_t* _buffer,uint16_t _start,uint16_t _leng);
  bool WAV_GetflagUpdate();
  void WAV_ClearflagUpdate(bool _state);
  uint32_t WAV_getSizeData(void);
  uint16_t WAV_getPointerUpdate();
  void WAV_setPointerUpdate(uint16_t _value);
  void DAC_Audio_Init(uint8_t DacPin, uint8_t TimerIndex,WAV_Class *_class);
  void DAC_playWav(WAV_Class *Wav,bool _repeat = false);
};


