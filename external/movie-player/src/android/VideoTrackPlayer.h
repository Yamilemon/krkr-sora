#pragma once

#include "TrackPlayer.h"
#include "CommonUtils.h"
#include "IMoviePlayer.h"

// -----------------------------------------------------------------------------
// ビデオトラックプレイヤ
// -----------------------------------------------------------------------------
class VideoTrackPlayer : public TrackPlayer
{
public:
  VideoTrackPlayer(AMediaExtractor *ex, int32_t trackIndex, MediaClock *timer);
  virtual ~VideoTrackPlayer();

  void Init();
  void Done();

  virtual void HandleOutputData(ssize_t bufIdx, AMediaCodecBufferInfo &bufInfo,
                                int flags) override;

  int32_t Width() const;
  int32_t Height() const;

  void SetOnVideoDecoded(IMoviePlayer::OnVideoDecoded func, PixelFormat format) {
    mOnVideoDecoded = func;
    mOnVideoFormat = format;
  }

private:
  virtual void HandleMessage(int32_t what, int64_t arg, void *data) override;

private:
  // video 情報
  int32_t mWidth, mHeight;

  // output 情報
  int32_t mOutputWidth, mOutputHeight;
  int32_t mOutputStride;
  int32_t mOutputColorFormat;
  int32_t mOutputColorRange;
  int32_t mOutputColorSpace;

  // オーディオコールバック
  IMoviePlayer::OnVideoDecoded mOnVideoDecoded;
  PixelFormat mOnVideoFormat;
};
