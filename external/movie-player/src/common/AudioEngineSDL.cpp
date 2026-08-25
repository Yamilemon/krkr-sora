#define MYLOG_TAG "AudioEngine"
#include "BasicLog.h"
#include "AudioEngine.h"
#include <SDL3/SDL.h>

class AudioEngineSDL : public AudioEngine
{
public:
  AudioEngineSDL();
  virtual ~AudioEngineSDL();

  virtual bool Init(AudioCallback callback, void* userData, AudioFormat format, int32_t channels,
            int32_t sampleRate);
  virtual void Done();

  virtual void Start();
  virtual void Stop();

  virtual void SetVolume(float vol);
  virtual float Volume() const;

private:
  SDL_AudioStream *mStream;
  AudioCallback mAudioCallback;
  void* mUserData;
  int mFrameSize;
  float mVolume;
  void ReadData(int additional_amount);
};

// -----------------------------------------------------------------------------
// AudioEngine
// -----------------------------------------------------------------------------
AudioEngineSDL::AudioEngineSDL()
: mStream(nullptr), mAudioCallback(nullptr), mUserData(nullptr), mFrameSize(0), mVolume(1.0f)
{
}

AudioEngineSDL::~AudioEngineSDL() 
{
  Done();
}

bool
AudioEngineSDL::Init(AudioCallback callback, void* userData, AudioFormat format, int32_t channels,
                  int32_t sampleRate)
{
  Done();
	SDL_AudioSpec spec;
  switch (format) {
  case AUDIO_FORMAT_U8:
    spec.format = SDL_AUDIO_U8;
    mFrameSize = channels * sizeof(uint8_t);
    break;
  case AUDIO_FORMAT_S16:
    spec.format = SDL_AUDIO_S16;
    mFrameSize = channels * sizeof(int16_t);
    break;
  case AUDIO_FORMAT_S32:
    spec.format = SDL_AUDIO_S32;
    mFrameSize = channels * sizeof(int32_t);
    break;
  case AUDIO_FORMAT_F32:
    spec.format = SDL_AUDIO_F32;
    mFrameSize = channels * sizeof(float);
    break;
  default:
    ASSERT(false, "Unknown audio format: format=%d\n", format);
    return false;
  }
  spec.channels   = channels;
  spec.freq       = sampleRate;

  mAudioCallback = callback;
  mUserData = userData;

	// オーディオデバイスの初期化
	mStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, [](void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    AudioEngineSDL *engine = static_cast<AudioEngineSDL *>(userdata);
    if (engine) {
      engine->ReadData(additional_amount);
    }
	}, this);
  return true;
}

void
AudioEngineSDL::ReadData(int additional_amount)
{
  if (mAudioCallback && mStream) {
    static std::vector<uint8_t> buffer;
    if (buffer.size() < additional_amount) {
        buffer.resize(additional_amount);
    }
    uint64_t frameCount = additional_amount / mFrameSize;
    uint64_t framesRead = 0;
    if (mAudioCallback(mUserData, (uint8_t *)&buffer[0], frameCount, &framesRead)) {
      SDL_PutAudioStreamData(mStream, &buffer[0], framesRead);
    }    
  }
}

void
AudioEngineSDL::Done()
{
    Stop();
    if (mStream) {
      SDL_DestroyAudioStream(mStream);
      mStream = nullptr;
    } 
}

void
AudioEngineSDL::Start()
{
  // nothing todo
}

void
AudioEngineSDL::Stop()
{
  // nothing doto
}

void
AudioEngineSDL::SetVolume(float vol)
{
  if (vol < 0.0f) {
    vol = 0.0f;
  }
  if (vol > 1.0f) {
    vol = 1.0f;
  }
  if (mVolume != vol) {
    mVolume = vol;
    if (mStream) {
      SDL_SetAudioStreamGain(mStream, vol);
    }
  }
}

float
AudioEngineSDL::Volume() const
{
  return mVolume;
}

AudioEngine* CreateAudioEngine()
{
  return new AudioEngineSDL();
}
