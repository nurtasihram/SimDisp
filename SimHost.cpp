#include "./wx/realtime.h"

#define SIMDISP_HOST
#define DLL_IMPORTS 1
#include "SimDisp.h"
#include "SimHost.h"

using namespace WX;

Thread eventBoxClient, actionBoxClient;
Process processHost;

struct BaseOf_Thread(SiDiHost) {
	inline void Start() {
		try {
			eventBoxClient.WaitForSignal();
		} catch (Exception err) {
			MsgBox(_T("Error"), err.operator String(), MB::IconError);
		}
		Process::Exit();
	}
} watchdog;

bool bClientBlocked = false;
struct BaseOf_Thread(EventBox) {
	Message msg;
	inline void Start() {
		CWindow sidi = force_cast<CWindow>(SimDisp::GetHWND());
		while (msg.GetThread()) {
			bClientBlocked = true;
			eventBoxClient.Post(msg);
			assert(msg.GetThread());
			bClientBlocked = false;
		}
	}
} eventBox;

void OnMouse(int16_t xPos, int16_t yPos, int16_t zPos, tSimDisp_MouseKey MouseKeys) {
	if (bClientBlocked) return;
	eventBox.Post(SIDI_MSG::SetOnMouse, MAKEWPARAM(xPos, yPos), MAKELPARAM(zPos, force_cast<uint8_t>(MouseKeys)));
}
void OnDestroy() {
	while (bClientBlocked) {}
	eventBox.Post(SIDI_MSG::SetOnDestroy);
	Message msg;
	assert(msg.GetThread());
}
BOOL OnResize(uint16_t nSizeX, uint16_t nSizeY) {
	if (bClientBlocked) return FALSE;
	eventBox.Post(SIDI_MSG::SetOnResize, nSizeX, nSizeY);
	while (bClientBlocked) {}
	return eventBox.msg.ParamW<BOOL>();
}

void MainProc(DWORD clientBoxID) {
	SimDisp::LoadDll(L"SimDisp.dll");
	eventBoxClient = Thread
		::Open(clientBoxID)
		.Accesses(ThreadAccess::QueryInfo | HandleAccess::Sync);
	assert(watchdog.Create());
	Message msg;
	(bool)msg.Peek();
	eventBoxClient.Post(SIDI_MSG::SIDI_NULL);
	assert(msg.GetThread());
	actionBoxClient = msg.ParamW<Thread>();
	processHost = msg.ParamL<Process>();
	while (msg.GetThread()) {
		try {
			switch (msg.ID<SIDI_MSG>()) {
				case SIDI_MSG::Open:
					if (SimDisp::Open(L"", msg.ParamW<uint16_t>(), msg.ParamL<uint16_t>())) {
						assert(eventBox.Create());
						msg.Param(true, eventBox.ID());
						SimDisp::SetOnMouse(OnMouse);
						SimDisp::SetOnDestroy(OnDestroy);
						SimDisp::SetOnResize(OnResize);
					}
					else
						msg.ParamW(false);
					break;
				case SIDI_MSG::Show:
					msg.ParamW(SimDisp::Show(msg.ParamW<BOOL>()));
					break;
				case SIDI_MSG::Close:
					SimDisp::Close();
					break;
				case SIDI_MSG::UserClose:
					SimDisp::UserClose();
					break;
				case SIDI_MSG::GetSize: {
					uint16_t xSize = 0, ySize = 0;
					SimDisp::GetSize(&xSize, &ySize);
					msg.Param(xSize, ySize);
					break;
				}
				case SIDI_MSG::GetSizeMax: {
					uint16_t xSize = 0, ySize = 0;
					SimDisp::GetSizeMax(&xSize, &ySize);
					msg.Param(xSize, ySize);
					break;
				}
				case SIDI_MSG::Flush:
					msg.ParamW(SimDisp::Flush());
					break;
				case SIDI_MSG::AutoFlush:
					msg.ParamW(SimDisp::AutoFlush(msg.ParamW<BOOL>()));
					break;
				case SIDI_MSG::HideCursor:
					msg.ParamW(SimDisp::HideCursor(msg.ParamW<BOOL>()));
					break;
				case SIDI_MSG::Resizeable:
					msg.ParamW(SimDisp::Resizeable(msg.ParamW<BOOL>()));
					break;
				case SIDI_MSG::GetHWND:
					msg.ParamW(SimDisp::GetHWND());
					break;
				case SIDI_MSG::SIDI_EXIT:
					goto _exit;
				default:
					break;
			}
			actionBoxClient.Post(msg);
		} catch (Exception err) {
			MsgBox(_T("Error"), err.operator String(), MB::IconError);
		}
	}
_exit:
	watchdog.Terminate();
	SimDisp::Close();
	SimDisp::CloseDll();
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd) {
	DWORD tid = 0;
	swscanf_s(lpCmdLine, L"%d", &tid);
	try {
		MainProc(tid);
	} catch (Exception err) {
		MsgBox(_T("Error"), err.operator String(), MB::IconError);
	}
	return 0;
}
