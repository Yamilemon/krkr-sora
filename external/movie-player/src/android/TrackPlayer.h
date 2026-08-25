#pragma once

#include "MessageLooper.h"
#include "CommonUtils.h"
#include "MediaClock.h"

#include <cstdint>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>

#include "media/NdkMediaCodec.h"
#include "media/NdkMediaExtractor.h"

// -----------------------------------------------------------------------------
// トラックプレイヤスレッド
// -----------------------------------------------------------------------------
class TrackPlayer : public MessageLooper
{
public:
  // トラックプレイヤ系共通メッセージ
  enum : int32_t
  {
    MSG_NOP = 0,
    MSG_PRELOAD,
    MSG_START,
    MSG_DECODE,
    MSG_PAUSE,
    MSG_RESUME,
    MSG_SET_LOOP,
    MSG_SEEK,
    MSG_STOP,
    COMMON_MSG_COUNT,
  };

  // トラックプレイヤ系共通ステート
  enum : int32_t
  {
    STATE_UNINIT,
    STATE_OPEN,
    STATE_PRELOADING,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_STOP,
    STATE_FINISH,
    COMMON_STATE_COUNT,
  };

  // トラックプレイヤ系共通イベントフラグ
  enum
  {
    EVENT_FLAG_PRELOADED  = 1 << 0,
    EVENT_FLAG_PLAY_READY = 1 << 1,
    EVENT_FLAG_STOPPED    = 1 << 2,
  };

public:
  TrackPlayer(AMediaExtractor *ex, int32_t trackIndex, MediaClock *timer);
  virtual ~TrackPlayer();

  void Init();
  void Done();

  virtual void Start();
  virtual void Stop();

  void Decode(int32_t flags = 0);
  void ProcessInput(int32_t flags);
  void ProcessOutput(int32_t flags);
  virtual void HandleOutputData(ssize_t bufIdx, AMediaCodecBufferInfo &bufInfo,
                                int32_t flags) = 0;

  bool IsValid() const;
  bool IsPlaying() const;
  bool Loop() const;

  int64_t Duration() const { return mDuration; }
  int64_t Position() const { return mPosition; }

  void SetState(int32_t newState);
  int32_t GetState() const;

  // 0: infinite wait
  bool WaitEvent(int32_t event, int64_t timeoutUs = 0);
  void ClearEvent(int32_t event);
  void ClearAllEvent();

protected:
  virtual void HandleMessage(int32_t what, int64_t arg, void *data) override;

protected:
  // AMediaCodec_dequeueInputBuffer() のタイムアウトusec
  static const int CODEC_DEQINPUT_TIMEOUT_USEC = 10000;

  // Decode() はメッセージを自己発行してチェインするので
  // 特殊な状況でチェインせずにワンショットデコードしたい場合のフラグ
  static const int32_t DECODER_FLAG_ONESHOT = 0x1;

  // Media API のインスタンス
  AMediaExtractor *mExtractor;
  AMediaCodec *mCodec;
  int32_t mTrackIndex;

  // ストリームフラグ
  bool mSawInputEOS;
  bool mSawOutputEOS;

  // ステート
  mutable std::mutex mStateMutex;
  int32_t mState;
  std::atomic_bool mIsLoop;

  // トラックのDuration/Position
  int64_t mDuration;
  std::atomic_int64_t mPosition;

  // メディアグローバルなタイマー参照
  MediaClock *mClock;

  // 同期用イベントフラグ
  EventFlag mEventFlag;
};