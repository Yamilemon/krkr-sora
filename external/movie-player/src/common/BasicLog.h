#pragma once

// 注意
// MYLOG_TAGをdefineで指定する関係上、BasicLog.hは基本的に実装ファイルからしか
// 呼ばないでください(ヘッダ中で呼ぶと最前以外のMYLOG_TAGが効かなくなる)
// インライン実装でログしたい場合はInlineLog.hを使用してください。

#if !defined(MOVIE_DEBUG)
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#define MOVIE_DEBUG
#endif
#endif

// ログタグ
#if !defined(MYLOG_TAG)
#define MYLOG_TAG "MoviePlayer"
#endif

#if defined(MOVIE_DEBUG)
#if defined(__ANDROID__)
#include <android/log.h>
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, MYLOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, MYLOG_TAG, __VA_ARGS__)
#define ASSERT(_cond, ...) \
  if (!(_cond))            \
  __android_log_assert("conditional", MYLOG_TAG, __VA_ARGS__)

#else // !ANDROID
#include <cstdio>
#define LOGV(...)              \
  {                            \
    printf("%s: ", MYLOG_TAG); \
    printf(__VA_ARGS__);       \
  }
#define LOGE(...)                   \
  {                                 \
    printf("%s: ERR: ", MYLOG_TAG); \
    printf(__VA_ARGS__);            \
  }
#define ASSERT(exp, ...) \
  if (!(exp)) {          \
    LOGV(__VA_ARGS__);   \
    abort();             \
  }
// nestegg版用の各種デバッグフラグ
// #define DEBUG_INFO_EVENTFLAG   // EventFlagトレース
// #define DEBUG_INFO_NESTEGG     // nesteggロガー
// #define DEBUG_PERF_DECODER     // デコーダ性能チェック
// #define DEBUG_PERF_PIXELCONV   // ピクセル変換性能チェック
// #define DEBUG_INFO_PACKET      // 入力パケット情報
// #define DEBUG_INFO_DECODER     // デコーダ情報
// #define DEBUG_INFO_CUES        // キュー(キーフレーム)情報
// #define DEBUG_INFO_MEDIACLOCK  // MEdiaClock情報
#endif

#else
// !MOVIE_DEBUG
#define LOGV(...)        ((void)0)
#define LOGE(...)        ((void)0)
#define ASSERT(exp, ...) ((void)0)
#endif

#if 0
// MEMO 現状使ってないはずだけど一応デバッグ作業で使いたいときがあるかも？
//      __FILE__マクロがフルパスになるので末尾のファイル名だけを切り出すやつ
#include <cstring>
#if defined(_WIN32) || defined(_WIN64)
#define __FILEX__ (strrchr(__FILE__, '\\') + 1)
#else
#define __FILEX__ (strrchr(__FILE__, '/') + 1)
#endif
#endif
