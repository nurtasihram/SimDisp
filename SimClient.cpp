

#include "./wx/realtime.h"

#define SIMDISP_HOST
#include "SimHost.h"

#undef SIMDISP_HOST
#define DLL_EXPORTS
#include "SimDisp.h"

using namespace WX;

class SiDiHost {
	Event actionDone;
	Process processHost;
	Thread actionBoxHost;
	inline void ShellHost(DWORD dwEventBoxID) {
		Message msg;
		assert(!msg.PeekThread());
		auto ps = Process::Create(O, Cats(S("SimHost.exe "), dwEventBoxID))
			.NewConsole()
			.Security(InheritHandle)
			.InheritHandles();
		processHost = ps;
		actionBoxHost = ps;
		assert(msg.GetThread());
		actionDone.Set();
	}

	Mutex activity;
	Message recieved;
	LThread actionBox = [&] {
		Message msg;
		assert(!msg.PeekThread());
		actionDone.Set();
		while (msg.GetThread()) {
			actionBoxHost.Post(msg);
			recieved.GetThread();
			actionDone.Set();
		}
	};

	Thread eventReq;
	Event eventClose;
	tSimDisp_OnDestroy lpfnOnDestroy = O;
	tSimDisp_OnMouse lpfnOnMouse = O;
	tSimDisp_OnResize lpfnOnResize = O;
	LThread eventBox = [&] {
		ShellHost(eventBox.ID());
		Message msg;
		while (msg.GetThread())
			switch (msg.ID<SIDI_MSG>()) {
				case SIDI_MSG::SetOnDestroy:
					if (lpfnOnDestroy)
						lpfnOnDestroy();
					eventClose.Set();
					eventReq.Post(msg);
					break;
				case SIDI_MSG::SetOnMouse:
					if (lpfnOnMouse)
						lpfnOnMouse(LOWORD(msg.ParamW()), HIWORD(msg.ParamW()),
									LOWORD(msg.ParamL()), force_cast<tSimDisp_MouseKey>(HIWORD(msg.ParamL())));
					eventReq.Post(msg);
					break;
				case SIDI_MSG::SetOnResize:
					if (lpfnOnResize)
						msg.ParamW(lpfnOnResize(msg.ParamW<uint16_t>(), msg.ParamL<uint16_t>()));
					else
						msg.ParamW(true);
					eventReq.Post(msg);
					break;
				default:
					break;
			}
	};
public:
	SiDiHost() {}
	~SiDiHost() {
		if (eventBox.StillActive())
			eventBox.Terminate();
		if (actionBox.StillActive())
			actionBox.Terminate();
		if (actionBoxHost.StillActive())
			processHost.Terminate();
	}

	inline void Create() {
		if (actionBox.StillActive()) actionBox.Terminate(), actionBox.Close();
		actionDone.Reset();
		actionBox.OnError([&](const Exception &err) {
			MsgBox(_T("Error"), err.operator String(), MB::IconError);
			actionDone.Set();
		});
		assert(actionBox.Create().Security(InheritHandle));
		actionDone.WaitForSignal();
		if (eventBox.StillActive()) actionBox.Terminate(), actionBox.Close();
		actionDone.Reset();
		eventBox.OnError([&](const Exception &err) {
			MsgBox(_T("Error"), err.operator String(), MB::IconError);
			actionDone.Set();
		});
		assert(eventBox.Create());
		actionDone.WaitForSignal();
		actionBoxHost.Post(SIDI_MSG::SIDI_NULL, (HANDLE)actionBox, GetCurrentProcess());
	}
	inline bool Open(const wchar_t *lpTitle, uint16_t xSize, uint16_t ySize) {
		Create();
		auto &msg = Send(SIDI_MSG::Open, xSize, ySize);
		if (!msg.ParamW<bool>())
			return false;
		eventReq = Thread::Open(msg.ParamL<DWORD>()).Accesses(ThreadAccess::QueryInfo | HandleAccess::Sync);
		if (!lpTitle)
			lpTitle = L"SimDisp (CLIENT) By Nurtas Ihram " __DATE__;
		auto hwnd = Send(SIDI_MSG::GetHWND).ParamW<HWND>();
		return SetWindowTextW(hwnd, lpTitle);
	}
	inline void UserClose() {
		if (eventReq.StillActive()) {
			eventClose.Reset();
			eventClose.WaitForSignal();
		}
	}
	inline void SetOnDestroy(tSimDisp_OnDestroy lpfnOnDestroy) reflect_to(this->lpfnOnDestroy = lpfnOnDestroy);
	inline void SetOnMouse(tSimDisp_OnMouse lpfnOnMouse) reflect_to(this->lpfnOnMouse = lpfnOnMouse);
	inline void SetOnResize(tSimDisp_OnResize lpfnOnResize) reflect_to(this->lpfnOnResize = lpfnOnResize);
	inline Message &Send(SIDI_MSG msg, WPARAM wParam = 0, LPARAM lParam = 0) {
		activity.WaitForSignal();
		actionDone.Reset();
		actionBox.Post(msg, wParam, lParam);
		assert(actionDone.WaitForSignal());
		activity.Release();
		return recieved;
	}
	inline auto&HostProcess() reflect_as(processHost);
} *lpSimHost = O;

#pragma region Function Exportation

REG_FUNC(BOOL, Open, const wchar_t *lpTitle, uint16_t xSize, uint16_t ySize) reflect_as(lpSimHost->Open(lpTitle, xSize, ySize));
REG_FUNC(BOOL, Show, BOOL bShow) reflect_as(lpSimHost->Send(SIDI_MSG::Show, bShow).ParamW<BOOL>());
REG_FUNC(void, Close, void) reflect_to(lpSimHost->Send(SIDI_MSG::Close));
REG_FUNC(void, UserClose, void) reflect_to(lpSimHost->UserClose());

REG_FUNC(void, GetSize, uint16_t *xSize, uint16_t *ySize) {
	auto &msg = lpSimHost->Send(SIDI_MSG::GetSize);
	if (xSize) *xSize = msg.ParamW<uint16_t>();
	if (ySize) *ySize = msg.ParamL<uint16_t>();
}
REG_FUNC(void, GetSizeMax, uint16_t *xSize, uint16_t *ySize) {
	auto &msg = lpSimHost->Send(SIDI_MSG::GetSizeMax);
	if (xSize) *xSize = msg.ParamW<uint16_t>();
	if (ySize) *ySize = msg.ParamL<uint16_t>();
}

REG_FUNC(void *, GetBuffer, void) {
	static File bmpBuff;
	static File::MapPointer buffer;
	if (auto ptr = &buffer) return ptr;
	bmpBuff = File().OpenMapping(Cats(_T("SIDI"), lpSimHost->HostProcess().ID()));
	buffer = bmpBuff.MapView();
	return &buffer;
}
REG_FUNC(BOOL, Flush, void) reflect_as(lpSimHost->Send(SIDI_MSG::Flush).ParamW<BOOL>());
REG_FUNC(BOOL, AutoFlush, BOOL bEnable) reflect_as(lpSimHost->Send(SIDI_MSG::AutoFlush, bEnable).ParamW<BOOL>());

REG_FUNC(BOOL, HideCursor, BOOL bHide) reflect_as(lpSimHost->Send(SIDI_MSG::HideCursor, bHide).ParamW<BOOL>());
REG_FUNC(BOOL, Resizeable, BOOL bEnable) reflect_as(lpSimHost->Send(SIDI_MSG::Resizeable, bEnable).ParamW<BOOL>());

REG_FUNC(void, SetOnDestroy, tSimDisp_OnDestroy lpfnOnDestroy) reflect_to(lpSimHost->SetOnDestroy(lpfnOnDestroy));
REG_FUNC(void, SetOnMouse, tSimDisp_OnMouse lpfnOnMouse) reflect_to(lpSimHost->SetOnMouse(lpfnOnMouse));
REG_FUNC(void, SetOnResize, tSimDisp_OnResize lpfnOnResize) reflect_to(lpSimHost->SetOnResize(lpfnOnResize));

#pragma endregion

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!lpSimHost)
				lpSimHost = new SiDiHost;
			break;
		case DLL_PROCESS_DETACH:
			if (lpSimHost)
				delete lpSimHost,
				lpSimHost = O;
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}
