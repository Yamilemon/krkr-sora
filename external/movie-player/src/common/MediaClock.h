#pragma once

#include "CommonUtils.h"

#include <cstdint>
#include <chrono>
#include <mutex>

class MediaClock
{
public:
  MediaClock();
  ~MediaClock();

  void Reset();

  bool IsStarted() const;

  void SetDuration(int64_t mediaDurationUs);
  int64_t GetDuration() const;

  void SetPresentationTime(int64_t ptsUs);
  int64_t GetPresentationTime() const;

  void SetPlaybackRate(float rate);
  float GetPlaybackRate() const;

  void SetStartMediaTime(int64_t mediaUs);
  int64_t GetStartSystemTime() const;
  int64_t GetStartMediaTime() const;
  void ClearStartMediaTime();

  void ClearAnchorTime();
  bool UpdateAnchorTime(int64_t mediaUs, int64_t realUs, int64_t maxMediaUs = INT64_MAX);
  int64_t GetMediaTime(int64_t realUs) const;
  int64_t GetRealTimeFor(int64_t targetMediaUs) const;

private:
  bool is_started() const;
  int64_t get_media_time(int64_t realUs) const;

private:
  mutable std::mutex mLock;

  int64_t mDurationUs;
  int64_t mPresentationTimeUs;

  int64_t mStartMediaTimeUs;
  int64_t mStartSystemTimeUs;

  int64_t mAnchorMediaTimeUs;
  int64_t mAnchorRealTimeUs;
  int64_t mMaxMediaTimeUs;

  float mPlaybackRate;
};
