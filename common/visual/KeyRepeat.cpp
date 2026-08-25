#include "tjsCommHead.h"

#include "SysInitIntf.h"
#include "TickCount.h"
#include "KeyRepeat.h"

//---------------------------------------------------------------------------
// tTVPKeyRepeatEmulator : A class for emulating keyboard key repeats.
//---------------------------------------------------------------------------
tjs_int32 tTVPKeyRepeatEmulator::HoldTime = 500; // keyboard key-repeats hold-time
tjs_int32 tTVPKeyRepeatEmulator::IntervalTime = 30; // keyboard key-repeats interval-time
//---------------------------------------------------------------------------
void tTVPKeyRepeatEmulator::GetKeyRepeatParam()
{
	static tjs_int ArgumentGeneration = 0;
	if(ArgumentGeneration != TVPGetCommandLineArgumentGeneration())
	{
		ArgumentGeneration = TVPGetCommandLineArgumentGeneration();
		tTJSVariant val;
		if(TVPGetCommandLine(TJS_W("-paddelay"), &val))
		{
			HoldTime = (int)val;
		}
		if(TVPGetCommandLine(TJS_W("-padinterval"), &val))
		{
			IntervalTime = (int)val;
		}
	}
}
//---------------------------------------------------------------------------
#define TVP_REPEAT_LIMIT 10
tTVPKeyRepeatEmulator::tTVPKeyRepeatEmulator() : Pressed(false)
{
}
//---------------------------------------------------------------------------
tTVPKeyRepeatEmulator::~tTVPKeyRepeatEmulator()
{
}
//---------------------------------------------------------------------------
void tTVPKeyRepeatEmulator::Down()
{
	Pressed = true;
	PressedTick = TVPGetRoughTickCount32();
	LastRepeatCount = 0;
}
//---------------------------------------------------------------------------
void tTVPKeyRepeatEmulator::Up()
{
	Pressed = false;
}
//---------------------------------------------------------------------------
tjs_int tTVPKeyRepeatEmulator::GetRepeatCount()
{
	// calculate repeat count, from previous call of "GetRepeatCount" function.
	GetKeyRepeatParam();

	if(!Pressed) return 0;
	if(HoldTime<0) return 0;
	if(IntervalTime<=0) return 0;

	tjs_int elapsed = (tjs_int)(TVPGetRoughTickCount32() - PressedTick);

	elapsed -= HoldTime;
	if(elapsed < 0) return 0; // still in hold time

	elapsed /= IntervalTime;

	tjs_int ret = elapsed - LastRepeatCount;
	if(ret > TVP_REPEAT_LIMIT) ret = TVP_REPEAT_LIMIT;

	LastRepeatCount = elapsed;

	return ret;
}
//---------------------------------------------------------------------------


