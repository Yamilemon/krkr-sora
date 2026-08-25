
#ifndef __T_APPLICATION_H__
#define __T_APPLICATION_H__

#include <vector>
#include <map>
#include <stack>

//---------------------------------------------------------------------------
// memory allocation class
//---------------------------------------------------------------------------
class iTVPMemoryAllocator {
public:
	virtual ~iTVPMemoryAllocator() {};
	virtual void* allocate( size_t size ) = 0;
	virtual void free( void* mem ) = 0;
};


tjs_string ExePath();

// 見通しのよい方法に変更した方が良い
extern int _argc;
extern tjs_char ** _wargv;

enum {
	mrOk,
	mrAbort,
	mrCancel,
};

enum {
  mtWarning = MB_ICONWARNING,
  mtError = MB_ICONERROR,
  mtInformation = MB_ICONINFORMATION,
  mtConfirmation = MB_ICONQUESTION,
  mtStop = MB_ICONSTOP,
  mtCustom = 0
};
enum {
	mbOK = MB_OK,
};

class AcceleratorKey {
	HACCEL hAccel_;
	ACCEL* keys_;
	int key_count_;

public:
	AcceleratorKey();
	~AcceleratorKey();
	void AddKey( WORD id, WORD key, BYTE virt );
	void DelKey( WORD id );
	HACCEL GetHandle() { return hAccel_; }
};
class AcceleratorKeyTable {
	std::map<HWND,AcceleratorKey*> keys_;
	HACCEL hAccel_;

public:
	AcceleratorKeyTable();
	~AcceleratorKeyTable();
	void AddKey( HWND hWnd, WORD id, WORD key, BYTE virt );
	void DelKey( HWND hWnd, WORD id );
	void DelTable( HWND hWnd );
	HACCEL GetHandle(HWND hWnd) {
		std::map<HWND,AcceleratorKey*>::iterator i = keys_.find(hWnd);
		if( i != keys_.end() ) {
			return i->second->GetHandle();
		}
		return hAccel_;
	}
};
class tTVPApplication {
	std::vector<class TTVPWindowForm*> windows_list_;
	tjs_string title_;

	bool is_attach_console_;
	tjs_string console_title_;
	AcceleratorKeyTable accel_key_;
	bool tarminate_;
	bool application_activating_;
	bool has_map_report_process_;

	class tTVPAsyncImageLoader* image_load_thread_;
	class tTVPStorageCacheThread* file_cache_thread_;

	std::stack<class tTVPWindow*> modal_window_stack_;
	std::vector<char> console_cache_;

private:
	void CheckConsole();
	void CloseConsole();
	void CheckDigitizer();
	void ShowException( const tjs_char* e );
	void Initialize() {}
	void Run();

public:
	tTVPApplication();
	~tTVPApplication();
	bool StartApplication( int argc, tjs_char* argv[] );

	tjs_string ExePath();
	tjs_string PluginPath();

	bool IsAttachConsole() { return is_attach_console_; }

	bool IsTarminate() const { return tarminate_; }

	HWND GetHandle();
	bool IsIconic() {
		HWND hWnd = GetHandle();
		if( hWnd != INVALID_HANDLE_VALUE ) {
			return 0 != ::IsIconic(hWnd);
		}
		return true; // そもそもウィンドウがない
	}
	void Minimize();
	void Restore();
	void BringToFront();

	void AddWindow( class TTVPWindowForm* win ) {
		windows_list_.push_back( win );
	}
	void RemoveWindow( class TTVPWindowForm* win );
	unsigned int GetWindowCount() const {
		return (unsigned int)windows_list_.size();
	}

	void FreeDirectInputDeviceForWindows();

	bool ProcessMessage( MSG &msg );
	void ProcessMessages();
	void HandleMessage();
	void HandleIdle(MSG &msg);

	tjs_string GetTitle() const { return title_; }
	void SetTitle( const tjs_string& caption );

	static inline int MessageDlg( const tjs_string& string, const tjs_string& caption, int type, int button ) {
		return ::MessageBox( nullptr, (const wchar_t*)string.c_str(), (const wchar_t*)caption.c_str(), type|button  );
	}
	void Terminate() {
		::PostQuitMessage(0);
	}
	void SetHintHidePause( int v ) {}
	void SetShowHint( bool b ) {}
	void SetShowMainForm( bool b ) {}


	HWND GetMainWindowHandle() const;

	int ArgC;
	tjs_char ** ArgV;

	void PostMessageToMainWindow(UINT message, WPARAM wParam, LPARAM lParam);


	void ModalStarted( class tTVPWindow* form ) {
		modal_window_stack_.push(form);
	}
	void ModalFinished();
	void OnActiveAnyWindow();
	void DisableWindows();
	void EnableWindows( const std::vector<class TTVPWindowForm*>& win );
	void GetDisableWindowList( std::vector<class TTVPWindowForm*>& win );
	void GetEnableWindowList( std::vector<class TTVPWindowForm*>& win, class TTVPWindowForm* activeWindow );

	
	void RegisterAcceleratorKey(HWND hWnd, char virt, short key, short cmd);
	void UnregisterAcceleratorKey(HWND hWnd, short cmd);
	void DeleteAcceleratorKeyTable( HWND hWnd );

	void OnActivate( HWND hWnd );
	void OnDeactivate( HWND hWnd );
	bool GetActivating() const { return application_activating_; }
	bool GetNotMinimizing() const;

	// -------------------------------------------------------------

	// Bitmap用のAllocatorを返す
	virtual iTVPMemoryAllocator *CreateBitmapAllocator();

	/**
	 * 画像の非同期読込み要求
	 */
	void LoadImageRequest( class iTJSDispatch2 *owner, class tTJSNI_Bitmap* bmp, const ttstr &name );

	// -------------------------------------------------------------

	void CacheFileRequest( const ttstr &name, bool fast=false );
	void CacheFileClear( const ttstr &name);
	void CacheFileClearOld(int keepTime, bool force);
	void CacheFileSetMaxSize( int maxSize);
	bool CacheIsLoading(bool fast=false) const;

	// -------------------------------------------------------------

	tjs_int GetDensity() const;

	// キー押し下げ状態取得
	bool GetAsyncKeyState(tjs_uint keycode, bool getcurrent);
};
std::vector<std::string>* LoadLinesFromFile( const tjs_string& path );

inline HINSTANCE GetHInstance() { return ((HINSTANCE)GetModuleHandle(0)); }
extern class tTVPApplication* Application;


#endif // __T_APPLICATION_H__
