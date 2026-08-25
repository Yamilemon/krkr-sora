#pragma once

#include <IMoviePlayer.h>
#include <cstdint>
#include <android/asset_manager.h>

// ムービープレイヤー実装クラス
class MoviePlayer : public IMoviePlayer
{
public:
  MoviePlayer(InitParam &param);
  virtual ~MoviePlayer();

  bool Open(const char *filepath);
  bool Open(IMovieReadStream *stream);

  bool Open(int fd, off_t offset, off_t length);
  bool Open(AAssetManager *mgr, const char *filepath);

  // XXX: 未実装
  virtual State GetState() const override { return State::STATE_UNINIT; }
  virtual void SetOnState(OnState func, void *userPtr) override {}

  virtual void Play(bool loop = false) override;
  virtual void Stop() override;
  virtual void Pause() override;
  virtual void Resume() override;
  virtual void Seek(int64_t posUs) override;
  virtual void SetLoop(bool loop) override;

  void SetColorFormat(ColorFormat format);

  int32_t Width() const;
  int32_t Height() const;

  // video info
  virtual bool IsVideoAvailable() const override;;
  virtual void GetVideoFormat(VideoFormat *format) const override;

  // audio info
  virtual bool IsAudioAvailable() const override;
  virtual void GetAudioFormat(AudioFormat *format) const override;

  // audio volume
  virtual void SetVolume(float volume) override;
  virtual float Volume() const override;

  virtual int64_t Duration() const override;
  virtual int64_t Position() const override;
  virtual bool IsPlaying() const override;
  virtual bool Loop() const override;

  virtual void SetOnAudioDecoded(OnAudioDecoded func, void *userPtr) override;

  virtual void SetOnVideoDecoded(OnVideoDecoded func) override;

private:
  void Init();
  void Done();

private:
  class MoviePlayerCore *mPlayer;
  InitParam mInitParam;
  AAsset *mAsset;

  OnVideoDecoded mOnVideoDecoded;
  void *mOnVideoDecodedUserPtr;
};