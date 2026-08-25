#pragma once

#include "Decoder.h"

struct OpusDecoder;

// MEMO
// libopusのdecoder state structがOpusDecoderという名前のため
// ストレートに命名すると衝突するのでOpusのDecoderだけ冗長名にしてある
class OpusAudioDecoder : public AudioDecoder
{
public:
  OpusAudioDecoder(CodecId codecId);
  virtual ~OpusAudioDecoder();

  virtual bool Configure(const Config &conf) override;
  virtual bool Done() override;
  virtual bool DecodeFrame(DecodedBuffer *dcBuf, FramePacket *packet) override;

  virtual const char *CodecName() const { return "opus"; }

private:
  bool mIsConfigured;
  int32_t mDecodeBufSamples;
  std::vector<int16_t> mDecodeBuf;
  OpusDecoder *mOpusDecoder;
};