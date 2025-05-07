#include "wx_realtime.h"

#define SIMDISP_HOST
#define DLL_IMPORTS 1
#include "SimDisp.h"
#include "SimHost.h"

using namespace WX;

namespace SimDispHost {

	bool bClientBlocked = false;

	Process processClient; // 客戶端進程
	Thread actionBoxClient; // 客戶端活動盒綫程
	Thread eventBoxClient; // 客戶端事件盒綫程

	LThread watchdog = [] { // 看門狗綫程
		try {
			/* 等待宿主綫程執行完成 */
			eventBoxClient.WaitForSignal();
		} catch (Exception err) {
			MsgBox(_T("Error"), err.operator String(), MB::IconError);
		}
		/* 結束宿主進程 */
		Process::Exit();
	};

	Message eventRet; // 事件返迴消息
	/// @brief 事件盒綫程
	struct BaseOf_Thread(EventBox) {
		void OnStart() {
			/* 獲取綫程消息 */
			while (eventRet.GetThread()) {
				/* 阻塞客戶端事件響應 */
				bClientBlocked = true;
				/* 向客戶端事件盒轉發消息 */
				eventBoxClient.Post(eventRet);
				/* 等待客戶端迴應消息 */
				assertl(eventRet.GetThread());
				/* 通塞客戶端事件響應 */
				bClientBlocked = false;
			}
		}
	} eventBox; // 事件盒綫程

	/// @brief 鼠標事件
	/// @param xPos 繪圖面板内光標x
	/// @param yPos 繪圖面板内光標y
	/// @param zPos 鼠標滾輪數
	/// @param MouseKeys 鼠鍵結構
	void OnMouse(int16_t xPos, int16_t yPos, int16_t zPos, tSimDisp_MouseKey MouseKeys) {
		if (bClientBlocked) return;
		eventBox.Post(SIDI_MSG::SetOnMouse, MAKEWPARAM(xPos, yPos), MAKELPARAM(zPos, reuse_as<uint8_t>(MouseKeys)));
	}

	/// @brief 鍵盤事件
	/// @param vk 虛擬鍵盤按鍵值
	/// @param bPressed 是否按下
	void OnKey(UINT vk, BOOL bPressed) {
		if (bClientBlocked) return;
		eventBox.Post(SIDI_MSG::SetOnKey, vk, bPressed);
	}

	/// @brief 窗體關閉事件
	void OnClose() {
		while (bClientBlocked) {}
		eventBox.Post(SIDI_MSG::SetOnClose);
	}

	/// @brief 窗體重設尺寸響應事件
	/// @param nSizeX 繪圖面板新尺寸x
	/// @param nSizeY 繪圖面板新尺寸y
	/// @return 是否接受新尺寸
	BOOL OnResize(uint16_t nSizeX, uint16_t nSizeY) {
		if (bClientBlocked) return FALSE;
		eventBox.Post(SIDI_MSG::SetOnResize, nSizeX, nSizeY);
		while (bClientBlocked) {}
		return eventRet.ParamW<BOOL>();
	}

	/// @brief 宿主消息事件響應器
	/// @param clientBoxID 客戶端事件盒綫程ID
	void HostEventProc(DWORD clientBoxID) {
		SimDisp::LoadDll(L"SimDisp.dll");
		/* 根據ID開啓客戶端事件盒綫程 */
		eventBoxClient = Thread
			::Open(clientBoxID)
			.Accesses(ThreadAccess::QueryInfo | HandleAccess::Sync);
		/* 創建宿主看門狗綫程 */
		assertl(watchdog.Create());
		Message msg;
		/* 初始化擋墻綫程的消息序列 */
		(void)(bool)msg.Peek();
		/* 向客戶端事件盒發送首個消息 */
		eventBoxClient.Post(SIDI_MSG::SIDI_NULL);
		/* 等待客戶端迴應消息 */
		assertl(msg.GetThread());
		/* 從迴應得消息中取得客戶端活動盒綫程和客戶端進程 */
		processClient = msg.ParamL<Process>();
		actionBoxClient = msg.ParamW<Thread>();
		/* 循環處理消息 */
		while (msg.GetThread()) {
			try {
				switch (msg.ID<SIDI_MSG>()) {
					case SIDI_MSG::Open:
						if (SimDisp::Open(L"", msg.ParamW<uint16_t>(), msg.ParamL<uint16_t>())) {
							assertl(eventBox.Create());
							msg.Param(true, eventBox.ID());
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
					case SIDI_MSG::HideCursor:
						msg.ParamW(SimDisp::HideCursor(msg.ParamW<BOOL>()));
						break;
					case SIDI_MSG::Resizeable:
						msg.ParamW(SimDisp::Resizeable(msg.ParamW<BOOL>()));
						break;
					case SIDI_MSG::SetOnClose:
						if (msg.ParamW<BOOL>())
							SimDisp::SetOnClose(OnClose);
						else
							SimDisp::SetOnClose(nullptr);
						break;
					case SIDI_MSG::SetOnMouse:
						if (msg.ParamW<BOOL>())
							SimDisp::SetOnMouse(OnMouse);
						else
							SimDisp::SetOnMouse(nullptr);
						break;
					case SIDI_MSG::SetOnKey:
						if (msg.ParamW<BOOL>())
							SimDisp::SetOnKey(OnKey);
						else
							SimDisp::SetOnKey(nullptr);
						break;
					case SIDI_MSG::SetOnResize:
						if (msg.ParamW<BOOL>())
							SimDisp::SetOnResize(OnResize);
						else
							SimDisp::SetOnResize(nullptr);
						break;
					case SIDI_MSG::GetHWND:
						msg.ParamW(SimDisp::GetHWND());
						break;
					case SIDI_MSG::ConsoleEnableShow:
						msg.ParamW(SimDisp::ConsoleEnableShow(msg.ParamW<BOOL>()));
						break;
					case SIDI_MSG::SIDI_EXIT:
						goto _exit;
					default:
						break;
				}
				/* 迴傳響應消息 */
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

};

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd) {
	DWORD tid = 0;
	swscanf_s(lpCmdLine, L"%d", &tid);
	try {
		SimDispHost::HostEventProc(tid);
	} catch (Exception err) {
		MsgBox(_T("Error"), err.operator String(), MB::IconError);
	}
	return 0;
}
