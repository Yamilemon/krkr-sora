#pragma once

// GL操作用 (for OGLDrawDevice)
class iTVPGLContext {
public:
    virtual int Release() = 0;
    virtual void *NativeWindow() = 0;
    virtual void GetSurfaceSize(int *width, int *height) = 0;
    virtual void MakeCurrent() = 0;
    virtual void Swap() = 0;
	virtual void SetWaitVSync( bool b ) = 0;

    static iTVPGLContext *GetContext(void *nativeWindow);
};

// gles初期化用
void InitGLES();
