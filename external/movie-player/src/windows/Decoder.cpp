#define MYLOG_TAG "Decoder"
#include "BasicLog.h"
#include "CommonUtils.h"
#include "Decoder.h"

#include "VpxDecoder.h"
#include "VorbisDecoder.h"
#include "OpusDecoder.h"

// -----------------------------------------------------------------------------
// Decoder
// -----------------------------------------------------------------------------
Decoder *
Decoder::CreateDecoder(CodecId codecId)
{
  Decoder *decoder = nullptr;

  switch (codecId) {
  case CODEC_V_VP8:
  case CODEC_V_VP9:
    decoder = new VpxDecoder(codecId);
    break;
  case CODEC_V_AV1:
    break;
  case CODEC_A_VORBIS:
    decoder = new VorbisDecoder(codecId);
    break;
  case CODEC_A_OPUS:
    decoder = new OpusAudioDecoder(codecId);
    break;
  default:
    ASSERT(false, "CreateDecoder: unknown codec id: codecId=%d\n", codecId);
    break;
  }

  return decoder;
}

Decoder::Decoder(CodecId codecId, DecoderType type)
: mCodecId(codecId)
, mDecodedFrames(0)
, mPendingInputs(0)
, mIsInpuEOS(false)
{
  size_t qInSize  = 4;
  size_t qOutSize = 4;
  switch (type) {
  case DECODER_TYPE_VIDEO:
    qInSize  = 4;
    qOutSize = 4;
    break;
  case DECODER_TYPE_AUDIO:
    qInSize  = 16;
    qOutSize = 16;
    break;
  default:
    ASSERT(false, "unknown decoder type: type=%d\n", type);
    break;
  }
  mFramePackets.Init(qInSize);
  mDecodedBuffers.Init(qOutSize);
  std::vector<DecodedBuffer> &bufs = mDecodedBuffers.Buffers();
  for (size_t i = 0; i < bufs.size(); i++) {
    bufs[i].InitByType((TrackType)type, i);
  }
}

void
Decoder::Config::Init(CodecId _codecId)
{
  codecId = _codecId;
  switch (codecId) {
  case CODEC_V_VP8:
  case CODEC_V_VP9:
    vpx.rgbFormat = PIXEL_FORMAT_UNKNOWN;
    break;
  case CODEC_V_AV1:
    break;
  case CODEC_A_VORBIS:
    vorbis = { 0 };
    break;
  case CODEC_A_OPUS:
    opus = { 0 };
    break;
  default:
    ASSERT(false, "Config::Init: unknown codec id: codecId=%d\n", codecId);
    break;
  }
}

Decoder::~Decoder() {}

void
Decoder::FlushSync()
{
  Flush();
  std::this_thread::yield();
  mEventFlag.Wait(EVENT_FLAG_FLUSH);
}

void
Decoder::Flush()
{
  // LOGV("Decoder::Flush()\n");

  Post(MSG_FLUSH);
}

void
Decoder::Start()
{
  StartThread();
}

void
Decoder::Stop()
{
  StopThread();
}

// デコーダ系共通の引数チェック処理
bool
Decoder::CommonDecodeArgCheck(DecodedBuffer *dcBuf, FramePacket *packet)
{
  if (packet == nullptr) {
    LOGE("invalid frame packet.\n");
    return false;
  }

  if (packet->Type() != (TrackType)Type()) {
    LOGE("invalid frame packet type: %d (must be %d)\n", packet->Type(), Type());
    return false;
  }

  if (dcBuf == nullptr) {
    LOGE("invalid decoded buffer.\n");
    return false;
  }

  if (dcBuf->Type() != (TrackType)Type()) {
    LOGE("invalid decoder buffer type: %d (must be %d)\n", dcBuf->Type(), Type());
    return false;
  }

  // EOSバッファは特殊バッファで個別のデコーダには回されないはずなので
  // ここに来てしまったらバグなのでASSERTで捕まえておく
  if (packet->isEndOfStream || dcBuf->isEndOfStream) {
    ASSERT(false, "BUG: EOS buffer input.\n");
    return false;
  }

  return true;
}

// デコーダ系共通のデバッグINFO
void
Decoder::CommonDebugFrameInfo(FramePacket *packet)
{
  LOGV("DecodeFrame[%s]: size=%6zu, time=%12" PRIu64 "\n", CodecName(), packet->dataSize,
       packet->timeStampNs);
}

void
Decoder::Decode()
{
  if (mIsInpuEOS) {
    // LOGV("Decoder::Decode: %s: Input reached EOS\n", CodecName());
    return;
  }

  // 出力バッファをチェック
  int32_t outputAvailables = mDecodedBuffers.SizeForWriter();
  if (outputAvailables == 0) {
    // 出力バッファの空きがないので入力パケットを消化せずに返る
    mPendingInputs++;
    return;
  }

  // 入力待ちの数と出力スロットの空き数の小さい方の回数デコーダを回す
  int32_t inputAvailables = mFramePackets.SizeForReader();
  int32_t targetCount     = std::min(outputAvailables, inputAvailables);
  for (int32_t i = 0; i < targetCount; i++) {
    int32_t packetIndex = mFramePackets.DequeueIndexForReader();
    if (packetIndex >= 0) {
      // LOGV("*** Decode - deq read buf index = %d\n", packetIndex);

      FramePacket *packet = mFramePackets.GetBuffer(packetIndex);
      ASSERT(packet != nullptr, "BUG?: invalid packet\n");

      size_t dcBufIndex    = mDecodedBuffers.DequeueIndexForWriter();
      DecodedBuffer *dcBuf = mDecodedBuffers.GetBuffer(dcBufIndex);
      // LOGV("w deq: %d\n", dcBufIndex);
      if (dcBuf) {
        if (packet->isEndOfStream) {
          // LOGV("input packet reached EOS\n");
          mIsInpuEOS = true;
          dcBuf->InitAsEOS(dcBufIndex);
          // LOGV("r enq: %d\n", dcBufIndex);
        } else {
          bool decodeSuccess = DecodeFrame(dcBuf, packet);
          ASSERT(decodeSuccess, "BUG?: decode failed.\n");
          // LOGV("r enq: %d\n", dcBufIndex);
        }

        if ((dcBuf->data && dcBuf->dataSize > 0) || dcBuf->isEndOfStream) {
          // デコード結果をリーダーバッファへ追加。EOSも対象。
          mDecodedBuffers.EnqueueBufferIndexForReader(dcBufIndex);
          // LOGV("enqueue decode: %d\n", dcBufIndex);
        } else {
          // デコード結果がない場合はデコードバッファを即時開放(Vorbisでこのケースが発生する)
          mDecodedBuffers.EnqueueBufferIndexForWriter(dcBufIndex);
        }
        mFramePackets.EnqueueBufferIndexForWriter(packetIndex);
        // LOGV("*** Decode - enq write buf index = %d\n", packetIndex);
      } else {
        // ロジック的にここでは必ず出力indexは取得できるはず
        LOGV("BUG?: valid frame packet must get here.\n");
      }
      mPendingInputs--;
    } else {
      // ロジック的にここでは必ず入力indexは取得できるはず
      LOGV("BUG?: valid frame packet must get here.\n");
    }
  }
}

void
Decoder::HandleMessage(int32_t what, int64_t arg, void *obj)
{
  // LOGV("handle msg %d\n", what);

  switch (what) {
  case MSG_INPUT_AVAILABLE:
    Decode();
    break;

  case MSG_OUTPUT_AVAILABLE:
    Decode();
    break;

  case MSG_FLUSH:
    mFramePackets.Clear();
    mDecodedBuffers.Clear();
    mIsInpuEOS     = false;
    mDecodedFrames = 0;
    mPendingInputs = 0;
    mEventFlag.Set(EVENT_FLAG_FLUSH);
    break;

  case MSG_QUIT:
    QuitLoop();
    break;

  default:
    ASSERT(false, "unknown message type: %d\n", what);
    break;
  }

  std::this_thread::yield();
}

int32_t
Decoder::DequeueFramePacketIndex()
{
  return mFramePackets.DequeueIndexForWriter();
}

FramePacket *
Decoder::GetFramePacket(int32_t bufIndex)
{
  return mFramePackets.GetBuffer(bufIndex);
}

bool
Decoder::QueueFramePacketIndex(int32_t bufIndex)
{
  mFramePackets.EnqueueBufferIndexForReader(bufIndex);
  Post(MSG_INPUT_AVAILABLE);
  return true;
}

int32_t
Decoder::DequeueDecodedBufferIndex()
{
  return mDecodedBuffers.DequeueIndexForReader();
}

DecodedBuffer *
Decoder::GetDecodedBuffer(int32_t bufIndex)
{
  return mDecodedBuffers.GetBuffer(bufIndex);
}

bool
Decoder::ReleaseDecodedBufferIndex(int32_t bufIndex)
{
  // LOGV("w enq: %d\n", bufIndex);
  mDecodedBuffers.EnqueueBufferIndexForWriter(bufIndex);
  Post(MSG_OUTPUT_AVAILABLE);
  return true;
}
