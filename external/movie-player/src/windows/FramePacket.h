#pragma once

#include "CommonUtils.h"
#include "Constants.h"
#include "BufferQueue.h"

struct FramePacket : public BufferQueueEntryBase
{
  int32_t bufIndex;
  TrackType type;
  int32_t trackNum;
  uint64_t timeStampNs;
  bool isKeyFrame;
  bool isEndOfStream;
  int32_t flags;
  int64_t arg; // 汎用情報

  FramePacket()
  : BufferQueueEntryBase()
  , type(TRACK_TYPE_UNKNOWN)
  , trackNum(-1)
  , timeStampNs(0)
  , isKeyFrame(false)
  , isEndOfStream(false)
  , flags(0)
  , arg(0)
  {}
  virtual ~FramePacket() {}

  virtual void Clear() override { Init(bufIndex); }
  virtual void Init(int32_t bufIdx)
  {
    bufIndex      = bufIdx;
    type          = TRACK_TYPE_UNKNOWN;
    trackNum      = -1;
    timeStampNs   = 0;
    isKeyFrame    = false;
    isEndOfStream = false;
    flags         = 0;
    arg           = 0;
  }

  void InitAsEOS() { InitAsEOS(bufIndex); }
  void InitAsEOS(int32_t bufIdx)
  {
    Init(bufIdx);
    isEndOfStream = true;
  }

  TrackType Type() const { return type; }

  void PrintInfo(int32_t blockFrameIndex)
  {
#if defined(MOVIE_DEBUG)
    if (isEndOfStream) {
      INLINE_LOGV("FramePacket: <<EOS>>\n");
    } else {
      const char *typeStr =
        (type == TRACK_TYPE_VIDEO ? "VIDEO"
                                  : (type == TRACK_TYPE_AUDIO ? "AUDIO" : "UNKNOWN"));
      INLINE_LOGV("FramePacket: type=%s, size=%6zu, track=%2d, time=%12" PRIu64 " %s\n",
                  typeStr, dataSize, trackNum, timeStampNs, isKeyFrame ? "<KEY>" : "");
    }
#endif
  }
};