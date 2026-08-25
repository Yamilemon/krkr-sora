#define MYLOG_TAG "AudioEngine"
#include "BasicLog.h"
#include "AudioEngine.h"

#ifdef EXTERNAL_MINIAUDIO

#define MA_NO_RESOURCE_MANAGER
#define MA_USE_STDINT
#include "miniaudio.h"
extern ma_engine *GetMiniAudioEngine();

static void DoneMiniAudio() {
  // nothing to do
}

#else

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_RESOURCE_MANAGER
#define MA_USE_STDINT
#include "miniaudio.h"

// -----------------------------------------------------------------------------
// miniaudio logger
// -----------------------------------------------------------------------------
static void
my_ma_logger(void *pUserData, ma_uint32 level, const char *pMessage)
{
  if (level >= MA_LOG_LEVEL_WARNING) {
    LOGE("miniaudio: %s\n", pMessage);
  } else {
    LOGV("miniaudio: %s\n", pMessage);
  }
}

static ma_engine *gEngine = NULL;

static void InitMiniAudio() 
{
  if (!gEngine) {
    gEngine = (ma_engine *)malloc(sizeof(ma_engine));
    if (gEngine) {
      ma_log_callback_init(my_ma_logger, NULL);
      // エンジン初期化
      ma_result result = ma_engine_init(NULL, gEngine);
      if (result != MA_SUCCESS) {
        LOGE("failed to initialize miniaudio engine: err=%d\n", result);
      }
    }
  }
}

ma_engine *GetMiniAudioEngine() 
{
  if (!gEngine) {
    InitMiniAudio();
  }
  return gEngine;
}

static void DoneMiniAudio() {
  if (gEngine) {
    ma_engine_uninit(gEngine);
    free(gEngine);
    gEngine = NULL;
  }
}

#endif // EXTERNAL_MINIAUDIO

class AudioEngineMiniAudio;

struct my_data_source
{
  ma_data_source_base base;
  ma_format format;
  ma_uint32 channels;
  ma_uint32 sampleRate;
  AudioEngineMiniAudio *engine;
};

class AudioEngineMiniAudio : public AudioEngine
{
public:
  AudioEngineMiniAudio();
  virtual ~AudioEngineMiniAudio();

  virtual bool Init(AudioCallback callback, void* userData, AudioFormat format, int32_t channels,
            int32_t sampleRate);
  virtual void Done();

  virtual void Start();
  virtual void Stop();

  virtual void SetVolume(float vol);
  virtual float Volume() const;

  ma_result ReadData(void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead);

private:
  float mVolume;
  ma_sound mSound;
  my_data_source mSource;
  int32_t mFrameSize;
  bool mInited;

  AudioCallback mAudioCallback;
  void* mUserData;
};


// -----------------------------------------------------------------------------
// miniaudio data source vtable
// -----------------------------------------------------------------------------
static ma_result
my_data_source_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount,
                    ma_uint64 *pFramesRead)
{
  my_data_source *self = (my_data_source *)pDataSource;
  if (self) {
    AudioEngineMiniAudio *engine = self->engine;
    if (engine) {
      return engine->ReadData(pFramesOut, frameCount, pFramesRead);
    }
  }
  // Read data here. Output in the same format returned by
  // my_data_source_get_data_format().
  return MA_AT_END;
}

static ma_result
my_data_source_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
  return MA_NOT_IMPLEMENTED;
}

static ma_result
my_data_source_get_data_format(ma_data_source *pDataSource, ma_format *pFormat,
                               ma_uint32 *pChannels, ma_uint32 *pSampleRate,
                               ma_channel *pChannelMap, size_t channelMapCap)
{
  // Return the format of the data here.
  my_data_source *self = (my_data_source *)pDataSource;
  if (pFormat) {
    *pFormat = self->format;
  }
  if (pChannels) {
    *pChannels = self->channels;
  }
  if (pSampleRate) {
    *pSampleRate = self->sampleRate;
  }
  return MA_SUCCESS;
}

static ma_result
my_data_source_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
  if (pCursor) {
    *pCursor = 0;
  }
  return MA_NOT_IMPLEMENTED;
}

static ma_result
my_data_source_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
  if (pLength) {
    *pLength = 0;
  }
  return MA_NOT_IMPLEMENTED;
};

static ma_data_source_vtable s_my_data_source_vtable = {
  my_data_source_read,
  my_data_source_seek,
  my_data_source_get_data_format,
  my_data_source_get_cursor,
  my_data_source_get_length,
  NULL,
  MA_DATA_SOURCE_SELF_MANAGED_RANGE_AND_LOOP_POINT
};

// -----------------------------------------------------------------------------
// AudioEngine
// -----------------------------------------------------------------------------
AudioEngineMiniAudio::AudioEngineMiniAudio()
: mAudioCallback(nullptr), mUserData(nullptr), mInited(false)
{
  // 先行して初期化しておく
  (void)GetMiniAudioEngine();
}

AudioEngineMiniAudio::~AudioEngineMiniAudio() {}

bool
AudioEngineMiniAudio::Init(AudioCallback callback, void* userData, AudioFormat format, int32_t channels,
                  int32_t sampleRate)
{
  if (mInited) {
    Done();
  }

  mAudioCallback = callback;
  mUserData = userData;

  // ソース初期化
  ma_format mf = ma_format_unknown;
  switch (format) {
  case AUDIO_FORMAT_U8:
    mf         = ma_format_u8;
    mFrameSize = channels;
    break;
  case AUDIO_FORMAT_S16:
    mf         = ma_format_s16;
    mFrameSize = channels * 2;
    break;
  case AUDIO_FORMAT_S32:
  case AUDIO_FORMAT_F32:
    mf         = ma_format_f32;
    mFrameSize = channels * 4;
    break;
  default:
    ASSERT(false, "Unknown audio format: format=%d\n", format);
    return false;
  }
  mSource.format     = mf;
  mSource.channels   = channels;
  mSource.sampleRate = sampleRate;
  mSource.engine     = this;

  auto dataSourceConfig   = ma_data_source_config_init();
  dataSourceConfig.vtable = &s_my_data_source_vtable;

  ma_result result = ma_data_source_init(&dataSourceConfig, &mSource);
  if (result != MA_SUCCESS) {
    LOGE("failed to initialize miniaudio data source: err=%d\n", result);
    return false;
  }

  // サウンド初期化
  result = ma_sound_init_from_data_source(GetMiniAudioEngine(), &mSource, 0, NULL, &mSound);
  if (result != MA_SUCCESS) {
    LOGE("failed to initialize miniaudio sound engine: err=%d\n", result);
    return false;
  }

  // 初期ボリューム取得
  mVolume = 1.0;
  ma_sound_set_volume(&mSound, mVolume);
  LOGV("initial sound volume: %f\n", mVolume);

  // LOGV("miniaudio engine initialized!\n");

  mInited = true;
  return true;
}

void
AudioEngineMiniAudio::Done()
{
  if (mInited) {
    Stop();
    ma_sound_uninit(&mSound);
    ma_data_source_uninit(&mSource);
    mInited = false;
  }
}

void
AudioEngineMiniAudio::Start()
{
  // MEMO
  // 多重startはma_sound_start()内部でケアされている
  // engine的にチェックが必要な場合は、自前でフラグを管理するか
  // ma_sound_is_playing()を使って内部状態を確認するかで対応可能。
  if (!mInited) {
    LOGE("AudioEngine is not initialized.\n");
    return;
  }
  ma_result result = ma_sound_start(&mSound);
  if (result != MA_SUCCESS) {
    LOGE("failed to start sound: err=%d\n", result);
  }
}

void
AudioEngineMiniAudio::Stop()
{
  if (!mInited) {
    LOGE("AudioEngine is not initialized.\n");
    return;
  }
  ma_result result = ma_sound_stop(&mSound);
  if (result != MA_SUCCESS) {
    LOGE("failed to stop sound: err=%d\n", result);
  }
}

void
AudioEngineMiniAudio::SetVolume(float vol)
{
  if (vol < 0.0f) {
    vol = 0.0f;
  }
  if (vol > 1.0f) {
    vol = 1.0f;
  }
  if (mVolume != vol) {
    mVolume = vol;
    if (mInited) {
      ma_sound_set_volume(&mSound, vol);
    }
  }
}

float
AudioEngineMiniAudio::Volume() const
{
  return mVolume;
}

ma_result
AudioEngineMiniAudio::ReadData(void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
  bool updated         = false;
  ma_uint64 framesRead = 0;
  size_t bytesToRead   = frameCount * mFrameSize;

  if (mAudioCallback) {
    updated = mAudioCallback(mUserData, (uint8_t *)pFramesOut, frameCount,
                            (uint64_t *)&framesRead);
  }

#if 1
  // BUSYで止めずにゼロフィルした音声を送る場合
  if (!updated) {
    memset(pFramesOut, 0, bytesToRead);
    framesRead = frameCount;
    updated    = true;
  }
#endif

  if (pFramesRead) {
    *pFramesRead = framesRead;
  }

  return updated ? MA_SUCCESS : MA_BUSY;
}

AudioEngine* CreateAudioEngine()
{
  return new AudioEngineMiniAudio();
}
