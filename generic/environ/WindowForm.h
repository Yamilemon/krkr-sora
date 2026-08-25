
#ifndef __WINDOW_FORM_H__
#define __WINDOW_FORM_H__

#include "TouchPoint.h"
#include "VelocityTracker.h"
#include "tvpinputdefs.h"
#include "WindowIntf.h"
#include "Application.h"
#include "NativeEventQueue.h"
#include "WindowFormEvent.h"

typedef unsigned long TShiftState;
extern tjs_uint32 TVP_TShiftState_To_uint32(TShiftState state);
extern TShiftState TVP_TShiftState_From_uint32(tjs_uint32 state);

/*
 * tTJSNI_Window が持つ Form クラスに要求されるメソッドを列挙したもの。
 * Form = Application->CreateWindowForm(this) として実体化される。
 * このファイル自体は使われず、各環境で各メソッドを実装するためのベースとして使用する
 */

class TTVPWindowForm : public FormEventHandler, TouchHandler {

protected:
	TouchPointList touch_points_;
	VelocityTrackers TouchVelocityTracker;
	VelocityTracker MouseVelocityTracker;

	NativeEventQueue<TTVPWindowForm> EventQueue;

	//-- TJS object related
	tTJSNI_Window * TJSNativeInstance;
	int LastMouseDownX, LastMouseDownY; // in Layer coodinates

	tTVPMouseCursorState mcs_;

public:
	virtual void *NativeWindowHandle() const = 0;
	virtual void DestroyNativeWindow() = 0; // Window破棄要求　
	virtual void GetSurfaceSize(int &w, int &h) const = 0; // サーフェースのサイズを取得
	virtual void ResizeWindow(int w, int h);
	void UpdateVideoOverlay();

private:
	static int MulDiv( int nNumber, int nNumerator, int nDenominator ) {
		return (int)(((int64_t)nNumber * (int64_t)nNumerator) / nDenominator);
	}

protected:
	void WndProc(NativeEvent& ev);

	TTVPWindowForm( class tTJSNI_Window* ni );
	virtual ~TTVPWindowForm();

	bool Closing;
	bool ProgramClosing;
	bool CanCloseWork;

	// 描画サイズ
	int mSurfaceWidth;
	int mSurfaceHeight;

	// カーソル位置
	tjs_int mCursorX;
	tjs_int mCursorY;

public:
	virtual tTVPRect CalcDestRect(int w, int h); //< DrawDeviceの表示領域を計算する

	void TranslateWindowToDrawArea(int &x, int &y);
	void TranslateWindowToDrawArea(float &x, float &y);
	void TranslateDrawAreaToWindow(int &x, int &y);

	// Windowが有効かどうか、無効だとイベントが配信されない
	bool GetFormEnabled() { return true; }

	// 閉じる 系処理
	virtual void OnClose();
	virtual void OnCloseCancel() {};

	bool OnCloseQuery();

	void Close();
	void InvalidateClose();
	void OnCloseQueryCalled(bool b);


	// 定期的に呼び出されるので、定期処理があれば実行する
	void TickBeat();

	// アクティブ/デアクティブ化された時に、Windowがアクティブかどうかチェックされる
	bool GetWindowActive() { return true; }

	// キー入力
	void OnKeyDown( tjs_int vk, int shift );
	void InternalKeyDown(tjs_uint16 key, tjs_uint32 shift);
	void OnKeyUp( tjs_int vk, int shift );
	void InternalKeyUp( tjs_uint16 key, tjs_uint32 shift );
	void OnKeyPress( tjs_int vk, int repeat, bool prevkeystate, bool convertkey );

	// マウス操作
	void OnMouseDown( int button, int shift, int x, int y );
	void OnMouseUp( int button, int shift, int x, int y );
	void OnMouseMove( int shift, int x, int y );
	void OnMouseWheel( int delta, int shift, int x, int y );

	// マウスカーソル
	void SetDefaultMouseCursor() {}
	void SetMouseCursor(tjs_int handle) {}
	void HideMouseCursor();
	void SetMouseCursorState(tTVPMouseCursorState mcs);
	tTVPMouseCursorState GetMouseCursorState() const;

	// カーソル位置制御
	virtual void GetCursorPos(tjs_int &x, tjs_int &y);
	virtual void SetCursorPos(tjs_int x, tjs_int y);
	virtual void SetCursorVisible(bool visible) {};

	void UpdateCursorPos(tjs_int x, tjs_int y); //< カーソル設定後処理

	// ヒント表示(マウスオーバー出来ないのでAndroidでは無効)
	void SetHintText(iTJSDispatch2* sender, const ttstr &text) {}
	void SetHintDelay( tjs_int delay ) {}
	tjs_int GetHintDelay() const { return 0; }

	// IME 入力関係、EditViewを最前面に貼り付けて、そこで入力させるのが現実的かな、好きな位置に表示は画面狭いとあまり現実的じゃないかも
	// 入力タイトルを指定して、入力受付、確定文字が返ってくるスタイルの方がいいか、モーダルにはならないから、確定後イベント通知かな
	void SetAttentionPoint(tjs_int left, tjs_int top, const struct tTVPFont * font ) {}
	void DisableAttentionPoint() {}
	void SetImeMode(tTVPImeMode mode) {}
	tTVPImeMode GetDefaultImeMode() const { return imDisable; }
	void ResetImeMode() {}

	// 表示/非表示
	virtual bool GetVisible() const { return true; };
	virtual void SetVisible(bool b) {};
	virtual void SetVisibleFromScript(bool b) { SetVisible(b); }

	void ShowWindowAsModal();

	// キャプション設定
	virtual void SetCaption( const tjs_string& v ) {};

	// サイズや位置など
	virtual void SetLeft( int l ) {}
	virtual int GetLeft() const { return 0; }
	virtual void SetTop( int t ) {}
	virtual int GetTop() const { return 0; }
	virtual void SetPosition( int l, int t ) {}
	// サイズ
	virtual void SetWidth( int w ){}
	virtual int GetWidth() const { return 0; }
	virtual void SetHeight( int h ){}
	virtual int GetHeight() const { return 0; }
	virtual void SetSize( int w, int h ){}
	// 最小、最大サイズ関係、Androidなどリサイズがないとしたら無効か
	virtual void SetMinWidth( int v ) {}
	virtual int GetMinWidth() const { return 0; }
	virtual void SetMinHeight( int v ) {}
	virtual int GetMinHeight() { return 0; }
	virtual void SetMinSize( int w, int h ) {}
	virtual void SetMaxWidth( int v ) {}
	virtual int GetMaxWidth() { return 0; }
	virtual void SetMaxHeight( int v ) {}
	virtual int GetMaxHeight() { return 0; }
	virtual void SetMaxSize( int w, int h ) {}

	// 内部のサイズ、実質的にこれが表示領域サイズ
	void SetInnerWidth( int w );
	void SetInnerHeight( int h );
	void SetInnerSize( int w, int h );

	virtual int GetInnerWidth() const;
	virtual int GetInnerHeight() const;

	// 境界サイズ、無効
	void SetBorderStyle( enum tTVPBorderStyle st) {}
	enum tTVPBorderStyle GetBorderStyle() const { return bsNone; }

	// 常に最前面表示、無効
	void SetStayOnTop( bool b ) {}
	bool GetStayOnTop() const { return true; }
	// 最前面へ移動
	void BringToFront() {}

	// フルスクリーン、無効と言うか常に真
	void SetFullScreenMode(bool b) {}
	bool GetFullScreenMode() const { return true; }

	//マウスキー(キーボードでのマウスカーソル操作)は無効
	void SetUseMouseKey(bool b) {}
	bool GetUseMouseKey() const { return false; }

	// 他ウィンドウのキー入力をトラップするか、無効
	void SetTrapKey(bool b) {}
	bool GetTrapKey() const { return false; }

	// ウィンドウマスクリージョンは無効
	//void SetMaskRegion(HRGN threshold);
	void SetMaskRegion(void* threshold) {}
	void RemoveMaskRegion() {}

	// フォースは常に真
	void SetFocusable(bool b) {}
	bool GetFocusable() const { return true; }

	// 表示ズーム関係(非サポート)
	void SetZoom(tjs_int numer, tjs_int denom, bool set_logical = true) {}
	void SetZoomNumer( tjs_int n ) {}
	tjs_int GetZoomNumer() const { return 1; }
	void SetZoomDenom(tjs_int d) {}
	tjs_int GetZoomDenom() const { return 1; }

	// タッチ入力関係 ( TouchPointList によって管理されている )
	void SetTouchScaleThreshold( double threshold ) { touch_points_.SetScaleThreshold( threshold ); }
	double GetTouchScaleThreshold() const { return touch_points_.GetScaleThreshold(); }
	void SetTouchRotateThreshold( double threshold ) { touch_points_.SetRotateThreshold( threshold ); }
	double GetTouchRotateThreshold() const { return touch_points_.GetRotateThreshold(); }
	tjs_real GetTouchPointStartX( tjs_int index ) const { return touch_points_.GetStartX(index); }
	tjs_real GetTouchPointStartY( tjs_int index ) const { return touch_points_.GetStartY(index); }
	tjs_real GetTouchPointX( tjs_int index ) const { return touch_points_.GetX(index); }
	tjs_real GetTouchPointY( tjs_int index ) const { return touch_points_.GetY(index); }
	tjs_int GetTouchPointID( tjs_int index ) const { return touch_points_.GetID(index); }
	tjs_int GetTouchPointCount() const { return touch_points_.CountUsePoint(); }
	void ResetTouchVelocity( tjs_int id ) {
		TouchVelocityTracker.end( id );
	}

	// タッチ入力のマウスエミュレートON/OFF
	virtual void SetEnableTouch( bool b ) {}
	virtual bool GetEnableTouch() const { return false; }
	virtual void SetEnableTouchMouse( bool b ) {}
	virtual bool GetEnableTouchMouse() const { return false; }

	// タッチ、マウス加速度
	bool GetTouchVelocity( tjs_int id, float& x, float& y, float& speed ) const {
		return TouchVelocityTracker.getVelocity( id, x, y, speed );
	}
	bool GetMouseVelocity( float& x, float& y, float& speed ) const {
		if( MouseVelocityTracker.getVelocity( x, y ) ) {
			speed = hypotf(x, y);
			return true;
		}
		return false;
	}
	void ResetMouseVelocity() {
		MouseVelocityTracker.clear();
	}

	// ディスプレイレイアウト
	enum {
		orientUnknown,		// ACONFIGURATION_ORIENTATION_ANY
		orientPortrait,		// ACONFIGURATION_ORIENTATION_PORT
		orientLandscape,	// ACONFIGURATION_ORIENTATION_LAND
		orientSquare		// ACONFIGURATION_ORIENTATION_SQUARE
	};

	// 画面表示向き取得
	virtual int GetDisplayOrientation() { return orientUnknown; }
	virtual int GetDisplayRotate() { return false; }

	void OnTouchDown( float x, float y, float cx, float cy, tjs_int id, tjs_uint64 tick );
	void OnTouchMove( float x, float y, float cx, float cy, tjs_int id, tjs_uint64 tick );
	void OnTouchUp( float x, float y, float cx, float cy, tjs_int id, tjs_uint64 tick );
	void OnTouchScaling( double startdist, double currentdist, double cx, double cy, int flag );
	void OnTouchRotate( double startangle, double currentangle, double distance, double cx, double cy, int flag );
	void OnMultiTouch();

	void OnResume();
	void OnPause();
	void OnResize();

	void OnDisplayRotate( tjs_int orientation, tjs_int density );

	NativeEventQueueIntarface* GetEventHandler() { return &EventQueue; }

	void PostEvent(const NativeEvent &ev) { EventQueue.PostEvent(ev); }

	// --------------------------------------------------------
	// FormEventHandler
	virtual void SendMessage( tjs_int message, tjs_int64 wparam, tjs_int64 lparam );
	virtual void SendTouchMessage( tjs_int type, float x, float y, float c, int id, tjs_uint64 tick );
	virtual void SendMouseMessage( tjs_int type, int button, int shift, int x, int y);
};

#endif
