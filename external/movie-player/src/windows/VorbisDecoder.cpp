#define MYLOG_TAG "VorbisDecoder"
#include "BasicLog.h"
#include "Constants.h"
#include "CommonUtils.h"
#include "VorbisDecoder.h"

// -----------------------------------------------------------------------------
// VorbisDecoder
// -----------------------------------------------------------------------------
VorbisDecoder::VorbisDecoder(CodecId codecId)
: AudioDecoder(codecId)
, mIsConfigured(false)
, mVorbisInfo({})
, mVorbisComment({})
, mVorbisDsp({})
, mVorbisBlock({})
{
  ASSERT(codecId == CODEC_A_VORBIS, "BUG: invalid codecId specified: codecId=%d\n",
         codecId);
}

VorbisDecoder::~VorbisDecoder()
{
  Done();
}

bool
VorbisDecoder::Configure(const Config &conf)
{
  vorbis_info_init(&mVorbisInfo);
  vorbis_comment_init(&mVorbisComment);

  mSampleRate = conf.vorbis.sampleRate;
  mChannels   = conf.vorbis.channels;

  // デコードバッファ。100ms で作っておき必要に応じて拡張。
  mDecodeBufSamples = mSampleRate * 0.1f;
  mDecodeBuf.resize(mDecodeBufSamples * mChannels, 0);

  // CodecPrivateに設定されているVorbisヘッダから初期化を行う
  if (conf.privateData.size() != 3) {
    LOGE("CodecPrivate data is invalid vorbis header.\n");
    return false;
  }

  int err = 0;
  for (int i = 0; i < conf.privateData.size(); i++) {
    ogg_packet oggPacket = { 0 };
    oggPacket.packet     = (unsigned char *)conf.privateData[i].data();
    oggPacket.bytes      = conf.privateData[i].size();
    oggPacket.b_o_s      = (i == 0);
    err = vorbis_synthesis_headerin(&mVorbisInfo, &mVorbisComment, &oggPacket);
    if (err != 0) {
      vorbis_comment_clear(&mVorbisComment);
      vorbis_info_clear(&mVorbisInfo);
      LOGE("invalid vorbis header: privateData index=%d\n", i);
      return false;
    }
  }

  err = vorbis_synthesis_init(&mVorbisDsp, &mVorbisInfo);
  if (err != 0) {
    LOGE("vorbis_synthesis_init() failed: err=%d\n", err);
    goto err;
  }

  err = vorbis_block_init(&mVorbisDsp, &mVorbisBlock);
  if (err != 0) {
    LOGE("vorbis_block_init() failed: err=%d\n", err);
    goto err;
  }

  // LOGV("codec initialized.\n");

  mIsConfigured = true;
  return true;

err:
  vorbis_block_clear(&mVorbisBlock);
  vorbis_dsp_clear(&mVorbisDsp);
  vorbis_info_clear(&mVorbisInfo);
  vorbis_comment_clear(&mVorbisComment);

  return false;
}

bool
VorbisDecoder::Done()
{
  if (mIsConfigured) {
    vorbis_block_clear(&mVorbisBlock);
    vorbis_dsp_clear(&mVorbisDsp);
    vorbis_info_clear(&mVorbisInfo);
    vorbis_comment_clear(&mVorbisComment);
    mIsConfigured = false;
  }
  return true;
}

bool
VorbisDecoder::DecodeFrame(DecodedBuffer *dcBuf, FramePacket *packet)
{
  if (!CommonDecodeArgCheck(dcBuf, packet)) {
    return false;
  }

#if defined(DEBUG_INFO_DECODER)
  CommonDebugFrameInfo(packet);
#endif

#if defined(DEBUG_PERF_DECODER)
  TimeMeasure tm("VorbisDecoder");
#endif

  ogg_packet oggPacket{};
  oggPacket.packet = packet->data;
  oggPacket.bytes  = packet->dataSize;

  int err = vorbis_synthesis(&mVorbisBlock, &oggPacket);
  if (err != 0) {
    LOGE("vorbis_synthesis() failed: err=0x%X\n", err);
    return false;
  }

  err = vorbis_synthesis_blockin(&mVorbisDsp, &mVorbisBlock);
  if (err != 0) {
    return false;
  }

  int samples, totalSamples = 0;
  float **pcm = nullptr;
  while ((samples = vorbis_synthesis_pcmout(&mVorbisDsp, &pcm)) > 0) {
    // デコードバッファを拡張
    totalSamples += samples;
    if (totalSamples > mDecodeBufSamples) {
      mDecodeBufSamples = totalSamples * 2;
      mDecodeBuf.resize(mDecodeBufSamples);
    }

    // floatデータをint16に変換する
    size_t n = 0;
    for (int i = 0; i < samples; i++) {
      for (int ch = 0; ch < mChannels; ch++) {
        int sample    = (int)(pcm[ch][i] * 32767.f);
        mDecodeBuf[n] = (int16_t)(std::max(-32768, std::min(sample, 32767)));
        n++;
      }
    }
    vorbis_synthesis_read(&mVorbisDsp, samples);
  }

#if defined(DEBUG_PERF_DECODER)
  tm.stop();
#endif

  // pcmが合成されないケースもあるので、合成された場合のみ出力する
  if (totalSamples > 0) {
    dcBuf->frame       = mDecodedFrames++;
    dcBuf->timeStampNs = packet->timeStampNs;

    dcBuf->a.format     = AUDIO_FORMAT_S16;
    dcBuf->a.channels   = mChannels;
    dcBuf->a.sampleRate = mSampleRate;
    dcBuf->a.samples    = totalSamples;

    // DecodedBufferに詰め直し
    size_t dataSize = totalSamples * mChannels * sizeof(int16_t);
    dcBuf->Resize(dataSize);
    dcBuf->dataSize = dataSize;
    memcpy(dcBuf->data, mDecodeBuf.data(), dataSize);
  } else {
    size_t dataSize = 0;
    dcBuf->Resize(dataSize);
    dcBuf->dataSize = dataSize;
  }

  return true;
}