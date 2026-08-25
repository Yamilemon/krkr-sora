
#include "tjsCommHead.h"

#include "WindowForm.h"
#include "Application.h"
#include "TickCount.h"
#include "Random.h"
#include "MsgImpl.h"
#include "LogIntf.h"
#include "VideoOvlIntf.h"

// Androidでは変換しない, ssShiftなどで統一的に扱っている。
tjs_uint32 TVP_TShiftState_To_uint32(TShiftState state) { return (tjs_uint32)state; }
TShiftState TVP_TShiftState_From_uint32(tjs_uint32 state){ return (TShiftState)state; }

// マウスのステート取得
int GetMouseButtonState() {
	int s = 0;
#if 0
	if(TVPGetAsyncKeyState(VK_LBUTTON)) s |= ssLeft;
	if(TVPGetAsyncKeyState(VK_RBUTTON)) s |= ssRight;
	if(TVPGetAsyncKeyState(VK_MBUTTON)) s |= ssMiddle;
#endif
	return s;
}

TTVPWindowForm::TTVPWindowForm( class tTJSNI_Window* ni )
 : LastMouseDownX(0)
 , LastMouseDownY(0)
 , touch_points_(this)
 , EventQueue(this,&TTVPWindowForm::WndProc)
 , TJSNativeInstance(ni) 
 , mcs_(mcsVisible)
 , mSurfaceWidth(0)
 , mSurfaceHeight(0)
 , mCursorX(0)
 , mCursorY(0)
 {
	EventQueue.Allocate();
	Application->AddWindow(this);

	Closing = false;
	ProgramClosing = false;
	CanCloseWork = false;
}

TTVPWindowForm::~TTVPWindowForm() {
	EventQueue.Deallocate();
	Application->DelWindow(this);
}

void TTVPWindowForm::WndProc(NativeEvent& ev) {
	switch( ev.Message ) {

	case AM_RESUME:
		OnResume();
		break;
	case AM_PAUSE:
		OnPause();
		break;

	case AM_SURFACE_CHANGED:
		// Surfaceが切り替わった
		if( TJSNativeInstance ) {
			TJSNativeInstance->ResetDrawDevice();
			TJSNativeInstance->Update();
		}
		break;
	case AM_SURFACE_CREATED:
		break;
	case AM_SURFACE_DESTORYED:
		if( TJSNativeInstance ) {
			TJSNativeInstance->ResetDrawDevice();
		}
		break;
	case AM_SURFACE_PAINT_REQUEST:
		if( TJSNativeInstance ) {
			TJSNativeInstance->Update();
		}
		break;

	case AM_REQUEST_UPDATE:
		if( TJSNativeInstance ) {
			TJSNativeInstance->RequestUpdate();
		}
		break;

	case AM_TOUCH_DOWN:
		OnTouchDown( ev.WParamf0, ev.WParamf1, ev.LParamf0, ev.LParamf0, ev.LParam1, ev.Result );
		break;
	case AM_TOUCH_MOVE:
		OnTouchMove( ev.WParamf0, ev.WParamf1, ev.LParamf0, ev.LParamf0, ev.LParam1, ev.Result );
		break;
	case AM_TOUCH_UP:
		OnTouchUp( ev.WParamf0, ev.WParamf1, ev.LParamf0, ev.LParamf0, ev.LParam1, ev.Result );
		break;
	case AM_KEY_DOWN:
		OnKeyDown( (tjs_int)ev.WParam, (int)ev.LParam );
		break;
	case AM_KEY_UP:
		OnKeyUp( (tjs_int)ev.WParam, (int)ev.LParam );
		break;

	case AM_MOUSE_DOWN:
		OnMouseDown( ev.WParam0, ev.WParam1, ev.LParam0, ev.LParam1 );
		break;

	case AM_MOUSE_UP:
		OnMouseUp( ev.WParam0, ev.WParam1, ev.LParam0, ev.LParam1 );
		break;

	case AM_MOUSE_MOVE:
		OnMouseMove( ev.WParam1, ev.LParam0, ev.LParam1 );
		break;

	case AM_MOUSE_WHEEL:
		OnMouseWheel( ev.WParam0, ev.WParam1, ev.LParam0, ev.LParam1 );
		break;

	case AM_DISPLAY_ROTATE:
		OnDisplayRotate( (tjs_int)ev.WParam, (tjs_int)ev.LParam );
		break;

	case AM_DISPLAY_RESIZE:
		OnResize();
		break;

	default:
		EventQueue.HandlerDefault( ev );
		break;
	}
}

void TTVPWindowForm::OnClose()
{
	// オブジェクト破棄
	if ( ProgramClosing ) {
		if( TJSNativeInstance ) {
			// Window を Invalidate 処理
			iTJSDispatch2 * obj = TJSNativeInstance->GetOwnerNoAddRef();
			TJSNativeInstance->NotifyWindowClose();
			obj->Invalidate(0, NULL, NULL, obj);
			TJSNativeInstance = NULL;
		}
	}
}

bool TTVPWindowForm::OnCloseQuery() {
	// closing actions are 3 patterns;
	// 1. closing action by the user
	// 2. "close" method
	// 3. object invalidation

	if( TVPGetBreathing() ) {
		return false;
	}

	// the default event handler will invalidate this object when an onCloseQuery
	// event reaches the handler.
	if(TJSNativeInstance) {
		iTJSDispatch2 * obj = TJSNativeInstance->GetOwnerNoAddRef();
		if(obj) {
			tTJSVariant arg[1] = {true};
			static ttstr eventname(TJS_W("onCloseQuery"));

			if(!ProgramClosing) {
				// close action does not happen immediately
				if(TJSNativeInstance) {
					TVPPostInputEvent( new tTVPOnCloseInputEvent(TJSNativeInstance) );
				}
				Closing = true; // waiting closing...
				return false;
			} else {
				CanCloseWork = true;
				TVPPostEvent(obj, obj, eventname, 0, TVP_EPT_IMMEDIATE, 1, arg);
					// this event happens immediately
					// and does not return until done
				return CanCloseWork; // CanCloseWork is set by the event handler
			}
		} else {
			return true;
		}
	} else {
		return true;
	}
}

void TTVPWindowForm::OnCloseQueryCalled( bool b ) 
{
	// closing is allowed by onCloseQuery event handler
	if( !ProgramClosing ) {
		// closing action by the user
		if( b ) {
			SetVisible( false );  // just hide

			Closing = false;
			if( TJSNativeInstance ) {
				if( TJSNativeInstance->IsMainWindow() ) {
					// this is the main window
					iTJSDispatch2 * obj = TJSNativeInstance->GetOwnerNoAddRef();
					obj->Invalidate(0, NULL, NULL, obj);
					// TJSNativeInstance = NULL; // この段階では既にthisが削除されているため、メンバーへアクセスしてはいけない
				}
			} else {
				delete this;
			}
		} else {
			Closing = false;
		}
	} else {
		// closing action by the program
		CanCloseWork = b;
	}
}

void TTVPWindowForm::Close() 
{
	// closing action by "close" method
	if( Closing ) return; // already waiting closing...

	ProgramClosing = true;
	try {
		if (OnCloseQuery() ) {
			OnClose();
		} else {
			OnCloseCancel();
		}
	} catch(...) {
		ProgramClosing = false;
		throw;
	}
	ProgramClosing = false;
}


void TTVPWindowForm::InvalidateClose() 
{
	// closing action by object invalidation;
	// this will not cause any user confirmation of closing the window.
	TJSNativeInstance = nullptr;
	SetVisible(false);
	DestroyNativeWindow();
	delete this;
}

// 定期的に呼び出されるので、定期処理があれば実行する
void TTVPWindowForm::TickBeat() {
}

// キー入力
void TTVPWindowForm::OnKeyDown( tjs_int vk, int shift ) {
	InternalKeyDown( vk, shift );
}

void TTVPWindowForm::InternalKeyDown(tjs_uint16 key, tjs_uint32 shift) {
	tjs_uint32 tick = TVPGetRoughTickCount32();
	TVPPushEnvironNoise(&tick, sizeof(tick));
	TVPPushEnvironNoise(&key, sizeof(key));
	TVPPushEnvironNoise(&shift, sizeof(shift));
	if( TJSNativeInstance ) {
		TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, key, shift));
	}
}

void TTVPWindowForm::OnKeyUp( tjs_int vk, int shift ) {
	InternalKeyUp( vk, shift );
}

void TTVPWindowForm::InternalKeyUp( tjs_uint16 key, tjs_uint32 shift ) {
	tjs_uint32 tick = TVPGetRoughTickCount32();
	TVPPushEnvironNoise(&tick, sizeof(tick));
	TVPPushEnvironNoise(&key, sizeof(key));
	TVPPushEnvironNoise(&shift, sizeof(shift));
	if( TJSNativeInstance ) {
		TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, key, shift));
	}
}
void TTVPWindowForm::OnKeyPress( tjs_int vk, int repeat, bool prevkeystate, bool convertkey ) {
}

tTVPRect
TTVPWindowForm::CalcDestRect(int w, int h)
{
    int sw = mSurfaceWidth;
    int sh = mSurfaceHeight;
    if (sw > 0 && sh > 0 && w > 0 && h > 0) {
        double scale = std::min((double)sw/w, (double)sh/h);
        int nw = (int)(w * scale);
        int nh = (int)(h * scale);
        int offx = (sw-nw)/2;
        int offy = (sh-nh)/2;
        TVPLOG_VERBOSE("screen size:{},{} scale:{} dest:{},{},{},{}", sw, sh, scale, offx, offy, nw, nh);
        return tTVPRect(offx,offy,offx+nw,offy+nh);
    }
    return tTVPRect(0,0,1,1);
}

void 
TTVPWindowForm::TranslateWindowToDrawArea(int &x, int &y) 
{
	if (TJSNativeInstance) {
		tTVPRect &destRect = TJSNativeInstance->GetDestRect();
		x -= destRect.left;
		y -= destRect.top;
	}
}

void 
TTVPWindowForm::TranslateWindowToDrawArea(float &x, float &y) 
{
	if (TJSNativeInstance) {
		tTVPRect &destRect = TJSNativeInstance->GetDestRect();
		x -= destRect.left;
		y -= destRect.top;
	}
}
void 
TTVPWindowForm::TranslateDrawAreaToWindow(int &x, int &y) 
{
	if (TJSNativeInstance) {
		tTVPRect &destRect = TJSNativeInstance->GetDestRect();
		x += destRect.left;
		y += destRect.top;
	}
}

void TTVPWindowForm::OnMouseDown( int button, int shift, int x, int y ) {

	//if( !CanSendPopupHide() ) DeliverPopupHide();

	TranslateWindowToDrawArea( x, y);
	//SetMouseCapture();
	MouseVelocityTracker.addMovement( TVPGetRoughTickCount32(), (float)x, (float)y );

	LastMouseDownX = x;
	LastMouseDownY = y;

	if(TJSNativeInstance) {
		tjs_uint32 s = TVP_TShiftState_To_uint32(shift);
		s |= GetMouseButtonState();
		TVPPostInputEvent( new tTVPOnMouseDownInputEvent(TJSNativeInstance, x, y, (tTVPMouseButton)button, s));
	}
}

void TTVPWindowForm::OnMouseUp( int button, int shift, int x, int y ) {
	TranslateWindowToDrawArea(x, y);
	//ReleaseMouseCapture();
	MouseVelocityTracker.addMovement( TVPGetRoughTickCount32(), (float)x, (float)y );
	if(TJSNativeInstance) {
		tjs_uint32 s = TVP_TShiftState_To_uint32(shift);
		s |= GetMouseButtonState();
		TVPPostInputEvent( new tTVPOnMouseUpInputEvent(TJSNativeInstance, x, y, (tTVPMouseButton)button, s));
	}
}

void TTVPWindowForm::OnMouseMove( int shift, int x, int y ) {
	TranslateWindowToDrawArea(x, y);
	MouseVelocityTracker.addMovement( TVPGetRoughTickCount32(), (float)x, (float)y );
	if( TJSNativeInstance ) {
		tjs_uint32 s = TVP_TShiftState_To_uint32(shift);
		s |= GetMouseButtonState();
		TVPPostInputEvent( new tTVPOnMouseMoveInputEvent(TJSNativeInstance, x, y, s), TVP_EPT_DISCARDABLE );
	}

	//RestoreMouseCursor();

	int pos = (y << 16) + x;
	TVPPushEnvironNoise(&pos, sizeof(pos));

	//LastMouseMovedPos.x = x;
	//LastMouseMovedPos.y = y;
}

void TTVPWindowForm::OnMouseWheel( int delta, int shift, int x, int y ) {
	TranslateWindowToDrawArea( x, y);
	// wheel
	if( TJSNativeInstance ) {
		tjs_uint32 s = TVP_TShiftState_To_uint32(shift);
		s |= GetMouseButtonState();
		TVPPostInputEvent(new tTVPOnMouseWheelInputEvent(TJSNativeInstance, s, delta, x, y));
	}
}

void 
TTVPWindowForm::HideMouseCursor() 
{
	SetMouseCursorState(mcsTempHidden); 
}

void
TTVPWindowForm::SetMouseCursorState(tTVPMouseCursorState mcs) 
{
	if (mcs != mcs_) {
		mcs_ = mcs;
		SetCursorVisible(mcs_ == mcsVisible);
	}
}

tTVPMouseCursorState 
TTVPWindowForm::GetMouseCursorState() const 
{
	return mcs_; 
}

void
TTVPWindowForm::GetCursorPos(tjs_int &x, tjs_int &y)
{
    x = mCursorX;
    y = mCursorY;
}

void
TTVPWindowForm::SetCursorPos(tjs_int x, tjs_int y)
{
    mCursorX = x;
    mCursorY = y;
}

void
TTVPWindowForm::UpdateCursorPos(tjs_int x, tjs_int y)
{
	// カーソル表示動作
	if (GetMouseCursorState() == mcsTempHidden) {
		SetMouseCursorState(mcsVisible);
	}
	// イベントでおりかえす
	if( TJSNativeInstance ) {
		//tjs_uint32 s = TVP_TShiftState_To_uint32(shift);
		//s |= GetMouseButtonState();
		TVPPostInputEvent( new tTVPOnMouseMoveInputEvent(TJSNativeInstance, x, y, 0), TVP_EPT_DISCARDABLE );
	}
}


// 表示/非表示
void TTVPWindowForm::ShowWindowAsModal() {
	// modal は対応しないので、例外出す
	TVPThrowExceptionMessage(TJS_W("Modal window is not supported."));
}

void TTVPWindowForm::SetInnerWidth(int w) 
{
	SetInnerSize(w, GetInnerHeight());
}

void TTVPWindowForm::SetInnerHeight(int h)
{
	SetInnerSize(GetInnerWidth(), h);
}

void TTVPWindowForm::SetInnerSize(int w, int h) 
{
	ResizeWindow(w, h); 
}

int TTVPWindowForm::GetInnerWidth() const 
{ 
	return mSurfaceWidth; 
};

int TTVPWindowForm::GetInnerHeight() const 
{
	return mSurfaceHeight; 
};

void TTVPWindowForm::ResizeWindow(int w, int h) 
{
	mSurfaceWidth = w;
	mSurfaceHeight = h;
};

void TTVPWindowForm::UpdateVideoOverlay()
{
	if (TJSNativeInstance) {
		TJSNativeInstance->UpdateVideoOverlay();
	}
}

void TTVPWindowForm::OnTouchDown( float x, float y, float cx, float cy, tjs_int id, tjs_uint64 tick ) 
{
	TranslateWindowToDrawArea(x, y);

	TouchVelocityTracker.start( id );
	TouchVelocityTracker.update( id, tick, (float)x, (float)y );

	if(TJSNativeInstance) {
		TVPPostInputEvent( new tTVPOnTouchDownInputEvent(TJSNativeInstance, x, y, cx, cy, id));
	}
	touch_points_.TouchDown( x, y ,cx, cy, id, static_cast<tjs_uint>(tick&0xffffffff) );
}

void TTVPWindowForm::OnTouchMove( float x, float y, float cx, float cy, tjs_int id, tjs_uint64 tick ) 
{
	TranslateWindowToDrawArea(x, y);

	TouchVelocityTracker.update( id, tick, (float)x, (float)y );

	if(TJSNativeInstance) {
		TVPPostInputEvent( new tTVPOnTouchMoveInputEvent(TJSNativeInstance, x, y, cx, cy, id));
	}
	touch_points_.TouchMove( x, y, cx, cy, id, static_cast<tjs_uint>(tick&0xffffffff) );
}

void TTVPWindowForm::OnTouchUp( float x, float y, float cx, float cy, tjs_int id, tjs_uint64 tick ) 
{
	TranslateWindowToDrawArea(x, y);

	TouchVelocityTracker.update( id, tick, (float)x, (float)y );

	if(TJSNativeInstance) {
		TVPPostInputEvent( new tTVPOnTouchUpInputEvent(TJSNativeInstance, x, y, cx, cy, id));
	}
	touch_points_.TouchUp( x, y, cx, cy, id, static_cast<tjs_uint>(tick&0xffffffff) );
}

void TTVPWindowForm::OnTouchScaling( double startdist, double currentdist, double cx, double cy, int flag ) 
{
	if (TJSNativeInstance) {
		TVPPostInputEvent( new tTVPOnTouchScalingInputEvent(TJSNativeInstance, startdist, currentdist, cx, cy, flag ));
	}
}

void TTVPWindowForm::OnTouchRotate( double startangle, double currentangle, double distance, double cx, double cy, int flag ) 
{
	if (TJSNativeInstance) {
		TVPPostInputEvent( new tTVPOnTouchRotateInputEvent(TJSNativeInstance, startangle, currentangle, distance, cx, cy, flag));
	}
}

void TTVPWindowForm::OnMultiTouch() 
{
	if (TJSNativeInstance ) {
		TVPPostInputEvent( new tTVPOnMultiTouchInputEvent(TJSNativeInstance) );
	}
}

void TTVPWindowForm::OnResume() 
{
	if(TJSNativeInstance) TJSNativeInstance->FireOnActivate(true);
}

void TTVPWindowForm::OnPause() 
{
	if(TJSNativeInstance) TJSNativeInstance->FireOnActivate(false);
}

void TTVPWindowForm::OnResize()
{
	GetSurfaceSize(mSurfaceWidth, mSurfaceHeight);
	if(TJSNativeInstance) {
		// here specifies TVP_EPT_REMOVE_POST, to remove redundant onResize events.
		TJSNativeInstance->SetUpdateDestRect();
		TVPPostInputEvent( new tTVPOnResizeInputEvent(TJSNativeInstance), TVP_EPT_REMOVE_POST );
	}
}

void TTVPWindowForm::OnDisplayRotate( tjs_int orientation, tjs_int density ) 
{
	if (TJSNativeInstance) {
		TVPPostInputEvent( new tTVPOnDisplayRotateInputEvent(TJSNativeInstance, orientation, -1, density, 0, 0));
	}
}

/**
 * システムからのイベント処理
 */
void TTVPWindowForm::SendMessage( tjs_int message, tjs_int64 wparam, tjs_int64 lparam ) 
{
	// Main Windowが存在する場合はそのWindowへ送る
	NativeEvent ev(message,wparam,lparam);
	PostEvent(ev);
}

void TTVPWindowForm::SendTouchMessage( tjs_int type, float x, float y, float c, int id, tjs_uint64 tick ) 
{
	NativeEvent ev;
	ev.Message = type;
	ev.WParamf0 = x;
	ev.WParamf1 = y;
	ev.LParamf0 = c;
	ev.LParam1 = id;
	ev.Result = tick;
	PostEvent(ev);
}

void TTVPWindowForm::SendMouseMessage( tjs_int type, int button, int shift, int x, int y)
{
	NativeEvent ev;
	ev.Message = type;
	ev.WParam0 = button;
	ev.WParam1 = shift;
	ev.LParam0 = x;
	ev.LParam1 = y;
	PostEvent(ev);
}
