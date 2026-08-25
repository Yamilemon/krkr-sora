#pragma once

#include "TrackPlayer.h"
#include <mutex>
#include <queue>

// -----------------------------------------------------------------------------
// オーディオトラックプレイヤ
// -----------------------------------------------------------------------------

// オーディオデコーダコールバック
typedef int32_t (*OnAudioDecoded)(void *userPtr, const uint8_t *data, size_t sizeBytes);

// バッファキューアイテム
struct AudioBufferItem {
  ssize_t bufIdx;         // AMediaCodecのバッファインデックス
  size_t offset;          // バッファ内のデータ開始オフセット
  size_t size;            // バッファのデータサイズ
  size_t readOffset;      // 読み出し済みオフセット
};

class AudioTrackPlayer : public TrackPlayer
{
public:
  AudioTrackPlayer(AMediaExtractor *ex, int32_t trackIndex, MediaClock *timer);
  virtual ~AudioTrackPlayer();

  void Init();
  void Done();

  virtual void HandleOutputData(ssize_t bufIdx, AMediaCodecBufferInfo &bufInfo,
                                int flags) override;

  int32_t SampleRate() const;
  int32_t Channels() const;
  int32_t BitsPerSample() const;
  int32_t Encoding() const;
  int32_t MaxInputSize() const;

  void SetOnAudioDecoded(OnAudioDecoded func, void *userPtr);

  // リングバッファから指定フレーム数を読み出す
  bool ReadFromRingBuffer(uint8_t* buffer, uint64_t frameCount, uint64_t* framesRead);

private:
  virtual void HandleMessage(int32_t what, int64_t arg, void *data) override;

private:
  // output 情報
  int32_t mChannels;
  int32_t mSampleRate;
  int32_t mBitsPerSample;
  int32_t mEncoding;
  int32_t mMaxInputSize;

  // オーディオコールバック
  OnAudioDecoded mOnAudioDecoded;
  void *mOnAudioDecodedUserPtr;

  // バッファキュー関連
  std::queue<AudioBufferItem> mBufferQueue;  // bufIdxのキュー
  std::mutex mBufferQueueMutex;

  // バッファをキューに追加
  void EnqueueBuffer(ssize_t bufIdx, size_t offset, size_t size);
  // キューに残っているバッファを全て解放
  void ReleaseAllBuffers();
};