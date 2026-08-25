#pragma once

#include "Decoder.h"

#include <vpx/vpx_decoder.h>

class VpxDecoder : public VideoDecoder
{
public:
  enum VpxType
  {
    VP8,
    VP9,
  };

public:
  VpxDecoder(CodecId codecId);
  virtual ~VpxDecoder();

  virtual bool Configure(const Config &conf) override;
  virtual bool Done() override;
  virtual bool DecodeFrame(DecodedBuffer *dcBuf, FramePacket *packet) override;

  virtual const char *CodecName() const;

  virtual PixelFormat OutputPixelFormat() const;

private:
  void CodecErrorMessage(const char *msg);
  void CopyToDecodedBuffer(DecodedBuffer *vdcBuf, uint64_t time, vpx_image *vpxImg,
                           vpx_image *vpxImgAlpha = nullptr);

private:
  bool mIsConfigured;
  bool mIsAlphaConfigured;
  PixelFormat mRgbFormat;

  vpx_codec_ctx_t mCodec;
  vpx_codec_iface_t *mIface;
  vpx_codec_flags_t mFlags;

  bool mAlphaMode;
  vpx_codec_ctx_t mAlphaCodec;
};