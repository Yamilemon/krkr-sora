#define MYLOG_TAG "AudioTrackPlayer"
#include "BasicLog.h"

#include "Constants.h"
#include "CommonUtils.h"
#include "AudioTrackPlayer.h"

AudioTrackPlayer::AudioTrackPlayer(AMediaExtractor *ex, int32_t trackIndex,
                                   MediaClock *timer)
: TrackPlayer(ex, trackIndex, timer)
, mOnAudioDecoded(nullptr)
, mOnAudioDecodedUserPtr(nullptr)
{
  Init();

  AMediaExtractor_selectTrack(mExtractor, mTrackIndex);

  LOGV("audio track: %d", mTrackIndex);

  // 現在のメディアタイマーにセットされているDurationよりも長い場合は
  // それをムービー全体のDurationとして反映する
  if (mDuration > mClock->GetDuration()) {
    mClock->SetDuration(mDuration);
  }

  // トラックフォーマット
  AMediaFormat *format = AMediaExtractor_getTrackFormat(mExtractor, mTrackIndex);
  AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &mDuration);

  // コーデックセットアップ
  const char *mime;
  AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
  mCodec = AMediaCodec_createDecoderByType(mime);
  AMediaCodec_configure(mCodec, format, nullptr, nullptr, 0);
  AMediaCodec_start(mCodec);

  // 出力フォーマット
  AMediaFormat *outFormat = AMediaCodec_getOutputFormat(mCodec);
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &mSampleRate);
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &mChannels);
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, &mMaxInputSize);

#ifdef AMEDIAFORMAT_KEY_BITS_PER_SAMPLE
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_BITS_PER_SAMPLE, &mBitsPerSample);
#else
  AMediaFormat_getInt32(outFormat, "bits-per-sample", &mBitsPerSample);
#endif
  if (mBitsPerSample <= 0) {
    mBitsPerSample = 16; // デフォルト16bitとしておく
  }

#ifdef AMEDIAFORMAT_KEY_PCM_ENCODING
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_PCM_ENCODING, &mEncoding);
#else
  AMediaFormat_getInt32(outFormat, "pcm-encoding", &mEncoding);
#endif
  if (mEncoding <= 0) {
    mEncoding = kAudioEncodingPcm16bit;
  }

  LOGV("audio duration=%lld, sampleRate=%d, channels=%d, bps=%d\n", mDuration,
       mSampleRate, mChannels, mBitsPerSample);
}

AudioTrackPlayer::~AudioTrackPlayer()
{
  Done();
}

void
AudioTrackPlayer::Init()
{
  mDuration = -1;

  mChannels      = -1;
  mSampleRate    = -1;
  mBitsPerSample = -1;
  mMaxInputSize  = -1;

  mOnAudioDecoded        = nullptr;
  mOnAudioDecodedUserPtr = nullptr;
}

void
AudioTrackPlayer::Done()
{
  // キューに残っているバッファを全て解放
  ReleaseAllBuffers();
}

void
AudioTrackPlayer::EnqueueBuffer(ssize_t bufIdx, size_t offset, size_t size)
{
  std::lock_guard<std::mutex> lock(mBufferQueueMutex);
  
  AudioBufferItem item;
  item.bufIdx = bufIdx;
  item.offset = offset;
  item.size = size;
  item.readOffset = 0;
  mBufferQueue.push(item);
}

void
AudioTrackPlayer::ReleaseAllBuffers()
{
  std::lock_guard<std::mutex> lock(mBufferQueueMutex);
  
  while (!mBufferQueue.empty()) {
    AudioBufferItem& item = mBufferQueue.front();
    if (mCodec) {
      AMediaCodec_releaseOutputBuffer(mCodec, item.bufIdx, false);
    }
    mBufferQueue.pop();
  }
  LOGV("All buffers released\n");
}

bool
AudioTrackPlayer::ReadFromRingBuffer(uint8_t* buffer, uint64_t frameCount, uint64_t* framesRead)
{
  if (frameCount == 0) {
    if (framesRead) *framesRead = 0;
    return false;
  }
  
  std::lock_guard<std::mutex> lock(mBufferQueueMutex);
  
  if (mBufferQueue.empty()) {
    if (framesRead) *framesRead = 0;
    return false;
  }
  
  int32_t frameSize = (mBitsPerSample / 8) * mChannels;
  size_t bytesRequested = frameCount * frameSize;
  size_t bytesWritten = 0;
  
  while (bytesWritten < bytesRequested && !mBufferQueue.empty()) {
    AudioBufferItem& item = mBufferQueue.front();
    
    // AMediaCodecからバッファを取得
    size_t bufSize;
    uint8_t* buf = AMediaCodec_getOutputBuffer(mCodec, item.bufIdx, &bufSize);
    if (!buf) {
      // バッファ取得失敗の場合はこのアイテムをスキップ
      AMediaCodec_releaseOutputBuffer(mCodec, item.bufIdx, false);
      mBufferQueue.pop();
      continue;
    }
    
    // このバッファから読み取れるバイト数
    size_t remainingInBuffer = item.size - item.readOffset;
    size_t bytesToCopy = std::min(remainingInBuffer, bytesRequested - bytesWritten);
    
    memcpy(buffer + bytesWritten, buf + item.offset + item.readOffset, bytesToCopy);
    bytesWritten += bytesToCopy;
    item.readOffset += bytesToCopy;
    
    // バッファを全て読み切ったら解放
    if (item.readOffset >= item.size) {
      AMediaCodec_releaseOutputBuffer(mCodec, item.bufIdx, false);
      mBufferQueue.pop();
    }
  }
  
  uint64_t actualFramesRead = bytesWritten / frameSize;
  
  // 不足分をゼロで埋める
  if (actualFramesRead < frameCount) {
    size_t remainingBytes = (frameCount - actualFramesRead) * frameSize;
    memset(buffer + bytesWritten, 0, remainingBytes);
  }
  
  if (framesRead) *framesRead = actualFramesRead;
  return actualFramesRead > 0;
}

int32_t
AudioTrackPlayer::SampleRate() const
{
  if (IsValid()) {
    return mSampleRate;
  } else {
    return -1;
  }
}

int32_t
AudioTrackPlayer::Channels() const
{
  if (IsValid()) {
    return mChannels;
  } else {
    return -1;
  }
}

int32_t
AudioTrackPlayer::BitsPerSample() const
{
  if (IsValid()) {
    return mBitsPerSample;
  } else {
    return -1;
  }
}

int32_t
AudioTrackPlayer::Encoding() const
{
  if (IsValid()) {
    return mEncoding;
  } else {
    return -1;
  }
}

int32_t
AudioTrackPlayer::MaxInputSize() const
{
  if (IsValid()) {
    return mMaxInputSize;
  } else {
    return -1;
  }
}

void
AudioTrackPlayer::HandleMessage(int32_t what, int64_t arg, void *obj)
{
  // LOGV("handle msg %d", what);

  // TODO 大体V/A共通になるので、問題なければDecode::HandleMessage()に集約管理
  //      個別実装が必要になるメッセージのみ各自で実装する感じに

  switch (what) {
  case MSG_START:
    SetState(STATE_PLAY);
    Decode();
    mEventFlag.Set(EVENT_FLAG_PLAY_READY);
    break;

  case MSG_PAUSE:
    if (GetState() == STATE_PLAY) {
      SetState(STATE_PAUSE);
      Post(MSG_NOP, 0, nullptr, true); // 発行済メッセージを全フラッシュ
    }
    break;

  case MSG_RESUME:
    if (GetState() == STATE_PAUSE) {
      mClock->Reset();
      SetState(STATE_PLAY);
      Decode();
    }
    break;

  case MSG_SEEK:
    AMediaExtractor_seekTo(mExtractor, arg, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC);
    AMediaCodec_flush(mCodec);
    // TODO starttimeをV/A共通にしてるので、同期処理していない現状ではおかしくなるかも？
    mClock->Reset();
    mSawInputEOS  = false;
    mSawOutputEOS = false;
    break;

  case MSG_STOP:
    SetState(STATE_STOP);
    AMediaCodec_stop(mCodec);
    // AMediaCodec_delete(mCodec);
    Post(MSG_NOP, 0, nullptr, true); // 発行済メッセージを全フラッシュ
    mEventFlag.Set(EVENT_FLAG_STOPPED);
    break;

  default:
    TrackPlayer::HandleMessage(what, arg, obj);
    break;
  }

  std::this_thread::yield();
}

void
AudioTrackPlayer::HandleOutputData(ssize_t bufIdx, AMediaCodecBufferInfo &bufInfo,
                                   int32_t flags)
{
  if (bufInfo.size > 0) {
    // 出力バッファの内容をハンドラに返す
    if (mOnAudioDecoded != nullptr) {
      size_t bufSize;
      uint8_t *buf = AMediaCodec_getOutputBuffer(mCodec, bufIdx, &bufSize);
      mOnAudioDecoded(mOnAudioDecodedUserPtr, buf + bufInfo.offset, bufInfo.size);
      // output buffer を開放
      AMediaCodec_releaseOutputBuffer(mCodec, bufIdx, false);
    } else {
      // コールバックが設定されていない場合はbufIdxをキューに追加
      // バッファの解放はReadFromRingBufferで行う
      EnqueueBuffer(bufIdx, bufInfo.offset, bufInfo.size);
    }
  }

  // オーディオは片っ端から送って、先方で順次再生バッファに詰めてもらう形で
  // 問題ない？
#if 0 
  // このフレームのPTSをタイマーに反映
  int64_t mediaTimeUs = bufInfo.presentationTimeUs;
  if (!mClock->IsStarted()) {
    mClock->SetStartTime(mediaTimeUs);
  }
  mClock->SetCurrentMediaTime(mediaTimeUs);

  //LOGV("audio output: id: %zu  size: %d  pts: %lld flags: %d", bufidx, info.size, mediaTimeUs, info.flags);

  // PTSが未来の場合は反映を待ち合わせる
  int64_t renderDelay = mClock->CalcDelay(mediaTimeUs);
  if (renderDelay > 0) {
    // LOGV("render delay sleep: %lld us \n", renderDelay);
    std::this_thread::sleep_for(std::chrono::microseconds(renderDelay));
  }
#endif

}

void
AudioTrackPlayer::SetOnAudioDecoded(OnAudioDecoded func, void *userPtr)
{
  mOnAudioDecoded        = func;
  mOnAudioDecodedUserPtr = userPtr;
}
