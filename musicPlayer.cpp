#include "esp32-hal-timer.h"
#include "musicPlayer.h"
/**
 * Class WAV 
 * */
WAV_Class::WAV_Header Header;

bool  WAV_Class::WAV_Init(uint8_t *buffer_header,uint16_t _size)
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
      IncreaseBy =((float)Header.SampleRate / Frequency_Timer)*2;
      Count = 0;
      LastIntCount = 0;
      Count_Byte=0;
      Count_Frame=0;
      Completed = true;
      HalfTransfer = false;
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
/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */
/* 
 * DAC AUDIO CLASS
 * */
uint32_t cp0_regs[18];

// because of the difficulty in passing parameters to interrupt handlers we have a global
// object of this type that points to the object the user creates.
DAC_Audio_Class *DacAudioClassGlobalObject;       

// interrupt stuff
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// The main interrupt routine called 50,000 times per second
void IRAM_ATTR onTimer() 
{  
  uint16_t IntPartOfCount;

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
  
  /*if(XTDacAudioClassGlobalObject->CurrentWav!=0)
  {
    if(XTDacAudioClassGlobalObject->CurrentWav->Completed==false)
    {      
	  // increase the counter, if it goes to a new integer digit then write to DAC
      XTDacAudioClassGlobalObject->CurrentWav->Count+=XTDacAudioClassGlobalObject->CurrentWav->IncreaseBy;                                 
      IntPartOfCount=floor(XTDacAudioClassGlobalObject->CurrentWav->Count);
      if(IntPartOfCount>XTDacAudioClassGlobalObject->CurrentWav->LastIntCount)
      {
        // gone to a new integer of count, we need to send a new value to the DAC
        XTDacAudioClassGlobalObject->CurrentWav->LastIntCount=IntPartOfCount;           // update the last int count to this new one
        dacWrite(XTDacAudioClassGlobalObject->DacPin,XTDacAudioClassGlobalObject->CurrentWav->Data[XTDacAudioClassGlobalObject->CurrentWav->DataIdx]);             												// write out the data
        XTDacAudioClassGlobalObject->CurrentWav->DataIdx++;
        if(XTDacAudioClassGlobalObject->CurrentWav->DataIdx>=XTDacAudioClassGlobalObject->CurrentWav->DataSize)  // end of data, flag end
          XTDacAudioClassGlobalObject->CurrentWav->Completed=true;  
      }  
    }
  }*/
  if(DacAudioClassGlobalObject->CurrentWav!=0)
  {
    if(DacAudioClassGlobalObject->CurrentWav->Completed==false)
    {
      DacAudioClassGlobalObject->CurrentWav->Count+=DacAudioClassGlobalObject->CurrentWav->IncreaseBy;
      IntPartOfCount=floor(DacAudioClassGlobalObject->CurrentWav->Count);
      if(IntPartOfCount>DacAudioClassGlobalObject->CurrentWav->LastIntCount)
      {
        DacAudioClassGlobalObject->CurrentWav->LastIntCount=IntPartOfCount;
        dacWrite(DacAudioClassGlobalObject->DacPin,DacAudioClassGlobalObject->CurrentWav->Buffer_Main[DacAudioClassGlobalObject->CurrentWav->Count_Byte]);
        if(DacAudioClassGlobalObject->CurrentWav->Count_Byte++>=Size_Buffer_WAV)
        {
          DacAudioClassGlobalObject->CurrentWav->HalfTransfer=true;

          DacAudioClassGlobalObject->CurrentWav->Count_Byte=0;
          DacAudioClassGlobalObject->CurrentWav->Count_Frame+=Size_Buffer_WAV;
          if(DacAudioClassGlobalObject->CurrentWav->Count_Frame>=Header.Subchunk2Size)
          {
            DacAudioClassGlobalObject->CurrentWav->Completed = true;
            Serial.println("Finish");
            Serial.println(DacAudioClassGlobalObject->CurrentWav->Count_Frame);
          }
        }
        if(DacAudioClassGlobalObject->CurrentWav->Count_Byte==1024)
        {
          DacAudioClassGlobalObject->CurrentWav->HalfTransfer=true;
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

DAC_Audio_Class::DAC_Audio_Class(uint8_t DacPin, uint8_t TimerIndex)
{
  // Using a prescaler of 80 gives a counting frequency of 1,000,000 (1MHz) and using
  // and calling the function every 20 counts of the freqency (the 20 below) means
  // that we will call our onTimer function 50,000 times a second
  timer = timerBegin(TimerIndex, 80, true);             // use timer 0, prescaler is 80 (divide by 8000), count up
  timerAttachInterrupt(timer, &onTimer, true);          // P3= edge trggered
  timerAlarmWrite(timer, 20, true);                    // will trigger 8000 times per second, so 8kHz is max freq.
  timerAlarmEnable(timer);                              // enable
  DacAudioClassGlobalObject=this;						            // set variable to use in ISR
  this->DacPin=DacPin;
  dacWrite(DacPin,0x7f);							                	// Set speaker to mid point to stop any clicks during sample playback
}

 void DAC_Audio_Class::DAC_playWav(WAV_Class *Wav)
 {
   Wav->Count=0;
   Wav->LastIntCount = 0;
   Wav->HalfTransfer = false;
   Wav->Completed = false;
   Wav->Count_Byte = 0;
   Wav->Count_Frame = 0;
   CurrentWav = Wav;
 }