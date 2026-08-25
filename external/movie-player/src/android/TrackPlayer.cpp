#define MYLOG_TAG "TrackPlayer"
#include "BasicLog.h"

#include "CommonUtils.h"
#include "Constants.h"
#include "TrackPlayer.h"

TrackPlayer::TrackPlayer(AMediaExtractor *ex, int32_t trackIndex, MediaClock *timer)
: mExtractor(ex)
, mCodec(nullptr)
, mTrackIndex(trackIndex)
, mState(STATE_UNINIT)
, mClock(timer)
{
  Init();
}

TrackPlayer::~TrackPlayer()
{
  Done();
  mClock = nullptr;
}

void
TrackPlayer::Init()
{
  mDuration = -1;
  mPosition = -1;

  mSawInputEOS  = false;
  mSawOutputEOS = false;

  mIsLoop = false;
}

void
TrackPlayer::Done()
{
  if (mCodec != nullptr) {
    AMediaCodec_stop(mCodec);
    mCodec = nullptr;
  }

  if (mExtractor != nullptr) {
    AMediaExtractor_delete(mExtractor);
    mExtractor = nullptr;
  }

  SetState(STATE_FINISH);
  ClearAllEvent();
  mClock = nullptr;
}

void
TrackPlayer::Start()
{
  StartThread();
}

void
TrackPlayer::Stop()
{
  StopThread();
}

bool
TrackPlayer::IsValid() const
{
  return mExtractor != nullptr;
}

bool
TrackPlayer::IsPlaying() const
{
  std::lock_guard<std::mutex> lock(mStateMutex);

  return (mState == STATE_PLAY || mState == STATE_PAUSE);
}

bool
TrackPlayer::Loop() const
{
  return mIsLoop;
}

bool
TrackPlayer::WaitEvent(int32_t event, int64_t timeoutUs)
{
  return mEventFlag.Wait(event, timeoutUs);
}

void
TrackPlayer::ClearEvent(int32_t event)
{
  mEventFlag.Clear(event);
}

void
TrackPlayer::ClearAllEvent()
{
  mEventFlag.ClearAll();
}

void
TrackPlayer::SetState(int32_t newState)
{
  std::lock_guard<std::mutex> lock(mStateMutex);

  mState = newState;
}

int32_t
TrackPlayer::GetState() const
{
  std::lock_guard<std::mutex> lock(mStateMutex);

  return mState;
}

void
TrackPlayer::HandleMessage(int32_t what, int64_t arg, void *data)
{
  switch (what) {
  case MSG_NOP: // no operation
    break;

  case MSG_DECODE:
    Decode();
    break;

  case MSG_SET_LOOP:
    mIsLoop = (arg != 0);
    break;

  default:
    ASSERT(false, "unknown message type: %d", what);
    break;
  }
}

void
TrackPlayer::ProcessInput(int32_t flags)
{
  if (mSawInputEOS) {
    return;
  }

  ssize_t bufIdx = AMediaCodec_dequeueInputBuffer(mCodec, CODEC_DEQINPUT_TIMEOUT_USEC);
  if (bufIdx >= 0) {
    size_t bufsize     = 0;
    uint32_t flags     = 0;
    uint8_t *buf       = AMediaCodec_getInputBuffer(mCodec, bufIdx, &bufsize);
    ssize_t sampleSize = AMediaExtractor_readSampleData(mExtractor, buf, bufsize);
    if (sampleSize < 0) {
      mSawInputEOS = true;
      sampleSize   = 0;
      flags        = AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
    }
    int64_t ptsUs = AMediaExtractor_getSampleTime(mExtractor);
    AMediaCodec_queueInputBuffer(mCodec, bufIdx, 0, sampleSize, ptsUs, flags);
    AMediaExtractor_advance(mExtractor);
  }
}

void
TrackPlayer::ProcessOutput(int32_t flags)
{
  if (mSawOutputEOS) {
    return;
  }

  AMediaCodecBufferInfo bufInfo;
  ssize_t bufIdx = AMediaCodec_dequeueOutputBuffer(mCodec, &bufInfo, 0);
  if (bufIdx >= 0) {
    if (bufInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
      mSawOutputEOS = true;
    }

    if (bufInfo.size > 0) {
      mPosition = bufInfo.presentationTimeUs;
    }

    // 出力バッファの内容をハンドリング
    // この処理についてはまるまる処理を投げて各自で実装させる
    HandleOutputData(bufIdx, bufInfo, flags);

  } else if (bufIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
    LOGV("output buffers changed");
  } else if (bufIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
    AMediaFormat *format = AMediaCodec_getOutputFormat(mCodec);
    LOGV("format changed to: %s", AMediaFormat_toString(format));
    AMediaFormat_delete(format);
  } else if (bufIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
    // LOGV("no output buffer right now");
  } else {
    // 未知のコードなので、念の為Error出力しておく
    LOGE("unexpected info code: %zd", bufIdx);
  }
}

void
TrackPlayer::Decode(int32_t flags)
{
  // MEMO
  // 現状ではIn/Outを逐次で1単位ずつ処理する
  // 必要なら各自をスレッドに分離して並行動作させること。
  ProcessInput(flags);
  ProcessOutput(flags);

  bool isDecodeCompleted = mSawInputEOS && mSawOutputEOS;
  if (isDecodeCompleted) {
    // 最後のフレームは(Duration-現フレームPTS)分だけ残す
    int64_t postDelay = mClock->GetDuration() - mClock->GetPresentationTime();
    std::this_thread::sleep_for(std::chrono::microseconds(postDelay));
    // LOGV("render post delay sleep: %lld us \n", postDelay);
    if (mIsLoop) {
      // LOGV("---- Loop ----\n");
      Post(MSG_SEEK, 0);
      Post(MSG_DECODE);
    } else {
      Post(MSG_STOP);
    }
  } else {
    // 停止中のseek動作時のワンショットレンダリングはメッセージチェインしない
    if (flags & DECODER_FLAG_ONESHOT) {
      return;
    }
    Post(MSG_DECODE);
  }
}