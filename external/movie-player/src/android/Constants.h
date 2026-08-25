#pragma once

#include <cstdint>

// -----------------------------------------------------------------------------
// Android版共通定数定義
// -----------------------------------------------------------------------------
// 主にNDKヘッダとして露出していないが、APIから取得できる値に対応する定数名を
// 内部実装(libstagefright)のヘッダからコピーしている。
// Android側のバージョンが進むと新規に値が増える可能性があるので、未対応の
// 数値が出てきた場合は適宜Androidのソースツリーを参照のこと。

// key: AMEDIAFORMAT_KEY_COLOR_FORMAT
// https://android.googlesource.com/platform/frameworks/av/+/refs/heads/master/media/libstagefright/include/media/stagefright/MediaCodecConstants.h
constexpr int32_t COLOR_Format12bitRGB444               = 3;
constexpr int32_t COLOR_Format16bitARGB1555             = 5;
constexpr int32_t COLOR_Format16bitARGB4444             = 4;
constexpr int32_t COLOR_Format16bitBGR565               = 7;
constexpr int32_t COLOR_Format16bitRGB565               = 6;
constexpr int32_t COLOR_Format18bitARGB1665             = 9;
constexpr int32_t COLOR_Format18BitBGR666               = 41;
constexpr int32_t COLOR_Format18bitRGB666               = 8;
constexpr int32_t COLOR_Format19bitARGB1666             = 10;
constexpr int32_t COLOR_Format24BitABGR6666             = 43;
constexpr int32_t COLOR_Format24bitARGB1887             = 13;
constexpr int32_t COLOR_Format24BitARGB6666             = 42;
constexpr int32_t COLOR_Format24bitBGR888               = 12;
constexpr int32_t COLOR_Format24bitRGB888               = 11;
constexpr int32_t COLOR_Format25bitARGB1888             = 14;
constexpr int32_t COLOR_Format32bitABGR8888             = 0x7F00A000;
constexpr int32_t COLOR_Format32bitARGB8888             = 16;
constexpr int32_t COLOR_Format32bitBGRA8888             = 15;
constexpr int32_t COLOR_Format8bitRGB332                = 2;
constexpr int32_t COLOR_FormatCbYCrY                    = 27;
constexpr int32_t COLOR_FormatCrYCbY                    = 28;
constexpr int32_t COLOR_FormatL16                       = 36;
constexpr int32_t COLOR_FormatL2                        = 33;
constexpr int32_t COLOR_FormatL24                       = 37;
constexpr int32_t COLOR_FormatL32                       = 38;
constexpr int32_t COLOR_FormatL4                        = 34;
constexpr int32_t COLOR_FormatL8                        = 35;
constexpr int32_t COLOR_FormatMonochrome                = 1;
constexpr int32_t COLOR_FormatRawBayer10bit             = 31;
constexpr int32_t COLOR_FormatRawBayer8bit              = 30;
constexpr int32_t COLOR_FormatRawBayer8bitcompressed    = 32;
constexpr int32_t COLOR_FormatRGBAFlexible              = 0x7F36A888;
constexpr int32_t COLOR_FormatRGBFlexible               = 0x7F36B888;
constexpr int32_t COLOR_FormatSurface                   = 0x7F000789;
constexpr int32_t COLOR_FormatYCbYCr                    = 25;
constexpr int32_t COLOR_FormatYCrYCb                    = 26;
constexpr int32_t COLOR_FormatYUV411PackedPlanar        = 18;
constexpr int32_t COLOR_FormatYUV411Planar              = 17;
constexpr int32_t COLOR_FormatYUV420Flexible            = 0x7F420888;
constexpr int32_t COLOR_FormatYUV420PackedPlanar        = 20;
constexpr int32_t COLOR_FormatYUV420PackedSemiPlanar    = 39;
constexpr int32_t COLOR_FormatYUV420Planar              = 19;
constexpr int32_t COLOR_FormatYUV420SemiPlanar          = 21;
constexpr int32_t COLOR_FormatYUV422Flexible            = 0x7F422888;
constexpr int32_t COLOR_FormatYUV422PackedPlanar        = 23;
constexpr int32_t COLOR_FormatYUV422PackedSemiPlanar    = 40;
constexpr int32_t COLOR_FormatYUV422Planar              = 22;
constexpr int32_t COLOR_FormatYUV422SemiPlanar          = 24;
constexpr int32_t COLOR_FormatYUV444Flexible            = 0x7F444888;
constexpr int32_t COLOR_FormatYUV444Interleaved         = 29;
constexpr int32_t COLOR_FormatYUVP010                   = 54;
constexpr int32_t COLOR_QCOM_FormatYUV420SemiPlanar     = 0x7fa30c00;
constexpr int32_t COLOR_TI_FormatYUV420PackedSemiPlanar = 0x7f000100;

// 内部定義と名前が被ってしまうので、ちょっとだけAndroid内部実装定義と
// 名前を変更してある。
// AMEDIAFORMAT_KEY_COLOR_RANGE
constexpr int32_t MEDIA_COLOR_RANGE_FULL    = 1;
constexpr int32_t MEDIA_COLOR_RANGE_LIMITED = 2;

// AMEDIAFORMAT_KEY_COLOR_STANDARD
constexpr int32_t COLOR_STANDARD_BT709      = 1;
constexpr int32_t COLOR_STANDARD_BT601_PAL  = 2;
constexpr int32_t COLOR_STANDARD_BT601_NTSC = 4;
constexpr int32_t COLOR_STANDARD_BT2020     = 6;

// key: AMEDIAFORMAT_KEY_PCM_ENCODING
// オーディオエンコーディング定数
// これは frameworks/av に定義がないのでさらに内部のヘッダで定義されている
// https://android.googlesource.com/platform/frameworks/av/+/refs/heads/master/media/libstagefright/foundation/include/media/stagefright/foundation/MediaDefs.h?autodive=0%2F
enum AudioEncoding
{
  kAudioEncodingInvalid        = 0,
  kAudioEncodingPcm16bit       = 2,
  kAudioEncodingPcm8bit        = 3,
  kAudioEncodingPcmFloat       = 4,
  kAudioEncodingPcm24bitPacked = 200,
  kAudioEncodingPcm32bit       = 201,
};

