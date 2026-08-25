#define MYLOG_TAG "VpxDecoder"
#include "BasicLog.h"
#include "Constants.h"
#include "CommonUtils.h"
#include "VpxDecoder.h"
#include "PixelConvert.h"

#include <vpx/vp8dx.h>

// -----------------------------------------------------------------------------
// VpxDecoder
// -----------------------------------------------------------------------------
VpxDecoder::VpxDecoder(CodecId codecId)
: VideoDecoder(codecId)
, mIsConfigured(false)
, mIsAlphaConfigured(false)
, mRgbFormat(PIXEL_FORMAT_UNKNOWN)
, mIface(nullptr)
, mFlags(0)
, mAlphaMode(false)
{
  switch (codecId) {
  case CODEC_V_VP8:
    mIface = &vpx_codec_vp8_dx_algo;
    break;
  case CODEC_V_VP9:
    mIface = &vpx_codec_vp9_dx_algo;
    break;
  default:
    ASSERT(false, "BUG: no vpx codecId specified: codecId=%d\n", codecId);
    break;
  }
}

VpxDecoder::~VpxDecoder()
{
  Done();
}

const char *
VpxDecoder::CodecName() const
{
  switch (mCodecId) {
  case CODEC_V_VP8:
    return "vp8";
  case CODEC_V_VP9:
    return "vp9";
  default:
    return "vpx[unknown]";
  }
}

void
VpxDecoder::CodecErrorMessage(const char *msg)
{
  const char *err = "decoder is not configured yet";
  if (mIsConfigured) {
    err = vpx_codec_error(&mCodec);
  }
  LOGV("%s: %s\n", msg, err);
}

bool
VpxDecoder::Configure(const Config &conf)
{
  mAlphaMode = conf.vpx.alphaMode;
  if (vpx_codec_dec_init(&mCodec, mIface, &conf.vpx.decCfg, mFlags)) {
    CodecErrorMessage("failed to configure vpx decoder");
    return false;
  }
  mIsConfigured = true;
  mRgbFormat = conf.vpx.rgbFormat;

  ASSERT(mRgbFormat == PIXEL_FORMAT_UNKNOWN || is_rgb_pixel_format(mRgbFormat),
         "config:rgbFormat must be rgb pixel format\n");

  if (mAlphaMode) {
    // conf.vpx.rgbFormat = ;
    if (vpx_codec_dec_init(&mAlphaCodec, mIface, &conf.vpx.decCfg, mFlags)) {
      CodecErrorMessage("failed to configure vpx alpha decoder");
      Done();
      return false;
    }
    mIsAlphaConfigured = true;
  }

  return true;
}

bool
VpxDecoder::Done()
{
  if (mIsConfigured) {
    if (vpx_codec_destroy(&mCodec)) {
      CodecErrorMessage("failed to destroy codec");
    }
    mIsConfigured = false;
  }
  if (mIsAlphaConfigured) {
    if (vpx_codec_destroy(&mAlphaCodec)) {
      CodecErrorMessage("failed to destroy codec");
    }
    mIsAlphaConfigured = false;
  }
  return true;
}

PixelFormat
VpxDecoder::OutputPixelFormat() const
{
  if (mRgbFormat == PIXEL_FORMAT_UNKNOWN) {
    // VPxデコーダの生出力は常にI420
    return PIXEL_FORMAT_I420;
  } else {
    return mRgbFormat;
  }
}

bool
VpxDecoder::DecodeFrame(DecodedBuffer *dcBuf, FramePacket *packet)
{
  if (!CommonDecodeArgCheck(dcBuf, packet)) {
    return false;
  }

#if defined(DEBUG_INFO_DECODER)
  CommonDebugFrameInfo(packet);
#endif

#if defined(DEBUG_PERF_DECODER)
  TimeMeasure tm("VPXDecoder");
#endif

  long deadline       = 0;
  vpx_codec_err_t err = VPX_CODEC_OK;
  err = vpx_codec_decode(&mCodec, packet->data, packet->dataSize, nullptr, deadline);
  if (err) {
    CodecErrorMessage("VPx: failed to decode frame");
    return false;
  }

  if (mAlphaMode && packet->adddataSize > 0) {
    err = vpx_codec_decode(&mAlphaCodec, packet->adddata, packet->adddataSize, nullptr,
                           deadline);
    if (err) {
      CodecErrorMessage("VPx: failed to decode frame");
      return false;
    }
  }

#if defined(DEBUG_PERF_DECODER)
  tm.stop();
#endif

  vpx_codec_iter_t iter = nullptr;
  vpx_image *img        = vpx_codec_get_frame(&mCodec, &iter);
  if (img != nullptr) {
    if (mAlphaMode) {
      vpx_codec_iter_t iter_alpha = nullptr;
      vpx_image *img_alpha        = vpx_codec_get_frame(&mAlphaCodec, &iter_alpha);
      CopyToDecodedBuffer(dcBuf, packet->timeStampNs, img, img_alpha);
    } else {
      CopyToDecodedBuffer(dcBuf, packet->timeStampNs, img);
    }
    mDecodedFrames++;
  }

  return true;
}

void
VpxDecoder::CopyToDecodedBuffer(DecodedBuffer *dcBuf, uint64_t time, vpx_image *vpxImg,
                                vpx_image *vpxImgAlpha)
{
  ASSERT(vpxImg != nullptr, "invalid image data.\n");
  ASSERT(dcBuf != nullptr, "invalid decoded buffer.\n");

  bool swapUv = false;
  PixelFormat srcFormat = PIXEL_FORMAT_I420;

  switch (vpxImg->fmt) {
  case VPX_IMG_FMT_YV12:
    // YV12はI420のUVを入れ替えたもの
    swapUv = true;
    srcFormat = PIXEL_FORMAT_I420;
    break;
  case VPX_IMG_FMT_I420:
    srcFormat = PIXEL_FORMAT_I420;
    break;
  case VPX_IMG_FMT_I422:
    srcFormat = PIXEL_FORMAT_I422;
    break;
  case VPX_IMG_FMT_I444:
    srcFormat = PIXEL_FORMAT_I444;
    break;
  // 未サポートリスト。必要に応じて適宜対応のこと
  // case VPX_IMG_FMT_RGB24: break;
  // case VPX_IMG_FMT_RGB32: break;
  // case VPX_IMG_FMT_RGB565: break;
  // case VPX_IMG_FMT_RGB555: break;
  // case VPX_IMG_FMT_UYVY: break;
  // case VPX_IMG_FMT_YUY2: break;
  // case VPX_IMG_FMT_YVYU: break;
  // case VPX_IMG_FMT_BGR24: break;
  // case VPX_IMG_FMT_RGB32_LE: break;
  // case VPX_IMG_FMT_ARGB: break;
  // case VPX_IMG_FMT_ARGB_LE: break;
  // case VPX_IMG_FMT_RGB565_LE: break;
  // case VPX_IMG_FMT_RGB555_LE: break;
  // case VPX_IMG_FMT_VPXYV12: break;
  // case VPX_IMG_FMT_VPXI420: break;
  // case VPX_IMG_FMT_444A: break;
  // case VPX_IMG_FMT_I42016: break;
  // case VPX_IMG_FMT_I42216: break;
  // case VPX_IMG_FMT_I44416: break;
  default:
    LOGE("unspported vpx image format: fmt=%d\n", vpxImg->fmt);
    return;
  }

  int uIndex  = swapUv ? VPX_PLANE_V : VPX_PLANE_U;
  int vIndex  = swapUv ? VPX_PLANE_U : VPX_PLANE_V;

  // src/dst params
  const uint8_t *yBuf   = vpxImg->planes[VPX_PLANE_Y];
  const uint8_t *uBuf   = vpxImg->planes[uIndex];
  const uint8_t *vBuf   = vpxImg->planes[vIndex];
  const uint8_t *aBuf   = vpxImgAlpha ? vpxImgAlpha->planes[VPX_PLANE_Y]
                                      : vpxImg->planes[VPX_PLANE_ALPHA];
  int32_t yStride       = vpxImg->stride[VPX_PLANE_Y];
  int32_t uStride       = vpxImg->stride[uIndex];
  int32_t vStride       = vpxImg->stride[vIndex];
  int32_t aStride       = vpxImgAlpha ? vpxImgAlpha->stride[VPX_PLANE_Y]
                                      : vpxImg->stride[VPX_PLANE_ALPHA];

  // 一応想定外のケースをチェック
  ASSERT(uStride == vStride, "illegale u/v stride. unknown format?\n");

  // color
  ColorRange colorRange = (vpxImg->range == VPX_CR_STUDIO_RANGE) ? COLOR_RANGE_LIMITED
                                                                 : COLOR_RANGE_FULL;
  ColorSpace colorSpace = COLOR_SPACE_UNKNOWN;
  switch (vpxImg->cs) {
  case VPX_CS_UNKNOWN:
  case VPX_CS_RESERVED:
    colorSpace = COLOR_SPACE_UNKNOWN;
    break;
  case VPX_CS_BT_601:
    colorSpace = COLOR_SPACE_BT_601;
    break;
  case VPX_CS_BT_709:
    colorSpace = COLOR_SPACE_BT_709;
    break;
  case VPX_CS_SMPTE_170:
    colorSpace = COLOR_SPACE_SMPTE_170;
    break;
  case VPX_CS_SMPTE_240:
    colorSpace = COLOR_SPACE_SMPTE_240;
    break;
  case VPX_CS_BT_2020:
    colorSpace = COLOR_SPACE_BT_2020;
    break;
  case VPX_CS_SRGB:
    colorSpace = COLOR_SPACE_SRGB;
    break;
  }

  // カウント/タイミング情報
  dcBuf->frame       = mDecodedFrames;
  dcBuf->timeStampNs = time;

  // 共通情報
  dcBuf->v.colorSpace = colorSpace;
  dcBuf->v.colorRange = colorRange;

  if (is_rgb_pixel_format(mRgbFormat)) {
    // ここでRGB変換を行って格納
    int32_t dstStride = vpxImg->d_w * 4;
    int32_t width     = vpxImg->d_w;
    int32_t height    = vpxImg->d_h;

    // ピクセルフォーマット
    dcBuf->v.format = mRgbFormat;

    // データ/表示領域情報
    // RGB変換時にvpxのパディング情報を全部落とすので、データ領域=表示領域
    dcBuf->v.width         = width;
    dcBuf->v.height        = height;
    dcBuf->v.displayWidth  = width;
    dcBuf->v.displayHeight = height;

    // ストライド
    dcBuf->v.stride[VDB_PLANE_PACKED] = width * height * 4;
    dcBuf->v.stride[VDB_PLANE_U]      = 0;
    dcBuf->v.stride[VDB_PLANE_V]      = 0;
    dcBuf->v.stride[VDB_PLANE_A]      = 0;

    // 出力バッファにデータサイズを設定して必要ならリサイズさせる
    size_t dataSize = width * height * 4;
    dcBuf->Resize(dataSize);
    dcBuf->dataSize = dataSize;

    // *data 各所のポインタをplanesに保持。
    // 現状ではyuvaのA-planeは無視される(A=255固定になる)
    // (libyuvにI420AlphaToARGBがあるので、将来的にはライブラリ側で対応可能か)
    dcBuf->v.planes[VDB_PLANE_PACKED] = dcBuf->data;
    dcBuf->v.planes[VDB_PLANE_U]      = nullptr;
    dcBuf->v.planes[VDB_PLANE_V]      = nullptr;
    dcBuf->v.planes[VDB_PLANE_A]      = nullptr;

    // 変換コピー
    convert_yuv_to_rgb32(dcBuf->data, dstStride, mRgbFormat, yBuf, uBuf, vBuf, aBuf,
                         yStride, uStride, aStride, width, height, srcFormat, colorSpace,
                         colorRange);
  } else {
    // YUVフォーマットのまま格納
    // ピクセルフォーマット
    dcBuf->v.format = PIXEL_FORMAT_I420;

    // データ/表示領域情報
    // mempyでベタコピーするために、widthはvpxImgそのままを使い
    // heightは末尾部分のパディングを無視できるので切り詰める。
    dcBuf->v.width         = vpxImg->w;
    dcBuf->v.height        = vpxImg->d_h;
    dcBuf->v.displayWidth  = vpxImg->d_w;
    dcBuf->v.displayHeight = vpxImg->d_h;

    // ストライド
    dcBuf->v.stride[VDB_PLANE_Y] = yStride;
    dcBuf->v.stride[VDB_PLANE_U] = uStride;
    dcBuf->v.stride[VDB_PLANE_V] = vStride;
    dcBuf->v.stride[VDB_PLANE_A] = aStride;

    // データサイズ
    int32_t ySize = yStride * dcBuf->v.height;
    int32_t uSize = uStride * dcBuf->v.height / 2;
    int32_t vSize = vStride * dcBuf->v.height / 2;
    int32_t aSize = aStride * dcBuf->v.height;

    // 出力バッファにデータサイズを設定して必要ならリサイズさせる
    bool hasAlpha   = (aBuf != nullptr);
    size_t dataSize = ySize + uSize + vSize + (hasAlpha ? aSize : 0);
    dcBuf->Resize(dataSize);
    dcBuf->dataSize = dataSize;

    // *data 各所のポインタをplanesに保持。
    dcBuf->v.planes[VDB_PLANE_Y] = dcBuf->data;
    dcBuf->v.planes[VDB_PLANE_U] = dcBuf->v.planes[VDB_PLANE_Y] + ySize;
    dcBuf->v.planes[VDB_PLANE_V] = dcBuf->v.planes[VDB_PLANE_U] + uSize;
    dcBuf->v.planes[VDB_PLANE_A] =
      hasAlpha ? dcBuf->v.planes[VDB_PLANE_V] + vSize : nullptr;

    // データをコピー
    memcpy(dcBuf->v.planes[VDB_PLANE_Y], yBuf, ySize);
    memcpy(dcBuf->v.planes[VDB_PLANE_U], uBuf, uSize);
    memcpy(dcBuf->v.planes[VDB_PLANE_V], vBuf, vSize);
    if (hasAlpha) {
      memcpy(dcBuf->v.planes[VDB_PLANE_A], aBuf, aSize);
    }
  }
}
