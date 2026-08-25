
#define MYLOG_TAG "PixelConvert"
#include "BasicLog.h"
#include "PixelConvert.h"

#include "libyuv.h"

#include <utility>
#include <chrono>

// YUVピクセルをRGBピクセルに変換する
// NV系フォーマットを入力する場合は uBuf に uv データへのポインタを入れてください。
// vBuf の内容は無視されます。
bool
convert_yuv_to_rgb32(uint8_t *dst, int32_t dstStride, PixelFormat dstFormat,
                     const uint8_t *yBuf, const uint8_t *uBuf, const uint8_t *vBuf,
                     const uint8_t *aBuf, int32_t yStride, int32_t uvStride,
                     int32_t aStride, int32_t width, int32_t height,
                     PixelFormat srcFormat, ColorSpace colorSpace, ColorRange colorRange)
{
  if (dst == nullptr) {
    LOGE("invalid destination.\n");
    return false;
  }

  // 未対応フォーマットを弾いておく
  if (!is_yuv_pixel_format(srcFormat)) {
    ASSERT(false, "source pixel format must be YUV.\n");
    return false;
  }

  if (!is_rgb_pixel_format(dstFormat)) {
    ASSERT(false, "source pixel format must be RGB.\n");
    return false;
  }

  // color space matrix
  const struct libyuv::YuvConstants *yuvConstants;
  yuvConstants   = &libyuv::kYuvI601Constants;
  bool isLimited = (colorRange == COLOR_RANGE_LIMITED);
  switch (colorSpace) {
  case COLOR_SPACE_BT_2020:
    yuvConstants = isLimited ? &libyuv::kYuv2020Constants : &libyuv::kYuvV2020Constants;
  case COLOR_SPACE_BT_709:
    yuvConstants = isLimited ? &libyuv::kYuvH709Constants : &libyuv::kYuvF709Constants;
    break;
  default:
    // ここで対応しているもの以外は libyuvに定義が無いので未サポートとして
    // BT.601 にフォールバックして対応する。一応DEBUGビルドでは捕まえておいて
    // データを作り直してもらうような形にしておく。
    ASSERT(false, "unsupported color space. please remake video."
                  "acceptable color space is BT.601, 709 or 2020.\n");
    // FALLTHROUGH
  case COLOR_SPACE_UNKNOWN: // unknown は多分csに無意識なデータで出てくるので601で通す
  case COLOR_SPACE_BT_601:
    yuvConstants = isLimited ? &libyuv::kYuvI601Constants : &libyuv::kYuvJPEGConstants;
    break;
    case COLOR_SPACE_IDENTITY:
    break;
  }

  using YuvAlphaConvFunc = decltype(libyuv::I420AlphaToARGBMatrix);
  using YuvConvFunc      = decltype(libyuv::I420ToARGBMatrix);
  using NvConvFunc       = decltype(libyuv::NV12ToARGBMatrix);

  YuvAlphaConvFunc *yuvAlphaConvert = nullptr;
  YuvConvFunc *yuvConvert           = nullptr;
  NvConvFunc *nvConvert             = nullptr;

  switch (srcFormat) {
  case PIXEL_FORMAT_I420:
    switch (dstFormat) {
    case PIXEL_FORMAT_ARGB:
      std::swap(uBuf, vBuf);
      yuvConvert = libyuv::I420ToRGBAMatrix;
      break;
    case PIXEL_FORMAT_ABGR:
      yuvConvert = libyuv::I420ToRGBAMatrix;
      break;
    case PIXEL_FORMAT_RGBA:
      std::swap(uBuf, vBuf);
      if (!aBuf) {
        yuvConvert = libyuv::I420ToARGBMatrix;
      } else {
        yuvAlphaConvert = libyuv::I420AlphaToARGBMatrix;
      }
      break;
    case PIXEL_FORMAT_BGRA:
      if (!aBuf) {
        yuvConvert = libyuv::I420ToARGBMatrix;
      } else {
        yuvAlphaConvert = libyuv::I420AlphaToARGBMatrix;
      }
      break;
    default:
      break;
    }
    break;
  case PIXEL_FORMAT_I422:
    switch (dstFormat) {
    case PIXEL_FORMAT_ARGB:
      std::swap(uBuf, vBuf);
      yuvConvert = libyuv::I422ToRGBAMatrix;
      break;
    case PIXEL_FORMAT_ABGR:
      yuvConvert = libyuv::I422ToRGBAMatrix;
      break;
    case PIXEL_FORMAT_RGBA:
      std::swap(uBuf, vBuf);
      if (!aBuf) {
        yuvConvert = libyuv::I422ToARGBMatrix;
      } else {
        yuvAlphaConvert = libyuv::I422AlphaToARGBMatrix;
      }
      break;
    case PIXEL_FORMAT_BGRA:
      if (!aBuf) {
        yuvConvert = libyuv::I422ToARGBMatrix;
      } else {
        yuvAlphaConvert = libyuv::I422AlphaToARGBMatrix;
      }
      break;
    default:
      break;
    }
    break;
  case PIXEL_FORMAT_I444:
    if (colorSpace == COLOR_SPACE_IDENTITY) {
      // TODO Y/U/VプレーンにR/G/Bが入っている状態なので単純にgatherして詰め込んで返す
      return true;
    }
    switch (dstFormat) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_ABGR:
      // TODO libyuv に機能がない。必要なら自前でswapするか別のライブラリを探すこと。
      break;
    case PIXEL_FORMAT_RGBA:
      std::swap(uBuf, vBuf);
      yuvConvert = libyuv::I444ToARGBMatrix;
      break;
    case PIXEL_FORMAT_BGRA:
      yuvConvert = libyuv::I444ToARGBMatrix;
      break;
    default:
      break;
    }
    break;
  case PIXEL_FORMAT_NV12:
    switch (dstFormat) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_ABGR:
      // TODO libyuv に機能がない。必要なら自前でswapするか別のライブラリを探すこと。
      break;
    case PIXEL_FORMAT_RGBA:
      nvConvert = libyuv::NV21ToARGBMatrix;
      break;
    case PIXEL_FORMAT_BGRA:
      nvConvert = libyuv::NV12ToARGBMatrix;
      break;
    default:
      break;
    }
    break;
  case PIXEL_FORMAT_NV21:
    switch (dstFormat) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_ABGR:
      // TODO libyuv に機能がない。必要なら自前でswapするか別のライブラリを探すこと。
      break;
    case PIXEL_FORMAT_RGBA:
      nvConvert = libyuv::NV12ToARGBMatrix;
      break;
    case PIXEL_FORMAT_BGRA:
      nvConvert = libyuv::NV21ToARGBMatrix;
      break;
    default:
      break;
    }
    break;
  default:
    ASSERT(false, "unsupported src image format.\n");
    return false;
    break;
  }

// #define PERF_CHECK
#if defined(DEBUG_PERF_PIXELCONV)
  using namespace std::chrono;
  high_resolution_clock::time_point begin = high_resolution_clock::now();
#endif

  if (nvConvert) {
    nvConvert(yBuf, yStride, uBuf, uvStride, dst, dstStride, yuvConstants, width, height);

  } else if (yuvAlphaConvert) {
    yuvAlphaConvert(yBuf, yStride, uBuf, uvStride, vBuf, uvStride, aBuf, aStride, dst,
                    dstStride, yuvConstants, width, height, 0);

  } else {
    yuvConvert(yBuf, yStride, uBuf, uvStride, vBuf, uvStride, dst, dstStride,
               yuvConstants, width, height);
  }

#if defined(DEBUG_PERF_PIXELCONV)
  high_resolution_clock::time_point end = high_resolution_clock::now();
  microseconds elapsed                  = duration_cast<microseconds>(end - begin);
  LOGV("PERF: yuv conv: %" PRId64 "[us]\n", elapsed.count());
#endif

  return true;
}

#if !defined(__ANDROID__)
#include "Decoder.h"
bool
convert_yuv_to_rgb32(uint8_t *dst, int32_t dstStride, PixelFormat dstFormat,
                     const DecodedBuffer *dcBuf)
{
  if (dst == nullptr) {
    LOGV("invalid destination.\n");
    return false;
  }

#if 0 // libyuvは逆strideもケアしてくれる
  if (dstStride <= 0) {
    LOGV("invalid destination stride.\n");
    return false;
  }
#endif

  if (dcBuf == nullptr) {
    LOGV("invalid decoded buffer.\n");
    return false;
  }

  const uint8_t *yBuf = dcBuf->v.planes[VDB_PLANE_Y];
  const uint8_t *uBuf = dcBuf->v.planes[VDB_PLANE_U];
  const uint8_t *vBuf = dcBuf->v.planes[VDB_PLANE_V];
  const uint8_t *aBuf = dcBuf->v.planes[VDB_PLANE_A];
  int32_t yStride     = dcBuf->v.stride[VDB_PLANE_Y];
  int32_t uStride     = dcBuf->v.stride[VDB_PLANE_U];
  int32_t vStride     = dcBuf->v.stride[VDB_PLANE_V];
  int32_t aStride     = dcBuf->v.stride[VDB_PLANE_A];
  return convert_yuv_to_rgb32(dst, dstStride, dstFormat, yBuf, uBuf, vBuf, aBuf, yStride,
                              uStride, aStride, dcBuf->v.displayWidth,
                              dcBuf->v.displayHeight, dcBuf->v.format,
                              dcBuf->v.colorSpace, dcBuf->v.colorRange);
}
#endif