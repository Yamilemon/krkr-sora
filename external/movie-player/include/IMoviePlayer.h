#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
class IMovieReadStream {
public:
    virtual int AddRef(void) = 0;
    virtual int Release(void) = 0;
    virtual size_t Read(void *buf, size_t size) = 0;
    virtual int64_t Tell() const = 0;
    virtual void Seek(int64_t offset, int origin) = 0;
    virtual size_t Size() const = 0;
};


class IMoviePlayer
{
public:
  // Color format constants for RenderFrame
  // The order is by byte sequence, so in LE it's ARGB=0xBBGGRRAA
  // Compatible with OpenGL and DXGI (DX10+), inverse of DX9
  // In libyuv, if API name is xxxToARGB, it's 0xBBGGRRAA in LE
  // Reference: In case of IMoviePlayer::BGRA
  //  Same order as IMoviePlayer::ColorFormat
  //    OpenGL: GL_BGRA
  //    DXGI:   DXGI_FORMAT_B8G8R8A8_UNORM
  //  Inverse order of IMoviePlayer::ColorFormat
  //    DX9:    D3DFMT_A8R8G8B8
  //    libyuv: ARGB
  enum ColorFormat
  {
    COLOR_UNKNOWN = -1,
    COLOR_NOCONV  = COLOR_UNKNOWN, // Output decoder output without conversion
    COLOR_ARGB    = 0,
    COLOR_ABGR    = 1,
    COLOR_RGBA    = 2,
    COLOR_BGRA    = 3,
    COLOR_I420    = 10,
    COLOR_NV12    = 11,
    COLOR_NV21    = 12,
  };

  // Audio data format
  enum PcmEncoding
  {
    PCM_UNKNOWN = -1,
    PCM_U8      = 0,
    PCM_S16     = 1,
    PCM_S32     = 2,
    PCM_F32     = 3,
  };

  // Video output format
  struct VideoFormat
  {
    int32_t width;
    int32_t height;
    float frameRate;
    ColorFormat colorFormat;
  };

  // Audio output format
  struct AudioFormat
  {
    int32_t sampleRate;
    int32_t channels;
    int32_t bitsPerSample;
    PcmEncoding encoding;
  };

  enum ColorRange
  {
    COLOR_RANGE_UNDEF = 0,
    COLOR_RANGE_LIMITED,
    COLOR_RANGE_FULL,
  };

  enum ColorSpace
  {
    COLOR_SPACE_UNKNOWN  = -1,
    COLOR_SPACE_IDENTITY = 0,
    COLOR_SPACE_BT_601,
    COLOR_SPACE_BT_709,
    COLOR_SPACE_SMPTE_170,
    COLOR_SPACE_SMPTE_240,
    COLOR_SPACE_BT_2020,
    COLOR_SPACE_SRGB,
  };

  enum VideoPlaneIndex
  {
    VIDEO_PLANE_PACKED = 0,
    VIDEO_PLANE_Y      = 0,
    VIDEO_PLANE_U      = 1,
    VIDEO_PLANE_V      = 2,
    VIDEO_PLANE_A      = 3,
    VIDEO_PLANE_COUNT
  };

  enum State
  {
    STATE_UNINIT,
    STATE_OPEN,
    STATE_PRELOADING,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_STOP, // Stopped
    STATE_FINISH, // Playback finished
  };

  // Creation parameters
  struct InitParam
  {
    ColorFormat videoColorFormat;
    bool useOwnAudioEngine;
    void Init()
    {
      videoColorFormat  = COLOR_UNKNOWN;
      useOwnAudioEngine = true;
    }
  };

  IMoviePlayer() {}
  virtual ~IMoviePlayer() {}

  virtual State GetState() const = 0;

  typedef int32_t (*OnState)(void *userPtr, State state);

  virtual void SetOnState(OnState func, void *userPtr) = 0;

  virtual void Play(bool loop = false) = 0;
  virtual void Stop()                  = 0;
  virtual void Pause()                 = 0;
  virtual void Resume()                = 0;
  virtual void Seek(int64_t posUs)     = 0;
  virtual void SetLoop(bool loop)      = 0;

  // video info
  virtual bool IsVideoAvailable() const                  = 0;
  virtual void GetVideoFormat(VideoFormat *format) const = 0;

  // audio info
  virtual bool IsAudioAvailable() const                  = 0;
  virtual void GetAudioFormat(AudioFormat *format) const = 0;

  // audio volume
  virtual void SetVolume(float volume) = 0;
  virtual float Volume() const         = 0;

  // info
  virtual int64_t Duration() const = 0;
  virtual int64_t Position() const = 0;
  virtual bool IsPlaying() const   = 0;
  virtual bool Loop() const        = 0;

  // Video decoder callback
  typedef std::function<void(char *dest, int pitch)> DestUpdater;
  typedef std::function<void(int w, int h, DestUpdater updater)> OnVideoDecoded;
  virtual void SetOnVideoDecoded(OnVideoDecoded callback) = 0;

  // XXX Considering replacement to match Video
  // Audio decoder callback
  typedef int32_t (*OnAudioDecoded)(void *userPtr, const uint8_t *data, size_t sizeBytes);
  // Get output audio notification function
  // If InitParam::useOwnAudioEngine is true, data is sucked into internal AudioEngine
  // and cannot be passed externally, so it is not called
  virtual void SetOnAudioDecoded(OnAudioDecoded func, void *userPtr) = 0;

  static IMoviePlayer *CreateMoviePlayer(const char *filename, InitParam &param);

  static IMoviePlayer *CreateMoviePlayer(IMovieReadStream *stream, InitParam &param);
};
