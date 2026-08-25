#define MYLOG_TAG "MediaClock"
#include "BasicLog.h"
#include "CommonUtils.h"
#include "MediaClock.h"

MediaClock::MediaClock()
: mDurationUs(-1)
, mPresentationTimeUs(-1)
, mAnchorMediaTimeUs(-1)
, mAnchorRealTimeUs(-1)
, mMaxMediaTimeUs(INT64_MAX)
, mStartMediaTimeUs(-1)
{}

MediaClock::~MediaClock()
{
  Reset();
}

void
MediaClock::Reset()
{
  std::lock_guard<std::mutex> lock(mLock);

  mDurationUs         = -1;
  mPresentationTimeUs = -1;

  mStartMediaTimeUs  = -1;
  mStartSystemTimeUs = -1;
  mMaxMediaTimeUs    = INT64_MAX;

  mAnchorMediaTimeUs = -1;
  mAnchorRealTimeUs  = -1;
  mPlaybackRate      = 1.0;
}

bool
MediaClock::IsStarted() const
{
  std::lock_guard<std::mutex> lock(mLock);

  return is_started();
}

bool
MediaClock::is_started() const
{
  return (mStartMediaTimeUs >= 0 && mAnchorMediaTimeUs >= 0);
}

void
MediaClock::SetDuration(int64_t mediaDurationUs)
{
  std::lock_guard<std::mutex> lock(mLock);

  mDurationUs = mediaDurationUs;
}

int64_t
MediaClock::GetDuration() const
{
  std::lock_guard<std::mutex> lock(mLock);

  return mDurationUs;
}

void
MediaClock::SetPresentationTime(int64_t ptsUs)
{
  std::lock_guard<std::mutex> lock(mLock);

  if (ptsUs >= mDurationUs) {
    // LOGV("presentation time exceeds the duration: %" PRId64 " / %" PRId64 "\n", ptsUs,
    // mDurationUs);
    ptsUs = mDurationUs;
  }

  mPresentationTimeUs = ptsUs;

#if defined(DEBUG_INFO_MEDIACLOCK)
  LOGV("set pts:%" PRId64 "\n", mPresentationTimeUs);
#endif
}

int64_t
MediaClock::GetPresentationTime() const
{
  std::lock_guard<std::mutex> lock(mLock);

  return mPresentationTimeUs;
}

void
MediaClock::SetStartMediaTime(int64_t mediaUs)
{
  std::lock_guard<std::mutex> lock(mLock);

  mStartSystemTimeUs = get_time_us();
  mStartMediaTimeUs  = mediaUs;

#if defined(DEBUG_INFO_MEDIACLOCK)
  LOGV("set start media time:%" PRId64 "\n", mStartMediaTimeUs);
#endif
}

int64_t
MediaClock::GetStartSystemTime() const
{
  std::lock_guard<std::mutex> lock(mLock);

  return mStartSystemTimeUs;
}

int64_t
MediaClock::GetStartMediaTime() const
{
  std::lock_guard<std::mutex> lock(mLock);

  return mStartMediaTimeUs;
}

void
MediaClock::ClearStartMediaTime()
{
  std::lock_guard<std::mutex> lock(mLock);

  mStartSystemTimeUs = -1;
  mStartMediaTimeUs  = -1;
}

void
MediaClock::ClearAnchorTime()
{
  std::lock_guard<std::mutex> lock(mLock);

  mAnchorMediaTimeUs = -1;
  mAnchorRealTimeUs  = -1;
}

bool
MediaClock::UpdateAnchorTime(int64_t mediaUs, int64_t realUs, int64_t maxMediaUs)
{
  if (mediaUs < 0 || realUs < 0) {
    LOGE("negative anchor time is not allowed.\n");
    return false;
  }

  std::lock_guard<std::mutex> lock(mLock);

  int64_t nowUs      = get_time_us();
  int64_t nowMediaUs = mediaUs + (nowUs - realUs) * (double)mPlaybackRate;

  mAnchorMediaTimeUs = nowMediaUs;
  mAnchorRealTimeUs  = nowUs;
  mMaxMediaTimeUs    = maxMediaUs;

#if defined(DEBUG_INFO_MEDIACLOCK)
  LOGV("m:%" PRId64 ", anchor_m:%" PRId64 ", anchor_r:%" PRId64 ", max_m:%" PRId64 "\n",
       mediaUs, mAnchorMediaTimeUs, mAnchorRealTimeUs, mMaxMediaTimeUs);
#endif

  return true;
}

void
MediaClock::SetPlaybackRate(float rate)
{
  std::lock_guard<std::mutex> lock(mLock);

  if (mAnchorRealTimeUs == -1) {
    mPlaybackRate = rate;
    return;
  }

  int64_t nowUs = get_time_us();
  int64_t nowMediaUs =
    mAnchorMediaTimeUs + (nowUs - mAnchorRealTimeUs) * (double)mPlaybackRate;

  mAnchorMediaTimeUs = nowMediaUs;
  mAnchorRealTimeUs  = nowUs;
  mPlaybackRate      = rate;
}

float
MediaClock::GetPlaybackRate() const
{
  std::lock_guard<std::mutex> lock(mLock);

  return mPlaybackRate;
}

int64_t
MediaClock::GetMediaTime(int64_t realUs) const
{
  std::lock_guard<std::mutex> lock(mLock);

  return get_media_time(realUs);
}

int64_t
MediaClock::get_media_time(int64_t realUs) const
{
  int64_t mediaUs =
    mAnchorMediaTimeUs + (realUs - mAnchorRealTimeUs) * (double)mPlaybackRate;
  if (mediaUs < mStartMediaTimeUs) {
    mediaUs = mStartMediaTimeUs;
  } else if (mediaUs < 0) {
    mediaUs = 0;
  }

  return mediaUs;
}

int64_t
MediaClock::GetRealTimeFor(int64_t targetMediaUs) const
{
  std::lock_guard<std::mutex> lock(mLock);

  if (mPlaybackRate == 0.0) {
    return -1; // avoid divide by zero
  }

  int64_t nowUs      = get_time_us();
  int64_t nowMediaUs = get_media_time(nowUs);

  return (targetMediaUs - nowMediaUs) / (double)mPlaybackRate + nowUs;
}
