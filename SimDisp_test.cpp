﻿#include <iostream>

#define DLL_IMPORTS
#define USE_AYXANDAR
#include "SimDisp.h"

#include "wx_realtime.h"

using namespace WX;

struct BaseOf_Thread(Graphics) {
	inline void Start() {
		auto buff = (uint32_t *)SimDisp::GetColors();
		uint16_t xMax = 0, yMax = 0;
		SimDisp::GetSizeMax(&xMax, &yMax);
		size_t nPix = xMax * yMax;
		for (;;) {
			for (uint8_t l = 0; l < 255; ++l) {
				for (size_t i = 0; i < nPix; ++i)
					buff[i] = RGB(l, l, l);
				Sleep(1);
			}
			for (uint8_t l = 255; l > 0; --l) {
				for (size_t i = 0; i < nPix; ++i)
					buff[i] = RGB(l, l, l);
				Sleep(1);
			}
		}
	}
} p;

void test_host() {
	SimDisp::LoadDll(_T("SimClient.dll"));
	assertl(SimDisp::Open(L"TUI", 400, 300));
	SimDisp::HideCursor(true);
	SimDisp::Show(true);
	SimDisp::SetOnMouse([](int16_t x, int16_t y, int16_t z, tSimDisp_MouseKey k) {
		Console[{ 0, 0 }].Log(_T("X: "), (int)x, _T("    "));
		Console[{ 0, 1 }].Log(_T("Y: "), (int)y, _T("    "));
		Console[{ 0, 2 }].Log(_T("Z: "), (int)z, _T("    "));
	});
	//SimDisp::Resizeable(true);
	SimDisp::Ayx.Init();
	SimDisp::Ayx.Fill(0xff0000);
	SimDisp::Ayx.Fill(0xff, { LPoint{ 0, 0 }, LPoint{ 70, 70 } });
//	assert(p.Create());
//	SimDisp::SetOnDestroy([] { p.Terminate(); });
	for (;;) {}
//	SimDisp::UserClose();
	SimDisp::CloseDll();
}

#include <conio.h>

int main() {
	Console.Title(_T("SimDisp"));
	try {
		test_host();
	} catch (WX::Exception err) {
		Console.Log(err);
	}
	Console.Log(_T("ALL TEST DONE!"));
	_getch();
	return 0;
}
