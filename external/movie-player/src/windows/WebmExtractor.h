#pragma once

#include "CommonUtils.h"
#include "Constants.h"
#include "MkvFileReader.h"
#include <nestegg/nestegg.h>

#include <string>
#include <vector>

struct TrackInfo
{
  int32_t index;
  TrackType type;
  CodecId codecId;
  union
  {
    struct
    {
      int32_t width;
      int32_t height;
      float frameRate;
      ColorRange colorRange;
      bool alphaMode;
    } v;
    struct
    {
      int32_t channels;
      int32_t bitDepth;
      float sampleRate;
      uint64_t codecDelay;
    } a;
  };
};

class Decoder;
struct FramePacket;

// Webm Extractor
class WebmExtractor
{
public:
  WebmExtractor();
  ~WebmExtractor();

  bool Open(const std::string &filePath);
  bool Open(IMovieReadStream *stream);
  bool SeekTo(long long positionUs);

  uint64_t GetDurationUs() const { return mDurationUs; }

  size_t GetTrackCount();
  bool GetTrackInfo(int32_t trackIndex, TrackInfo *info);
  bool GetCodecPrivateData(int32_t trackIndex,
                           std::vector<std::vector<uint8_t>> &privateData);

  bool SelectTrack(TrackType type, int32_t trackIndex);

  TrackType NextFramePacketType();
  bool ReadSampleData(FramePacket *packet);
  bool Advance();

  bool IsReachedEOS() const { return mIsReachedEOS; }

private:
  bool OpenSetup();

  static void NestEggLogCallback(nestegg *ctx, unsigned int severity, char const *fmt,
                                 ...);
  static int MyRead(void *buffer, size_t length, void *userdata);
  static int MySeek(int64_t offset, int whence, void *userdata);
  static int64_t MyTell(void *userdata);

  void CheckFirstTouch();

private:
  bool mIsReachedEOS;
  bool mIsFirstRead;

  uint64_t mDurationUs;

  nestegg *mCtx;
  unsigned int mTracks; //< トラック数

  int mVideoTrack; //< 対象ビデオトラック
  int mAudioTrack; //< 対象オーディオトラック
  bool mVideoAlphaMode;

  nestegg_packet *mPkt;

  unsigned int mCurrentTrack;
  TrackType mCurrentTrackType;
  unsigned int mFrames;
  unsigned int mFrameIndex;
  uint64_t mTimeStampNs;
  int64_t mDiscardPadding;
  bool mIsKeyFrame;

  IMkvFileReader *mReader;
};
