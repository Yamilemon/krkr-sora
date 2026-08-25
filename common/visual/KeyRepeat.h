#pragma once

#include "tjsCommHead.h"

//---------------------------------------------------------------------------
// tTVPKeyRepeatEmulator : A class for emulating keyboard key repeats.
//---------------------------------------------------------------------------
class tTVPKeyRepeatEmulator
{
	tjs_uint32 PressedTick;
	bool  Pressed;
	tjs_int LastRepeatCount;

	static tjs_int32 HoldTime; // keyboard key-repeats hold-time
	static tjs_int32 IntervalTime; // keyboard key-repeats interval-time


	/*
		          hold time           repeat interval time
		   <------------------------> <--->
		   +------------------------+ +-+ +-+ +-+
		___|                        |_| |_| |_| |____
		   <------------------------------------->
		   key pressed                           key released
		-----------------------------------------------------------> time

	*/

	static void GetKeyRepeatParam();

public:
	tTVPKeyRepeatEmulator();
	~tTVPKeyRepeatEmulator();

	void Down();
	void Up();

	tjs_int GetRepeatCount();
};
//---------------------------------------------------------------------------
