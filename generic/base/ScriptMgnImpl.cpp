//---------------------------------------------------------------------------
// TJS2 Script Managing
//---------------------------------------------------------------------------
#include "tjsCommHead.h"
#include <memory>
#include "Application.h"
#include "MsgIntf.h"
#include "CharacterSet.h"
#include "StorageIntf.h"

//---------------------------------------------------------------------------
// Hash Map Object を書き出すためのサブプロセスとして起動しているかどうか
// チェックする
// Windows 以外では、ないものとして扱ってもいいか
bool TVPCheckProcessLog() { return false; }
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Script system initialization script
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
ttstr TVPGetSystemInitializeScript()
{
	// read system init file from resource

	tjs_string path = Application->ResourcePath() + TJS_W("SysInitScript.tjs");
	tjs_uint64 flen;
	auto buf = TVPReadStream(path.c_str(), &flen);
	if( buf.get() == nullptr ) {
		TVPThrowExceptionMessage(TVPCannotOpenStorage, ttstr(path.c_str()));
	}

	tjs_string ret((tjs_char*)(buf.get())+1, flen/2-1);
	//TVPLOG_VEBOSE(ret);


	return ttstr(ret.c_str());
}
//---------------------------------------------------------------------------
