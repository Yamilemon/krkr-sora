#pragma once

#include "CommonUtils.h"
#include "Constants.h"

// コールバック関数の型定義
typedef bool (*AudioCallback)(void* userData, uint8_t* buffer, uint64_t frameCount, uint64_t* framesRead);
class AudioEngine
{
public:
  AudioEngine(){}
  virtual ~AudioEngine(){}

  virtual bool Init(AudioCallback callback, void* userData, AudioFormat format, int32_t channels,
            int32_t sampleRate) = 0;
  virtual void Done() = 0;

  virtual void Start() = 0;
  virtual void Stop() = 0;

  virtual void SetVolume(float vol) = 0;
  virtual float Volume() const = 0;
};

extern AudioEngine* CreateAudioEngine();
