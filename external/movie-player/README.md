# About

WebM 動画再生ライブラリです

# LICENSE

このライブラリ自体は MIT License です。利用してるライブラリはそれぞれのライセンスに従ってください。

# 実装概要

細かい事項については各アーキテクチャごとに後ろの方に別項があるので参照してください。

- Android
  - NDK の MediaExtractor+MediaCodec で対応

- Windows (汎用実装)
  - nestegg + libvpx で cpu でのデコードで対応
  - miniaudio を内蔵して音声対応
  - CPU ベース処理で generic な作りなので、別アーキテクチャでも動作可能
    - `windows/` ディレクトリでやってるので、本格的に linux とか MacOS への対応する場合はディレクトリ名をなんか考えたい

## 音声対応

デフォルトは内臓された miniaudio で再生します（AudioEngine.cpp）

cmake 時に MOVIEPLAYER_EXTERNAL_MINIAUDIO 指定の場合は、別途本体側でバージョンが合った miniaudio 実体を準備したうえで、
以下の関数を準備する必要があります。

```
extern ma_engine *GetMiniAudioEngine();
```

cmake 時に MOVIEPLAYER_EXTERNAL_SDLAUDIO 指定の場合は SDL3 での再生になります。SDL3をあわせてリンクする必要があります。

## 把握している問題

- 共通
  - pause/resume したときに、復帰後の数フレームがフレームスキップ扱いになる
  - ※YUVテクスチャ処理がうまく対応できてない? 現在 ARBG 以外だと動作不良かも
- Linux
  - 確認した WSL 環境では movie_player_test での描画が正しく行われない


# ディレクトリ構造

- `src/`
- `include/`
  - ライブラリコードです。
- `extlibs/`
  - 使用している外部ライブラリコードです。
- `test/`
  - 各機種用の再生テストコードです。
- `test/dat`
  - 再生テスト用の webm 動画です。プロジェクトの都合上、
    テストプログラムのツリー側にコピーして使ってたりします(Android とか)。

# コード構造

## 共通構造

`include/IMoviePlayer.h` に `IMoviePlayer` を定義してあります。
これを継承して各アーキテクチャ用の `MoviePlayer.h`に
実装クラスの定義を配置してあります。

### 利用方法

```
static IMoviePlayer *CreateMoviePlayer(const char *filename, InitParam &param);

static IMoviePlayer *CreateMoviePlayer(IMovieReadStream *stream, InitParam &param);
```

`IMoviePlayer`のインスタンスを作成して使用します。
`param.videoColorFormat` に出力したいカラーフォーマットを指定してください。

それぞれ生成した後に、
`SetOnState`, `SetOnVideoDecoded` で、ステート取得およびビデオ描画
データ取得用のメソッドを登録してから `Play` で再生開始します。

## Windows 対応について

### 設計方針

CPU 処理する 汎用ライブラリを使用して c++11 の std メインで組んでいるので
Windows 以外にもそのまま持っていけるように作っています。

内部の構造としては、Android が stagefright 由来ぽい形に
なっているので、Win 版もそれに寄せた作りにしてあります
(Extractor ＋ Decoder)。

#### Seek 処理について

Seek は最近傍の Cue ポイント(キーフレーム)へジャンプする仕様になっています。
デコーダの仕様上、キーフレームに飛ばないと正しくデコードできないためです。
(これは Win 版に限らずこのようになっています)

### ライブラリのビルド方法

vcpkg + cmake の環境を想定しています。
ルートの CMakeLists.txt を使用してビルドしてください。

```bash
# windows用
cmake --preset x64-windows
cmake --build build/x64-windows --config Release

# linux
cmake --preset x64-linux
cmake --build build/x64-linux --config Release

# android ライブラリだけビルドの場合
# ※通常はプロジェクト側から直接 cmake 参照でビルドさせます
cmake --preset arm64-android
cmake --build build/arm64-android --config Release
```

### テストコード

テストコードは現状 2 つ用意してあります。

- `tests/windows/movie_player_test.cpp`
  - OpenGL で描画するテスト
    - LEFT/RIGHT キー: 2 秒毎のシーク
      - ※直近キーフレームへのシークなのでシーク間隔が大きすぎると期待した動作をしません(5 秒間隔だったりすると、2 秒シークでは最近傍が一生現在地に吸い付きます)
    - SPACE: ポーズトグル
    - ENTER: 先頭へ巻き戻し
    - ESCAPE: 再生停止してそのままテストプログラムが終了
    - マウスホイール上下: 音声ボリューム上下
- `tests/windows/movie_exporter.cpp`
  - 動画を一定間隔(1 秒)ごとに BMP 出力するテスト

`tests/windows/CMakeLists.txt` で両方同時にビルドされるようにしてあります。

※上位の CMakeLists.tx から -DBUILD_TEST=ON であわせてビルドされます

### 採用ライブラリ等

原則 vcpkg で参照します。

- libvpx / vcpkg / Windows 版のみ
- libogg / vcpkg / Windows 版のみ
- libvorbis / vcpkg / Windows 版のみ
- libopus / vcpkg / Windows 版のみ
- libyuv / 自前(`extlibs/`のもの)
- nestegg / 自前(`extlibs/`のもの) / Windows 版のみ
- miniaudio / vcpkg

なるべく vcpkg で揃える方針で作業しています。
libyuv については、他アーキテクチャの対応もありどのみち自前で抱えているので、
それらに揃えるということで Win でも自前で抱えたものを参照するようにしてあります。

### libyuv の VisualStudio ビルドに関する問題

libyuv には SSE/AVX のアセンブリ実装が含まれるのですが、
これらは **VisualStudio では 32bit ビルド以外は非対応** となっています。
インラインアセンブラで書かれているせいだと思われます。
MSVC 用のイントリンジクスによるコードも部分的にあるようなのですが、
本ライブラリの利用範囲では対応していません。

clang ではコンパイルできるコードが用意されているため、
Windows/x64 に関してのみ、clang で prebuild したバイナリを参照するような
形にしてあります。

`extlibs/prebuild/win64` が当該バイナリの配置場所となります。

### libyuv の prebuild バイナリの更新方法

Visual Studio 2019 を想定しています。
Clang サポートは C++ 環境でも標準ではインストールされないと思うので
Visual Studio Installer から追加インストールしておいてください。
以下の 2 パッケージを導入すれば問題ないと思います。

- v142 ビルドツール用 C++ Clang-cl (x64/x86)
- Windows 用 C++ Clang コンパイラ

VS2019 以降の環境ではどうなるかは不明ですが、
相当するパッケージを導入して対応可能だと思います。

`extlibs/make_prebuild.sh` を vcvars64(VisualStudio の x64 用
コマンドプロンプト環境セットアップバッチ)
相当の環境にした msys あるいは cygwin 上で実行してください。
必要なビルドを行い、`extlibs/prebuild/win64` 配下のバイナリファイルを更新します。

VS のコマンドプロンプト(いわゆる DOS 窓)から作業を行う場合は
シェルスクリプト内のコマンドを適宜コピペ実行するなり
バッチ(.bat)化するなりして対応してください。

現在 git ツリーに配置してあるバイナリは VS2019 の Clang(12.0.0)
で作成したものとなります。

## Linux(Ununtu)対応について

Windows 版は、実態としては Windows に依存するコードは含まないか
あるいは部分的に ifdef で対応しているため、vcpkg の対応している環境上では
そのままビルドできる状態になっています。
これにより Linux でのビルドについても一応対応してあります。

※WSL2(WSLg)環境で movie_player_test が動作することまでは確認していますが、
描画が正しく行われてなくて調査中

※ソース類のディレクトリ名が`windows/`のまま参照しています。
将来的には `generic/` とか `common/` とかあるいは `src/` 直下に展開とか
なんかそういう感じに移動したいですが、暫定的にそのままのディレクトリ名です。

WSL2 の Ubuntu22.04 でライブラリ部をビルド確認した際の
ステップバイステップの記録です。

- vcpkg を導入する
  - https://github.com/microsoft/vcpkg のドキュメント通りに導入します
  - 必要な前提パッケージについてもドキュメント内にあるので参照してください
- `VCPKG_ROOT`設定
  - `export VCPKG_ROOT=/foo/bar/vcpkg`
- 依存ライブラリをインストールする
  - `./vcpkg/vcpkg install libvpx libogg libvorbis opus`
  - libvpx が nasm と pkgconfig を要求するので導入
    - `sudo apt-get install nasm pkgconfig`
- cmake が無いのでインストール
  - `sudo apt-get install cmake`
- プロジェクトルートにビルド用の一時ディレクトリを作成してビルド
  - `mkdir ubuntu`
  - `cd ubuntu/`
  - `cmake ..`
  - `cmake --build . -j`
- テストプログラムのビルドはさらに OpenGL 関係のパッケージを導入
  - `./vcpkg/vcpkg install glfw3 glew`
    - 必要なら以下のパッケージを導入
    - glfw3 が要求
      - `sudo apt-get install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev`
    - glew が要求
      - `sudo apt-get install libxmu-dev libxi-dev libgl-dev`
  - `cd test/`
  - `mkdir ubuntu`
  - `cd ubuntu/`
  - `cmake ../windows`
  - `cmake --build . -j`

## Android 対応について

NDK の MediaExtractor + MediaCodec を使用してフル C++で組んであります。

デコーダは内部で std::thread を使用してスレッド化してあります。
MoviewPlayer のインスタンスごとにスレッドが生まれる感じになります。

### 要求 API バージョン

AMediaDataSource 他を扱う関係で API29 (Android 10.0) 以上を要求します。

### テストプログラム

`test/android` を AndroidStudio で開いてビルド、実行してください。
cmake で直接トップフォルダを参照してビルドされます

### libyuv

MediaCodec の出力は YUV なので RGB への変換に libyuv を導入してあります(`extlibs/libyuv`)。

テストコードはエミュレータ(Pixel3)でのみ確認していますが
実機だとなにか想定外のフォーマットで出力されてくる可能性があります。
