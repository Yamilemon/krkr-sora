#pragma once

#include "VideoTrackPlayer.h"
#include "AudioTrackPlayer.h"
#include "MediaClock.h"
#include "media/NdkMediaExtractor.h"
#include <functional>

class AudioEngine;
class IMovieReadStream;

// ムービープレイヤー内部実装クラス
class MoviePlayerCore
{
public:
  MoviePlayerCore(bool useAudioEngine=true);
  virtual ~MoviePlayerCore();

  bool Open(const char *filepath);
  bool Open(int fd, off_t offset, off_t length);
  bool Open(IMovieReadStream *stream);

  void Start();

  void Play(bool loop = false);
  void Stop();
  void Pause();
  void Resume();
  void Seek(int64_t posUs);
  void SetLoop(bool loop);

  void SetPixelFormat(PixelFormat format);

  bool IsVideoAvailable() const;
  int32_t Width() const;
  int32_t Height() const;
  //未実装
  //float FrameRate() const;
  //PixelFormat OutputPixelFormat() const;

  bool IsAudioAvailable() const;
  int32_t SampleRate() const;
  int32_t Channels() const;
  int32_t BitsPerSample() const;
  int32_t Encoding() const;
  void SetVolume(float volume);
  float Volume() const;

  int64_t Duration() const;
  int64_t Position() const;
  bool IsPlaying() const;
  bool Loop() const;

  void SetOnVideoDecoded(IMoviePlayer::OnVideoDecoded func) {
    if (mVideoTrackPlayer) {
      mVideoTrackPlayer->SetOnVideoDecoded(func, mPixelFormat);
    }
  }

  void SetOnAudioDecoded(OnAudioDecoded func, void *userPtr) {
    if (mAudioTrackPlayer) {
      mAudioTrackPlayer->SetOnAudioDecoded(func, userPtr);
    }
  }

  bool ReadAudioData(uint8_t* buffer, uint64_t frameCount, uint64_t* framesRead);

private:
  void Init();
  void Done();

  AMediaExtractor *CreateExtractor(const char *filepath);
  AMediaExtractor *CreateExtractor(int fd, off_t offset, off_t length);
  AMediaExtractor *CreateExtractor(IMovieReadStream *stream);

  bool SetupVideoTrackPlayer(AMediaExtractor *ex);
  bool SetupAudioTrackPlayer(AMediaExtractor *ex);

  enum
  {
    TRACK_VIDEO = 0,
    TRACK_AUDIO,
  };
  void SendTrackControl(int track, int32_t msg, int64_t arg = 0, void *data = nullptr);
  void WaitTrackEvent(int track, int32_t event, int64_t timeoutUs = 0);

private:
  VideoTrackPlayer *mVideoTrackPlayer;
  AudioTrackPlayer *mAudioTrackPlayer;

  MediaClock mClock;

  int mFd;

  bool mIsLoop;

  // video 情報
  PixelFormat mPixelFormat;

#ifdef INNER_AUDIOENGINE
  // 内部オーディオエンジン
  AudioEngine *mAudioEngine;
#endif
};