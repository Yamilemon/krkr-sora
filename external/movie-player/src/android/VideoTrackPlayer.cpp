#define MYLOG_TAG "VideoTrackPlayer"
#include "BasicLog.h"

#include "CommonUtils.h"
#include "Constants.h"
#include "VideoTrackPlayer.h"
#include "PixelConvert.h"

// -----------------------------------------------------------------------------
// カラー関連定数変換処理
// -----------------------------------------------------------------------------
// MediaCodecの内部カラーフォーマットからCommonのPixelFormatへ変換
static PixelFormat
conv_color_format(int32_t nativeFormat)
{
  // これ自体は単純な変換だが、Android環境では特定のデバイスから
  // 想定外データが来る可能性はあるので、一応ASSERTで捕まえるようにしてある

  // MEMO
  // Packed: YUV,YUV,YUV... インタリーブ配置される
  // Planer: YY..., UU..., VV... プレーン配置される
  // Semi-: YY..., UVUV... YとUVはプレーン配置で、UV側内部はインタリーブ配置される
  // Packed(Semi)Planer: ??? 過去互換的、歴史的な名称っぽい？

  PixelFormat pixelFormat = PIXEL_FORMAT_UNKNOWN;

  switch (nativeFormat) {
  case COLOR_FormatYUV420Flexible:
  case COLOR_FormatYUV420Planar:
    pixelFormat = PIXEL_FORMAT_I420;
    break;
  case COLOR_FormatYUV420SemiPlanar:
    pixelFormat = PIXEL_FORMAT_NV12;
    break;
  case COLOR_FormatYUV422Planar:
    pixelFormat = PIXEL_FORMAT_I422;
    break;
  case COLOR_FormatYUV444Flexible:
  case COLOR_FormatYUV444Interleaved:
    pixelFormat = PIXEL_FORMAT_I444;
    break;
  default:
    ASSERT(false, "*** Unsupported color format!! format=%d ***", nativeFormat);
    break;
  }

  return pixelFormat;
}

// MediaCodecの内部カラースペースからCommonのColorSpaceへ変換
static ColorSpace
conv_color_space(int32_t nativeColorSpace)
{
  ColorSpace cs = COLOR_SPACE_BT_601;

  switch (nativeColorSpace) {
  case COLOR_STANDARD_BT709:
    cs = COLOR_SPACE_BT_709;
    break;
  case COLOR_STANDARD_BT601_PAL:
  case COLOR_STANDARD_BT601_NTSC:
    cs = COLOR_SPACE_BT_601;
    break;
  case COLOR_STANDARD_BT2020:
    cs = COLOR_SPACE_BT_2020;
    break;
  }

  return cs;
}

// MediaCodecの内部カラーレンジからCommonのColorRangeへ変換
static ColorRange
conv_color_range(int32_t nativeColorRange)
{
  ColorRange cr = COLOR_RANGE_LIMITED;

  switch (nativeColorRange) {
  case MEDIA_COLOR_RANGE_FULL:
    cr = COLOR_RANGE_FULL;
    break;
  default:
    cr = COLOR_RANGE_LIMITED;
    break;
  }

  return cr;
}

// -----------------------------------------------------------------------------
// ビデオトラックプレイヤ
// -----------------------------------------------------------------------------
VideoTrackPlayer::VideoTrackPlayer(AMediaExtractor *ex, int32_t trackIndex,
                                   MediaClock *timer)
: TrackPlayer(ex, trackIndex, timer)
{
  Init();

  AMediaExtractor_selectTrack(mExtractor, mTrackIndex);

  LOGV("video track: %d", mTrackIndex);

  // トラックフォーマット
  AMediaFormat *format = AMediaExtractor_getTrackFormat(mExtractor, mTrackIndex);
  AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &mWidth);
  AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &mHeight);
  AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &mDuration);

  LOGV("video duration = %lld", mDuration);

  // 現在のメディアタイマーにセットされているDurationよりも長い場合は
  // それをムービー全体のDurationとして反映する
  if (mDuration > mClock->GetDuration()) {
    mClock->SetDuration(mDuration);
  }

  // コーデックセットアップ
  const char *mime;
  AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
  mCodec = AMediaCodec_createDecoderByType(mime);
  AMediaCodec_configure(mCodec, format, nullptr, nullptr, 0);
  AMediaCodec_start(mCodec);

  // 出力フォーマット
  AMediaFormat *outFormat = AMediaCodec_getOutputFormat(mCodec);
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_WIDTH, &mOutputWidth);
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_HEIGHT, &mOutputHeight);
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_STRIDE, &mOutputStride);
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, &mOutputColorFormat);
#if __ANDROID_API__>= __ANDROID_API_P__ //28
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_COLOR_RANGE, &mOutputColorRange);
  AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_COLOR_STANDARD, &mOutputColorSpace);
#endif

  LOGV("video w=%d, h=%d, stride=%d, colorFormat=%d, colorRange=%d, colorSpace=%d",
       mOutputWidth, mOutputHeight, mOutputStride, mOutputColorFormat, mOutputColorRange,
       mOutputColorSpace);
}

VideoTrackPlayer::~VideoTrackPlayer()
{
  Done();
}

void
VideoTrackPlayer::Init()
{
  mWidth  = -1;
  mHeight = -1;

  mOutputWidth       = -1;
  mOutputHeight      = -1;
  mOutputStride      = -1;
  mOutputColorFormat = -1;
  mOutputColorRange  = -1;
  mOutputColorSpace  = -1;

  mOnVideoDecoded = nullptr;
  mOnVideoFormat = PIXEL_FORMAT_UNKNOWN;
}

void
VideoTrackPlayer::Done()
{}

int32_t
VideoTrackPlayer::Width() const
{
  if (IsValid()) {
    return mOutputWidth;
  } else {
    return -1;
  }
}

int32_t
VideoTrackPlayer::Height() const
{
  if (IsValid()) {
    return mOutputHeight;
  } else {
    return -1;
  }
}

void
VideoTrackPlayer::HandleMessage(int32_t what, int64_t arg, void *obj)
{
  // LOGV("handle msg %d", what);

  switch (what) {
  case MSG_START:
    SetState(STATE_PLAY);
    Decode();
    mEventFlag.Set(EVENT_FLAG_PLAY_READY);
    break;

  case MSG_PAUSE:
    if (GetState() == STATE_PLAY) {
      SetState(STATE_PAUSE);
      Post(MSG_NOP, 0, nullptr, true); // 発行済メッセージを全フラッシュ
    }
    break;

  case MSG_RESUME:
    if (GetState() == STATE_PAUSE) {
      mClock->Reset();
      SetState(STATE_PLAY);
      Decode();
    }
    break;

  case MSG_SEEK:
    AMediaExtractor_seekTo(mExtractor, arg, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC);
    AMediaCodec_flush(mCodec);
    // TODO starttimeをV/A共通にしてるので、同期処理していない現状ではおかしくなるかも？
    mClock->Reset();
    mSawInputEOS  = false;
    mSawOutputEOS = false;
    if (GetState() != STATE_PLAY) {
      // 再生中ではない場合は、シーク動作のために一度だけデコード処理をする
      Decode(DECODER_FLAG_ONESHOT);
    }
    break;

  case MSG_STOP:
    SetState(STATE_STOP);
    AMediaCodec_stop(mCodec);
    // AMediaCodec_delete(mCodec);
    Post(MSG_NOP, 0, nullptr, true); // 発行済メッセージを全フラッシュ
    mEventFlag.Set(EVENT_FLAG_STOPPED);
    break;

  default:
    TrackPlayer::HandleMessage(what, arg, obj);
    break;
  }

  std::this_thread::yield();
}

void
VideoTrackPlayer::HandleOutputData(ssize_t bufIdx, AMediaCodecBufferInfo &bufInfo,
                                   int32_t flags)
{
  // 出力バッファの内容をコピーする
  if (bufInfo.size > 0) {

    // このフレームのPTSをタイマーに反映
    int64_t mediaTimeUs = bufInfo.presentationTimeUs;
    if (!mClock->IsStarted()) {
      mClock->SetStartMediaTime(mediaTimeUs);
    }
    mClock->SetPresentationTime(mediaTimeUs);

    if (mOnVideoDecoded == nullptr) {
      return;
    }

    size_t bufSize;
    uint8_t *buf = AMediaCodec_getOutputBuffer(mCodec, bufIdx, &bufSize);
    // YUVのプレーンごとのストライドなどを取得する方法が見当たらないので
    // ソースプレーンがパディングされずに連続して配置されるものとして扱っている
    // 一部デバイスや特殊サイズムービーだとうまくいかない可能性あり
    const uint8_t *yBuf = buf + bufInfo.offset;
    const uint8_t *uBuf = yBuf + mOutputWidth * mOutputHeight;
    const uint8_t *vBuf = uBuf + (mOutputWidth * mOutputHeight / 4);
    int32_t yStride     = mOutputWidth;
    int32_t uvStride    = mOutputWidth / 2;
    PixelFormat colorFormat = conv_color_format(mOutputColorFormat);
    if (is_nv_pixel_format(colorFormat)) {
      uvStride = mOutputWidth; // NVはUVがパックド
    }
    mOnVideoDecoded(mOutputWidth, mOutputHeight, [&](char *dest, int pitch) {
      convert_yuv_to_rgb32((uint8_t*)dest, pitch, mOnVideoFormat, yBuf, uBuf, vBuf, 0, yStride, uvStride, 0,
                          mOutputWidth, mOutputHeight, colorFormat, (ColorSpace)mOutputColorSpace,
                          (ColorRange)mOutputColorRange);
    });
    AMediaCodec_releaseOutputBuffer(mCodec, bufIdx, false);
  }
}
