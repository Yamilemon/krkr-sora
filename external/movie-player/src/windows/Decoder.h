#pragma once

#include "CommonUtils.h"
#include "Constants.h"
#include "MessageLooper.h"
#include "FramePacket.h"

#include <vpx/vpx_decoder.h>

#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

// -----------------------------------------------------------------------------
// DecodedBuffer
// -----------------------------------------------------------------------------
enum VdbPlaneIndex
{
  VDB_PLANE_PACKED = 0,
  VDB_PLANE_Y      = 0,
  VDB_PLANE_U      = 1,
  VDB_PLANE_V      = 2,
  VDB_PLANE_A      = 3,
  VDB_PLANE_COUNT
};

struct DecodedBuffer : public BufferQueueEntryBase
{
  // common
  TrackType type;
  int32_t bufIndex;
  uint64_t timeStampNs;
  int64_t frame;
  bool isEndOfStream;

  // type specific
  union
  {
    // type: video
    struct
    {
      PixelFormat format;
      ColorSpace colorSpace;
      ColorRange colorRange;

      int32_t width;
      int32_t height;
      int32_t displayWidth;
      int32_t displayHeight;

      uint8_t *planes[VDB_PLANE_COUNT]; // *data 内の各プレーン先頭ポインタ
      int32_t stride[VDB_PLANE_COUNT];  // 各プレーンのストライド
    } v;

    // type: audio
    struct
    {
      AudioFormat format;
      int32_t channels;
      int32_t sampleRate;
      int32_t samples; // バッファ内のサンプル数
    } a;
  };

  DecodedBuffer()
  : BufferQueueEntryBase()
  , type(TRACK_TYPE_UNKNOWN)
  {
    Init(-1);
  }
  virtual ~DecodedBuffer() {}

  TrackType Type() const { return type; }

  void InitAsEOS() { InitAsEOS(bufIndex); }
  void InitAsEOS(int32_t bufIdx)
  {
    Init(bufIdx);
    Resize(0);
    dataSize      = 0;
    isEndOfStream = true;
  }

  virtual void Clear() override { Init(bufIndex); }
  virtual void Init(int32_t bufIdx) override { InitByType(type, bufIdx); }

  void ClearByType(TrackType type) { InitByType(type, bufIndex); }
  void InitByType(TrackType _type, int32_t bufIdx)
  {
    type          = _type;
    bufIndex      = bufIdx;
    timeStampNs   = -1;
    frame         = -1;
    isEndOfStream = false;

    switch (type) {
    case TRACK_TYPE_UNKNOWN:
      // nothing to do.
      break;
    case TRACK_TYPE_VIDEO: {
      v.format     = PIXEL_FORMAT_UNKNOWN;
      v.colorSpace = COLOR_SPACE_UNKNOWN;
      v.colorRange = COLOR_RANGE_UNDEF;

      v.width         = 0;
      v.height        = 0;
      v.displayWidth  = 0;
      v.displayHeight = 0;

      for (size_t i = 0; i < VDB_PLANE_COUNT; i++) {
        v.planes[i] = nullptr;
        v.stride[i] = 0;
      }
    } break;
    case TRACK_TYPE_AUDIO: {
      a.channels   = 0;
      a.sampleRate = 0;
    } break;
    // case TRACK_TYPE_TEXT:     break;
    // case TRACK_TYPE_METADATA: break;
    default:
      INLINE_LOGE("BUG?: unknown decoded buffer type: %d\n", _type);
      break;
    }
    // INLINE_LOGE("clear: %d\n", type);
  }

  void CopyFrom(DecodedBuffer *from, bool copyBufIndex = true)
  {
    if (from->isEndOfStream) {
      InitAsEOS(from->bufIndex);
      return;
    }

    // データコピー
    BufferQueueEntryBase::CopyFrom(from);

    // 情報コピー
    type          = from->type;
    timeStampNs   = from->timeStampNs;
    frame         = from->frame;
    isEndOfStream = false;
    if (copyBufIndex) {
      bufIndex = from->bufIndex;
    }

    switch (type) {
    case TRACK_TYPE_UNKNOWN:
      // nothing to do.
      break;
    case TRACK_TYPE_VIDEO: {
      v.format     = from->v.format;
      v.colorSpace = from->v.colorSpace;
      v.colorRange = from->v.colorRange;

      v.width         = from->v.width;
      v.height        = from->v.height;
      v.displayWidth  = from->v.displayWidth;
      v.displayHeight = from->v.displayHeight;

      for (size_t i = 0; i < VDB_PLANE_COUNT; i++) {
        v.planes[i] = data + (from->v.planes[i] - from->data);
        v.stride[i] = from->v.stride[i];
      }
    } break;
    case TRACK_TYPE_AUDIO: {
      a.channels   = from->a.channels;
      a.sampleRate = from->a.sampleRate;
    } break;
    // case TRACK_TYPE_TEXT:     break;
    // case TRACK_TYPE_METADATA: break;
    default:
      INLINE_LOGE("BUG?: unknown decoded buffer type: %d\n", type);
      break;
    }
    // INLINE_LOGE("clear: %d\n", type);
  }
};

// -----------------------------------------------------------------------------
// Decoder
// -----------------------------------------------------------------------------
enum DecoderType
{
  DECODER_TYPE_UNKNOWN = TRACK_TYPE_UNKNOWN,
  DECODER_TYPE_VIDEO   = TRACK_TYPE_VIDEO,
  DECODER_TYPE_AUDIO   = TRACK_TYPE_AUDIO,
};

class Decoder : public MessageLooper
{
public:
  enum Message
  {
    MSG_FLUSH,
    MSG_INPUT_AVAILABLE,
    MSG_OUTPUT_AVAILABLE,
    MSG_QUIT,
  };

  struct Config
  {
    void Init(CodecId codecId);

    CodecId codecId;
    union
    {
      struct
      {
        PixelFormat rgbFormat;
        vpx_codec_dec_cfg_t decCfg;
        bool alphaMode;
      } vpx;
      struct
      {
        int32_t sampleRate;
        int32_t channels;
      } vorbis;
      struct
      {
        int32_t sampleRate;
        int32_t channels;
      } opus;
    };

    // CodecPrivateデータ。WebMでは実質的にVorbis専用。
    std::vector<std::vector<uint8_t>> privateData;
  };

public:
  static Decoder *CreateDecoder(CodecId codecId);

public:
  Decoder(CodecId codecId, DecoderType type);
  virtual ~Decoder();

  virtual DecoderType Type() const { return DECODER_TYPE_UNKNOWN; }

  virtual bool Configure(const Config &conf) = 0;
  virtual bool Done()                        = 0;

  virtual void Start();
  virtual void Stop();

  const CodecId GetCodecId() const { return mCodecId; }

  virtual const char *CodecName() const = 0;

  int32_t DequeueFramePacketIndex();
  FramePacket *GetFramePacket(int32_t bufIndex);
  bool QueueFramePacketIndex(int32_t bufIndex);

  int32_t DequeueDecodedBufferIndex();
  DecodedBuffer *GetDecodedBuffer(int32_t bufIndex);
  bool ReleaseDecodedBufferIndex(int32_t bufIndex);

  void Flush();
  virtual void FlushSync();
  virtual bool DecodeFrame(DecodedBuffer *dcBuf, FramePacket *packet) = 0;

  // MessageLooper
  virtual void HandleMessage(int32_t what, int64_t arg, void *data) override;

  uint64_t DecodedFrames() const { return mDecodedFrames; }

  bool IsInputEOS() const { return mIsInpuEOS; }
  void ResetInputEOS() { mIsInpuEOS = false; }

protected:
  void Decode();

  bool CommonDecodeArgCheck(DecodedBuffer *dcBuf, FramePacket *packet);
  void CommonDebugFrameInfo(FramePacket *packet);

protected:
  CodecId mCodecId;
  uint64_t mDecodedFrames;
  int32_t mPendingInputs;
  std::atomic_bool mIsInpuEOS;

  BufferQueue<FramePacket> mFramePackets;
  BufferQueue<DecodedBuffer> mDecodedBuffers;

  // 同期用イベントフラグ
  enum
  {
    EVENT_FLAG_FLUSH = 1 << 0,
  };
  EventFlag mEventFlag;
};

class VideoDecoder : public Decoder
{
public:
  VideoDecoder(CodecId codecId)
  : Decoder(codecId, DECODER_TYPE_VIDEO)
  {}
  virtual ~VideoDecoder() {}

  virtual DecoderType Type() const { return DECODER_TYPE_VIDEO; }

  // デコーダの出力ピクセルフォーマット
  virtual PixelFormat OutputPixelFormat() const = 0;
};

class AudioDecoder : public Decoder
{
public:
  AudioDecoder(CodecId codecId)
  : Decoder(codecId, DECODER_TYPE_AUDIO)
  {}
  virtual ~AudioDecoder() {}

  virtual DecoderType Type() const { return DECODER_TYPE_AUDIO; }

  // TODO 出力フォーマットは、S16決め打ちになっているので
  //      外部変更可能にする場合は初期化インタフェースを追加して対応のこと
  int32_t SampleRate() const { return mSampleRate; }
  int32_t Channels() const { return mChannels; }
  int32_t BitsPerSample() const { return sizeof(int16_t) * 8; }
  int32_t Encoding() const { return AUDIO_FORMAT_S16; }

protected:
  int32_t mSampleRate;
  int32_t mChannels;
};