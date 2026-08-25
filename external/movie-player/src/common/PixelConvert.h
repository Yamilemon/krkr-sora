#pragma once

#include "CommonUtils.h"

#include <cstdint>

bool
convert_yuv_to_rgb32(uint8_t *dst, int32_t dstStride, PixelFormat dstFormat,
                     const uint8_t *yBuf, const uint8_t *uBuf, const uint8_t *vBuf, const uint8_t *aBuf,
                     int32_t yStride, int32_t uvStride, int32_t aStride,
                     int32_t width, int32_t height,
                     PixelFormat srcFormat, ColorSpace colorSpace, ColorRange colorRange);

#if !defined(__ANDROID__)
struct DecodedBuffer;
bool
convert_yuv_to_rgb32(uint8_t *dst, int32_t dstStride, PixelFormat dstFormat,
                     const DecodedBuffer *dcBuf);
#endif