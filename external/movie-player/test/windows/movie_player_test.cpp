#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>

#include "GLRenderer.h"
#include "IMoviePlayer.h"

#define TEST_PRE_CONV_YUV // 固定カラーフォーマット指定を使用
// #define TEST_DIB_MODE  // DIBビットマップ想定の処理テストを行う(表示は逆さまになる)

static int64_t seekStepUs = 2'000'000; // 2s
static bool isPause       = false;
void
key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  auto player = (IMoviePlayer *)glfwGetWindowUserPointer(window);
  if (!player->IsPlaying()) {
    return;
  }
  int64_t pos = player->Position();
  if (action == GLFW_PRESS) {
    if (key == GLFW_KEY_LEFT) {
      player->Seek(pos - seekStepUs);
    } else if (key == GLFW_KEY_RIGHT) {
      player->Seek(pos + seekStepUs);
    } else if (key == GLFW_KEY_SPACE) {
      if (isPause) {
        player->Resume();
      } else {
        player->Pause();
      }
      isPause = !isPause;
    } else if (key == GLFW_KEY_ENTER) {
      player->Seek(0);
    } else if (key == GLFW_KEY_ESCAPE) {
      player->Stop();
    }
  }
}

void
scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
  // printf("xoffset=%lf, yoffset=%lf\n", xoffset, yoffset);

  auto player = (IMoviePlayer *)glfwGetWindowUserPointer(window);
  // if (!player->IsPlaying()) {
  //   return;
  // }

  if (!player->IsAudioAvailable()) {
    return; // no audio
  }

  float vol = player->Volume();
  if (yoffset > 0) {
    vol += 0.1f;
  } else if (yoffset < 0) {
    vol -= 0.1f;
  }
  player->SetVolume(std::max(std::min(1.f, vol), 0.f));

  printf("Volume: %f\n", player->Volume());
}

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("  Usage: %s <input file> \n", argv[0]);
    return -1;
  }
  std::string testFilePath = argv[1];

  // 乱数
  std::random_device rnd;
  std::mt19937 mt(rnd());

  // テスト用描画環境をセットアップ
  GLFWwindow *glfwWin = nullptr;
  Renderer renderer;
  glfwWin = renderer.InitGL(1920, 1080, "Movie Player Test App");
  renderer.InitRenderer();
  renderer.EnableVSync(true);
  renderer.SetTextureRenderScale(0.9f);

  // キー/マウスコールバック設定
  glfwSetKeyCallback(glfwWin, key_callback);
  glfwSetScrollCallback(glfwWin, scroll_callback);

  // MoviePlayer を作成
  IMoviePlayer::InitParam param;
  param.Init();
  // param.useOwnAudioEngine = false; // アプリ側でオーディオエンジンを持つ場合false
#if defined(TEST_PRE_CONV_YUV) && !defined(TEST_DIB_MODE)
  // デコード時にYUV>BGR変換まで行う
  param.videoColorFormat = IMoviePlayer::COLOR_BGRA;
#else
  // UNKNOWN/NOCONV の場合は内部でYUV変換を行わない。シェーダなどで対応すると性能的に有利
  param.videoColorFormat = IMoviePlayer::COLOR_NOCONV;
#endif
  IMoviePlayer *player = IMoviePlayer::CreateMoviePlayer(testFilePath.c_str(), param);
  if (player == nullptr) {
    printf("Failed to create MoviePlayer! \n");
    goto finish;
  }

  // GLFWウィンドウにプレイヤーを紐付ける(イベントハンドラ用)
  glfwSetWindowUserPointer(glfwWin, player);

  {
    // 情報取得
    IMoviePlayer::VideoFormat vf;
    player->GetVideoFormat(&vf);
    int32_t w      = vf.width;
    int32_t h      = vf.height;
    float fps      = vf.frameRate;
    uint64_t total = player->Duration();
    printf("MOVIE: Width = %d Height = %d Fps = %.2f\n", w, h, fps);

    // pause/resume テスト用
    std::uniform_int_distribution<> rand(0, total / 4);
    int64_t pausePoint  = rand(mt);
    int32_t pauseFrames = 120;

    // 描画済みフレームを受け取るバッファを用意
    int pixelBytes  = w * h * 4;
    uint8_t *pixels = new uint8_t[pixelBytes];
    memset(pixels, 0xff, pixelBytes);

    // 描画済みフレームのバッファを表示するためのテクスチャを準備
    // テストピクセルデータで埋めておく
    uint32_t *p = (uint32_t *)pixels;
    renderer.InitTexture(w, h);
    for (size_t y = 0; y < h; y++) {
      for (size_t x = 0; x < w; x++) {
        *p = 0xFF00FF00;
        p++;
      }
    }
    renderer.UpdateTexture(pixels, pixelBytes);
    bool updateFlag = false;

    // 描画済みフレーム受け取りコールバックを設定
    player->SetOnVideoDecoded(
        [&](int vw, int vh, IMoviePlayer::DestUpdater updater) {
          // 描画済みフレームを受け取る
#if defined(TEST_DIB_MODE)
          updater((char *)pixels + w * (h-1) * 4, -w * 4); // DIB想定で逆さま
#else
          updater((char *)pixels, w * 4);
#endif
          updateFlag = true;
        });

    // 再生開始
    bool pausing = false;
    bool loop    = true;
    player->Play(loop);

    // 疑似レンダリングループ
    // 再生終了か画面タッチで終了
    int frameCount = 0;
    while (!glfwWindowShouldClose(glfwWin)) {
      if (!player->IsPlaying()) {
        printf("MoviePlayerTest: play finished\n");
        break;
      }
      if (updateFlag) {
        renderer.UpdateTexture(pixels, pixelBytes);
        updateFlag = false;
      } 
      renderer.Render(frameCount);

      glfwSwapBuffers(glfwWin);
      glfwPollEvents();
      frameCount++;
      // printf(" Time = %" PRId64 " / %" PRId64 " usec \n", player->Position(), total);
    }

    delete[] pixels;
  }

finish:
  if (player != nullptr) {
    delete player;
    player = nullptr;
  }

  renderer.DoneGL();

  return 0;
}