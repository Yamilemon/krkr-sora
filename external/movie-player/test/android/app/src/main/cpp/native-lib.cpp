#include <string>
#include <thread>

#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "media/NdkMediaDataSource.h"

// #define LOG_NDEBUG 0
#define TAG       "MoviePlayer"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOG_ASSERT(_cond, ...) \
  if (!(_cond))                \
  __android_log_assert("conditional", TAG, __VA_ARGS__)

// -----------------------------------------------------------------------------
// ムービー再生テスト用コード
// -----------------------------------------------------------------------------

#include "IMoviePlayer.h"


static AAssetManager *s_assetManager = nullptr;
 
class AssetReadStream : public IMovieReadStream
{
  AAsset *mAsset;
  int mRefCount;

public:
  AssetReadStream(AAsset *asset) : mAsset(asset), mRefCount(1) {}
  virtual ~AssetReadStream()
  {
    if (mAsset) {
      AAsset_close(mAsset);
    }
  }
  virtual int AddRef(void)
  {
    return ++mRefCount;
  }
  virtual int Release(void)
  {
    int count = --mRefCount;
    if (count <= 0) {
      delete this;
    }
    return count;
  }
  virtual size_t Read(void *buf, size_t size)
  {
    int r = AAsset_read(mAsset, buf, size);
    return (r < 0) ? 0 : r;
  }
  virtual int64_t Tell() const
  {
    off_t cur = AAsset_seek(mAsset, 0, SEEK_CUR);
    return (int64_t)cur;
  }
  virtual void Seek(int64_t offset, int origin)
  {
    AAsset_seek(mAsset, (off_t)offset, origin);
  }
  virtual size_t Size() const
  {
    off64_t len = AAsset_getLength64(mAsset);
    return (size_t)len;
  }
};

class MyMoviePlayer
{
public:
  MyMoviePlayer() : m_player(nullptr), m_videoWidth(0), m_videoHeight(0), 
                    m_videoBuffer(nullptr), m_videoBufferSize(0), m_updateFlag(false)
  {

  }

  virtual ~MyMoviePlayer()
  {
    Shutdown();
  }

  void Shutdown()
  {
    if (m_player)
    {
      delete m_player;
      m_player = nullptr;
    }
    if (m_videoBuffer)
    {
      delete[] m_videoBuffer;
      m_videoBuffer = nullptr;
    }
    m_videoBufferSize = 0;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_updateFlag = false;
  }

  void UpdateVideoBuffer(int w, int h)
  {
    if (w != m_videoWidth || h != m_videoHeight)
    {
      m_videoWidth = w;
      m_videoHeight = h;
      m_videoBufferSize = w * h * 4;
      if (m_videoBuffer)
      {
        delete[] m_videoBuffer;
        m_videoBuffer = nullptr;
      }
      m_videoBuffer = new char[m_videoBufferSize];
    }
  }

  bool Create(const char *path)
  {
    Shutdown();
    if (!s_assetManager)
      return false;

    AAsset *asset = AAssetManager_open(s_assetManager, path, AASSET_MODE_RANDOM);
    if (asset)
    {
      IMovieReadStream *stream = new AssetReadStream(asset);
      IMoviePlayer::InitParam param;
      param.Init();
      param.videoColorFormat = IMoviePlayer::COLOR_RGBA;
      m_player = IMoviePlayer::CreateMoviePlayer(stream, param);
      stream->Release();

      if (m_player && m_player->IsVideoAvailable())
      {
        IMoviePlayer::VideoFormat format;
        m_player->GetVideoFormat(&format);
        UpdateVideoBuffer(format.width, format.height);

        m_player->SetOnVideoDecoded(
            [this](int w, int h, IMoviePlayer::DestUpdater updater) {
              UpdateVideoBuffer(w, h);
              updater(m_videoBuffer, w * 4);
              m_updateFlag = true;
            });
      }
    }
    return (m_player != nullptr);
  }

  jobject GetUpdatedBuffer(JNIEnv *env)
  {
    if (!m_updateFlag)
    {
      return NULL; // 更新なし
    }
    m_updateFlag = false; // フラグをクリア
    return env->NewDirectByteBuffer(m_videoBuffer, m_videoBufferSize);
  }

  int GetWidth()
  {
    return m_videoWidth;
  }

  int GetHeight()
  {
    return m_videoHeight;
  }

  bool IsPlaying()
  {
    if (m_player)
    {
      return m_player->IsPlaying();
    }
    return false;
  }

  void Play(bool loop)
  {
    if (m_player)
    {
      m_player->Play(loop);
    }
  }

  void Stop()
  {
    if (m_player)
    {
      m_player->Stop();
    }
  }

  int Duration()
  {
    if (m_player)
    {
      return m_player->Duration();
    }
    return -100;
  }

  int Position()
  {
    if (m_player)
    {
      return m_player->Position();
    }
    return -1;
  }

private:
  IMoviePlayer *m_player;
  int m_videoWidth;
  int m_videoHeight;
  char *m_videoBuffer;
  int m_videoBufferSize;
  bool m_updateFlag;
};

#ifdef __cplusplus
extern "C"
{
#endif

  static MyMoviePlayer *getHandle(JNIEnv *env, jobject thiz)
  {
    jclass clazz = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(clazz, "mNativeHandle", "J");
    if (fid == nullptr) return nullptr;
    return reinterpret_cast<MyMoviePlayer *>(env->GetLongField(thiz, fid));
  }

  static void setHandle(JNIEnv *env, jobject thiz, MyMoviePlayer *player)
  {
    jclass clazz = env->GetObjectClass(thiz);
    jfieldID fid = env->GetFieldID(clazz, "mNativeHandle", "J");
    if (fid != nullptr) {
        env->SetLongField(thiz, fid, reinterpret_cast<jlong>(player));
    }
  }

  // AssetManagerを登録
  JNIEXPORT void JNICALL Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_setAssetManager(
    JNIEnv *env, jclass clazz, jobject assetManager)
  {
    s_assetManager = AAssetManager_fromJava(env, assetManager);
  }

  // ファイル名を与えてプレイヤーをクリエイト
  JNIEXPORT jboolean JNICALL Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_createPlayer(
    JNIEnv *env, jobject thiz, jstring jpath)
  {
    jboolean success = false;

    // 指定されたファイルをオープン
    const char *tmp_path_str = env->GetStringUTFChars(jpath, nullptr);
    LOGV("open file: %s", tmp_path_str);

    MyMoviePlayer *player = getHandle(env, thiz);
    if (!player) {
         player = new MyMoviePlayer();
         setHandle(env, thiz, player);
    }

    if (player->Create(tmp_path_str)) {
        success = true;
    }

    env->ReleaseStringUTFChars(jpath, tmp_path_str);

    return success;
  }

  // 更新バッファ
  JNIEXPORT jobject JNICALL Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_getUpdatedBuffer(JNIEnv* env, jobject thiz) {
    MyMoviePlayer *player = getHandle(env, thiz);
    if (player) {
        return player->GetUpdatedBuffer(env);
    }
    return NULL; 
  }

  // width
  JNIEXPORT jint JNICALL
  Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_getWidth(JNIEnv *env, jobject thiz)
  {
    MyMoviePlayer *player = getHandle(env, thiz);
    return player ? player->GetWidth() : -1;
  }

  // height
  JNIEXPORT jint JNICALL
  Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_getHeight(JNIEnv *env, jobject thiz)
  {
    MyMoviePlayer *player = getHandle(env, thiz);
    return player ? player->GetHeight() : -1;
  }

  // 再生状態
  JNIEXPORT jboolean JNICALL
  Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_isPlaying(JNIEnv *env, jobject thiz)
  {
    MyMoviePlayer *player = getHandle(env, thiz);
    return player ? player->IsPlaying() : false;
  }

  // 再生開始
  JNIEXPORT void JNICALL Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_startPlayer(
    JNIEnv *env, jobject thiz, jboolean loop)
  {
    MyMoviePlayer *player = getHandle(env, thiz);
    if (player) {
      player->Play(loop);
    }
  }

  // 再生停止
  JNIEXPORT void JNICALL
  Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_stopPlayer(JNIEnv *env, jobject thiz)
  {
    MyMoviePlayer *player = getHandle(env, thiz);
    if (player) {
      player->Stop();
    }
  }

  // 再生停止
  JNIEXPORT void JNICALL
  Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_shutdownPlayer(JNIEnv *env, jobject thiz)
  {
    MyMoviePlayer *player = getHandle(env, thiz);
    if (player) {
      delete player;
      setHandle(env, thiz, nullptr);
    }
  }

  // duration
  JNIEXPORT jint JNICALL
  Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_getDuration(JNIEnv *env, jobject thiz)
  {
    MyMoviePlayer *player = getHandle(env, thiz);
    return player ? player->Duration() : -100;
  }

  // position
  JNIEXPORT jint JNICALL
  Java_jp_wamsoft_testmovieplayer_MyMoviePlayer_getPosition(JNIEnv *env, jobject thiz)
  {
    MyMoviePlayer *player = getHandle(env, thiz);
    return player ? player->Position() : -1;
  }

#ifdef __cplusplus
}
#endif
