package jp.wamsoft.testmovieplayer;

import java.nio.ByteBuffer;

public class MyMoviePlayer {
    
    // ネイティブインスタンスへのポインタ
    private long mNativeHandle = 0;

    public MyMoviePlayer() {
    }

    // JNIインターフェース
    public native boolean createPlayer(String path);

    public native ByteBuffer getUpdatedBuffer();

    public native int getWidth();

    public native int getHeight();

    public native boolean isPlaying();

    public native void startPlayer(boolean loop);

    public native void stopPlayer();

    public native void shutdownPlayer();

    public native int getDuration();

    public native int getPosition();

    public static native void setAssetManager(Object assetManager);
}
