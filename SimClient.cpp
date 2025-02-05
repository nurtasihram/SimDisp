

#include "./wx/realtime.h"

#define SIMDISP_HOST
#include "SimHost.h"

#undef SIMDISP_HOST
#define DLL_EXPORTS
#include "SimDisp.h"

using namespace WX;

namespace SimDispClient {

	Mutex activity;
	Process processHost; // 宿主進程
	Thread actionBoxHost; // 宿主活動盒
	Thread eventBoxHost; // 宿主事件盒
	Message eventRet; // 事件返迴消息
	Event eventDone; // 完成事件

	struct BaseOf_Thread(EventBox) {
		void OnStart() {
			Message msg;
			/* 初始化擋墻綫程的消息序列 */
			assertl(!msg.PeekThread());
			/* 設置活動完成事件 */
			eventDone.Set();
			/* 循環獲取活動事件消息 */
			while (msg.GetThread()) {
				/* 向宿主事件盒轉發消息 */
				actionBoxHost.Post(msg);
				/* 等待迴應消息 */
				eventRet.GetThread();
				/* 設置完成事件 */
				eventDone.Set();
			}
		}
		inline void OnCatch(const Exception &err) {
			MsgBox(Cats(_T("Client EventBox error [PID:"), ID(), _T("]")), err.operator String(), MB::IconError);
			eventDone.Set();
		}
	} eventBox;

	Event eventClose;
	tSimDisp_OnClose _lpfnOnClose = O; // 窗體關閉事件類
	tSimDisp_OnMouse _lpfnOnMouse = O; // 鼠標事件類
	tSimDisp_OnResize _lpfnOnResize = O; // 窗體重設尺寸響應事件類
	/// @brief 活動盒綫程類
	struct BaseOf_Thread(ActionBox) {
		void OnStart() {
			Message msg;
			/* 初始化綫程的消息序列 */
			assertl(!msg.PeekThread());
			/* 開啓宿主進程 */
			auto ps = Process::Create(O, Cats(S("SimHost.exe "), ID()))
				.NewConsole()
				.Security(InheritHandle)
				.InheritHandles();
			processHost = ps;
			actionBoxHost = ps;
			/* 等待宿主完成初始化 */
			assertl(msg.GetThread());
			/* 設置完成事件 */
			eventDone.Set();
			/* 循環獲取活動事件消息 */
			while (msg.GetThread())
				switch (msg.ID<SIDI_MSG>()) {
					case SIDI_MSG::SetOnClose:
						if (_lpfnOnClose)
							_lpfnOnClose();
						eventClose.Set();
						eventBoxHost.Post(msg);
						break;
					case SIDI_MSG::SetOnMouse:
						if (_lpfnOnMouse)
							_lpfnOnMouse(LOWORD(msg.ParamW()), HIWORD(msg.ParamW()),
										 LOWORD(msg.ParamL()), force_cast<tSimDisp_MouseKey>(HIWORD(msg.ParamL())));
						eventBoxHost.Post(msg);
						break;
					case SIDI_MSG::SetOnResize:
						if (_lpfnOnResize)
							msg.ParamW(_lpfnOnResize(msg.ParamW<uint16_t>(), msg.ParamL<uint16_t>()));
						else
							msg.ParamW(true);
						eventBoxHost.Post(msg);
						break;
					default:
						break;
				}
		}
		inline void OnCatch(const Exception &err) {
			MsgBox(Cats(_T("Client ActionBox error [PID:"), ID(), _T("]")), err.operator String(), MB::IconError);
			eventDone.Set();
		}
	} actionBox; // 活動盒綫程

	/// @brief 發送消息至宿主
	/// @param msg 活動ID
	/// @param wParam 參數w
	/// @param lParam 參數l
	/// @return 迴應消息
	inline Message &Send(SIDI_MSG msg, WPARAM wParam = 0, LPARAM lParam = 0) {
		activity.WaitForSignal();
		eventDone.Reset();
		eventBox.Post(msg, wParam, lParam);
		assertl(eventDone.WaitForSignal());
		activity.Release();
		return eventRet;
	}

	/// @brief 開啓虛擬銀幕
	/// @param lpTitle 窗體標題
	/// @param xSize 虛擬銀幕寬
	/// @param ySize 虛擬銀幕高
	/// @return 是否開啓成功
	inline bool Open(const wchar_t *lpTitle, uint16_t xSize, uint16_t ySize) {
		/* 創建事件盒 */
		if (eventBox.StillActive()) eventBox.Terminate(), eventBox.Close();
		eventDone.Reset();
		assertl(eventBox.Create().Security(InheritHandle));
		eventDone.WaitForSignal();
		/* 創建活動盒 */
		if (actionBox.StillActive()) eventBox.Terminate(), eventBox.Close();
		eventDone.Reset();
		assertl(actionBox.Create());
		eventDone.WaitForSignal();
		actionBoxHost.Post(SIDI_MSG::SIDI_NULL, (HANDLE)eventBox, GetCurrentProcess());
		/* 發送開啓消息 */
		auto &msg = Send(SIDI_MSG::Open, xSize, ySize);
		if (!msg.ParamW<bool>())
			return false;
		/* 開啓宿主事件盒 */
		eventBoxHost = Thread::Open(msg.ParamL<DWORD>())
			.Accesses(ThreadAccess::QueryInfo | HandleAccess::Sync);
		if (!lpTitle)
			lpTitle = L"SimDisp (CLIENT) By Nurtas Ihram " __DATE__;
		auto hwnd = Send(SIDI_MSG::GetHWND).ParamW<HWND>();
		return SetWindowTextW(hwnd, lpTitle);
	}

	inline void Close() {
		Send(SIDI_MSG::Close);
		if (actionBox.StillActive())
			actionBox.Terminate();
		if (eventBox.StillActive())
			eventBox.Terminate();
		if (actionBoxHost.StillActive())
			processHost.Terminate();
	}

	/// @brief 等待用戶關閉
	inline void UserClose() {
		if (eventBoxHost.StillActive()) {
			eventClose.Reset();
			eventClose.WaitForSignal();
		}
	}

	/// @brief 窗體關閉事件類
	inline void SetOnClose(tSimDisp_OnClose lpfnOnClose) {
		lpfnOnClose = lpfnOnClose;
		Send(SIDI_MSG::SetOnClose, (bool)lpfnOnClose);
	}

	/// @brief 鼠標事件類
	/// @param xPos 繪圖面板内光標x
	/// @param yPos 繪圖面板内光標y
	/// @param zPos 鼠標滾輪數
	/// @param MouseKeys 鼠鍵結構
	inline void SetOnMouse(tSimDisp_OnMouse lpfnOnMouse) {
		_lpfnOnMouse = lpfnOnMouse;
		Send(SIDI_MSG::SetOnMouse, (bool)_lpfnOnClose);
	}

	/// @brief 窗體重設尺寸響應事件類
	/// @param nSizeX 繪圖面板新尺寸x
	/// @param nSizeY 繪圖面板新尺寸y
	/// @return 是否接受新尺寸
	inline void SetOnResize(tSimDisp_OnResize lpfnOnResize) {
		_lpfnOnResize = lpfnOnResize;
		Send(SIDI_MSG::SetOnResize, (bool)_lpfnOnClose);
	}

	inline auto&HostProcess() reflect_as(processHost);

}

#pragma region Function Exportation

REG_FUNC(BOOL, Open, const wchar_t *lpTitle, uint16_t xSize, uint16_t ySize) reflect_as(SimDispClient::Open(lpTitle, xSize, ySize));
REG_FUNC(BOOL, Show, BOOL bShow) reflect_as(SimDispClient::Send(SIDI_MSG::Show, bShow).ParamW<BOOL>());
REG_FUNC(void, Close, void) reflect_to(SimDispClient::Close());
REG_FUNC(void, UserClose, void) reflect_to(SimDispClient::UserClose());

REG_FUNC(void, GetSize, uint16_t *xSize, uint16_t *ySize) {
	auto &msg = SimDispClient::Send(SIDI_MSG::GetSize);
	if (xSize) *xSize = msg.ParamW<uint16_t>();
	if (ySize) *ySize = msg.ParamL<uint16_t>();
}
REG_FUNC(void, GetSizeMax, uint16_t *xSize, uint16_t *ySize) {
	auto &msg = SimDispClient::Send(SIDI_MSG::GetSizeMax);
	if (xSize) *xSize = msg.ParamW<uint16_t>();
	if (ySize) *ySize = msg.ParamL<uint16_t>();
}

REG_FUNC(void *, GetColors, void) {
	static File bmpBuff;
	static File::MapPointer buffer;
	if (auto ptr = &buffer) return ptr;
	bmpBuff = File().OpenMapping(Cats(_T("SIDI"), SimDispClient::HostProcess().ID()));
	buffer = bmpBuff.MapView();
	return &buffer;
}

REG_FUNC(BOOL, HideCursor, BOOL bHide) reflect_as(SimDispClient::Send(SIDI_MSG::HideCursor, bHide).ParamW<BOOL>());
REG_FUNC(BOOL, Resizeable, BOOL bEnable) reflect_as(SimDispClient::Send(SIDI_MSG::Resizeable, bEnable).ParamW<BOOL>());

REG_FUNC(void, SetOnClose, tSimDisp_OnClose lpfnOnClose) reflect_to(SimDispClient::SetOnClose(lpfnOnClose));
REG_FUNC(void, SetOnMouse, tSimDisp_OnMouse lpfnOnMouse) reflect_to(SimDispClient::SetOnMouse(lpfnOnMouse));
REG_FUNC(void, SetOnResize, tSimDisp_OnResize lpfnOnResize) reflect_to(SimDispClient::SetOnResize(lpfnOnResize));

REG_FUNC(HWND, GetHWND, void) reflect_as(SimDispClient::Send(SIDI_MSG::GetHWND).ParamW<HWND>());
REG_FUNC(DWORD, ConsoleEnableShow, BOOL bEnableOpen) reflect_as(SimDispClient::Send(SIDI_MSG::ConsoleEnableShow, bEnableOpen).ParamW<DWORD>());

#pragma endregion

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_PROCESS_DETACH:
			SimDispClient::Close();
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}
