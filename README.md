# 吉里吉里Z multi platform

## GPU Canvas

### 编译方法

```bash
cmake --build --preset x86-windows-win --config Release --target krkrz
```

### 测试脚本

```tjs
System.exitOnWindowClose = true;

var win = new Window();
win.width = 800;
win.height = 600;
win.caption = "我他妈直接用canvas绘制";
win.visible = true;

// steam测试，用canvas进行绘制的窗口可以正常呼出steam overlay
// 避免了layer的懒加载从而导致窗口不刷新的问题
// Plugins.link("krkrsteam-d.dll");

// Canvas即时绘制接口，Node/Scene只保存逻辑状态
// 真正的绘制是在OGLDrawDevice.onDraw中。
class CanvasNode {
    var parent = null;
    var children = [];
    var serial = 0;

    var x = 0;
    var y = 0;
    var scaleX = 1.0;
    var scaleY = 1.0;
    var rotation = 0.0; // 角度
    var z = 0;
    var opacity = 1.0;
    var visible = true;

    function addChild(node) {
        if (node.parent != null) node.parent.removeChild(node);
        node.parent = this;
        node.serial = children.count;
        children.push(node);
        children.sort(function(a, b) {
            if (a.z != b.z) return a.z - b.z;
            return a.serial - b.serial;
        });
        return node;
    }

    function removeChild(node) {
        for (var i = 0; i < children.count; i++) {
            if (children[i] == node) {
                children.erase(i);
                node.parent = null;
                return;
            }
        }
    }

    // parent*local，数组格式为：[m11, m12, m21, m22, tx, ty]。
    function makeWorldTransform(parentTransform) {
        var c = Math.cos(rotation);
        var s = Math.sin(rotation);
        var a = c * scaleX;
        var b = s * scaleX;
        var d = c * scaleY;
        var e = -s * scaleY;

        return [
            parentTransform[0] * a + parentTransform[2] * b,
            parentTransform[1] * a + parentTransform[3] * b,
            parentTransform[0] * e + parentTransform[2] * d,
            parentTransform[1] * e + parentTransform[3] * d,
            parentTransform[0] * x + parentTransform[2] * y + parentTransform[4],
            parentTransform[1] * x + parentTransform[3] * y + parentTransform[5]
        ];
    }

    function draw(canvas, parentTransform, parentOpacity) {
        if (!visible || opacity <= 0.0) return;

        var worldTransform = makeWorldTransform(parentTransform);
        var worldOpacity = parentOpacity * opacity;

        drawSelf(canvas, worldTransform, worldOpacity);
        for (var i = 0; i < children.count; i++) {
            children[i].draw(canvas, worldTransform, worldOpacity);
        }
    }

    // Group 节点本身不画任何东西，Sprite子类要重写这个方法。
    function drawSelf(canvas, transform, alpha) {}
}

class CanvasSprite extends CanvasNode {
    var texture = null;
    var shader = null;
    var matrix = new Matrix32();

    function drawSelf(canvas, transform, alpha) {
        if (texture == null) return;

        matrix.set(transform[0], transform[1], transform[2], transform[3],
            transform[4], transform[5]);
        canvas.matrix = matrix;

        if (shader != null) {
            shader.uOpacity = alpha;
            canvas.drawTexture(texture, shader);
        } else {
            canvas.drawTexture(texture);
        }
    }
}

class CanvasScene {
    var root = new CanvasNode();

    function draw(canvas) {
        root.draw(canvas, [1.0, 0.0, 0.0, 1.0, 0.0, 0.0], 1.0);
    }
}

class oglDD extends Window.OGLDrawDevice {
    var fadeShader = null;
    var tex = null;
    var off = null;
    var phase = 0.0;

    var offscreenScene = new CanvasScene();
    var mainScene = new CanvasScene();
    var offscreenGroup = new CanvasNode();
    var movingGroup = new CanvasNode();

    function oglDD() {
        // 不要用Window.OGLDrawDevice，直接OGLDrawDevice就行，太坏了渡边老贼
        super.OGLDrawDevice();
    }

    function onInit() {
        tex = new Texture("22.jpg");
        off = new Offscreen(512, 512);

        // 透明度shader，底层没有透明度设置的接口，这里只能这么写
        // 其他的类似：灰度、色相、亮度、对比度、染色，模糊、锐化、马赛克、描边，mask、规则图转场、溶解、扭曲，混合
        // 应该也是要shader
        fadeShader = new ShaderProgram(
            "attribute vec2 a_pos;" +
            "attribute vec2 a_texCoord;" +
            "uniform mat4 a_modelMat4;" +
            "uniform vec2 a_size;" +
            "varying vec2 v_texCoord;" +
            "void main() {" +
            "  mat4 ortho = mat4(" +
            "    vec4(2.0/a_size.x, 0.0, 0.0, 0.0)," +
            "    vec4(0.0, -2.0/a_size.y, 0.0, 0.0)," +
            "    vec4(0.0, 0.0, -1.0, 0.0)," +
            "    vec4(-1.0, 1.0, 0.0, 1.0));" +
            "  gl_Position = ortho * a_modelMat4 * vec4(a_pos, 0.0, 1.0);" +
            "  v_texCoord = a_texCoord;" +
            "}",
            "precision mediump float;" +
            "varying vec2 v_texCoord;" +
            "uniform sampler2D s_tex0;" + // s_tex0类似↓，uniform声明的都是
            "uniform float uOpacity;" + // 这里的uOpacity对象会映射到tjs上可进行设置
            "void main() {" +
            "  vec4 color = texture2D(s_tex0, v_texCoord);" +
            "  color.a *= uOpacity;" +
            "  gl_FragColor = color;" +
            "}",
            0, 0
        );

        // 父子关系1：offscreenScene->offscreenGroup->sourceSprite。
        // 移动offscreenGroup时，sourceSprite会跟随它。
        offscreenGroup.x = 0;
        offscreenGroup.y = 0;
        offscreenScene.root.addChild(offscreenGroup);

        var sourceSprite = new CanvasSprite();
        sourceSprite.texture = tex;
        sourceSprite.x = 106;
        sourceSprite.y = 157;
        // 22.jpg is 600x450. Fit it into the remaining 406x406 FBO area.
        sourceSprite.scaleX = 406.0 / 600.0;
        sourceSprite.scaleY = 406.0 / 600.0;
        offscreenGroup.addChild(sourceSprite);

        // 父子关系2：mainScene->movingGroup->offscreenSprite。
        // 动画修改movingGroup，子图片会同时继承位移和透明度。
        movingGroup.z = 100;
        mainScene.root.addChild(movingGroup);

        var offscreenSprite = new CanvasSprite();
        offscreenSprite.texture = off;
        offscreenSprite.shader = fadeShader;
        movingGroup.addChild(offscreenSprite);
    }

    function onDraw() {
        if (tex == null || off == null || fadeShader == null) return;

        var canvas = this.canvas;

        // 先把子场景画到离屏 FBO。
        canvas.renderTarget = off;
        canvas.blendMode = 1; // bmOpaque
        canvas.clear(0xff203050);
        offscreenScene.draw(canvas);

        // 然后将离屏画面作为movingGroup的子节点绘制到窗口。
        canvas.renderTarget = null;
        canvas.blendMode = 2; // bmAlpha
        canvas.clear(0xff202020);
        mainScene.draw(canvas);
    }

    function updateScene(dt) {
        phase += dt * 0.0022;
        movingGroup.x = 144 + Math.sin(phase) * 110;
        movingGroup.y = 44 + Math.cos(phase * 0.7) * 35;
        movingGroup.opacity = 0.15 + (Math.sin(phase * 1.4) + 1.0) * 0.425;
    }
}

var ogl = new oglDD();
ogl.createCanvas();

var lastTick = -1;
function onAnimationTimer() {
    // 窗口不可用（关了）就别调用定时器了
    if (!isvalid win) {
        timer.enabled = false;
        return;
    }

    var tick = System.getTickCount();
    if (lastTick < 0) lastTick = tick;
    var dt = tick - lastTick;
    lastTick = tick;
    if (dt > 100) dt = 100;

    ogl.updateScene(dt);
    win.requestUpdate();
}

// 应该能改用addContinuousHandler，不过估计没啥用，差不多的玩意
var timer = new Timer(onAnimationTimer, "");
timer.interval = 16;
timer.enabled = true;

win.drawDevice = ogl;

// 关闭窗口时把timer也停了
win.onCloseQuery = function(canClose) {
    timer.enabled = false;
    System.terminate();
};
```

### 运行要求

libEGL.dll 和 libGLESv2.dll 支持，放到 exe 同目录下。

## 概要

マルチプラットフォーム展開を想定した吉里吉里Zです

- システム基本制御は SDL3 を使います
- OpenGLベース描画機構を持ちます Canvas/Screen/Texture/Shader
- 極力外部ライブラリを参照する形で構築されています。

外部ライブラリの参照には vcpkg を利用しています。
SDL3 は最新版を利用する関係で FettchContents で処理されます。

## 開発環境準備

### Windows

Windows用に Visual Studio をインストールして
C++ コンパイラ を使える状態にしておきます。

あわせて Visual Studio 付属の Cmake / Ninja を利用します。

make を使いたい場合は、msys2 をインストールして基礎開発ツールを導入しておきます。

```bash
pacman -S base-devel
```

### Linux

整備中

### OSX

整備中

### vcpkg 環境準備

各環境に vcpkg を導入します。

※Visual Studio 2022 以降は vcpkg があわせて導入されます。
自前環境を使う場合は競合してまうのでどちらかでいれるようにしてください。

https://learn.microsoft.com/ja-jp/vcpkg/get_started/overview

vcpkg のフォルダを環境変数 VCPKG_ROOT に設定しておきます。

```bash
# dos
set VCPKG_ROOT="c:\work\vcpkg"

# msys/cygwin
export VCPKG_ROOT='c:\work\vcpkg'
```

## ビルド

### ソースのチェックアウト

git clone 後 submodule 更新しておいてください

```bash
git submodule update --init
```

### ビルド

CMakePresets.json 中のプリセット定義をつかってビルドします。
必要なライブラリは vcpkg.json によってセットアップされます。

ビルドフォルダはデフォルトでは build/プリセット名 になっています。
また Generator は Ninja Multi Config での生成になります。

vpkg.json で外部ライブラリを扱うため、
CMAKE_TOOLCHAIN_FILE は vcpkg のものが指定されています。

```bash
cmake --preset x86-windows --config Release
cmake --build build/x86-windows
```

ビルドに必要な定義が行われた Makefile が準備されていいます。
make が使える環境ではこちらが利用可能です

```bash
# 構築対象 preset設定（未定義時はOSで自動判定）
export PRESET=x86-windows
# ビルドタイプ指定（未定義時は Release）
export BUILD_TYPE=Release
#export BUILD_TYPE=Debug

# cmake オプション指定
# KRKRZ_USE_SJIS  デフォルトをSJIS(MBSC) にする
export CMAKEOPT="-DKRKRZ_USE_SJIS=ON"

# cmake プロジェクト生成
# この段階で vcpkg が処理されてライブラリが準備されます
make prebuild

# cmake でビルド
make build

# サンプル実行
make run

# インストール処理
INSTALL_PREFIX=install make install
```

### ビルド設定

処理内容詳細は Makefile と CMakeList.txt を参照して下さい。

ビルド用の以下の特殊な CMake変数があります

| 変数 | 説明 |
|------|------|
| `KRKRZ_VARIANT=WIN` | 旧来のWindows版準拠で構築します |
| `KRKRZ_VARIANT=SDL` | SDLバージョンで作成します（デフォルト） |
| `KRKRZ_VARIANT=LIB` | ライブラリ版KRKRZを作成します |

KRKRZ_VARIANT=SDL / LIB では、旧来の Windows版固有の機能が排除
された GENERICバージョンの吉里吉里になります。

GENERICバージョンあわせのプラグインをビルドする場合は、tp_stub.h を
読み込む前に __GENERIC__ を定義しておく必要があるので注意してください。

tp_stub/krkrz.cmake を使う場合は KRKRZ_VARIANT が定義されている場合は
自動で __GENERIC__ が追加されます。

※特に変数指定がない場合、tp_stub.h は __WINVER__ を定義して
旧WIN版互換あわせでの動作になります。

### そのほか特殊変数

**MASTER**
: ビルド時に定義されているとログレベルが WARNING で固定になります（INFOログがコンソール表示されなくなります）

    未定義時は、起動時ログレベルが Release 版は INFO、Debug版は DEBUG になります。
    起動時オプション -loglevel=ERROR,WARNING,INFO,DEBUG,VERBOSE で変更可能になります

**KRKRZ_REPL**
: 対話型 TJS REPL 機能のビルドスイッチ。Win / Mac / Linux ではデフォルト ON、
    それ以外 (Android, iOS) では OFF。詳細は [doc/REPL.md](doc/REPL.md) 参照。
    機能ON の場合は起動時オプション -repl でコンソールで REPL が起動します。

ログ処理の仕組み、ファイル出力、TJS から見た API 等は
[doc/Logging.md](doc/Logging.md) を参照してください。

### テスト実行

Makefile にそのままトップフォルダで実行可能なルールが定義されています。

```bash
# cmake 経由で実行
make run
```

WINVER で OpenGL 機能動作時は以下のファイル構成が必要になります

```
plugin/                     プラグインフォルダ
  libEGL.dll                OpenGL の egl用DLL
  libGLESv2.dll             OpenGL の GLES2用DLL
plugin64/                   プラグインフォルダ 64bit
  libEGL.dll                OpenGL の egl用DLL
  libGLESv2.dll             OpenGL の GLES2用DLL
```

SDL 版は OS側で OpenGLES 実装が存在する場合はそれが使われますが
無い場合は同様の DLL が必要になります

### SIMDパリティテスト

`tests/simd_parity_test.cpp` に画像処理SIMD（SSE2 / AVX2 / NEON）と
C リファレンス実装の出力を byte 単位で比較する CTest テスト
（`krkrz_simd_parity_test` / テスト名 `simd_parity`）が用意されています。

このテストは `tvpgl.c` / `blend_function.cpp` / 各 `*_sse2.cpp` /
`*_avx2.cpp` / `*_neon.cpp` / `detect_cpu.cpp` 等 SIMD コアのみを直接
リンクするスタンドアロンターゲットで、SDL3 / OpenGL / vcpkg のランタイム
依存はありません。`KRKRZ_BUILD_TESTS=ON`（デフォルト）かつターゲットアーキ
テクチャが x86 系または ARM 系のときに有効化されます。

```bash
# Makefile 経由 (prebuild 済みであること)
make test

# cmake / ctest 直接実行
cmake --build $(BUILD_PATH) --config Release --target krkrz_simd_parity_test
ctest --test-dir $(BUILD_PATH) -C Release -R simd_parity --output-on-failure
```

期待される出力:

- x86 (Windows / Linux / macOS): `[SSE2 vs C reference]` と
  `[AVX2 vs C reference]` の 2 セクションが走り、それぞれ全項目 pass。
- ARM / ARM64 (Linux / Android): `[NEON vs C reference]` セクションが走る。

PsBlend ファミリは SSE2 側が 7bit 量子化のため harness 側で
`tol_alpha=-1, tol_rgb=2`（ColorDodge5 のみ `tol_rgb=8`）の tolerance
policy が適用されます。それ以外は byte-exact 比較です。

### DAP スモークテスト

`tests/dap_smoke.py` は krkrz の DAP サーバ動作を最小確認する Python
スクリプトです。`-dap=<port>` で krkrz を起動し、TCP 経由で initialize /
attach / evaluate / scopes / variables / step 系 / disconnect の往復が
正常応答することを VSCode 拡張なしで検証します。

```bash
python tests/dap_smoke.py build/x64-windows-sdl/Release/krkrz64.exe data
```

最終行に `[smoke] PASS: all phases verified` が出れば OK。

## デバッグ実行

### VisualStudio でのデバッグ

以下の手順でソースデバッグできます

- Visual Studio を起動して、プロジェクトなしの状態のウインドウに実行ファイルをドロップする
- デバッグのプロパティの作業フォルダにプロジェクトフォルダを指定（プラグインフォルダの参照先になるため）
- デバッグのプロパティの引数に data フォルダの場所をフルパスで指定（現行仕様がexe相対もしくは絶対パス）

### VSCode でのデバッグ (C++ ネイティブ)

C++ レベルでデバッグする場合は次のような launch.json を準備します。
program 部分に生成される実行ファイルのパス名を直接記載します。
args で処理対象フォルダを指定できます（フルパスになるように記載して下さい）

launch.json

```json
{
    // IntelliSense を使用して利用可能な属性を学べます。
    // 既存の属性の説明をホバーして表示します。
    // 詳細情報は次を確認してください: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "WINデバッグ起動",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "build/x86-windows/Debug/krkrz.exe",
            "args": ["${workspaceFolder}/data"],
            "stopAtEntry": false,
            "console":"externalTerminal",
            "cwd": "${workspaceFolder}",
            "environment": []
        }
    ]
}
```

### VSCode + DAP による TJS スクリプトデバッグ

吉里吉里Z は Debug Adapter Protocol (DAP) サーバを内蔵しており、
専用の VSCode 拡張 [krkrz-vscode](https://github.com/wamsoft/krkrz-vscode)
を使うと TJS2 スクリプトを通常のプログラミング言語と同じ感覚でデバッグ
できます (BP / ステップ実行 / コールスタック / 変数 inspect / 条件付き BP /
log point / Watch 式評価 など)。

起動例:

```bash
krkrz64.exe -dap=6635 ${workspaceFolder}/data
```

VSCode 側で `krkrz` 拡張をインストールし、`launch.json` に attach 設定を
追加するだけで接続できます。

ビルド時オプション `KRKRZ_ENABLE_DAP` (デフォルト ON) を OFF にすると
DAP 関連コードは全て `#ifdef` で除外されます。

詳細な使い方・既知制限・拡張のビルド方法は [krkrz-vscode の README](https://github.com/wamsoft/krkrz-vscode) を参照してください。

TJS2 / KAG (.ks) のシンタックスハイライトも同拡張に同梱されています。
KAG (.ks) 行への BP は仕様上対応不可ですが、`[iscript]...[endscript]` 内の
TJS なら BP 設置可能です。

## その他情報

### 自動生成ファイル

吉里吉里Z本体にはいくつかの自動生成ファイルが存在します。
自動生成ファイルは直接編集せず、生成元のファイルを編集します。
生成には主にbatファイルとperlが使用されているので、perlのインストールが必要です。
各生成ファイルを左に ':' 以降に生成元ファイルを列挙します。

tjs2/syntax/compile.bat で以下のファイルが生成されます。

| 生成ファイル | 生成元 |
|-------------|--------|
| tjs.tab.cpp / tjs.tab.hpp | tjs.y |
| tjsdate.tab.cpp / tjsdate.tab.hpp | tjsdate.y |
| tjspp.tab.cpp / tjspp.tab.hpp | tjspp.y |
| tjsDateWordMap.cc | gen_wordtable.bat |

これらのファイルの生成には bison が必要です。
bison には libiconv2.dll libintl3.dll regex2.dll が必要なので一緒にインストールする必要があります。

- http://gnuwin32.sourceforge.net/packages/bison.htm
- http://gnuwin32.sourceforge.net/packages/libintl.htm
- http://gnuwin32.sourceforge.net/packages/libiconv.htm
- http://gnuwin32.sourceforge.net/packages/regex.htm

visual/glgen/gengl.bat で以下のファイルが生成されます。

| 生成ファイル | 生成元 |
|-------------|--------|
| tvpgl.c / tvpgl.h | maketab.c / tvpps.c |

base/win32/makestub.bat で以下のファイルが生成されます。

| 生成ファイル | 生成元 |
|-------------|--------|
| FuncStubs.cpp / FuncStubs.h | makestub.pl内で指定されたヘッダーファイル内のTJS_EXP_FUNC_DEF/TVP_GL_FUNC_PTR_EXTERN_DECLマクロで記述された関数 |
| tp_stub.cpp / tp_stub.h | 同上 |