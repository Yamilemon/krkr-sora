#define MYLOG_TAG "MoviePlayerCore"
#include "BasicLog.h"
#include "MoviePlayerCore.h"

#include "AudioEngine.h"
#include "IMoviePlayer.h"

// 静的コールバック関数を追加
static bool AudioDataCallback(void* userData, uint8_t* buffer, uint64_t frameCount, uint64_t* framesRead)
{
  MoviePlayerCore* player = static_cast<MoviePlayerCore*>(userData);
  if (player) {
    return player->GetAudioFrame(buffer, frameCount, framesRead, nullptr);
  }
  return false;
}

MoviePlayerCore::MoviePlayerCore(PixelFormat pixelFormat, bool useAudioEngine)
: mState(STATE_UNINIT)
, mPixelFormat(pixelFormat)
#ifdef INNER_AUDIOENGINE
, mAudioEngine(nullptr)
#endif
, mOnStateFunc(nullptr)
, mOnAudioDecodedFunc(nullptr)
, mOnVideoDecodedFunc(nullptr)
{
#ifdef INNER_AUDIOENGINE
  if (useAudioEngine) {
    mAudioEngine = CreateAudioEngine();
  }
#endif
  Init();
}

MoviePlayerCore::~MoviePlayerCore()
{
  Done();
}

void
MoviePlayerCore::Init()
{
  mWidth             = -1;
  mHeight            = -1;
  mOutputPixelFormat = PIXEL_FORMAT_UNKNOWN;

  mSawVideoInputEOS  = false;
  mSawAudioInputEOS  = false;
  mSawVideoOutputEOS = false;
  mSawAudioOutputEOS = false;

  mLastVideoFrameEnd = false;
  mLastAudioFrameEnd = false;

  mIsLoop = false;

  mExtractor    = nullptr;
  mVideoDecoder = nullptr;
  mAudioDecoder = nullptr;

  mClock.Reset();

  mVideoFrame = mVideoFrameNext = nullptr;
  mVideoFrameLastGet            = nullptr;
  mDummyFrame.InitByType(TRACK_TYPE_VIDEO, -1);

  mAudioQueuedBytes       = 0;
  mAudioDataPos           = 0;
  mAudioUnitSize          = 0;
  mAudioOutputFrames      = 0;
  mAudioCodecDelayUs      = 0;
  mAudioResumeMediaTimeUs = 0;
  mAudioTime.Reset();
}

void
MoviePlayerCore::InitDummyFrame()
{
  if (mVideoDecoder) {
    mDummyFrame.InitByType(TRACK_TYPE_VIDEO, -1);
    mDummyFrame.Resize(mWidth * mHeight * 4);
    mDummyFrame.timeStampNs = 0;
    mDummyFrame.frame       = 0;
    memset(mDummyFrame.data, 0xff, mDummyFrame.capacity);
    {
      std::lock_guard<std::mutex> lock(mVideoFrameMutex);

      mVideoFrame = &mDummyFrame;
      EnqueueVideo(mVideoFrame);
    }
  }
}

void
MoviePlayerCore::Done()
{
  Flush();
  StopThread();

#ifdef INNER_AUDIOENGINE
  if (mAudioEngine) {
    mAudioEngine->Done();
    delete mAudioEngine;
    mAudioEngine = nullptr;
  }
#endif

  if (mVideoDecoder) {
    mVideoDecoder->Stop();
    delete mVideoDecoder;
    mVideoDecoder = nullptr;
  }

  if (mAudioDecoder) {
    mAudioDecoder->Stop();
    delete mAudioDecoder;
    mAudioDecoder = nullptr;
  }

  if (mExtractor) {
    delete mExtractor;
    mExtractor = nullptr;
  }

  mOnStateFunc = nullptr;
  mOnAudioDecodedFunc = nullptr;
  mOnVideoDecodedFunc = nullptr;
}

void
MoviePlayerCore::Start()
{
  if (mVideoDecoder) {
    mVideoDecoder->Start();
  }

  if (mAudioDecoder) {
    mAudioDecoder->Start();
  }

  StartThread();
}

void
MoviePlayerCore::Play(bool loop)
{
  if (IsRunning()) {
    Post(MoviePlayerCore::MSG_SET_LOOP, loop);
    Post(MoviePlayerCore::MSG_START);
    mEventFlag.Wait(EVENT_FLAG_PLAY_READY);
  }
}

void
MoviePlayerCore::Stop()
{
  if (IsRunning()) {
    Post(MoviePlayerCore::MSG_STOP);
    mEventFlag.Wait(EVENT_FLAG_STOPPED);
  }
}

void
MoviePlayerCore::Pause()
{
  if (IsRunning()) {
    Post(MoviePlayerCore::MSG_PAUSE);
  }
}

void
MoviePlayerCore::Resume()
{
  if (IsRunning()) {
    Post(MoviePlayerCore::MSG_RESUME);
  }
}

void
MoviePlayerCore::Seek(int64_t posUs)
{
  if (IsRunning()) {
    Post(MoviePlayerCore::MSG_SEEK, posUs);
  }
}

void
MoviePlayerCore::SetLoop(bool loop)
{
  if (IsRunning()) {
    Post(MoviePlayerCore::MSG_SET_LOOP, loop);
  }
}

bool
MoviePlayerCore::IsVideoAvailable() const
{
  return (mVideoDecoder != nullptr);
}

int32_t
MoviePlayerCore::Width() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return mWidth;
}

int32_t
MoviePlayerCore::Height() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return mHeight;
}

float
MoviePlayerCore::FrameRate() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return mFrameRate;
}

PixelFormat
MoviePlayerCore::OutputPixelFormat() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return mOutputPixelFormat;
}

bool
MoviePlayerCore::IsAudioAvailable() const
{
  return (mAudioDecoder != nullptr);
}

int32_t
MoviePlayerCore::SampleRate() const
{
  if (IsAudioAvailable()) {
    return mAudioDecoder->SampleRate();
  } else {
    return -1;
  }
}

int32_t
MoviePlayerCore::Channels() const
{
  if (IsAudioAvailable()) {
    return mAudioDecoder->Channels();
  } else {
    return -1;
  }
}

int32_t
MoviePlayerCore::BitsPerSample() const
{
  if (IsAudioAvailable()) {
    return mAudioDecoder->BitsPerSample();
  } else {
    return -1;
  }
}

int32_t
MoviePlayerCore::Encoding() const
{
  if (IsAudioAvailable()) {
    return mAudioDecoder->Encoding();
  } else {
    return -1;
  }
}

int64_t
MoviePlayerCore::Duration() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return mClock.GetDuration();
}

int64_t
MoviePlayerCore::Position() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return mClock.GetPresentationTime();
}

bool
MoviePlayerCore::IsPlaying() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return (mState == STATE_PLAY || mState == STATE_PAUSE || mState == STATE_PRELOADING);
}

bool
MoviePlayerCore::Loop() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return mIsLoop;
}

void
MoviePlayerCore::SelectTargetTrack()
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  size_t trackNum = mExtractor->GetTrackCount();
  for (size_t i = 0; i < trackNum; i++) {
    TrackInfo info;
    mExtractor->GetTrackInfo(i, &info);
    // LOGV("TRACK[%d]: type=%d, codec=%d\n", i, info.type, info.codecId);

    if (info.codecId == CODEC_UNKNOWN) {
      LOGV(" *** unknown codec! ignore track #%d ***\n", (int)i);
      continue;
    }

    switch (info.type) {
    case TRACK_TYPE_VIDEO: {
      if (mVideoDecoder != nullptr) {
        // 複数ビデオトラックの場合は最初のトラックのみ対象とする
        continue;
      }

      if (info.codecId == CODEC_UNKNOWN) {
        LOGV("unsupported video codec! skip this track: index=%d\n", (int)i);
        continue;
      }

      mWidth     = info.v.width;
      mHeight    = info.v.height;
      mFrameRate = info.v.frameRate;

      mVideoDecoder = (VideoDecoder *)Decoder::CreateDecoder(info.codecId);
      ASSERT(mVideoDecoder != nullptr, "failed to create video decoder\n");

      InitDummyFrame();

      if (info.codecId == CODEC_V_VP8 || info.codecId == CODEC_V_VP9) {
        Decoder::Config config;
        config.Init(info.codecId);
        config.vpx.decCfg.w = info.v.width;
        config.vpx.decCfg.h = info.v.height;
        // libvpxのスレッド数を、ncpus-4 と 2 の大きい方を指定する
        // 5コア以上なら4スレッド、4コア以下なら2スレッド。
        // テストでは1080pくらいなら4スレッド以上は特に変わらない感じ。
        config.vpx.decCfg.threads = get_num_of_cpus() > 4 ? 4 : 2;
        config.vpx.rgbFormat      = mPixelFormat;
        config.vpx.alphaMode      = info.v.alphaMode;
        mVideoDecoder->Configure(config);

        mOutputPixelFormat = mVideoDecoder->OutputPixelFormat();
      }

      mExtractor->SelectTrack(TRACK_TYPE_VIDEO, i);

      if (mVideoDecoder) {
        LOGV(" VIDEO: codec=%s, width=%d, height=%d, fps=%f\n",
             mVideoDecoder->CodecName(), info.v.width, info.v.height, info.v.frameRate);
      }

    } break;
    case TRACK_TYPE_AUDIO: {
      if (mAudioDecoder != nullptr) {
        // 複数オーディオトラックの場合は最初のトラックのみ対象とする
        continue;
      }

      if (info.codecId == CODEC_UNKNOWN) {
        LOGV("unsupported audio codec! skip this track: index=%d\n", (int)i);
        continue;
      }

      mAudioDecoder = (AudioDecoder *)Decoder::CreateDecoder(info.codecId);
      ASSERT(mAudioDecoder != nullptr, "failed to create audio decoder\n");

      Decoder::Config config;
      config.Init(info.codecId);
      if (info.codecId == CODEC_A_VORBIS) {
        config.vorbis.channels   = info.a.channels;
        config.vorbis.sampleRate = info.a.sampleRate;
      } else if (info.codecId == CODEC_A_OPUS) {
        config.opus.channels   = info.a.channels;
        config.opus.sampleRate = info.a.sampleRate;
      }
      mExtractor->GetCodecPrivateData(i, config.privateData);
      mAudioDecoder->Configure(config);

      mAudioCodecDelayUs = ns_to_us(info.a.codecDelay);

      // TODO AUDIO_FORMAT_S16 で固定。汎用にするならインタフェース追加
      AudioFormat audioFormat = AUDIO_FORMAT_S16;
      switch (audioFormat) {
      case AUDIO_FORMAT_U8:
        mAudioUnitSize = info.a.channels * 1;
        break;
      case AUDIO_FORMAT_S16:
        mAudioUnitSize = info.a.channels * 2;
        break;
      case AUDIO_FORMAT_S32:
      case AUDIO_FORMAT_F32:
        mAudioUnitSize = info.a.channels * 4;
        break;
      }

#ifdef INNER_AUDIOENGINE
      // 自前オーディオ再生の場合はAudioEngineを初期化する
      if (mAudioEngine != nullptr) {
        mAudioEngine->Init(AudioDataCallback, this, audioFormat, info.a.channels, info.a.sampleRate);
      }
#endif

      mExtractor->SelectTrack(TRACK_TYPE_AUDIO, i);

      if (mAudioDecoder) {
        LOGV(" AUDIO: codec=%s, channels=%d, sampleRate=%f, depth=%d, codecDelay=%" PRIu64
             "\n",
             mAudioDecoder->CodecName(), info.a.channels, info.a.sampleRate,
             info.a.bitDepth, mAudioCodecDelayUs);
      }

    } break;
    default: // ignore
      break;
    }
  }
}

bool
MoviePlayerCore::Open(const char *filepath)
{
  mExtractor   = new WebmExtractor();
  bool success = mExtractor->Open(filepath);
  if (!success) {
    LOGV("failed to create Extractor\n");
    return false;
  }
  OpenSetup();
  return true;
}

bool
MoviePlayerCore::Open(IMovieReadStream *stream)
{
  mExtractor   = new WebmExtractor();
  bool success = mExtractor->Open(stream);
  if (!success) {
    LOGV("failed to create Extractor\n");
    return false;
  }
  OpenSetup();
  return true;
}

void
MoviePlayerCore::OpenSetup()
{
  mClock.SetDuration(mExtractor->GetDurationUs());

  SelectTargetTrack();
  InitStatusFlags();
  Start();

  // Play() がかかるまでプリロードする
  PreLoadInput();
}

void
MoviePlayerCore::InitStatusFlags()
{
  mSawVideoInputEOS = mSawVideoOutputEOS = mLastVideoFrameEnd = !IsVideoAvailable();
  mSawAudioInputEOS = mSawAudioOutputEOS = mLastAudioFrameEnd = !IsAudioAvailable();
}

void
MoviePlayerCore::DemuxInput()
{
  if (mSawVideoInputEOS && mSawAudioInputEOS) {
    return;
  }

  bool isPreloading  = IsCurrentState(STATE_PRELOADING);
  bool isInputFilled = false;
  bool reachedEOS    = false;

  do {
    Decoder *decoder = nullptr;
    TrackType type   = mExtractor->NextFramePacketType();
    switch (type) {
    case TRACK_TYPE_VIDEO:
      decoder = mVideoDecoder;
      ASSERT(decoder != nullptr, "BUG: invali video decoder.\n");
      break;
    case TRACK_TYPE_AUDIO:
      decoder = mAudioDecoder;
      ASSERT(decoder != nullptr, "BUG: invali audio decoder.\n");
      break;
    case TRACK_TYPE_UNKNOWN:
    default:
      break;
    }

    int32_t packetIndex = -1;
    if (decoder && !mExtractor->IsReachedEOS()) {
      packetIndex = InputToDecoder(decoder, false);
      if (packetIndex < 0) {
        isInputFilled = true; // 入力プリロード終了
      }
    }

    // 入力パケットを生成してEOSフラグを設定
    if (mExtractor->IsReachedEOS()) {
      if (IsVideoAvailable() && !mSawVideoInputEOS) {
        // LOGV("video EOS\n");
        packetIndex = InputToDecoder(mVideoDecoder, true);
        if (packetIndex >= 0) {
          mSawVideoInputEOS = true;
        }
      }
      if (IsAudioAvailable() && !mSawAudioInputEOS) {
        // LOGV("audio EOS\n");
        packetIndex = InputToDecoder(mAudioDecoder, true);
        if (packetIndex >= 0) {
          mSawAudioInputEOS = true;
        }
      }
    }
  } while (isPreloading && !isInputFilled);
}

int32_t
MoviePlayerCore::InputToDecoder(Decoder *decoder, bool inputIsEOS)
{
  int32_t packetIndex = decoder->DequeueFramePacketIndex();
  if (packetIndex >= 0) {
    FramePacket *packet = decoder->GetFramePacket(packetIndex);
    if (inputIsEOS) {
      packet->InitAsEOS();
    } else {
      mExtractor->ReadSampleData(packet);
      mExtractor->Advance();
    }
    decoder->QueueFramePacketIndex(packetIndex);
  }

  return packetIndex;
}

void
MoviePlayerCore::HandleVideoOutput()
{
  if (!mVideoDecoder) {
    return;
  }

  bool isPreloading     = IsCurrentState(STATE_PRELOADING);
  bool isFrameReady     = false;
  bool isFirstOfPreload = isPreloading;
  bool isFrameSkipping  = false;
  int32_t dcBufIndex    = -1;

  do {
    // 次フレームが未セットならデコード出力を吸い上げ
    if (!mSawVideoOutputEOS && mVideoFrameNext == nullptr) {
      dcBufIndex = mVideoDecoder->DequeueDecodedBufferIndex();
      if (dcBufIndex >= 0) {
        DecodedBuffer *buf = mVideoDecoder->GetDecodedBuffer(dcBufIndex);
        mSawVideoOutputEOS = buf->isEndOfStream;
        if (!buf->isEndOfStream) {
          if (buf->dataSize > 0) {
            SetVideoFrameNext(buf);
          }
        } else {
          // LOGV("output EOS: video\n");
        }
      } else if (isFrameSkipping) {
        // フレームスキップ解消中にデコード結果を吸い上げきった
        isFrameSkipping = false;
        std::this_thread::yield();
      }
    }

    // 次フレームに入れ替えるかチェック
    if (mVideoFrameNext) {
      isFrameSkipping = false;

      if (isFirstOfPreload) {
        // プリロード時の初回は強制的にDecodedFrame更新
        UpdateVideoFrameToNext();
        isFirstOfPreload = false;
        isFrameReady     = true;
      } else if (mClock.IsStarted()) {
        // フレームスキップ判断のスレッショルド: 1/2フレーム遅れたら飛ばす
        int64_t frameSkipThresh = s_to_us(1 / mFrameRate / 2);
        int64_t timeDiff        = CalcDiffVideoTimeAndNow(mVideoFrameNext);
        if (false && timeDiff >= frameSkipThresh) {
          // フレームスキップ
          LOGV("*** video frame skipped: frame=%" PRId64 ", pts=%" PRId64
               ", diff=%" PRId64 ", thresh=%" PRId64 "\n",
               mVideoFrameNext->frame, ns_to_us(mVideoFrameNext->timeStampNs), timeDiff,
               frameSkipThresh);
          SetVideoFrameNext(nullptr);
          isFrameSkipping = true;
        } else if (timeDiff >= 0) {
          // 出力ビデオフレーム更新
          // LOGV("video update: pts=%" PRId64 ", diff=%" PRId64 "\n",
          //      ns_to_us(mVideoFrameNext->timeStampNs), timeDiff);
          UpdateVideoFrameToNext();
          isFrameReady = true;
        }
      }
    }

    // デコード吸い上げが完了＆次フレームが空の状態ならば
    // 最終ビデオフレームなのでエンドタイムをチェックして
    // 終了フラグを立てる
    if (mSawVideoOutputEOS && mVideoFrameNext == nullptr) {
      // LOGV("video last frame\n");
      int64_t timeDiff = CalcDiffVideoTimeAndNow(mVideoFrame);
      if (timeDiff >= 0) {
        // LOGV("video last frame END\n");
        mLastVideoFrameEnd = true;
      }
    }

  } while ((isPreloading && !isFrameReady) || isFrameSkipping);
}

void
MoviePlayerCore::HandleAudioOutput()
{
  if (!mAudioDecoder) {
    return;
  }

  if (mSawAudioOutputEOS) {
    return;
  }

  bool isPreloading  = IsCurrentState(STATE_PRELOADING);
  bool isFrameReady  = false;
  int32_t dcBufIndex = -1;

  do {
    dcBufIndex = mAudioDecoder->DequeueDecodedBufferIndex();
    if (dcBufIndex >= 0) {
      DecodedBuffer *buf = mAudioDecoder->GetDecodedBuffer(dcBufIndex);
      mSawAudioOutputEOS = buf->isEndOfStream;
      if (buf->dataSize > 0 || buf->isEndOfStream) {
        // EOSバッファも終了確認マーカーとしてキューイングする
        EnqueueAudio(buf);
        isFrameReady = true;
      }
    } else {
      std::this_thread::yield();
    }
  } while (isPreloading && !isFrameReady);
}

void
MoviePlayerCore::Decode()
{
  DemuxInput();
  HandleVideoOutput();
  HandleAudioOutput();

  if (IsCurrentState(STATE_PRELOADING)) {
    return;
  }

  bool sawInputEOS  = mSawVideoInputEOS && mSawAudioInputEOS;
  bool sawOutputEOS = mSawVideoOutputEOS && mSawAudioOutputEOS;
  bool lastFrameEnd = mLastVideoFrameEnd && mLastAudioFrameEnd;

  bool isMovieDone = (sawInputEOS && sawOutputEOS && lastFrameEnd);
  if (isMovieDone) {
    if (mIsLoop) {
      LOGV("---- Loop ----\n");
      Post(MSG_SEEK, 0);
      Post(MSG_DECODE);
    } else {
      Post(MSG_FINISH);
    }
  } else {
    Post(MSG_DECODE);
  }
}

void
MoviePlayerCore::HandleMessage(int32_t what, int64_t arg, void *data)
{
  // LOGV("handle msg %d\n", what);
  switch (what) {
  case MSG_PRELOAD:
    SetState(STATE_PRELOADING);
    Decode();
    mEventFlag.Set(EVENT_FLAG_PRELOADED);
    break;

  case MSG_START:
    SetState(STATE_PLAY);
    Decode();
    mEventFlag.Set(EVENT_FLAG_PLAY_READY);
    break;

  case MSG_DECODE:
    Decode();
    break;

  case MSG_PAUSE:
    if (IsCurrentState(STATE_PLAY)) {
      // 発行済のメッセージを全フラッシュ
      // その後MSG_DECODEを発行しないので、そのままデコード処理はポーズする
      SetState(STATE_PAUSE);
      Post(MSG_NOP, 0, nullptr, true);
    }
    break;

  case MSG_RESUME:
    if (IsCurrentState(STATE_PAUSE)) {
      mClock.ClearStartMediaTime();

      if (IsAudioAvailable()) {
        mAudioOutputFrames = 0;

        // audio有効時のメディアクロック更新はGetAudioFrame()からなので
        // 非同期アクセスが起点となり、ビデオフレームの更新確認時に初回更新が
        // かかってない可能性がある。そのため、ここで前回の最終タイムを使用して
        // start/anchorに更新をかけておく。
        // 非同期アクセスの保護は audio frame用の mutex を借りる
        std::lock_guard<std::mutex> lock(mAudioFrameMutex);
        mClock.SetStartMediaTime(mAudioResumeMediaTimeUs);
        mClock.SetPresentationTime(mAudioResumeMediaTimeUs);
        mClock.UpdateAnchorTime(mAudioResumeMediaTimeUs, get_time_us(), INT64_MAX);
      }

      // レジューム直後はビデオを強制的に次フレームに更新
      if (IsVideoAvailable() && mVideoFrameNext) {
        UpdateVideoFrameToNext();
      }

      SetState(STATE_PLAY);
      Post(MSG_DECODE);
    }
    break;

  case MSG_SET_LOOP: {
    std::lock_guard<std::mutex> lock(mApiMutex);
    mIsLoop = (arg != 0);
  } break;

  case MSG_SEEK: {
    Flush();
    mExtractor->SeekTo(arg);
    State savedState = GetState();
    SetState(STATE_PRELOADING);
    Decode();
    SetState(savedState);
  } break;

  case MSG_STOP:
    SetVideoFrame(&mDummyFrame);
    SetState(STATE_STOP);
    Post(MSG_NOP, 0, nullptr, true); // flush msg
    mEventFlag.Set(EVENT_FLAG_STOPPED);
    break;

  case MSG_FINISH:
    SetVideoFrame(&mDummyFrame);
    SetState(STATE_FINISH);
    Post(MSG_NOP, 0, nullptr, true); // flush msg
    mEventFlag.Set(EVENT_FLAG_STOPPED);
    break;

  case MSG_NOP: // no operation
    break;

  default:
    ASSERT(false, "unknown message type: %d\n", what);
    break;
  }

  std::this_thread::yield();
}

void
MoviePlayerCore::Flush()
{
  // ビデオ出力フレームをフラッシュ
  // 現在のフレームの内容をダミーフレームにコピー(bufIndexはコピーしない)
  // BufferQueueの造りの関係上、Flush()したときに現在mDecodedFrameで
  // 参照しているフレームだけ残してflushみたいなことが大変なのでこのように対応。
  // あるいは mDecodedFrameを毎回ポインタではなくコピー保持にする方法でも良いが
  // 現状ではポインタ保持なのでFlushの絡むseek処理のみこのように対応している。
  // (実際の所スレッド絡みなので毎回コピー保持したほうが丸いので、将来的にはそうなるかも)
  if (mVideoFrame) {
    mDummyFrame.CopyFrom(mVideoFrame, false);
    SetVideoFrame(&mDummyFrame);
    SetVideoFrameNext(nullptr);
  }

  mVideoFrameLastGet = nullptr;

  // オーディオ出力待ちキューをフラッシュ＆ステータス類をリセット
  if (mAudioDecoder != nullptr) {
    std::lock_guard<std::mutex> lock(mAudioFrameMutex);
    while (!mAudioFrameQueue.empty()) {
      DecodedBuffer *buf = mAudioFrameQueue.front();
      mAudioFrameQueue.pop();
      mAudioDecoder->ReleaseDecodedBufferIndex(buf->bufIndex);
    }
    mAudioQueuedBytes       = 0;
    mAudioDataPos           = 0;
    mAudioOutputFrames      = 0;
    mAudioResumeMediaTimeUs = 0;
    mAudioTime.Reset();
  }

  // デコーダ類をフラッシュ
  if (mVideoDecoder != nullptr) {
    mVideoDecoder->FlushSync();
  }
  if (mAudioDecoder != nullptr) {
    mAudioDecoder->FlushSync();
  }

  mClock.ClearStartMediaTime();
  mClock.ClearAnchorTime();

  InitStatusFlags();

  mLastVideoFrameEnd = false;
}

void
MoviePlayerCore::UpdateVideoFrameToNext()
{
  if (mVideoDecoder) {
    // mDecodedFrameNextをmDecodedFrameへスライドする
    DecodedBuffer *newFrame = mVideoFrameNext;
    mVideoFrameNext         = nullptr;
    SetVideoFrame(newFrame);
  }
}

void
MoviePlayerCore::SetVideoFrame(DecodedBuffer *newFrame)
{
  if (mVideoDecoder) {
    std::lock_guard<std::mutex> lock(mVideoFrameMutex);

    DecodedBuffer *prevFrame = mVideoFrame;
    mVideoFrame              = newFrame;

    if (prevFrame == mVideoFrame) {
      // 前フレームと同じなら何もしない
      return;
    }

    EnqueueVideo(mVideoFrame);

    // LOGV("new video frame: pts=%" PRId64 " us\n", ns_to_us(mVideoFrame->timeStampNs));

    // 前フレームがダミーフレームでなければ開放
    if (prevFrame && prevFrame != &mDummyFrame) {
      mVideoDecoder->ReleaseDecodedBufferIndex(prevFrame->bufIndex);
    }

    // 新フレームがダミーフレームではなく、
    // Audioトラックがない場合は、メディアタイムをビデオフレームのPTSで更新
    if (!IsAudioAvailable() && newFrame != &mDummyFrame) {
      int64_t mediaTimeUs = ns_to_us(mVideoFrame->timeStampNs);
      if (!mClock.IsStarted()) {
        mClock.SetStartMediaTime(mediaTimeUs);
      }
      mClock.SetPresentationTime(mediaTimeUs);

      int64_t nowUs          = get_time_us();
      int64_t nowMediaUs     = mediaTimeUs;
      int64_t durationUs     = s_to_us(1.0 / mFrameRate);
      int64_t maxMediaTimeUs = mediaTimeUs + durationUs;
      mClock.UpdateAnchorTime(nowMediaUs, nowUs, maxMediaTimeUs);
    }
  }
}

void
MoviePlayerCore::SetVideoFrameNext(DecodedBuffer *nextFrame)
{
  if (mVideoDecoder) {
    std::lock_guard<std::mutex> lock(mVideoFrameMutex);

    // フレームスキップなどで破棄するケースではnextがnon-nullで呼ばれるので
    // その場合は先に現在のnextを開放する
    if (mVideoFrameNext != nullptr) {
      mVideoDecoder->ReleaseDecodedBufferIndex(mVideoFrameNext->bufIndex);
      mVideoFrameNext = nullptr;
    }
    mVideoFrameNext = nextFrame;
  }
}

bool
MoviePlayerCore::GetVideoFrame(const DecodedBuffer **videoFrame)
{
  std::lock_guard<std::mutex> lock(mVideoFrameMutex);

  // とりあえず常に返しておいて問題はないはずなので返しておく
  *videoFrame = mVideoFrame;

  if (mVideoFrameLastGet != mVideoFrame) {
    mVideoFrameLastGet = mVideoFrame;
    return true;
  } else {
    return false;
  }
}

int64_t
MoviePlayerCore::CalcDiffVideoTimeAndNow(DecodedBuffer *targetFrame) const
{
  if (!targetFrame) {
    return -1;
  }

  int64_t mediaUs    = ns_to_us(targetFrame->timeStampNs);
  int64_t nextRealUs = mClock.GetRealTimeFor(mediaUs);
  int64_t nowUs      = get_time_us();

  return nowUs - nextRealUs;
}

void
MoviePlayerCore::EnqueueAudio(DecodedBuffer *data)
{
  std::lock_guard<std::mutex> lock(mAudioFrameMutex);

  if (mOnAudioDecodedFunc) {

    uint64_t readFrames = 0;
    int64_t mediaTimeUs = 0;

    uint64_t timeBase     = mAudioTime.base;
    uint64_t outputFrames = mAudioTime.outputFrames;
    uint64_t nextOutputFrames = outputFrames;
    uint64_t nextTimeBase     = 0;

    mOnAudioDecodedFunc(data->data, data->dataSize);

    mAudioDecoder->ReleaseDecodedBufferIndex(data->bufIndex);

    // 時刻計算処理

    if (timeBase != data->timeStampNs) {
      timeBase         = data->timeStampNs;
      outputFrames     = 0;
      nextOutputFrames = 0;
    }
    nextTimeBase = data->timeStampNs;

    uint64_t frames = data->dataSize / mAudioUnitSize;
    readFrames += frames;
    nextOutputFrames += frames;

    // EOSバッファはフラグのみの空バッファなので打ち切り
    if (data->isEndOfStream) {
      // LOGV("audio last frame END\n");
      mLastAudioFrameEnd = true;
    }

    // 現状では、ここが呼ばれた時点でそこまでの送出フレームが
    // 再生されきった状態とみなす。より正確にするには、使用している
    // オーディオエンジンの現在の再生位置を取得する必要がある。
    int64_t sampleRate = mAudioDecoder->SampleRate();
    int64_t timeOffset = calc_audio_duration_us(outputFrames, sampleRate);
    mediaTimeUs        = ns_to_us(timeBase) + timeOffset - mAudioCodecDelayUs;
    if (mediaTimeUs >= 0) {
      if (!mClock.IsStarted()) {
        mClock.SetStartMediaTime(mediaTimeUs);
      }
      mClock.SetPresentationTime(mediaTimeUs);

      int64_t durationUs     = calc_audio_duration_us(readFrames, sampleRate);
      int64_t maxMediaTimeUs = mediaTimeUs + durationUs;
      int64_t nowUs          = get_time_us();
      int64_t nowMediaUs     = mClock.GetStartMediaTime() +
                           calc_audio_duration_us(mAudioOutputFrames, sampleRate);
      mClock.UpdateAnchorTime(nowMediaUs, nowUs, maxMediaTimeUs);

      // resumu時に最速でstart/anchorを復帰するために再生終了時点のPTSを保存
      mAudioResumeMediaTimeUs = maxMediaTimeUs;
    } else {
      // negative timeはpresentationしてはいけないとmkv仕様書に記載なので読み捨て
      // (作り上すでにメモリコピーしちゃってるけど、framesReadをいじって対応)
      readFrames  = 0;
      // LOGV("* negative pts *\n");
    }

    mAudioTime.base         = nextTimeBase;
    mAudioTime.outputFrames = nextOutputFrames;

    mAudioOutputFrames += readFrames;

  } else {
    mAudioFrameQueue.push(data);
    mAudioQueuedBytes += data->dataSize;
  }
}

void
MoviePlayerCore::EnqueueVideo(DecodedBuffer *data)
{
  if (mOnVideoDecodedFunc) {
    if (data->v.width > 0 && data->v.height > 0) {
      // ビデオフレームのサイズが0x0でない場合はコールバックを呼ぶ
      mOnVideoDecodedFunc(data);
    } else {
      // サイズが0x0の場合は、コールバックは呼ばない
      LOGV("video frame size is 0x0, skip callback\n");
    }
  }
}

bool
MoviePlayerCore::GetAudioFrame(uint8_t *frames, int64_t frameCount, uint64_t *framesRead,
                               uint64_t *timeStampUs)
{
  bool hasNewFrame    = false;
  uint64_t readFrames = 0;
  int64_t mediaTimeUs = 0;

  if (IsAudioAvailable()) {
    std::lock_guard<std::mutex> lock(mAudioFrameMutex);

    // プリロード中と完走後にリセットされるまでの期間は出力を行わない
    if (IsCurrentState(STATE_PRELOADING) || mLastAudioFrameEnd) {
      readFrames  = 0;
      hasNewFrame = false;
      goto out;
    }

    uint64_t reqBytes     = frameCount * mAudioUnitSize;
    uint8_t *dst          = frames;
    uint64_t bytesToRead  = reqBytes;
    uint64_t timeBase     = mAudioTime.base;
    uint64_t outputFrames = mAudioTime.outputFrames;

    uint64_t nextOutputFrames = outputFrames;
    uint64_t nextTimeBase     = 0;

    bool first = true;
    while (bytesToRead > 0 && mAudioFrameQueue.size() > 0) {
      const DecodedBuffer *data = mAudioFrameQueue.front();

      // EOSバッファはフラグのみの空バッファなので打ち切り
      if (data->isEndOfStream) {
        // LOGV("audio last frame END\n");
        mLastAudioFrameEnd = true;
        break;
      }

      if (first) {
        // 初回のみ前回からバッファのPTSが変わってる場合にbase更新
        if (timeBase != data->timeStampNs) {
          timeBase         = data->timeStampNs;
          outputFrames     = 0;
          nextOutputFrames = 0;
        }
        first = false;
      } else {
        // 初回以外でPTSが変わっていればnextOutputFramesをリセット
        if (nextTimeBase != data->timeStampNs) {
          nextOutputFrames = 0;
        }
      }
      nextTimeBase = data->timeStampNs;

      uint64_t frames = 0;
      int remain      = data->dataSize - mAudioDataPos;
      if (bytesToRead < remain) {
        memcpy(dst, data->data + mAudioDataPos, bytesToRead);
        dst += bytesToRead;
        frames = (bytesToRead / mAudioUnitSize);
        mAudioDataPos += bytesToRead;
        bytesToRead = 0;
      } else {
        memcpy(dst, data->data + mAudioDataPos, remain);
        mAudioFrameQueue.pop();
        mAudioDecoder->ReleaseDecodedBufferIndex(data->bufIndex);
        dst += remain;
        frames = (remain / mAudioUnitSize);
        bytesToRead -= remain;
        mAudioDataPos = 0;
      }
      readFrames += frames;
      nextOutputFrames += frames;
    }
    mAudioQueuedBytes -= (reqBytes - bytesToRead);
    hasNewFrame = true;

    // 最終かつ読み込みが0フレームだった場合(ちょうどEOSバッファだけ残った場合)
    // メディアクロック系は触らずにすぐ戻る
    if (mLastAudioFrameEnd && readFrames == 0) {
      readFrames  = 0;
      hasNewFrame = false;
      goto out;
    }

    // 不足分は無音で埋める
    // TODO S16以外では無音が0でない可能性がある。S16以外に対応する場合は注意
    if (bytesToRead > 0) {
      memset(dst, 0, bytesToRead);
      readFrames += (bytesToRead / mAudioUnitSize);
      ASSERT(readFrames == frameCount, "BUG: invalid audio frame count.\n");
    }

    // 現状では、ここが呼ばれた時点でそこまでの送出フレームが
    // 再生されきった状態とみなす。より正確にするには、使用している
    // オーディオエンジンの現在の再生位置を取得する必要がある。
    int64_t sampleRate = mAudioDecoder->SampleRate();
    int64_t timeOffset = calc_audio_duration_us(outputFrames, sampleRate);
    mediaTimeUs        = ns_to_us(timeBase) + timeOffset - mAudioCodecDelayUs;
    if (mediaTimeUs >= 0) {
      if (!mClock.IsStarted()) {
        mClock.SetStartMediaTime(mediaTimeUs);
      }
      mClock.SetPresentationTime(mediaTimeUs);

      int64_t durationUs     = calc_audio_duration_us(readFrames, sampleRate);
      int64_t maxMediaTimeUs = mediaTimeUs + durationUs;
      int64_t nowUs          = get_time_us();
      int64_t nowMediaUs     = mClock.GetStartMediaTime() +
                           calc_audio_duration_us(mAudioOutputFrames, sampleRate);
      mClock.UpdateAnchorTime(nowMediaUs, nowUs, maxMediaTimeUs);

      // resumu時に最速でstart/anchorを復帰するために再生終了時点のPTSを保存
      mAudioResumeMediaTimeUs = maxMediaTimeUs;
    } else {
      // negative timeはpresentationしてはいけないとmkv仕様書に記載なので読み捨て
      // (作り上すでにメモリコピーしちゃってるけど、framesReadをいじって対応)
      readFrames  = 0;
      hasNewFrame = false;
      // LOGV("* negative pts *\n");
    }

    mAudioTime.base         = nextTimeBase;
    mAudioTime.outputFrames = nextOutputFrames;

#if 0 // DEBUG
    LOGV("time=%" PRId64 ", base=%" PRId64 ", offset=%" PRId64 "\n", mediaTimeUs,
         ns_to_us(timeBase), timeOffset);
#endif
  } else {
    // 音声トラックがない
    readFrames  = 0;
    hasNewFrame = false;
  }

out:
  mAudioOutputFrames += readFrames;
  if (framesRead) {
    *framesRead = readFrames;
  }

  if (timeStampUs) {
    *timeStampUs = mediaTimeUs;
  }

  return hasNewFrame;
}

void
MoviePlayerCore::PreLoadInput()
{
  SetState(STATE_PRELOADING);
  Post(MSG_PRELOAD);
  mEventFlag.Wait(EVENT_FLAG_PRELOADED);
}

void
MoviePlayerCore::SetState(State newState)
{
#ifdef INNER_AUDIOENGINE
  if (mAudioEngine) {
    switch (newState) {
    case STATE_PLAY:
      mAudioEngine->Start();
      break;
    case STATE_PAUSE:
    case STATE_STOP:
    case STATE_FINISH:
      mAudioEngine->Stop();
      break;
    default:
      // nothing to do.
      break;
    }
  }
#endif
  if (mState != newState) {
    mState = newState;
    if (mOnStateFunc) {
      mOnStateFunc(mState);
    }
  }
}

MoviePlayerCore::State
MoviePlayerCore::GetState() const
{
  std::lock_guard<std::mutex> lock(mApiMutex);

  return mState;
}

bool
MoviePlayerCore::IsCurrentState(State state) const
{
  return GetState() == state;
}

void
MoviePlayerCore::SetVolume(float volume)
{
#ifdef INNER_AUDIOENGINE
  if (mAudioEngine) {
    mAudioEngine->SetVolume(volume);
  }
#endif
}

float
MoviePlayerCore::Volume() const
{
  float volume = 1.0f;
#ifdef INNER_AUDIOENGINE
  if (mAudioEngine) {
    volume = mAudioEngine->Volume();
  }
#endif
  return volume;
}