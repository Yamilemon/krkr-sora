#pragma once

// ヘッダー内のインライン実装用の固定タグロガー
// (BasicLog.hは #ifdef MYLOG_TAG でログタグを処理する関係上、ヘッダ内で
// BasicLog.hを読んでしまうと、実装ファイルで指定したMYLOG_TAGが正しく反映されない
// なので、簡易的にヘッダ内のインライン実装でログを出すとき用にMYLOG_TAGを
// 固定した別マクロを用意しておく。MYLOG_TAGは効かなくなるので、インライン実装で
// メッセージを出したいときはログメッセージ中で適宜情報を埋め込むこと)

#if !defined(MOVIE_DEBUG)
#if defined(__ANDROID__)
#if !defined(NDEBUG)
#define MOVIE_DEBUG
#endif
#else
// !ANDROID
#if defined(_DEBUG) || defined(DEBUG)
#define MOVIE_DEBUG
#endif
#endif
#endif

#if !defined(INLINE_TAG)
#define INLINE_TAG "MoviePlayer_inl"
#endif

// 現状ではAndroidのみlog apiを使用して、その他ではprintfへ通す
#if defined(MOVIE_DEBUG)
#if defined(__ANDROID__)
#include <android/log.h>
#define INLINE_LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, INLINE_TAG, __VA_ARGS__)
#define INLINE_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, INLINE_TAG, __VA_ARGS__)
#define INLINE_ASSERT(_cond, ...) \
  if (!(_cond))                   \
  __android_log_assert("conditional", INLINE_TAG, __VA_ARGS__)
#else // !ANDROID
#include <cstdio>
#include <cstring>
#define INLINE_LOGV(...)        \
  {                             \
    printf("%s: ", INLINE_TAG); \
    printf(__VA_ARGS__);        \
  }
#define INLINE_LOGE(...)             \
  {                                  \
    printf("%s: ERR: ", INLINE_TAG); \
    printf(__VA_ARGS__);             \
  }
#define INLINE_ASSERT(exp, ...) \
  if (!(exp)) {                 \
    INLINE_LOGV(__VA_ARGS__);   \
    abort();                    \
  }
#endif
#else // !MOVIE_DEBUG
#define INLINE_LOGV(...)        ((void)0)
#define INLINE_LOGE(...)        ((void)0)
#define INLINE_ASSERT(exp, ...) ((void)0)
#endif
