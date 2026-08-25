#pragma once

// -----------------------------------------------------------------------------
// common enum/constants
// -----------------------------------------------------------------------------
enum TrackType
{
  TRACK_TYPE_VIDEO   = 0,
  TRACK_TYPE_AUDIO,
  // TRACK_TYPE_TEXT,     // vtt, subtitle
  // TRACK_TYPE_METADATA, // metadata
  // TRACK_TYPE_COUNT,
  TRACK_TYPE_UNKNOWN = -1,
};

enum CodecId
{
  CODEC_V_VP8   = 0,
  CODEC_A_VORBIS,
  CODEC_V_VP9,
  CODEC_A_OPUS,
  CODEC_V_AV1,
  CODEC_UNKNOWN = -1,
};
