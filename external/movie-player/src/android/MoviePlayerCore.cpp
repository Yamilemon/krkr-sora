#define MYLOG_TAG "MoviePlayerCore"
#include "BasicLog.h"

#include "Constants.h"
#include "CommonUtils.h"
#include "MediaClock.h"
#include "VideoTrackPlayer.h"
#include "AudioTrackPlayer.h"
#include "MoviePlayerCore.h"

#include "AudioEngine.h"
#include "IMoviePlayer.h"

#include <unistd.h>

#include "media/NdkMediaCrypto.h"
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaError.h"
#include "media/NdkMediaFormat.h"
#include "media/NdkMediaExtractor.h"
#include "media/NdkMediaDataSource.h"

// -----------------------------------------------------------------------------
// MoviePlayerCore
// -----------------------------------------------------------------------------
MoviePlayerCore::MoviePlayerCore(bool useAudioEngine)
: mVideoTrackPlayer(nullptr)
, mAudioTrackPlayer(nullptr)
, mFd(-1)
, mIsLoop(false)
{
#ifdef INNER_AUDIOENGINE
  if (useAudioEngine) {
    mAudioEngine = CreateAudioEngine();
  }
#endif
  Init();
}

MoviePlayerCore::~MoviePlayerCore()
{
  Done();
}

void
MoviePlayerCore::Init()
{
  mIsLoop = false;

  mPixelFormat = PIXEL_FORMAT_UNKNOWN;
}

void
MoviePlayerCore::Done()
{
#ifdef INNER_AUDIOENGINE
  if (mAudioEngine) {
    mAudioEngine->Done();
    delete mAudioEngine;
    mAudioEngine = nullptr;
  }
#endif

  if (mAudioTrackPlayer != nullptr) {
    mAudioTrackPlayer->Stop();
    delete mAudioTrackPlayer;
    mAudioTrackPlayer = nullptr;
  }

  if (mVideoTrackPlayer != nullptr) {
    mVideoTrackPlayer->Stop();
    delete mVideoTrackPlayer;
    mVideoTrackPlayer = nullptr;
  }

  if (mFd >= 0) {
    ::close(mFd);
    mFd = -1;
  }
}

void
MoviePlayerCore::Start()
{
  if (mAudioTrackPlayer) {
    mAudioTrackPlayer->Start();
  }
  if (mVideoTrackPlayer) {
    mVideoTrackPlayer->Start();
  }
}

bool
MoviePlayerCore::Open(const char *filepath)
{
  LOGV("open file: %s", filepath);

  AMediaExtractor *vEx = CreateExtractor(filepath);
  AMediaExtractor *aEx = CreateExtractor(filepath);
  bool isVideoFound    = vEx && SetupVideoTrackPlayer(vEx);
  bool isAudioFound    = aEx && SetupAudioTrackPlayer(aEx);
  if (isVideoFound || isAudioFound) {
    Start();
    return true;
  }
  return false;
}

bool 
MoviePlayerCore::Open(IMovieReadStream *stream)
{
  if (stream) {
    AMediaExtractor *vEx = CreateExtractor(stream);
    AMediaExtractor *aEx = CreateExtractor(stream);
    bool isVideoFound    = vEx && SetupVideoTrackPlayer(vEx);
    bool isAudioFound    = aEx && SetupAudioTrackPlayer(aEx);
    if (isVideoFound || isAudioFound) {
      Start();
      return true;
    }
  }
  return false;
}

bool
MoviePlayerCore::Open(int fd, off_t offset, off_t length)
{
  if (fd >= 0) {
    mFd                  = fd;
    AMediaExtractor *vEx = CreateExtractor(dup(fd), offset, length);
    AMediaExtractor *aEx = CreateExtractor(dup(fd), offset, length);
    bool isVideoFound    = vEx && SetupVideoTrackPlayer(vEx);
    bool isAudioFound    = aEx && SetupAudioTrackPlayer(aEx);
    if (isVideoFound || isAudioFound) {
      Start();
      return true;
    }
  }
  return false;
}

AMediaExtractor *
MoviePlayerCore::CreateExtractor(const char *filepath)
{
  AMediaExtractor *ex = AMediaExtractor_new();
  media_status_t err  = AMediaExtractor_setDataSource(ex, filepath);
  if (err != AMEDIA_OK) {
    LOGE("setDataSource error: %d", err);
    AMediaExtractor_delete(ex);
    return nullptr;
  }
  return ex;
}

static AMediaDataSource* CreateDataSource(IMovieReadStream* stream) {
  if (!stream) {
    return nullptr;
  }
  AMediaDataSource* dataSource = AMediaDataSource_new();
  if (!dataSource) {
    LOGE("Failed to create AMediaDataSource");
    return nullptr;
  }
  stream->AddRef(); // Ensure the stream is kept alive while used by AMediaDataSource
  AMediaDataSource_setUserdata(dataSource, (void*)stream);
  AMediaDataSource_setReadAt(dataSource, [](void* userdata, off64_t offset, void* buffer, size_t size) -> ssize_t {
    IMovieReadStream* stream = static_cast<IMovieReadStream*>(userdata);
    if (stream) {
      stream->Seek(offset, SEEK_SET);
      size_t bytesRead = stream->Read(buffer, size);
      return bytesRead > 0 ? bytesRead : -1;
    }
    return -1;
  });
  AMediaDataSource_setClose(dataSource, [](void* userdata) {
    IMovieReadStream* stream = static_cast<IMovieReadStream*>(userdata);
    if (stream) {
      stream->Release();
    }
  });
  AMediaDataSource_setGetSize(dataSource, [](void* userdata) -> ssize_t {
    IMovieReadStream* stream = static_cast<IMovieReadStream*>(userdata);
    return stream ? stream->Size() : 0;
  });
  return dataSource;
}

AMediaExtractor *
MoviePlayerCore::CreateExtractor(IMovieReadStream *stream)
{
  if (stream) {
    AMediaExtractor *ex = AMediaExtractor_new();
    AMediaDataSource *dataSource = CreateDataSource(stream);
    media_status_t err  = AMediaExtractor_setDataSourceCustom(ex, dataSource);
    if (err != AMEDIA_OK) {
      LOGE("setDataSource error: %d", err);
      AMediaExtractor_delete(ex);
#if __ANDROID_API__ >= 29
      AMediaDataSource_close(dataSource); // 失敗時にデータソースを開放
#endif
      AMediaDataSource_delete(dataSource); // メモリ解放
      return nullptr;
    }
    return ex;
  }
  return nullptr;
}

AMediaExtractor *
MoviePlayerCore::CreateExtractor(int fd, off_t offset, off_t length)
{
  AMediaExtractor *ex = AMediaExtractor_new();
  media_status_t err  = AMediaExtractor_setDataSourceFd(ex, fd, offset, length);
  if (err != AMEDIA_OK) {
    LOGE("setDataSource error: %d", err);
    AMediaExtractor_delete(ex);
    return nullptr;
  }
  return ex;
}

bool
MoviePlayerCore::SetupVideoTrackPlayer(AMediaExtractor *ex)
{
  // 初めて見つけたビデオトラックのみを対象にする
  int numtracks = AMediaExtractor_getTrackCount(ex);
  for (int i = 0; i < numtracks; i++) {
    AMediaFormat *format = AMediaExtractor_getTrackFormat(ex, i);
    const char *s        = AMediaFormat_toString(format);
    const char *mime;
    if (!AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
      LOGV("no mime type");
      AMediaFormat_delete(format);
      AMediaExtractor_delete(ex);
      return false;
    } else if (!strncmp(mime, "video/", 6)) {
      mVideoTrackPlayer = new VideoTrackPlayer(ex, i, &mClock);
      AMediaFormat_delete(format);
      return true;
    } else {
      AMediaFormat_delete(format);
    }
  }

  AMediaExtractor_delete(ex);
  return false;
}

bool
MoviePlayerCore::ReadAudioData(uint8_t* buffer, uint64_t frameCount, uint64_t* framesRead)
{
  if (mAudioTrackPlayer) {
    return mAudioTrackPlayer->ReadFromRingBuffer(buffer, frameCount, framesRead);
  }
  return false;
}


bool
MoviePlayerCore::SetupAudioTrackPlayer(AMediaExtractor *ex)
{
  // 初めて見つけたオーディオトラックのみを対象にする
  int numtracks = AMediaExtractor_getTrackCount(ex);
  for (int i = 0; i < numtracks; i++) {
    AMediaFormat *format = AMediaExtractor_getTrackFormat(ex, i);
    const char *s        = AMediaFormat_toString(format);
    const char *mime;
    if (!AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
      LOGV("no mime type");
      AMediaFormat_delete(format);
      AMediaExtractor_delete(ex);
      return false;
    } else if (!strncmp(mime, "audio/", 6)) {
      mAudioTrackPlayer = new AudioTrackPlayer(ex, i, &mClock);
      AMediaFormat_delete(format);

#ifdef INNER_AUDIOENGINE
      // 自前オーディオ再生の場合はAudioEngineを初期化する
      if (mAudioEngine != nullptr) {
        // XXX でコードされてるので通常これ？
        AudioFormat audioFormat = AUDIO_FORMAT_S16;
        mAudioEngine->Init([](void *userData, uint8_t *buffer, uint64_t frameCount, uint64_t *framesRead) {
            MoviePlayerCore* player = static_cast<MoviePlayerCore*>(userData);
            if (player) {
              return player->ReadAudioData(buffer, frameCount, framesRead);
            }
            return false;
        }, this, audioFormat, mAudioTrackPlayer->Channels(), mAudioTrackPlayer->SampleRate());
        mAudioEngine->Start();
      }
#endif

      return true;
    } else {
      AMediaFormat_delete(format);
    }
  }

  AMediaExtractor_delete(ex);
  return false;
}

void
MoviePlayerCore::SendTrackControl(int track, int32_t msg, int64_t arg, void *data)
{
  switch (track) {
  case TRACK_VIDEO:
    if (mVideoTrackPlayer != nullptr) {
      mVideoTrackPlayer->Post(msg, arg, data);
    }
    break;
  case TRACK_AUDIO:
    if (mAudioTrackPlayer != nullptr) {
      mAudioTrackPlayer->Post(msg, arg, data);
    }
    break;
  default:
    ASSERT(false, "unknown track type: %d", track);
    break;
  }
}

void
MoviePlayerCore::WaitTrackEvent(int track, int32_t event, int64_t timeoutUs)
{
  switch (track) {
  case TRACK_VIDEO:
    if (mVideoTrackPlayer != nullptr) {
      mVideoTrackPlayer->WaitEvent(event, timeoutUs);
    }
    break;
  case TRACK_AUDIO:
    if (mAudioTrackPlayer != nullptr) {
      mAudioTrackPlayer->WaitEvent(event, timeoutUs);
    }
    break;
  default:
    ASSERT(false, "unknown track type: %d", track);
    break;
  }
}

void
MoviePlayerCore::Play(bool loop)
{
  LOGV("play");

  SetLoop(loop);

  SendTrackControl(TRACK_AUDIO, AudioTrackPlayer::MSG_START);
  SendTrackControl(TRACK_VIDEO, VideoTrackPlayer::MSG_START);

  WaitTrackEvent(TRACK_AUDIO, AudioTrackPlayer::EVENT_FLAG_PLAY_READY);
  WaitTrackEvent(TRACK_VIDEO, VideoTrackPlayer::EVENT_FLAG_PLAY_READY);
}

void
MoviePlayerCore::Stop()
{
  LOGV("stop");

  SendTrackControl(TRACK_AUDIO, AudioTrackPlayer::MSG_STOP);
  SendTrackControl(TRACK_VIDEO, VideoTrackPlayer::MSG_STOP);

  WaitTrackEvent(TRACK_AUDIO, AudioTrackPlayer::EVENT_FLAG_STOPPED);
  WaitTrackEvent(TRACK_VIDEO, VideoTrackPlayer::EVENT_FLAG_STOPPED);
}

void
MoviePlayerCore::Pause()
{
  LOGV("pause");

  SendTrackControl(TRACK_AUDIO, AudioTrackPlayer::MSG_PAUSE);
  SendTrackControl(TRACK_VIDEO, VideoTrackPlayer::MSG_PAUSE);
}

void
MoviePlayerCore::Resume()
{
  LOGV("resume");

  SendTrackControl(TRACK_AUDIO, AudioTrackPlayer::MSG_RESUME);
  SendTrackControl(TRACK_VIDEO, VideoTrackPlayer::MSG_RESUME);
}

void
MoviePlayerCore::Seek(int64_t posUs)
{
  LOGV("seek posUs=%lld", posUs);

  SendTrackControl(TRACK_AUDIO, AudioTrackPlayer::MSG_SEEK, posUs);
  SendTrackControl(TRACK_VIDEO, VideoTrackPlayer::MSG_SEEK, posUs);
}

void
MoviePlayerCore::SetLoop(bool loop)
{
  LOGV("set loop=%d", loop);

  mIsLoop = loop;
  SendTrackControl(TRACK_AUDIO, AudioTrackPlayer::MSG_SET_LOOP, loop);
  SendTrackControl(TRACK_VIDEO, VideoTrackPlayer::MSG_SET_LOOP, loop);
}

void
MoviePlayerCore::SetPixelFormat(PixelFormat format)
{
  mPixelFormat = format;
}

bool
MoviePlayerCore::IsVideoAvailable() const
{
  return (mVideoTrackPlayer != nullptr && mVideoTrackPlayer->IsValid());
}

int32_t
MoviePlayerCore::Width() const
{
  if (mVideoTrackPlayer) {
    return mVideoTrackPlayer->Width();
  } else {
    return -1;
  }
}

int32_t
MoviePlayerCore::Height() const
{
  if (mVideoTrackPlayer) {
    return mVideoTrackPlayer->Height();
  } else {
    return -1;
  }
}

bool
MoviePlayerCore::IsAudioAvailable() const
{
  return (mAudioTrackPlayer != nullptr && mAudioTrackPlayer->IsValid());
}

int32_t
MoviePlayerCore::SampleRate() const
{
  if (mAudioTrackPlayer != nullptr) {
    return mAudioTrackPlayer->SampleRate();
  } else {
    return -1;
  }
}

int32_t
MoviePlayerCore::Channels() const
{
  if (mAudioTrackPlayer != nullptr) {
    return mAudioTrackPlayer->Channels();
  } else {
    return -1;
  }
}

int32_t
MoviePlayerCore::BitsPerSample() const
{
  if (mAudioTrackPlayer != nullptr) {
    return mAudioTrackPlayer->BitsPerSample();
  } else {
    return -1;
  }
}

int32_t
MoviePlayerCore::Encoding() const
{
  if (mAudioTrackPlayer != nullptr) {
    return mAudioTrackPlayer->Encoding();
  } else {
    return -1;
  }
}

int64_t
MoviePlayerCore::Duration() const
{
  return mClock.GetDuration();
}

int64_t
MoviePlayerCore::Position() const
{
  return mClock.GetPresentationTime();
}

bool
MoviePlayerCore::IsPlaying() const
{
  bool isVideoPlaying = mVideoTrackPlayer && mVideoTrackPlayer->IsPlaying();
  bool isAudioPlaying = mAudioTrackPlayer && mAudioTrackPlayer->IsPlaying();
  return (isVideoPlaying || isAudioPlaying);
}

bool
MoviePlayerCore::Loop() const
{
  return mIsLoop;
}

void
MoviePlayerCore::SetVolume(float volume)
{
#ifdef INNER_AUDIOENGINE
  if (mAudioEngine) {
    mAudioEngine->SetVolume(volume);
  }
#endif
}

float
MoviePlayerCore::Volume() const
{
  float volume = 1.0f;
#ifdef INNER_AUDIOENGINE
  if (mAudioEngine) {
    volume = mAudioEngine->Volume();
  }
#endif
  return volume;
}