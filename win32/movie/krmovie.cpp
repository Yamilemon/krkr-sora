//---------------------------------------------------------------------------
// krmovie.cpp ( part of KRMOVIE.DLL )
// (c)2001-2009, W.Dee <dee@kikyou.info> and contributors
//---------------------------------------------------------------------------

/*
	We must separate this module because sucking MS library has a lack of
	compiler portability.

	This requires DirectX7 or later or Windows Media Player 6.4 or later for
	playbacking MPEG streams.

	Modified by T.Imoto <http://www.kaede-software.com>
*/

//---------------------------------------------------------------------------

#include <windows.h>
#include "tp_stub.h"
#include "dsoverlay.h"
#include "krmovie.h"

#include "asyncio.h"
#include "asyncrdr.h"

#include "OptionInfo.h"

#ifdef _MSC_VER
#if defined(_M_AMD64) || defined(_M_X64)
#pragma comment(linker, "/EXPORT:GetAPIVersion")
#pragma comment(linker, "/EXPORT:GetMFVideoOverlayObject")
#pragma comment(linker, "/EXPORT:GetMixingVideoOverlayObject")
#pragma comment(linker, "/EXPORT:GetVideoOverlayObject")
#pragma comment(linker, "/EXPORT:GetVideoLayerObject")
#pragma comment(linker, "/EXPORT:V2Link")
#pragma comment(linker, "/EXPORT:V2Unlink")
#else
#pragma comment(linker, "/EXPORT:GetAPIVersion=_GetAPIVersion@4")
#pragma comment(linker, "/EXPORT:GetMFVideoOverlayObject=_GetMFVideoOverlayObject@28")
#pragma comment(linker, "/EXPORT:GetMixingVideoOverlayObject=_GetMixingVideoOverlayObject@28")
#pragma comment(linker, "/EXPORT:GetVideoOverlayObject=_GetVideoOverlayObject@28")
#pragma comment(linker, "/EXPORT:GetVideoLayerObject=_GetVideoLayerObject@28")
#pragma comment(linker, "/EXPORT:V2Link=_V2Link@4")
#pragma comment(linker, "/EXPORT:V2Unlink=_V2Unlink@0")
#endif
#endif

#include "resource.h"

// ハンドル保持用
static HINSTANCE gModule = NULL;

//---------------------------------------------------------------------------
// about string retrieving
//---------------------------------------------------------------------------
static ttstr TVPReadAboutStringFromResource() {
	const char *buf = NULL;
	unsigned int size = 0;
	HRSRC hRsrc = ::FindResource(gModule, MAKEINTRESOURCE(IDR_LICENSE_TEXT), TEXT("TEXT"));
	if( hRsrc != NULL ) {
		size = ::SizeofResource( gModule, hRsrc );
		HGLOBAL hGlobal = ::LoadResource( gModule, hRsrc );
		if( hGlobal != NULL ) {
			buf = reinterpret_cast<const char*>(::LockResource(hGlobal));
		}
	}
	if( buf == NULL ) ttstr(TJS_W("Resource Read Error."));

	char* src = new char[size+1];
	if (src == NULL) return ttstr(TJS_W("Resource Read Error."));
	memcpy( src, buf, size );
	src[size] = 0;

	// UTF-8 to UTF-16
	size_t len = TVPUtf8ToWideCharString( src, NULL );
	if( len < 0 ) return ttstr(TJS_W("Resource Read Error."));
	tjs_char* tmp = new tjs_char[len+1];
	ttstr ret;
	if( tmp ) {
		try {
			len = TVPUtf8ToWideCharString( src, tmp );
		} catch(...) {
			delete[] tmp;
			throw;
		}
		tmp[len] = 0;
		ret = ttstr( tmp );
		delete[] tmp;
	}
	delete[] src;
	return ret;
}

//---------------------------------------------------------------------------
// DllMain
//---------------------------------------------------------------------------
BOOL APIENTRY DllMain( HINSTANCE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	gModule = hModule;
    return TRUE;
}
//---------------------------------------------------------------------------
// GetVideoOverlayObject
//---------------------------------------------------------------------------
EXPORT(void) GetVideoOverlayObject(
	HWND callbackwin, IStream *stream, const tjs_char * streamname,
	const tjs_char *type, unsigned __int64 size, iTVPVideoOverlay **out)
{
	*out = new tTVPDSVideoOverlay;

	if( *out )
		static_cast<tTVPDSVideoOverlay*>(*out)->BuildGraph( callbackwin, stream, streamname, type, size );
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// GetAPIVersion
//---------------------------------------------------------------------------
EXPORT(void) GetAPIVersion(DWORD *ver)
{
	*ver = TVP_KRMOVIE_VER;
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// V2Link : Initialize TVP plugin interface
//---------------------------------------------------------------------------
EXPORT(HRESULT) V2Link(iTVPFunctionExporter *exporter)
{
// メモリ確保位置でブレークを貼るには以下のメソッドで確保番号を指定する。
// ブレークがかかった後は、呼び出し履歴(コールスタック)を見て、どこで確保されたメモリがリークしているか探る。
// _CrtDumpMemoryLeaks でデバッグ出力にリークしたメモリの確保番号が出るので、それを入れればOK
// 確保順が不確定な場合は辛いが、スクリプトを固定すればほぼ同じ順で確保されるはず。
//	_CrtSetBreakAlloc(53);	// 指定された回数目のメモリ確保時にブレークを貼る

	TVPInitImportStub(exporter);

	// output copyright
	TVPAddImportantLog(TVPReadAboutStringFromResource());

	return S_OK;
}
//---------------------------------------------------------------------------
// V2Unlink : Uninitialize TVP plugin interface
//---------------------------------------------------------------------------
EXPORT(HRESULT) V2Unlink()
{
	TVPUninitImportStub();

#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif

	return S_OK;
}
//---------------------------------------------------------------------------

