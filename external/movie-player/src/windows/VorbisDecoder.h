#pragma once

#include "Decoder.h"

#include <vorbis/codec.h>

class VorbisDecoder : public AudioDecoder
{
public:
  VorbisDecoder(CodecId codecId);
  virtual ~VorbisDecoder();

  virtual bool Configure(const Config &conf) override;
  virtual bool Done() override;
  virtual bool DecodeFrame(DecodedBuffer *dcBuf, FramePacket *packet) override;

  virtual const char *CodecName() const { return "vorbis"; }

private:
  bool mIsConfigured;
  int32_t mDecodeBufSamples;
  std::vector<int16_t> mDecodeBuf;

  vorbis_info mVorbisInfo;
  vorbis_comment mVorbisComment;
  vorbis_dsp_state mVorbisDsp;
  vorbis_block mVorbisBlock;
  int64_t mPacketCount;
};