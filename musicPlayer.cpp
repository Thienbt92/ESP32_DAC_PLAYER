#include "esp32-hal-timer.h"
#include "musicPlayer.h"
#include "soc/sens_reg.h"

/**
 * Class WAV 
 * */
WAV_Class::WAV_Header Header;
WAV_Class *DacAudioClassGlobalObject;
uint32_t cp0_regs[18];  
volatile uint8_t LastDacValue = 0x7f;
// interrupt stuff
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */
// The main interrupt routine called 50,000 times per second
void IRAM_ATTR onTimer() 
{  
 // get FPU state, we need to do this until the issue of using floats inside an interupt is fixed
  uint32_t cp_state = xthal_get_cpenable();
  
  if(cp_state) 
  {
    // Save FPU registers
    xthal_save_cp0(cp0_regs);
  } 
  else 
  {
    // enable FPU
    xthal_set_cpenable(1);
  }
  if(DacAudioClassGlobalObject!=NULL)     // Chỉ chạy nếu con trỏ không rỗng
  {
    if(Header.Chunk_ID==0x52494646)
    {
      if(DacAudioClassGlobalObject->WAV_Playing==true)  // Đẩy dữ liệu ra DAC khi cờ báo
      {
          LastDacValue = DacAudioClassGlobalObject->Buffer_Main[DacAudioClassGlobalObject->Count_Byte];
          CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);  //Disable Tone
          if(DacAudioClassGlobalObject->DacPin==25) {
              CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
              SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, LastDacValue, RTC_IO_PDAC1_DAC_S);
              SET_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC | RTC_IO_PDAC1_DAC_XPD_FORCE);
          }
          else if(DacAudioClassGlobalObject->DacPin==26) {
              CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);
              SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, LastDacValue, RTC_IO_PDAC2_DAC_S);
              SET_PERI_REG_MASK(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_XPD_DAC | RTC_IO_PDAC2_DAC_XPD_FORCE);
          }
          if(++DacAudioClassGlobalObject->Count_Byte>=Size_Buffer_WAV)
          {
            DacAudioClassGlobalObject->Count_Byte=0;
            DacAudioClassGlobalObject->Count_Frame+=Size_Buffer_WAV;
            if(DacAudioClassGlobalObject->Count_Frame>=Header.Subchunk2Size)
            {
              Serial.println("Finish");
              DacAudioClassGlobalObject->WAV_Playing = false;
              DacAudioClassGlobalObject->Play_Finish=true;
            }
            else
            {
              DacAudioClassGlobalObject->HalfTransfer=true;
              DacAudioClassGlobalObject->Pointer_Update = 1024;
            }
          }
          if(DacAudioClassGlobalObject->Count_Byte==1024)
          {
            DacAudioClassGlobalObject->HalfTransfer=true;
            DacAudioClassGlobalObject->Pointer_Update = 0;
          }
      } 
    }
  }
  // return fpu to previous state
   if(cp_state) 
   {
    // Restore FPU registers
    xthal_restore_cp0(cp0_regs);
  } 
  else 
  {
    // turn it back off
    xthal_set_cpenable(0);
  }
}
/*xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx*/
bool  WAV_Class::WAV_Init(void)
{
  //WAV_Header Header_old = Header;
  // The canonical WAVE format starts with the RIFF header:
  Header.Chunk_ID       = 0;
  Header.chunkSize      = 0;
  Header.Format         = 0;
  // The "fmt " subchunk describes the sound data's format:
  Header.Subchunk1ID    = 0;
  Header.Subchunk1Size  = 0;
  Header.AudioFormat    = 0;
  Header.NumChannels    = 0;
  Header.SampleRate     = 0;
  Header.ByteRate       = 0;
  Header.BlockAlign     = 0;
  Header.BitsPerSample  = 0;
  // The "data" subchunk contains the size of the data and the actual sound:
  Header.Subchunk2ID    = 0;
  Header.Subchunk2Size  = 0;

  Freq_Timer = 62;//(uint32_t)((float)Frequency_Timer /(Header.SampleRate*2));
  Count = 0;
  LastIntCount = 0;
  Count_Byte=0;
  Count_Frame=0;
  WAV_Playing = false;
  HalfTransfer = false;
  Pointer_Update = 0;
  Play_Finish=false;
  return true;
}
bool  WAV_Class::WAV_UpdateHeader(uint8_t *buffer_header,uint16_t _size)
{
  WAV_Header Header_old = Header;
  // The canonical WAVE format starts with the RIFF header:
  Header.Chunk_ID       = (uint32_t)((buffer_header[0]<<24)|(buffer_header[1]<<16)|(buffer_header[2]<<8)|(buffer_header[3]));
  Header.chunkSize      = (uint32_t)((buffer_header[7]<<24)|(buffer_header[6]<<16)|(buffer_header[5]<<8)|(buffer_header[4]));
  Header.Format         = (uint32_t)((buffer_header[8]<<24)|(buffer_header[9]<<16)|(buffer_header[10]<<8)|(buffer_header[11]));
  // The "fmt " subchunk describes the sound data's format:
  Header.Subchunk1ID    = (uint32_t)((buffer_header[12]<<24)|(buffer_header[13]<<16)|(buffer_header[14]<<8)|(buffer_header[15]));
  Header.Subchunk1Size  = (uint32_t)((buffer_header[19]<<24)|(buffer_header[18]<<16)|(buffer_header[17]<<8)|(buffer_header[16]));
  Header.AudioFormat    = (uint16_t)((buffer_header[21]<<8)|(buffer_header[20]));
  Header.NumChannels    = (uint16_t)((buffer_header[23]<<8)|(buffer_header[22]));
  Header.SampleRate     = (uint32_t)((buffer_header[27]<<24)|(buffer_header[26]<<16)|(buffer_header[25]<<8)|(buffer_header[24]));
  Header.ByteRate       = (uint32_t)((buffer_header[31]<<24)|(buffer_header[30]<<16)|(buffer_header[29]<<8)|(buffer_header[28]));
  Header.BlockAlign     = (uint16_t)((buffer_header[33]<<8)|(buffer_header[32]));
  Header.BitsPerSample  = (uint16_t)((buffer_header[35]<<8)|(buffer_header[34]));
  // The "data" subchunk contains the size of the data and the actual sound:
  Header.Subchunk2ID    = (uint32_t)((buffer_header[36]<<24)|(buffer_header[37]<<16)|(buffer_header[38]<<8)|(buffer_header[39]));
  Header.Subchunk2Size  = (uint32_t)((buffer_header[43]<<24)|(buffer_header[42]<<16)|(buffer_header[41]<<8)|(buffer_header[40]));
  if(Header.Chunk_ID==0x52494646)
  {
    if(Header.Subchunk2Size<=Header.chunkSize)
    {
      //Freq_Timer =(uint32_t)((float)Frequency_Timer /(Header.SampleRate*2));
      Count = 0;
      LastIntCount = 0;
      Count_Byte=0;
      Count_Frame=0;
      WAV_Playing = false;
      HalfTransfer = false;
      Pointer_Update = 0;
      Play_Finish=false;
      return true;
    }
  }
  else
  {
    Header = Header_old;
    return false;
  }
  return false;  
}
/**
 * Hàm trả về kích thước số byte Header của file Wav
 * */
uint8_t WAV_Class::WAV_getSizeHeader(void)
{
  return Size_Header_WAV;
}
/**
 * Hàm Cập Nhật Dữ Liệu Mới Vào Bộ Đệm.
 * */
void WAV_Class::WAV_UpdateBuffer(uint8_t* _buffer,uint16_t _start,uint16_t _leng)
{
  for(int i=0;i<_leng;i++)
    Buffer_Main[i+_start] = _buffer[i];
}
/**
 * 
 * */
bool WAV_Class::WAV_GetflagUpdate()
{
  return HalfTransfer;
}
void WAV_Class::WAV_ClearflagUpdate(bool _state)
{
  HalfTransfer = false;
}
uint32_t WAV_Class::WAV_getSizeData(void)
{
  return Header.Subchunk2Size;
}

void WAV_Class::DAC_Audio_Init(uint8_t DacPin, uint8_t TimerIndex,WAV_Class *_class)
{
  // Using a prescaler of 80 gives a counting frequency of 1,000,000 (1MHz) and using
  // and calling the function every 20 counts of the freqency (the 20 below) means
  // that we will call our onTimer function 50,000 times a second
  timer = timerBegin(TimerIndex, 80, true);             // use timer 0, prescaler is 80 (divide by 8000), count up
  timerAttachInterrupt(timer, &onTimer, true);          // P3= edge trggered
  timerAlarmWrite(timer, Freq_Timer, true);                    // will trigger 8000 times per second, so 8kHz is max freq.
  timerAlarmEnable(timer);                              // enable
  _class->DacPin= DacPin;
  dacWrite(DacPin,0x7f);							                	// Set speaker to mid point to stop any clicks during sample playback
}

 void WAV_Class::DAC_playWav(WAV_Class *Wav,bool _repeat)
 {
   Wav->Count=0;
   Wav->LastIntCount = 0;
   Wav->HalfTransfer = false;
   Wav->WAV_Playing = true;
   Wav->Count_Byte = 0;
   Wav->Count_Frame = 0;
   Wav->Pointer_Update = 0;
   Wav->Play_Finish=false;
   Wav->Play_Repeat = _repeat;
   DacAudioClassGlobalObject = Wav;
 }

