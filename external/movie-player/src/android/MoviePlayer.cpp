#include "MoviePlayer.h"
#include "Constants.h"
#include "MoviePlayerCore.h"

// TODO 本当に必要なもののみにする
// #include <stdio.h>
// #include <errno.h>
// #include <inttypes.h>
// #include <sys/time.h>
// #include <unistd.h>

#include "media/NdkMediaCrypto.h"
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaError.h"
#include "media/NdkMediaFormat.h"
#include "media/NdkMediaExtractor.h"
#include "media/NdkMediaDataSource.h"
#include <android/asset_manager.h>

#include <android/log.h>
#define TAG       "MoviePlayer"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ASSERT(_cond, ...) \
  if (!(_cond))            \
  __android_log_assert("conditional", TAG, __VA_ARGS__)

static inline PixelFormat
conv_color_format(IMoviePlayer::ColorFormat colorFormat)
{
  PixelFormat pixelFormat = PIXEL_FORMAT_UNKNOWN;

  switch (colorFormat) {
  case IMoviePlayer::COLOR_UNKNOWN:
    pixelFormat = PIXEL_FORMAT_UNKNOWN;
    break;
  case IMoviePlayer::COLOR_ABGR:
    pixelFormat = PIXEL_FORMAT_ABGR;
    break;
  case IMoviePlayer::COLOR_ARGB:
    pixelFormat = PIXEL_FORMAT_ARGB;
    break;
  case IMoviePlayer::COLOR_RGBA:
    pixelFormat = PIXEL_FORMAT_RGBA;
    break;
  case IMoviePlayer::COLOR_BGRA:
    pixelFormat = PIXEL_FORMAT_BGRA;
    break;
  case IMoviePlayer::COLOR_I420:
    pixelFormat = PIXEL_FORMAT_I420;
    break;
  case IMoviePlayer::COLOR_NV12:
    pixelFormat = PIXEL_FORMAT_NV12;
    break;
  case IMoviePlayer::COLOR_NV21:
    pixelFormat = PIXEL_FORMAT_NV21;
    break;
  default:
    ASSERT(false, "unknown color format: %d\n", colorFormat);
    break;
  }

  return pixelFormat;
}

static inline IMoviePlayer::ColorFormat
conv_pixel_format(PixelFormat pixelFormat)
{
  IMoviePlayer::ColorFormat colorFormat = IMoviePlayer::COLOR_UNKNOWN;

  switch (pixelFormat) {
  case PIXEL_FORMAT_UNKNOWN:
    colorFormat = IMoviePlayer::COLOR_UNKNOWN;
    break;
  case PIXEL_FORMAT_ABGR:
    colorFormat = IMoviePlayer::COLOR_ABGR;
    break;
  case PIXEL_FORMAT_ARGB:
    colorFormat = IMoviePlayer::COLOR_ARGB;
    break;
  case PIXEL_FORMAT_RGBA:
    colorFormat = IMoviePlayer::COLOR_RGBA;
    break;
  case PIXEL_FORMAT_BGRA:
    colorFormat = IMoviePlayer::COLOR_BGRA;
    break;
  case PIXEL_FORMAT_I420:
    colorFormat = IMoviePlayer::COLOR_I420;
    break;
  case PIXEL_FORMAT_NV12:
    colorFormat = IMoviePlayer::COLOR_NV12;
    break;
  case PIXEL_FORMAT_NV21:
    colorFormat = IMoviePlayer::COLOR_NV21;
    break;
  default:
    ASSERT(false, "unknown internal pixel format: %d\n", colorFormat);
    break;
  }

  return colorFormat;
}

// -----------------------------------------------------------------------------
// MoviePlayer
// -----------------------------------------------------------------------------
MoviePlayer::MoviePlayer(InitParam &param)
: mPlayer(nullptr)
, mInitParam(param)
, mAsset(nullptr)
{
  Init();
}

MoviePlayer::~MoviePlayer()
{
  Done();
}

void
MoviePlayer::Init()
{}

void
MoviePlayer::Done()
{
  if (mPlayer != nullptr) {
    mPlayer->Stop();
    delete mPlayer;
    mPlayer = nullptr;
  }

  if (mAsset) {
    AAsset_close(mAsset);
    mAsset = nullptr;
  }
}

bool
MoviePlayer::Open(const char *filepath)
{
  mPlayer = new MoviePlayerCore(mInitParam.useOwnAudioEngine);
  mPlayer->SetPixelFormat(conv_color_format(mInitParam.videoColorFormat));
  return mPlayer->Open(filepath);
}

bool 
MoviePlayer::Open(IMovieReadStream *stream)
{
  mPlayer = new MoviePlayerCore(mInitParam.useOwnAudioEngine);
  mPlayer->SetPixelFormat(conv_color_format(mInitParam.videoColorFormat));
  return mPlayer->Open(stream);
}

bool
MoviePlayer::Open(int fd, off_t offset, off_t length)
{
  mPlayer = new MoviePlayerCore(mInitParam.useOwnAudioEngine);
  mPlayer->SetPixelFormat(conv_color_format(mInitParam.videoColorFormat));
  return mPlayer->Open(fd, offset, length);
}

bool
MoviePlayer::Open(AAssetManager *manager, const char *filepath)
{
  if ((mAsset = AAssetManager_open(manager, filepath, AASSET_MODE_RANDOM))) {
    int fd;
    off_t offset;
    off_t length;
    if ((fd = AAsset_openFileDescriptor(mAsset, &offset, &length)) >= 0) {
      return Open(fd, offset, length);
    }
  }
  return false;
}

void
MoviePlayer::Play(bool loop)
{
  LOGV("play");

  if (mPlayer) {
    return mPlayer->Play(loop);
  }
}

void
MoviePlayer::Stop()
{
  LOGV("stop");

  if (mPlayer) {
    return mPlayer->Stop();
  }
}

void
MoviePlayer::Pause()
{
  LOGV("pause");

  if (mPlayer) {
    return mPlayer->Pause();
  }
}

void
MoviePlayer::Resume()
{
  LOGV("resume");

  if (mPlayer) {
    return mPlayer->Resume();
  }
}

void
MoviePlayer::Seek(int64_t posUs)
{
  LOGV("seek posUs=%ld", posUs);

  if (mPlayer) {
    return mPlayer->Seek(posUs);
  }
}

void
MoviePlayer::SetLoop(bool loop)
{
  LOGV("set loop=%d", loop);

  if (mPlayer) {
    return mPlayer->SetLoop(loop);
  }
}

bool
MoviePlayer::IsVideoAvailable() const
{
  if (mPlayer) {
    return mPlayer->IsVideoAvailable();
  } else {
    return false;
  }
}

void
MoviePlayer::GetVideoFormat(VideoFormat *format) const
{
  if (IsVideoAvailable() && format != nullptr) {
    format->width       = mPlayer->Width();
    format->height      = mPlayer->Height();
    //未実装
    //format->frameRate   = mPlayer->FrameRate();
    //format->colorFormat = conv_pixel_format(mPlayer->OutputPixelFormat());
  }
}

void
MoviePlayer::SetColorFormat(ColorFormat format)
{
  LOGV("set ColorFormat=%d", format);
  if (mPlayer) {
    mPlayer->SetPixelFormat(conv_color_format(mInitParam.videoColorFormat));
  }
}

int32_t
MoviePlayer::Width() const
{
  if (mPlayer) {
    return mPlayer->Width();
  } else {
    return -1;
  }
}

int32_t
MoviePlayer::Height() const
{
  if (mPlayer) {
    return mPlayer->Height();
  } else {
    return -1;
  }
}

bool
MoviePlayer::IsAudioAvailable() const
{
  if (mPlayer) {
    return mPlayer->IsAudioAvailable();
  } else {
    return false;
  }
}

void
MoviePlayer::GetAudioFormat(AudioFormat *format) const
{
  if (IsAudioAvailable() && format != nullptr) {
    format->sampleRate     = mPlayer->SampleRate();
    format->channels       = mPlayer->Channels();
    format->bitsPerSample  = mPlayer->BitsPerSample();
    int32_t nativeEncoding = mPlayer->Encoding();
    switch (nativeEncoding) {
    case kAudioEncodingPcm16bit:
      format->encoding = PCM_S16;
      break;
    case kAudioEncodingPcm8bit:
      format->encoding = PCM_U8;
      break;
    case kAudioEncodingPcmFloat:
      format->encoding = PCM_F32;
      break;
    case kAudioEncodingPcm32bit:
      format->encoding = PCM_S32;
      break;
    default:
      format->encoding = PCM_UNKNOWN;
      ASSERT(false, "unsupported audio encoding: %d", nativeEncoding);
      break;
    }
  }
}

void
MoviePlayer::SetVolume(float volume)
{
  if (mPlayer) {
    mPlayer->SetVolume(volume);
  }
}

float
MoviePlayer::Volume() const
{
  float volume = 1.0f;
  if (mPlayer) {
    volume = mPlayer->Volume();
  }
  return volume;
}

int64_t
MoviePlayer::Duration() const
{
  if (mPlayer) {
    return mPlayer->Duration();
  } else {
    return -1;
  }
}

int64_t
MoviePlayer::Position() const
{
  if (mPlayer) {
    return mPlayer->Position();
  } else {
    return -1;
  }
}

bool
MoviePlayer::IsPlaying() const
{
  if (mPlayer) {
    return mPlayer->IsPlaying();
  } else {
    return false;
  }
}

bool
MoviePlayer::Loop() const
{
  if (mPlayer) {
    return mPlayer->Loop();
  } else {
    return false;
  }
}

void
MoviePlayer::SetOnAudioDecoded(OnAudioDecoded func, void *userPtr)
{
  if (mPlayer) {
    mPlayer->SetOnAudioDecoded(func, userPtr);
  }
}

void
MoviePlayer::SetOnVideoDecoded(OnVideoDecoded func)
{
  if (mPlayer) {
    mPlayer->SetOnVideoDecoded(func);
  }
}

IMoviePlayer *
IMoviePlayer::CreateMoviePlayer(const char *filename, InitParam &param)
{
  MoviePlayer *player = new MoviePlayer(param);
  player->SetColorFormat(param.videoColorFormat);
  if (player->Open(filename)) {
    return player;
  }
  delete player;
  return nullptr;
}

IMoviePlayer *
IMoviePlayer::CreateMoviePlayer(IMovieReadStream *stream, InitParam &param)
{
  MoviePlayer *player = new MoviePlayer(param);
  if (player->Open(stream)) {
    return player;
  }
  delete player;
  return nullptr;
}
