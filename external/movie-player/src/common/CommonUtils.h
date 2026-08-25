#pragma once

#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cinttypes>

#include <string>

// -----------------------------------------------------------------------------
// logger for inline implementations
// -----------------------------------------------------------------------------
#include "InlineLog.h"

// -----------------------------------------------------------------------------
// processor count
// -----------------------------------------------------------------------------
#include <thread>
inline int
get_num_of_cpus()
{
  unsigned int ncpu = std::thread::hardware_concurrency();
  if (ncpu == 0) {
    ncpu = 1; // for safety
  }
  // LOGV("CPU cores: %d\n", ncpu);

  return ncpu;
}

// -----------------------------------------------------------------------------
// time unit conversion
// -----------------------------------------------------------------------------
// clang-format off
inline int64_t  us_to_ns(int64_t us)  { return us * 1000; }
inline uint64_t us_to_ns(uint64_t us) { return us * 1000; }

inline int64_t  ns_to_us(int64_t ns)  { return ns / 1000; }
inline uint64_t ns_to_us(uint64_t ns) { return ns / 1000; }

inline double   us_to_s(int64_t us)   { return us / 1'000'000.0; }
inline double   ns_to_s(int64_t ns)   { return ns / 1'000'000'000.0; }

inline int64_t  s_to_us(double s)     { return s * 1'000'000; }
inline int64_t  s_to_ns(double s)     { return s * 1'000'000'000.0; }
// clang-format on

// -----------------------------------------------------------------------------
// time functions
// -----------------------------------------------------------------------------
#include <chrono>

// MEMO
// VSではhigh_resolution_clockはVS2015からsteady_clockの別名になっていて、
// 中身はQueryPerformanceCounter関数を使用した実装になっている。

// 単純な精度確認用
static void
simple_time_resolution_test()
{
  using namespace std::chrono;

  auto a = high_resolution_clock::now();
  auto b = high_resolution_clock::now();
  //printf("%" PRId64 " ns\n", duration_cast<nanoseconds>(b - a).count());
}

static int64_t
get_time_ns()
{
  using namespace std::chrono;

#if 0
  // 単純な精度確認用
  auto a = high_resolution_clock::now();
  auto b = high_resolution_clock::now();
  printf("%" PRId64 " ns\n", duration_cast<nanoseconds>(b - a).count());
#endif

  auto now    = high_resolution_clock::now();
  auto now_ns = time_point_cast<nanoseconds>(now);
  return now_ns.time_since_epoch().count();
}

static int64_t
get_time_us()
{
  using namespace std::chrono;

  auto now    = high_resolution_clock::now();
  auto now_us = time_point_cast<microseconds>(now);
  return now_us.time_since_epoch().count();
}

// 区間計時
class TimeMeasure
{
public:
  TimeMeasure(const std::string label, bool autoStart = true)
  : label(label)
  , isActive(false)
  {
    if (autoStart) {
      start();
    }
  }
  ~TimeMeasure() { stop(); }

  void start()
  {
    begin    = std::chrono::high_resolution_clock::now();
    isActive = true;
  }

  void stop()
  {
    using namespace std::chrono;
    if (isActive) {
      high_resolution_clock::time_point end = high_resolution_clock::now();
      microseconds elapsed                  = duration_cast<microseconds>(end - begin);
      //LOGV("PERF: %s: %" PRId64 "[us]\n", label.c_str(), elapsed.count());

      isActive = false;
    }
  }

private:
  bool isActive;
  std::string label;
  std::chrono::high_resolution_clock::time_point begin;
};

// -----------------------------------------------------------------------------
// internal audio format
// -----------------------------------------------------------------------------
enum AudioFormat
{
  AUDIO_FORMAT_U8 = 0,
  AUDIO_FORMAT_S16,
  AUDIO_FORMAT_S32,
  AUDIO_FORMAT_F32,
};

// オーディオフレーム列のdurationを計算する
inline int64_t
calc_audio_duration_us(int64_t frameCount, int64_t sampleRate)
{
  return (frameCount * 1'000'000 / sampleRate);
}

inline int64_t
calc_audio_duration_ns(int64_t frameCount, int64_t sampleRate)
{
  return (frameCount * 1'000'000'000 / sampleRate);
}

// -----------------------------------------------------------------------------
// internal pixel format
// -----------------------------------------------------------------------------
enum PixelFormat
{
  PIXEL_FORMAT_UNKNOWN = -1,

  PIXEL_FORMAT_RGB_BEGIN = 0,
  PIXEL_FORMAT_ABGR      = PIXEL_FORMAT_RGB_BEGIN,
  PIXEL_FORMAT_ARGB,
  PIXEL_FORMAT_RGBA,
  PIXEL_FORMAT_BGRA,
  PIXEL_FORMAT_RGB_END = PIXEL_FORMAT_BGRA,

  PIXEL_FORMAT_YUV_BEGIN,
  PIXEL_FORMAT_I420 = PIXEL_FORMAT_YUV_BEGIN,
  PIXEL_FORMAT_I422,
  PIXEL_FORMAT_I444,
  PIXEL_FORMAT_NV12,
  PIXEL_FORMAT_NV21,
  PIXEL_FORMAT_YUV_END = PIXEL_FORMAT_NV21,
};

// RGB判定
inline bool
is_rgb_pixel_format(PixelFormat format)
{
  return (PIXEL_FORMAT_RGB_BEGIN <= format && format <= PIXEL_FORMAT_RGB_END);
}

// YUV判定
inline bool
is_yuv_pixel_format(PixelFormat format)
{
  return (PIXEL_FORMAT_YUV_BEGIN <= format && format <= PIXEL_FORMAT_YUV_END);
}

// NV系判定
inline bool
is_nv_pixel_format(PixelFormat format)
{
  return (format == PIXEL_FORMAT_NV12 || format == PIXEL_FORMAT_NV21);
}

// -----------------------------------------------------------------------------
// internal color defines
// -----------------------------------------------------------------------------
enum ColorRange
{
  COLOR_RANGE_UNDEF = 0,
  COLOR_RANGE_LIMITED,
  COLOR_RANGE_FULL,
};

enum ColorSpace
{
  COLOR_SPACE_UNKNOWN  = -1,
  COLOR_SPACE_IDENTITY = 0,
  COLOR_SPACE_BT_601,
  COLOR_SPACE_BT_709,
  COLOR_SPACE_SMPTE_170,
  COLOR_SPACE_SMPTE_240,
  COLOR_SPACE_BT_2020,
  COLOR_SPACE_SRGB,
};

// -----------------------------------------------------------------------------
// thread safe queue
// -----------------------------------------------------------------------------
#include <condition_variable>
#include <mutex>
#include <queue>
#include <chrono>

template<class T>
class BlockingSafeQueue
{
public:
  BlockingSafeQueue() {}
  ~BlockingSafeQueue() {}

  void Enqueue(T t)
  {
    std::lock_guard<std::mutex> lock(mMutex);

    mQueue.push(t);
    mCond.notify_one();
  }

  // timeout 0: infinite
  bool Dequeue(T &result, int64_t timeoutUs = 0)
  {
    std::chrono::microseconds timeout(timeoutUs);
    if (timeoutUs <= 0) {
      timeout = std::chrono::microseconds::max();
    }

    std::unique_lock<std::mutex> lock(mMutex);

    while (mQueue.empty()) {
      std::cv_status result = mCond.wait_for(lock, timeout);
      if (result == std::cv_status::timeout) {
        INLINE_LOGE("SafeQueue: dequeue time out: timeout=%" PRId64 "\n",
                    timeout.count());
        return false;
      }
    }
    result = mQueue.front();
    mQueue.pop();

    return true;
  }

  size_t Size() const
  {
    std::lock_guard<std::mutex> lock(mMutex);

    return mQueue.size();
  }

  void Clear()
  {
    std::lock_guard<std::mutex> lock(mMutex);

    mQueue = {};
  }

private:
  std::queue<T> mQueue;
  mutable std::mutex mMutex;
  std::condition_variable mCond;
};

template<class T>
class SafeQueue
{
public:
  SafeQueue() {}
  ~SafeQueue() {}

  void Enqueue(T t)
  {
    std::lock_guard<std::recursive_mutex> lock(mMutex);

    mQueue.push(t);
  }

  bool Dequeue(T &result)
  {
    std::lock_guard<std::recursive_mutex> lock(mMutex);

    if (mQueue.empty()) {
      return false;
    }

    result = mQueue.front();
    mQueue.pop();

    return true;
  }

  size_t Size() const
  {
    std::lock_guard<std::recursive_mutex> lock(mMutex);

    return mQueue.size();
  }

  void Clear()
  {
    std::lock_guard<std::recursive_mutex> lock(mMutex);

    mQueue = {};
  }

  // FrontとPopはmutex保護していない
  // ループ処理などで全体をロックした状態で作業したい場合にDequeueでは
  // 用を成さないケースでmutexを参照してlockした上で使用すること
  bool Front(T &result)
  {
    if (mQueue.empty()) {
      return false;
    }

    result = mQueue.front();

    return true;
  }

  bool Pop()
  {
    if (mQueue.empty()) {
      return false;
    }

    mQueue.pop();

    return true;
  }

  void Lock() { mMutex.lock(); }
  void Unlock() { mMutex.unlock(); }

private:
  std::queue<T> mQueue;
  // メンバ関数内部にロックが閉じてる操作と、Lock/Unlockで外部からのロックが
  // 必要な操作との2系統にわかれてしまったので、多重にロックするケースをケアするため
  // 念のためrecursiveにしておく
  mutable std::recursive_mutex mMutex;
};

// -----------------------------------------------------------------------------
// Event Flag
// -----------------------------------------------------------------------------
#include <mutex>
#include <condition_variable>

class EventFlag
{
public:
  explicit EventFlag(int32_t initial = 0x0)
  : mEvent(initial)
  {}
  EventFlag(const EventFlag &)            = delete;
  EventFlag &operator=(const EventFlag &) = delete;

  // timeout 0: infinite
  bool Wait(int32_t event, int64_t timeoutUs = 0)
  {
    INLINE_ASSERT(event != 0, "Invalid event flag (no bit rised).\n");

    std::unique_lock<std::mutex> lock(mMutex);

#if defined(DEBUG_INFO_EVENTFLAG)
    INLINE_LOGV("wait event: %x, timeout = %" PRId64 "us, current mEvent: %x", event,
                timeoutUs, mEvent);
#endif

    bool noTimeout = true;

    if (timeoutUs > 0) {
      std::chrono::microseconds timeout(timeoutUs);
      noTimeout = mCond.wait_for(lock, timeout, [&] { return (mEvent & event) != 0; });
    } else {
      mCond.wait(lock, [&] { return (mEvent & event) != 0; });
    }

    // 現実装ではタイムアウトにかかわらずフラグは落として帰る
    // 対応の振り分けが必要なケースが出たら適宜引数追加などで対応のこと
    Clear(event);

    return noTimeout;
  }

  void Set(int32_t event)
  {
    INLINE_ASSERT(event != 0, "Invalid event flag (no bit rise).\n");

    std::lock_guard<std::mutex> lock(mMutex);

#if defined(DEBUG_INFO_EVENTFLAG)
    INLINE_LOGV("set event: %x", event);
#endif

    mEvent |= event;
    mCond.notify_all();
  }

  void Clear(int32_t event)
  {
#if defined(DEBUG_INFO_EVENTFLAG)
    INLINE_LOGV("clear event: %x", event);
#endif
    mEvent &= ~event;
  }

  void ClearAll() { Clear(0xffffffff); }

private:
  std::mutex mMutex;
  std::condition_variable mCond;
  int32_t mEvent;
};

// -----------------------------------------------------------------------------
// Event Object
// -----------------------------------------------------------------------------
class EventObject
{
public:
  explicit EventObject(bool initialState = false)
  : mFlag(initialState)
  {}
  EventObject(const EventObject &)            = delete;
  EventObject &operator=(const EventObject &) = delete;

  void Wait()
  {
    std::unique_lock<std::mutex> lock(mMutex);

    mCond.wait(lock, [&] { return mFlag; });
  }

  void Set()
  {
    std::lock_guard<std::mutex> lock(mMutex);

    mFlag = true;
    mCond.notify_all();
  }

  void Reset()
  {
    std::lock_guard<std::mutex> lock(mMutex);

    mFlag = false;
  }

private:
  std::mutex mMutex;
  std::condition_variable mCond;
  bool mFlag;
};