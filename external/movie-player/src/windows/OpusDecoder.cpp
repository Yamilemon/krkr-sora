#define MYLOG_TAG "OpusAudioDecoder"
#include "BasicLog.h"
#include "Constants.h"
#include "CommonUtils.h"
#include "OpusDecoder.h"

#include <opus/opus.h>

// -----------------------------------------------------------------------------
// OpusAudioDecoder
// (libopusのOpusDecoderと名前衝突するので、冗長な名前になっている)
// -----------------------------------------------------------------------------
OpusAudioDecoder::OpusAudioDecoder(CodecId codecId)
: AudioDecoder(codecId)
, mIsConfigured(false)
, mOpusDecoder(nullptr)
{
  ASSERT(codecId == CODEC_A_OPUS, "BUG: invalid codecId specified: codecId=%d\n",
         codecId);
}

OpusAudioDecoder::~OpusAudioDecoder()
{
  Done();
}

bool
OpusAudioDecoder::Configure(const Config &conf)
{
  mSampleRate = conf.opus.sampleRate;
  mChannels   = conf.opus.channels;

  int err      = OPUS_OK;
  mOpusDecoder = opus_decoder_create(mSampleRate, mChannels, &err);
  if (err == OPUS_OK) {
    // デコードバッファ。仕様より120ms。
    mDecodeBufSamples = mSampleRate * 0.12f;
    mDecodeBuf.resize(mDecodeBufSamples * mChannels, 0);
  }

  mIsConfigured = (err == OPUS_OK);
  return mIsConfigured;
}

bool
OpusAudioDecoder::Done()
{
  if (mOpusDecoder) {
    opus_decoder_destroy(mOpusDecoder);
    mOpusDecoder = nullptr;
  }

  mIsConfigured = false;
  return true;
}

bool
OpusAudioDecoder::DecodeFrame(DecodedBuffer *dcBuf, FramePacket *packet)
{
  if (!CommonDecodeArgCheck(dcBuf, packet)) {
    return false;
  }

#if defined(DEBUG_INFO_DECODER)
  CommonDebugFrameInfo(packet);
#endif

#if defined(DEBUG_PERF_DECODER)
  TimeMeasure tm("OpusDecoder");
#endif

  int samples = opus_decode(mOpusDecoder, packet->data, packet->dataSize,
                            mDecodeBuf.data(), mDecodeBufSamples, 0);
  if (samples < 0) {
    LOGE("opus_decode() failed: err=0x%X\n", samples);
    return false;
  }

#if defined(DEBUG_PERF_DECODER)
  tm.stop();
#endif

  dcBuf->frame       = mDecodedFrames++;
  dcBuf->timeStampNs = packet->timeStampNs;

  dcBuf->a.format     = AUDIO_FORMAT_S16;
  dcBuf->a.channels   = mChannels;
  dcBuf->a.sampleRate = mSampleRate;
  dcBuf->a.samples    = samples;

  // DecodedBufferに詰め直し
  size_t dataSize = samples * mChannels * sizeof(int16_t);
  dcBuf->Resize(dataSize);
  dcBuf->dataSize = dataSize;
  memcpy(dcBuf->data, mDecodeBuf.data(), dataSize);

  return true;
}