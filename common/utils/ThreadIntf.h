//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000 W.Dee <dee@kikyou.info> and contributors

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Thread base class
//---------------------------------------------------------------------------
#ifndef ThreadIntfH
#define ThreadIntfH
#include "tjsNative.h"

#include <condition_variable>
#include <mutex>
#include <atomic>

/*[*/

//---------------------------------------------------------------------------
// tTVPThreadPriority
//---------------------------------------------------------------------------
enum tTVPThreadPriority
{
	ttpIdle, ttpLowest, ttpLower, ttpNormal, ttpHigher, ttpHighest, ttpTimeCritical
};
//---------------------------------------------------------------------------

typedef void (*tTVPThreadFunc)(void *arg);

//---------------------------------------------------------------------------
// Native Thread Wrapper
//---------------------------------------------------------------------------

class tTVPNativeThreadIntf
{
public:
	virtual ~tTVPNativeThreadIntf() {};
	virtual void Start(tTVPThreadFunc func, void *arg, tTVPThreadPriority pri) = 0;
	virtual void WaitFor() = 0;
	virtual void SetPriority(tTVPThreadPriority pri) = 0;
	virtual void SetProcessorNo(int no) = 0;
#ifdef _WIN32
	virtual HANDLE GetHandle() const = 0; /* win32 specific */
#endif
};

/*]*/

TJS_EXP_FUNC_DEF(void, TVPYieldNativeThread, (int Millisecontds=0));
TJS_EXP_FUNC_DEF(tTVPNativeThreadIntf*, TVPCreateNativeThread, ());


//---------------------------------------------------------------------------
// tTVPThread
//---------------------------------------------------------------------------
class tTVPThread
{

private:
	tTVPNativeThreadIntf *Thread;
	bool Terminated;

	std::mutex Mtx;
	std::condition_variable Cond;
	bool ThreadStarting;
	tTVPThreadPriority ThreadPriority;

public:
	static void StartFunc(void *arg);
	void StartProc();

public:
	tTVPThread();
	virtual ~tTVPThread();

	bool GetTerminated() const { return Terminated; }
	void SetTerminated(bool s) { Terminated = s; }
	void Terminate() { Terminated = true; }

protected:
	virtual void Execute() {}

public:
	void StartThread();
	void WaitFor();

	tTVPThreadPriority GetPriority();
	void SetPriority(tTVPThreadPriority pri);

	void SetProcessorNo(int no);

#ifdef _WIN32
	HANDLE GetHandle() const; /* win32 specific */
#endif
};
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// tTVPThreadEvent
//---------------------------------------------------------------------------
class tTVPThreadEvent
{
	std::mutex Mtx;
	std::condition_variable Cond;
	bool IsReady;

public:
	tTVPThreadEvent() : IsReady(false) {}
	virtual ~tTVPThreadEvent() {}

	void Set() {
		{
			std::lock_guard<std::mutex> lock(Mtx);
			IsReady = true;
		}
		Cond.notify_all();
	}
	/*
	void Reset() {
		std::lock_guard<std::mutex> lock(Mtx);
		IsReady = false;
	}
	*/
	bool WaitFor( tjs_uint timeout ) {
		std::unique_lock<std::mutex> lk( Mtx );
		if( timeout == 0 ) {
			Cond.wait( lk, [this]{ return IsReady;} );
			IsReady = false;
			return true;
		} else {
			//std::cv_status result = Cond.wait_for( lk, std::chrono::milliseconds( timeout ) );
			//return result == std::cv_status::no_timeout;
			bool result = Cond.wait_for( lk, std::chrono::milliseconds( timeout ), [this]{ return IsReady;} );
			IsReady = false;
			return result;
		}
	}
};
//---------------------------------------------------------------------------


/*[*/
const tjs_int TVPMaxThreadNum = 8;
typedef void (TJS_USERENTRY *TVP_THREAD_TASK_FUNC)(void *);
typedef void * TVP_THREAD_PARAM;
/*]*/

TJS_EXP_FUNC_DEF(tjs_int, TVPGetProcessorNum, ());
TJS_EXP_FUNC_DEF(tjs_int, TVPGetThreadNum, ());
TJS_EXP_FUNC_DEF(void, TVPBeginThreadTask, (tjs_int num));
TJS_EXP_FUNC_DEF(void, TVPExecThreadTask, (TVP_THREAD_TASK_FUNC func, TVP_THREAD_PARAM param));
TJS_EXP_FUNC_DEF(void, TVPEndThreadTask, ());

#endif
