package jp.wamsoft.testmovieplayer;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.FileDescriptor;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.Random;

/**
 * movieplayerのテストプログラム ムービーサイズでBitmapを作成して、SurfaceViewのCanvasに描画する ビットマップのストレッチは通常ではテクスチャ描画で行うはずだが
 * Bitmapだと面倒なのでこのテストでは何もせずにそのまま見切れた状態で描画する。
 */

public class TestMovieView extends SurfaceView implements SurfaceHolder.Callback {
    Random random = new Random();
    Thread thread = null; // 表示用スレッド
    Bitmap movieBitmap; // ムービー用ビットマップ

    int width = 0;
    int height = 0;

    // ファイルはリソースから取得する
    private static Resources mResources;

    public TestMovieView(Context context) {
        super(context);
        init(context);
    }

    public TestMovieView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public TestMovieView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context);
    }

    public TestMovieView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init(context);
    }

    private void init(Context context) {
        getHolder().addCallback(this);
        mResources = context.getResources();
    }

    // 画像を表示するスレッド
    private class DrawThread extends Thread {
        public void run() {
            SurfaceHolder holder = getHolder();
            while (mPlayer != null && mPlayer.isPlaying()) {
                if (updateMovie()) {
                    Canvas canvas = holder.lockCanvas();
                    if (canvas != null) {
                        drawMovie(canvas);
                        holder.unlockCanvasAndPost(canvas);
                    }
                }
                try {
                    sleep(10);
                } catch (InterruptedException e) {
                }
            }
            Log.d("TestMovieView", "******* isPlaying() done ******");
        }
    }

    // ムービーセットアップ
    private void setupMovie() {

        if (mPlayer == null) {
            mPlayer = new MyMoviePlayer();
        }
        //mPlayer.createPlayer("Big_Buck_Bunny_1080_10s_1MB_VP8.webm");
        mPlayer.createPlayer("title.webm");

        width = mPlayer.getWidth();
        height = mPlayer.getHeight();

        // ピクセルのパック表現@LE: 0xAABBGGRR
        // (A & 0xff) << 24 | (B & 0xff) << 16 | (G & 0xff) << 8 | (R & 0xff);
        movieBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);

        // 開始
        boolean loop = true;
        mPlayer.startPlayer(loop);
    }

    // レンダリングされた動画フレームを取得してBitmapにコピー
    private boolean updateMovie() {
        if (mPlayer != null) {
            ByteBuffer buffer = mPlayer.getUpdatedBuffer();
            if (buffer != null) {
                movieBitmap.copyPixelsFromBuffer(buffer);
                return true;
            }
        }
        return false;
    }

    // 画像を表示する
    private void drawMovie(Canvas canvas) {
        Paint paint = new Paint();
        canvas.drawBitmap(movieBitmap, 0, 0, paint);
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
        // ムービーをセットアップ
        setupMovie();
        // 描画スレッドを開始
        thread = new DrawThread();
        thread.start();
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {}

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
        thread = null;
        if (mPlayer != null) {
            mPlayer.stopPlayer();
             // Playスレッドが止まるのを待つべきだが、とりあえずstop
        }
    }

    private MyMoviePlayer mPlayer;

    public int duration() {
        return mPlayer != null ? mPlayer.getDuration() : 0;
    }

    public int position() {
        return mPlayer != null ? mPlayer.getPosition() : 0;
    }
}
