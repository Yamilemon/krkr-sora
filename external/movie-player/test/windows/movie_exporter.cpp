#include <cstdio>
#include <cinttypes>

#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <filesystem> // C++17

#include "IMoviePlayer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./stb_image_write.h"

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("  Usage: %s <input file> \n", argv[0]);
    return -1;
  }
  std::string testFilePath = argv[1];

  std::filesystem::path p = testFilePath;
  std::string outBaseName = p.stem().string() + "_";

  // MoviePlayer を作成
  IMoviePlayer::InitParam param;
  param.Init();
  param.videoColorFormat = IMoviePlayer::COLOR_RGBA;
  IMoviePlayer *player   = IMoviePlayer::CreateMoviePlayer(testFilePath.c_str(), param);
  if (player == nullptr) {
    printf("Failed to create MoviePlayer! \n");
    goto finish;
  }

  {
    // テストメモ：
    // 寸法情報などは、Open()後はPlay()前に取得可能であることを想定しているので
    // テストではそのような順序になるようにしてある。将来的にもしそこでエラーに
    // なる場合はしかるべく対処すること。

    // 情報取得
    IMoviePlayer::VideoFormat vf;
    player->GetVideoFormat(&vf);
    int32_t w      = vf.width;
    int32_t h      = vf.height;
    float fps      = vf.frameRate;
    uint64_t total = player->Duration();
    printf("MOVIE: Width = %d Height = %d / Duration = %" PRId64 " us\n", w, h, total);

    // RenderFrame() で描画済みフレームを受け取るバッファを用意
    int pixelBytes  = w * h * 4;
    uint8_t *pixels = new uint8_t[pixelBytes];
    memset(pixels, 0xff, pixelBytes);

    bool updateFlag = false;
    // 描画済みフレーム受け取りコールバックを設定
    player->SetOnVideoDecoded(
        [&](int vw, int vh, IMoviePlayer::DestUpdater updater) {
          updater((char *)pixels, w * 4);
          updateFlag = true;
        });

    // 再生開始
    bool loop = false;
    player->Play(loop);

    // 疑似レンダリングループ
    // 再生終了か画面タッチで終了
    int frameCount = 0;
    while (player->IsPlaying()) {
      if (updateFlag) {
        // フレーム更新があったらテクスチャ更新
        // bmp出力(pngだと1fpsでもおっつかない
        // あと、テスト用なのでとにかく画像が出れば何でもいいので)
        std::string outFile = outBaseName + std::to_string(frameCount) + ".bmp";
        printf("%s @ time = %" PRId64 " / %" PRId64 " us \n", outFile.c_str(),
              player->Position(), total);
        stbi_write_bmp(outFile.c_str(), w, h, 4, pixels);
        frameCount++;
        updateFlag = false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 1fps
    }

    delete[] pixels;
  }

finish:
  if (player != nullptr) {
    delete player;
    player = nullptr;
  }

  return 0;
}