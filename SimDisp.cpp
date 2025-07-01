
#include <atlbase.h>

#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")

#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")

#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "wx_console.h"
#include "wx_window.h"
#include "wx_control.h"
#include "wx_dialog.h"
#include "wx_realtime.h"
#include "wx_file.h"

#define DLL_EXPORTS
#include "SimDisp.h"

#include "resource.h"

HINSTANCE hinst = NULL;

#pragma region SimDisp

using namespace WX;

auto
&&d02_ = "02d"_nx,
&&d4_  = " 4d"_nx;

/// @brief 繪製徽標
/// @param dc 繪圖裝置
/// @param org 繪圖原點
/// @param r 徽標半徑
/// @return 原返繪圖裝置
static DC &TayKit(DC &dc, LPoint org, int r) {
	auto &&brushWhite = Brush::White(),
		 &&brushBlack = Brush::Black();
	auto &&penWhite = Pen::White(),
		 &&penBlack = Pen::Black();
	auto &&rect0 = org + LRect{ -r, -r, r, r };
	--r;
	auto &&rect1 = org + LRect{ -r / 2, -r, r / 2, 0 };
	auto &&p1 = org + LPoint{ 0, -r },
		 &&p2 = org + LPoint{ 0,  r };
	dc(penBlack);
	dc(brushWhite).DrawPie(rect0, p1, p2);
	dc(brushBlack).DrawPie(rect0, p2, p1);
	dc(brushWhite)(penWhite).DrawEllipse(rect1);
	dc(brushBlack)(penBlack).DrawEllipse(rect1 + LPoint{ 0, r });
	rect1 = (rect1 - org) / 3 + org;
	dc(brushBlack)(penBlack).DrawEllipse(rect1 - LPoint{ 0, (r - 1) / 3 });
	dc(brushWhite)(penWhite).DrawEllipse(rect1 + LPoint{ 0, (r - 1) / 3 * 2 });
	return dc;
}
static Icon makeIcon(int size) {
	Bitmap bmpColor = Bitmap().Create(size);
	Bitmap bmpMask = Bitmap::Create(size).BitsPerPixel(1);
	TayKit(WX::DC::CreateCompatible()(bmpColor)
		   .Fill(Brush::Black()), size / 2, size / 2);
	DC::CreateCompatible()(bmpMask)
		.Fill(Brush::White())
		(Brush::Black()).DrawEllipse({ 0, 0, size, size });
	return Icon::Create(bmpColor, bmpMask);
};

/// @brief SimDisp窗體類
class BaseOf_Window(SimDispWnd) {
	SFINAE_Window(SimDispWnd);
private:

	/// @brief 繪圖面板邊界
	LRect Border = 20;

	/// @brief 繪圖面板窗體類
	class BaseOf_Window(GraphPanel) {
		SFINAE_Window(GraphPanel);
	private:

		/// @brief 父窗體
		SimDispWnd &parent;

#pragma region Direct2D
	private: // Direct2D 對象

		CComPtr<ID2D1Factory> D2D_Factory;
		CComPtr<IWICImagingFactory> WIC_Factory;
		CComPtr<ID2D1HwndRenderTarget> RenderTarget;
		CComPtr<IWICBitmap> wicBitmap;
		CComPtr<ID2D1Bitmap> D2D_bitmap;

		/// @brief Direct2D 共享位圖
		class MyBitmap : public IWICBitmapLock {

			/// @brief 位圖尺寸
			LSize size = { GetSystemMetrics(SM_CXSCREEN) + 1, GetSystemMetrics(SM_CYSCREEN) + 1 };

			/// @brief 内存映射文件
			///		命名的内存映射文件，用扵跨進程共享位圖色彩緩衝區
			File bmpBuff = File()
				.CreateMapping(size.cx * size.cy * 4)
				.Name(Cats(_T("SIDI"), GetCurrentProcessId()))
				.Security(InheritHandle);

			/// @brief 色彩緩衝文件
			///		命名的内存映射文件，用扵跨進程共享位圖色彩緩衝區
			File::MapPointer ptr = bmpBuff.MapView();

			/// @brief 色彩緩衝區
			uint32_t *lpBits = ptr;

		private:
			HRESULT STDMETHODCALLTYPE GetSize(
				__RPC__out UINT *puiWidth,
				__RPC__out UINT *puiHeight) override {
				*puiWidth = size.cx;
				*puiHeight = size.cy;
				return S_OK;
			}
			HRESULT STDMETHODCALLTYPE GetStride(
				__RPC__out UINT *pcbStride) override {
				*pcbStride = size.cx * 4;
				return S_OK;
			}
			HRESULT STDMETHODCALLTYPE GetDataPointer(
				__RPC__out UINT *pcbBufferSize,
				__RPC__deref_out_ecount_full_opt(*pcbBufferSize) WICInProcPointer *ppbData) override {
				*pcbBufferSize = size.cx * size.cy * 4;
				*ppbData = (WICInProcPointer)lpBits;
				return S_OK;
			}
			HRESULT STDMETHODCALLTYPE GetPixelFormat(
				__RPC__out WICPixelFormatGUID *pPixelFormat) override {
				*pPixelFormat = GUID_WICPixelFormat32bppBGR;
				return S_OK;
			}
			HRESULT STDMETHODCALLTYPE QueryInterface(
				REFIID riid,
				_COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override {
				return S_OK;
			}
			ULONG STDMETHODCALLTYPE AddRef(void) override {
				return 0;
			}
			ULONG STDMETHODCALLTYPE Release(void) override {
				return 0;
			}

		public:

			/// @brief 位圖字節數
			inline auto Bytes() reflect_as(size.cx * size.cy * 4);

			/// @brief 文件映射句柄
			inline HANDLE FileMapping() const reflect_as(bmpBuff);

			/// @brief 位圖尺寸
			inline LSize Size() const reflect_as(size);

			// @brief 位圖緩衝區
			inline auto Colors() reflect_as(lpBits);

		} bmpItf;
#pragma endregion

	public:
		bool bMaskKeyboard = false, // 是否屏蔽鍵盤響應事件
			 bMaskMouse = false, // 是否屏蔽鼠標響應事件
			 bMaskTouch = false, // 是否屏蔽觸摸響應事件
			 bHideCursor = false; // 是否在繪圖面板上隱藏光標

	public:
		GraphPanel(SimDispWnd &parent) : parent(parent) {}

#pragma region Precreate
	private: // 預建制
		WxClass() {
			xClass() {
				Styles(CStyle::Redraw);
				Cursor(IDC_ARROW);
			}
		};
	public:
		inline auto Create() {
			return super::Create()
				.Parent(parent)
				.Styles(WS::Child | WS::Visible);
		}
#pragma endregion

#pragma region Events
	private: // 窗體事件組

		/// @brief 創建窗體事件
		///		初始化Direct2D對象
		/// @return 是否建制成功
		inline bool OnCreate(RefAs<CreateStruct *>) {
			assertl(SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2D_Factory)));
			assertl(SUCCEEDED(CoCreateInstance(
				CLSID_WICImagingFactory,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IWICImagingFactory,
				(void **)&WIC_Factory)));
			auto sz = Size();
			assertl(SUCCEEDED(D2D_Factory->CreateHwndRenderTarget(
				D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE),
				D2D1::HwndRenderTargetProperties(self, D2D1::SizeU(sz.cx, sz.cy)),
				&RenderTarget)));
			assertl(SUCCEEDED(RenderTarget->CreateSharedBitmap(
				IID_IWICBitmapLock, &bmpItf, O, &D2D_bitmap)));
			memset(bmpItf.Colors(), ~0, bmpItf.Bytes());
			return true;
		}

		/// @brief 窗體繪製事件
		///		渲染Direct2D圖像
		inline void OnPaint() {
			auto &&ps = BeginPaint();
			if (RenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED)
				return;
			OnRender();
		}

		/// @brief 尺寸改變後事件
		/// @param state 狀態
		/// @param cx 新寬
		/// @param cy 新高
		inline void OnSize(UINT state, int cx, int cy) {
			if (RenderTarget)
				RenderTarget->Resize(D2D1::SizeU(cx, cy));
		}

#pragma region Mouse
	private: // 鼠標事件組

		/// @brief 設置光標事件
		inline bool OnSetCursor(HWND hwndCursor, UINT codeHitTest, UINT msg) {
			if (!bHideCursor)
				return false;
			if (!bMaskMouse) {
				SetCursor(O);
				return true;
			}
			static auto &&CursorNo = Module()->Cursor(IDC_NO);
			SetCursor(CursorNo);
			return true;
		}

		/// @brief 鼠標移動事件
		/// @param x 窗體内光標x
		/// @param y 窗體内光標y
		/// @param keyFlags 鼠標鍵
		inline void OnMouseMove(int x, int y, UINT keyFlags) {
			if (TrackMouse().Flags(TME::Flag::Hover | TME::Flag::Leave))
				parent.OnPanelMouse(x, y, keyFlags);
		}
		inline void OnLButtonUp(int x, int y, UINT keyFlags) { parent.OnPanelMouse(x, y, keyFlags); }
		inline void OnMButtonUp(int x, int y, UINT keyFlags) { parent.OnPanelMouse(x, y, keyFlags); }
		inline void OnRButtonUp(int x, int y, UINT keyFlags) { parent.OnPanelMouse(x, y, keyFlags); }
		inline void OnLButtonDown(int x, int y, UINT keyFlags) { parent.OnPanelMouse(x, y, keyFlags); }
		inline void OnMButtonDown(int x, int y, UINT keyFlags) { parent.OnPanelMouse(x, y, keyFlags); }
		inline void OnRButtonDown(int x, int y, UINT keyFlags) { parent.OnPanelMouse(x, y, keyFlags); }
		inline void OnMouseLeave() { parent.OnPanelMouse(-1, -1, 0); }
		inline void OnMouseWheel(int x, int y, int z, UINT fwKeys) {
			auto &&pt = ScreenToClient({ x, y });
			parent.OnPanelMouse(pt.x, pt.y, fwKeys, z);
		}
#pragma endregion
#pragma endregion

	public:

		/// @brief 渲染位圖
		inline void OnRender() {
			if (RenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED)
				return;
			RenderTarget->BeginDraw();
			auto &&size = D2D_bitmap->GetSize();
			RenderTarget->DrawBitmap(
				D2D_bitmap,
				D2D1::RectF(0.0f, 0.0f, size.width, size.height)
			);
			assertl(SUCCEEDED(RenderTarget->EndDraw()));
		}

		/// @brief 保存位圖到文件
		/// @param lpFilename 文件名
		/// @return 是否保存成功
		inline bool SaveToFile(LPCTSTR lpFilename) {
			try {
				::SaveToFile(
					DC(),
					Palette::Default(),
					ClipBitmap(
						Bitmap::Create(MaxSize()).Colors(Colors()),
						Size()),
					File::Create(lpFilename).CreateAlways().Accesses(FileAccess::GenericWrite));
				return true;
			} catch (WX::Exception err) {
				parent.MsgBox(_T("Save to file error"), err.operator String(), MB::IconError);
			}
			return false;
		}

		/// @brief 位圖色彩緩衝區
		inline void *Colors() reflect_as(bmpItf.Colors());

		/// @brief 文件映射句柄
		inline HANDLE FileMapping() reflect_as(bmpItf.FileMapping());

		/// @brief 位圖最大尺寸
		inline LSize MaxSize() const reflect_as(bmpItf.Size());

	} panel = self;

#pragma region Precreate
private: // 預建制
	/// @brief 主要功能選單指令ID
	enum {
		IDM_NULL,
		IDM_PRINTSCREEN,
		IDM_EXIT,
		IDM_RESIZE,
		IDM_INVERT,
		IDM_HIDECONSOLE,
		IDM_KEYBOARDMAP,
		IDM_HIDECURSOR,
		IDM_LOCKCURSOR,
		IDM_MASK_MOUSE,
		IDM_MASK_TOUCH,
		IDM_MASK_KEYBOARD,
		IDM_ABOUT
	};
	/// @brief 主要功能選單
	WX::Menu menu;
	WxClassEx() {
		xClass() {
			Styles(CStyle::Redraw);
			Cursor(IDC_ARROW);
			Background(SysColor::Window);
			Icon(makeIcon(128));
			IconSmall(makeIcon(48));
		}
	};

public:
	inline auto Create() {
		return super::Create()
			.Styles(WS::MinimizeBox | WS::Caption | WS::SysMenu | WS::ClipChildren)
			.Size(500)
			.Position(100)
			.Menu(menu
				  .Popup(_T("&Screen"), MenuPopup()
						 .String(_T("&Print Screen"), IDM_PRINTSCREEN)
						 .Separator()
						 .String(_T("&Resize Screen"), IDM_RESIZE, bResizeable)
						 .String(_T("&Invert Screen"), IDM_INVERT, bResizeable)
						 .Separator()
						 .String(_T("Show C&onsole"), IDM_HIDECONSOLE, bConsole)
						 .String(_T("Show &Keyboard Map"), IDM_KEYBOARDMAP)
						 .Separator()
						 .Popup(_T("&Cursor Control"), MenuPopup()
								.Check(_T("&Hide Cursor On Screen"), IDM_HIDECURSOR, false, true)
								.Check(_T("&Lock Mouse On Screen"), IDM_LOCKCURSOR))
						 .Separator()
						 .String(_T("&Exit"), IDM_EXIT))
				  .Popup(_T("&Event"), MenuPopup()
						 .Check(_T("Mask &Mouse Event"), IDM_MASK_MOUSE, panel.bMaskMouse)
						 .Check(_T("Mask &Touch Event"), IDM_MASK_TOUCH, panel.bMaskTouch)
						 .Check(_T("Mask &Keyboard Event"), IDM_MASK_KEYBOARD, panel.bMaskKeyboard))
				  .Popup(_T("&Help"), MenuPopup()
						 .String(_T("&About"), IDM_ABOUT)));
	}
#pragma endregion

#pragma region Events
private:

	/// @brief 狀態欄
	StatusBar sbar;

	/// @brief 創建窗體事件
	/// @param lpCreate 創建結構指針
	/// @return 是否建制成功
	inline bool OnCreate(RefAs<CreateStruct *> lpCreate) {
		assertl(sbar.Create(self));
		assertl(panel.Create()
			   .Position(Border.left_top())
			   .Size(lpCreate->Size() - LSize(1)));
		Size(AdjustRect(lpCreate->Size() + Border.right_bottom() + Border.left_top() + LPoint(0, sbar.Size().cy)));
		uIDTimer_FlushPriod = SetTimer(1, 12);
		return true;
	}

	/// @brief 窗體關閉事件迴調函數
	void(*pfnOnClose)() = O;
	/// @brief 窗體關閉事件
	inline void OnClose() {
		if (bConsole)
			wCon.Close();
		if (uIDTimer_FlushPriod)
			KillTimer(uIDTimer_FlushPriod),
			uIDTimer_FlushPriod = 0;
		if (wCon)
			wCon.Styles(WS::Visible | WS::OverlappedWindow);
		if (pfnOnClose)
			pfnOnClose();
		PostQuitMessage(0);
	}

	/// @brief 尺寸改變後事件
	/// @param state 狀態
	/// @param cx 新寬
	/// @param cy 新高
	inline void OnSize(UINT state, int cx, int cy) {
		auto sz = Size();
		sbar.SetParts({
			sz.cx / 6,
			2 * sz.cx / 6,
			3 * sz.cx / 6,
			4 * sz.cx / 6,
			-1 });
		sz = panel.Size();
		sbar.Text(0, Cats(sz.cx, _T("x"), sz.cy));
		sbar.FixSize();
	}

	class BaseOf_Dialog(Keyboard) {
		SFINAE_Dialog(Keyboard);
		SimDispWnd &parent;
	public:
		Keyboard(SimDispWnd &parent) : parent(parent) {}
		~Keyboard() {}

	private:
		static inline auto Forming() reflect_as(IDD_OSK);

	private:

		UINT_PTR uIDTimer_LedPriod = 0;
		bool InitDialog() {
			uIDTimer_LedPriod = SetTimer(50, 0);
			return true;
		}

		/// @brief 窗體關閉事件
		void OnClose() reflect_to(parent.ShowKeyboard(false));

		bool bNumLock = false,
			 bCapsLock = false,
			 bScroll = false;
		void OnTimer(UINT id) {
			bool bNumLock = GetKeyState(VK_NUMLOCK);
			if (this->bNumLock != bNumLock) {
				Item<Button>(IDC_LED_NUM).Invalidate();
				this->bNumLock = bNumLock;
			}
			bool bCapsLock = GetKeyState(VK_CAPITAL);
			if (this->bCapsLock != bCapsLock) {
				Item<Button>(IDC_LED_CAPS).Invalidate();
				this->bCapsLock = bCapsLock;
			}
			bool bScroll = GetKeyState(VK_SCROLL);
			if (this->bScroll != bScroll) {
				Item<Button>(IDC_LED_SCROLL).Invalidate();
				this->bScroll = bScroll;
			}
		}

		Brush greenBrush = Brush::CreateSolid(RGB(0, 255, 0));
		inline HBRUSH OnCtlColorStatic(HDC hdc, HWND hwndChild) {
			if (hwndChild == Item<Button>(IDC_LED_NUM)) {
				if (bNumLock)
					return greenBrush;
			}
			elif (hwndChild == Item<Button>(IDC_LED_CAPS)) {
				if (bCapsLock)
					return greenBrush;
			}
			elif (hwndChild == Item<Button>(IDC_LED_SCROLL)) {
				if (bScroll)
					return greenBrush;
			}
			HandleNext();
			return O; // Unreachable
		}

	public:

		/// @brief 顯示鍵盤事件
		/// @param scanCode 鍵盤掃描碼
		/// @param bPressed 是否按下
		void ShowKey(UINT scanCode, bool bPressed) {
			if (!self)
				return;
			if (auto &&btn = Item<Button>(scanCode + SCAN_CODE_0))
				btn.States(bPressed ? ButtonState::Checked : ButtonState::Unchecked);
		}

	};
	/// @brief 虛擬鍵盤對話框
	Keyboard dlg = self;
	/// @brief 顯示虛擬鍵盤
	/// @param bKeyboard 顯示虛擬鍵盤
	void ShowKeyboard(bool bKeyboard) {
		if (bKeyboard) {
			if (!dlg)
				dlg.Create(O, hinst);
			dlg.Show();
		}
		elif (dlg)
			dlg.Hide();
		this->bKeyboard = bKeyboard;
		if (menu)
			menu(IDM_KEYBOARDMAP).Check(bKeyboard);
	}
	/// @brief 虛擬鍵盤事件迴調函數
	tSimDisp_OnKey pfnOnKey = O;
	/// @brief 按鍵按下事件
	/// @param vk 虛擬鍵碼
	/// @param wRepeat 重復次數
	/// @param flags 鍵盤狀態
	inline void OnKeyDown(UINT vk, int16_t wRepeat, KEY_FLAGS flags) {
		if (panel.bMaskKeyboard) return;
		if (!flags.bPrevious) {
			if (pfnOnKey)
				pfnOnKey(vk, true);
			dlg.ShowKey(flags.wScanCode, true);
		}
	}
	/// @brief 按鍵釋放事件
	/// @param vk 虛擬鍵碼
	/// @param wRepeat 重復次數
	/// @param flags 鍵盤狀態
	inline void OnKeyUp(UINT vk, int16_t wRepeat, KEY_FLAGS flags) {
		if (panel.bMaskKeyboard) return;
		if (pfnOnKey)
			pfnOnKey(vk, false);
		dlg.ShowKey(flags.wScanCode, false);
	}

	bool bLastIconic = false; // 上次是否為圖標狀態
	bool bResizeable = false; // 是否能修改尺寸
	bool bConsole = false; // 是否顯示控制臺
	bool bKeyboard = false; // 是否顯示虛擬鍵盤
	/// @brief 響應新尺寸迴調函數
	tSimDisp_OnResize pfnOnResize = O;
	/// @brief 尺寸位置改變事件
	/// @param pWndPos 窗體尺寸位置信息
	/// @return 是否處理本次事件
	inline bool OnWindowPosChanging(RefAs<WndPos *> pWndPos) {
		if ((pWndPos->Flags() & SWP::NoSize) == SWP::NoSize)
			return false;
		if (bLastIconic) {
			bLastIconic = false;
			return true;
		}
		if (Iconic()) {
			bLastIconic = true;
			return true;
		}
		auto &&border = Size() - ClientSize() + LSize(Border.left_top() + Border.right_bottom());
		if (sbar.Enabled()) border.cy += sbar.Size().cy;
		if (ConsoleVisible()) {
			wCon.Position(Rect().left_bottom())
				.Size({ Size().cx, wCon.Size().cy });
		}
		if (bResizeable) {
			LSize sznPanel = pWndPos->Size() - border + LSize(1), szPanel = panel.Size();
			if (sznPanel.cx <= 0) (*pWndPos)->cx = border.cx + 1, sznPanel.cx = 1;
			if (sznPanel.cy <= 0) (*pWndPos)->cy = border.cy + 1, sznPanel.cy = 1;
			if (pfnOnResize && sznPanel != szPanel) {
				if (!pfnOnResize((uint16_t)sznPanel.cx, (uint16_t)sznPanel.cy)) {
					pWndPos->Flags(pWndPos->Flags() | SWP::NoSize | SWP::NoMove);
					return false;
				}
			}
			panel.Size(sznPanel);
			return true;
		}
		return true;
	}

	/// @brief 窗體指令響應事件
	/// @param id 指令ID
	/// @param hwndCtl 控件句柄
	/// @param codeNotify 提示碼
	inline void OnCommand(int id, HWND hwndCtl, UINT codeNotify) {
		switch (id) {
			case IDM_PRINTSCREEN:
				PrintScreen();
				break;
			case IDM_EXIT:
				Message().OnClose().Post();
				break;
			case IDM_RESIZE:
				OnResizeBox();
				break;
			case IDM_INVERT:
				Resize(~panel.Size());
				break;
			case IDM_HIDECONSOLE:
				if (menu)
					ConsoleShow(!ConsoleVisible());
				break;
			case IDM_KEYBOARDMAP:
				ShowKeyboard(!bKeyboard);
				break;
			case IDM_HIDECURSOR: HideCursor(!panel.bHideCursor); break;
			case IDM_LOCKCURSOR:
				break;
			case IDM_MASK_MOUSE: MaskMouse(!panel.bMaskMouse); break;
			case IDM_MASK_TOUCH: MaskTouch(!panel.bMaskTouch); break;
			case IDM_MASK_KEYBOARD: MaskKeyboard(!panel.bMaskKeyboard); break;
			case IDM_ABOUT:
				MsgBox(
					_T("About: (" __DATE__ ")"),
					_T("Simulator display for embedded OS test\n")
					_T("By Nurtas Ihram"),
					MB::Ok | MB::IconInformation);
				break;
		}
	}

	/// @brief 原先控制臺是否可見
	bool bConsoleRestore = false;
	/// @brief 窗體系統指令響應事件
	/// @param cmd 指令ID
	/// @param x 銀幕光標位置x
	/// @param y 銀幕光標位置y
	inline void OnSysCommand(UINT cmd, int x, int y) {
		switch (cmd) {
			case SC_MINIMIZE:
				bConsoleRestore = ConsoleVisible();
				ConsoleShow(false);
				break;
			case SC_RESTORE:
				ConsoleShow(bConsoleRestore);
				break;
		}
		DefProcOf().OnSysCommand(cmd, x, y);
	}

	/// @brief 週期頁面刷新定時器識別ID
	UINT_PTR uIDTimer_FlushPriod = 0;
	/// @brief 定時器定時相應事件
	/// @param uID 到期點的定時器ID
	inline void OnTimer(UINT uID) {
		if (uID == uIDTimer_FlushPriod)
			panel.OnRender();
	}

private: // 自定義子事件

	/// @brief 鼠標事件迴調函數
	tSimDisp_OnMouse pfnOnMouse = O;
	/// @brief 繪圖面板鼠標迴調事件
	///		同步響應繪圖面板產生的鼠標事件數據
	/// @param x 繪圖面板内光標x
	/// @param y 繪圖面板内光標y
	/// @param keyFlags 鼠鍵標識
	/// @param z 鼠標滾輪數
	inline void OnPanelMouse(int x, int y, UINT keyFlags, int z = 0) {
		sbar.Text(1, Cats(_T("X: "), d4_(x)));
		sbar.Text(2, Cats(_T("Y: "), d4_(y)));
		sbar.Text(3, Cats(_T("Z: "), d4_(z)));
		tSimDisp_MouseKey mk;
		mk.Left = bool(keyFlags & MK_LBUTTON);
		mk.Middle = bool(keyFlags & MK_MBUTTON);
		mk.Right = bool(keyFlags & MK_RBUTTON);
		mk.Control = bool(keyFlags & MK_CONTROL);
		mk.Shift = bool(keyFlags & MK_SHIFT);
		if (panel.bMaskMouse)
			sbar.Text(4, _T("mouse event masked"));
		elif (x < 0 || y < 0) {
			sbar.Text(4, _T("Mouse leave"));
			mk.Leave = 1;
		}
		else
			sbar.Text(4, Cats(
				mk.Left ? _T("L") : _T(" "),
				mk.Middle ? _T("M") : _T(" "),
				mk.Right ? _T("R") : _T(" "),
				mk.Control ? _T("Ctrl") : _T("    "),
				mk.Shift ? _T("Shift") : _T("     ")));
		if (pfnOnMouse && !panel.bMaskMouse)
			pfnOnMouse(x, y, z, mk);
	}

	/// @brief 開啓重設大小對話框
	inline void OnResizeBox() {

		/// @brief 重設大小對話框類
		class BaseOf_Dialog(ResizeBox) {
			SFINAE_Dialog(ResizeBox);
		public:

			/// @brief 新大小
			LSize size;
			ResizeBox(LSize size) : size(size) {}

#pragma region Precreate
		private: // 預建制
			/// @brief 
			enum { IDE_CX = 0x20, IDE_CY };
			static inline LPDLGTEMPLATE Forming() {
				static auto &&hDlg = DFact()
					.Style(WS::Caption | WS::SysMenu | WS::Popup)
					.Caption(L"New size")
					.Size({ 145, 75 })
					.Add(DCtl(L"&X:")
						 .Style(WS::Child | WS::Visible | StaticStyle::CenterImage)
						 .Position({ 24, 12 }).Size({ 14, 14 }))
					.Add(DCtl(DClass::Edit)
						 .Style(WS::Child | WS::Visible | EditStyle::Number | WS::TabStop)
						 .Position({ 42, 12 }).Size({ 72, 14 })
						 .ID(IDE_CX))
					.Add(DCtl(L"&Y:")
						 .Style(WS::Child | WS::Visible | StaticStyle::CenterImage)
						 .Position({ 24, 30 }).Size({ 72, 14 }))
					.Add(DCtl(DClass::Edit)
						 .Style(WS::Child | WS::Visible | EditStyle::Number | WS::TabStop)
						 .Position({ 42,30 }).Size({ 72, 14 })
						 .ID(IDE_CY))
					.Add(DCtl(L"&OK", DClass::Button)
						 .Style(WS::Child | WS::Visible | WS::TabStop)
						 .Position({ 18, 48 }).Size({ 50, 14 })
						 .ID(IDOK))
					.Add(DCtl(L"&Cancel", DClass::Button)
						 .Style(WS::Child | WS::Visible | WS::TabStop)
						 .Position({ 72, 48 }).Size({ 50, 14 })
						 .ID(IDCANCEL)).Make();
				return &hDlg;
			}
#pragma endregion

#pragma region Event
		private:

			/// @brief 對話框初始化事件
			inline bool InitDialog() {
				Item(IDE_CX).Int(size.cx);
				Item(IDE_CY).Int(size.cy);
				return true;
			}

			/// @brief 窗體指令響應事件
			/// @param id 指令ID
			/// @param hwndCtl 控件句柄
			/// @param codeNotify 提示碼
			inline void OnCommand(int id, HWND hwndCtl, UINT codeNotify) {
				switch (id) {
					case IDOK: {
						LSize sz = { Item(IDE_CX).Int(), Item(IDE_CY).Int() };
						if (size != sz) size = sz;
						else id = IDCANCEL;
					}
					case IDCANCEL:
						End(id);
						break;
					default:
						break;
				}
			}

			/// @brief 窗體關閉事件
			inline void OnClose() reflect_to(End(IDCANCEL));

			/// @brief 異常捕捉事件
			/// @param err 例外信息包
			inline void OnCatch(const Exception &err) {
				MsgBox(_T("Dialog error"), _T("Invalid input"), MB::IconError);
				End(IDCANCEL);
			}
#pragma endregion

		} rsBox = panel.Size();
		if (rsBox.Box(self) != IDOK)
			return;
		if (!Resize(rsBox.size))
			MsgBox(_T("Dialog error"), _T("Resize failed"), MB::IconError);
	}
#pragma endregion

public: // 一般操作

	/// @brief 打印虛擬銀幕到文件
	/// @param lpFilename 文件名
	/// @return 是否保存成功
	inline bool PrintScreen(LPCTSTR lpFilename) reflect_as(panel.SaveToFile(lpFilename));
	
	/// @brief 打印虛擬銀幕到文件對話框
	/// @return 是否保存成功
	inline bool PrintScreen() {
		auto &&time = SysTime();
		String file =
			sprintf(_T("%04d%02d%02d%02d%02d%02d.bmp"),
					time.wYear, time.wMonth, time.wDay,
					time.wHour, time.wSecond, time.wSecond)
			.Resize(MAX_PATH * 2);
		if (!ChooserFile()
			.File(file)
			.Parent(self)
			.Styles(ChooserFile::Style::Explorer)
			.Title(_T("Printscreen to bitmap"))
			.Filter(_T("Bitmap file (*.bmp)\0*.bmp*\0\0"))
			.SaveFile())
			return false;
		if (PrintScreen(file)) return true;
		MsgBox(_T("Print screen"), _T("Save bitmap failed!"), MB::Ok);
		return false;
	}

	/// @brief 使能自修改尺寸
	/// @param bResizeable 窗體和繪圖面板是否能修改尺寸
	inline void Resizeable(bool bResizeable = true) {
		this->bResizeable = bResizeable;
		if (menu) {
			menu(IDM_RESIZE).Enable(bResizeable);
			menu(IDM_INVERT).Enable(bResizeable);
		}
		if (self)
			Styles(bResizeable ? Styles() + WS::SizeBox : Styles() - WS::SizeBox);
	}

	/// @brief 修改尺寸
	/// @param sz 尺寸
	/// @return 是否修改尺寸成功
	inline bool Resize(LSize sz) {
		if (!bResizeable) return false;
		if (sz.cx == 0 || sz.cy == 0) return false;
		try {
			auto rc = AdjustRect(sz + LSize(Border.right_bottom() + Border.left_top()) + LSize(0, sbar.Size().cy));
			Size(rc);
			return true;
		} catch (...) {}
		return false;
	}

	/// @brief 隱藏光標
	/// @param bHide 是否隱藏光標
	inline void HideCursor(bool bHide) {
		panel.bHideCursor = bHide;
		if (!menu) return;
		menu(IDM_HIDECURSOR).Check(bHide);
	}

private: // 控制臺操作
	Window wCon;
public:

	/// @brief 判斷控制臺是否可見
	/// @return 控制臺是否可見
	inline bool ConsoleVisible() {
		return wCon && bConsole ? (wCon.Styles() & WS::Visible) == WS::Visible : false;
	}

	/// @brief 使能控制臺
	/// @param bEnable 是否使能控制臺
	inline void ConsoleEnable(bool bEnable) {
		bConsole = bEnable;
		if (menu)
			menu(IDM_HIDECONSOLE)
				.Enable(bEnable);
		if (bEnable) {
			if (wCon)
				if (wCon.Visible()) return;
			Console.Alloc();
			wCon = Console.Window();
			assertl(wCon);
			wCon.Styles(WS::SizeBox)
				.Size({ panel.Size().cx, 200 })
				.Position({ 0, panel.Rect().bottom });
			Console.Select();
		}
		else {
			Console.Free();
		}
	}

	/// @brief 開啓控制臺
	/// @param bOpen 是否開啓控制臺
	inline void ConsoleShow(bool bOpen) {
		if ((!bConsole && bOpen) || !wCon) ConsoleEnable(true);
		if (bOpen)
			wCon.Styles(wCon.Styles() | WS::Visible);
		else
			wCon.Styles(wCon.Styles() & ~WS::Visible);
		wCon.Position(Rect().left_bottom())
			.Size({ Size().cx, wCon.Size().cy });
		Console.Reopen();
		if (menu)
			menu(IDM_HIDECONSOLE).Check(bOpen);
	}

	/// @brief 控制臺使能開啓
	///		先使能後開啓控制臺
	/// @param bEnableOpen 是否先使能後開啓
	inline void ConsoleEnableShow(bool bEnableOpen) {
		if (bConsole) {
			if (ConsoleVisible())
				ConsoleShow(bEnableOpen);
			elif (bEnableOpen)
				ConsoleShow(true);
			else
				ConsoleEnable(false);
		}
		else
			ConsoleEnable(bEnableOpen);
	}

#pragma region User Event Control
public: // 用戶活动事件控制

	/// @brief 屏蔽鼠標事件
	/// @param bMask 是否使能屏蔽
	inline void MaskMouse(bool bMask) {
		panel.bMaskMouse = bMask;
		if (!menu) return;
		menu(IDM_HIDECURSOR).Enable(!bMask);
		menu(IDM_MASK_MOUSE).Check(bMask);
		sbar.Text(4, _T("mouse event masked"));
	}

	/// @brief 屏蔽觸摸事件
	/// @param bMask 是否使能屏蔽
	inline void MaskTouch(bool bMask) {
		panel.bMaskTouch = bMask;
		if (!menu) return;
		menu(IDM_MASK_TOUCH).Check(bMask);
	}

	/// @brief 屏蔽鍵盤事件
	/// @param bMask 是否使能屏蔽
	inline void MaskKeyboard(bool bMask) {
		panel.bMaskKeyboard = bMask;
		if (!menu) return;
		menu(IDM_MASK_KEYBOARD).Check(bMask);
	}

	/// @brief 設置鼠標事件
	/// @param lpfnOnMouse 鼠標事件迴調函數
	inline void SetOnMouse(tSimDisp_OnMouse lpfnOnMouse) reflect_to(pfnOnMouse = lpfnOnMouse);

	/// @brief 設置尺寸修改事件
	/// @param lpfnOnResize 響應新尺寸迴調函數
	inline void SetOnResize(tSimDisp_OnResize lpfnOnResize) reflect_to(pfnOnResize = lpfnOnResize);

	/// @brief 設置鍵盤事件
	/// @param lpfnOnKey 鍵盤事件迴調函數
	inline void SetOnKey(tSimDisp_OnKey lpfnOnKey) reflect_to(pfnOnKey = lpfnOnKey)

	/// @brief 設置關閉事件
	/// @param lpfnOnClose 窗體關閉事件迴調函數
	inline void SetOnClose(tSimDisp_OnClose lpfnOnClose) reflect_to(pfnOnClose = lpfnOnClose);
#pragma endregion

public:

	/// @brief 色彩緩衝區
	inline void *Colors() reflect_as(panel.Colors());

	/// @brief 文件映射句柄
	inline HANDLE FileMapping() reflect_as(panel.FileMapping());

	/// @brief 虛擬銀幕尺寸
	inline LSize ScreenSize() const reflect_as(panel.Size());

	/// @brief 虛擬銀幕最大尺寸
	inline LSize ScreenSizeMax() const reflect_as(panel.MaxSize());

};

#pragma endregion

/// @brief SimDisp客戶端綫程類
class BaseOf_Thread(SimDispClient) {
	SFINAE_Thread(SimDispClient);
private:

	/// @brief 標題
	LPCTSTR lpszTitle = _T("SimDisp Display By Nurtas Ihram");
	/// @brief 虛擬銀幕大小
	uint16_t xSize = 400, ySize = 300;
	/// @brief SimDisp窗體類
	SimDispWnd *pWndSiDi = O;
	/// @brief 初始化完成事件
	Event evtInited = Event::Create();
	/// @brief 錯誤標注
	bool bError = false;

protected:

	/// @brief 綫程啓動事件
	inline void OnStart() {
		assertl(SUCCEEDED(CoInitialize(NULL)));
		evtInited.Reset();
		bError = false;
		pWndSiDi = new SimDispWnd;
		if (!pWndSiDi->Create().Size({ xSize, ySize }).Caption(lpszTitle))
			return;
		pWndSiDi->Update();
		evtInited.Set();
		Message msg;
	_retry:
		try {
			while (msg.Get()) {
				msg.Translate();
				msg.Dispatch();
			}
		}
		catch (Exception err) {
			(void)err;
			goto _retry;
		}
		CoUninitialize();
		evtInited.Reset();
	}

	/// @brief 異常捕捉事件
	/// @param err 例外信息包
	/// @return 是否重試
	inline bool OnCatch(const Exception &err) {
		switch (pWndSiDi->MsgBox(_T("Message process error"),
							  err.operator String(),
							  MB::IconError | MB::AbortRetryIgnore)) {
			case IDIGNORE:
			case IDRETRY:
				return false;
			case IDABORT:
				break;
		}
		bError = true;
		evtInited.Set();
		CoUninitialize();
		if (!pWndSiDi) return false;
		if (!*pWndSiDi) return false;
		return false;
	}

public:
	~SimDispClient() reflect_to(Close());

	/// @brief 關閉
	///		關閉客戶端窗體與綫程
	inline void Close() {
		if (pWndSiDi) {
			delete pWndSiDi;
			pWndSiDi = O;
		}
		TerminateWait(1000);
	}

	/// @brief 等待用戶關閉
	///		關閉客戶端窗體與綫程
	inline void UserClose() {
		if (!self) return;
		if (!pWndSiDi) {
			Terminate();
			return;
		}
		WaitForSignal();
		delete pWndSiDi;
		pWndSiDi = O;
	}

	/// @brief 創建客戶端
	/// @param lpszTitle 客戶端窗體標題
	/// @param xSize 虛擬銀幕寬
	/// @param ySize 虛擬銀幕高
	/// @return 是否創建成功
	inline bool Create(LPCTSTR lpszTitle, uint16_t xSize, uint16_t ySize) {
		this->lpszTitle = lpszTitle;
		this->xSize = xSize;
		this->ySize = ySize;
		if (!super::Create().Security(InheritHandle))
			return false;
		evtInited.WaitForSignal();
		return !bError;
	}

	/// @brief 安全執行窗體命令
	/// @tparam AnyMethod 執行方法類
	/// @param f 執行方法
	/// @return 是否執行成功
	template<class AnyMethod>
	inline bool Do(const AnyMethod & f) {
		if (!pWndSiDi) return false;
		try {
			f(*pWndSiDi);
		} catch (Exception err) {
			pWndSiDi->MsgBox(_T("Action error"), err.operator String(), MB::IconError);
			return false;
		}
		return true;
	}
} *lpSimDisp = O;

#pragma region Function Exportation
REG_FUNC(BOOL, Open, const wchar_t *lpTitle, uint16_t xSize, uint16_t ySize) reflect_as(lpSimDisp->Create(lpTitle, xSize, ySize));
REG_FUNC(BOOL, Show, BOOL bShow) reflect_as(lpSimDisp->Do([&](SimDispWnd &Win) { if (bShow) Win.Show().BringToTop(); else Win.Hide(); }));
REG_FUNC(void, Close, void) reflect_to(lpSimDisp->Close());
REG_FUNC(void, UserClose, void) reflect_to(lpSimDisp->UserClose());
REG_FUNC(BOOL, IsRunning, void) reflect_as(lpSimDisp ? lpSimDisp->StillActive() : false);

REG_FUNC(void, GetSize, uint16_t *xSize, uint16_t *ySize) {
	lpSimDisp->Do([=](SimDispWnd &Win) {
		auto &&s = Win.ScreenSize();
		if (xSize)
			*xSize = (uint16_t)s.cx;
		if (ySize)
			*ySize = (uint16_t)s.cy;
	});
}
REG_FUNC(void, GetSizeMax, uint16_t *xSize, uint16_t *ySize) {
	lpSimDisp->Do([=](SimDispWnd &Win) {
		auto &&s = Win.ScreenSizeMax();
		if (xSize)
			*xSize = (uint16_t)s.cx;
		if (ySize)
			*ySize = (uint16_t)s.cy;
	});
}

REG_FUNC(void *, GetColors, void) {
	void *lpBuffer = O;
	lpSimDisp->Do([&](SimDispWnd &Win) { lpBuffer = Win.Colors(); });
	return lpBuffer;
}

REG_FUNC(BOOL, HideCursor, BOOL bHide) reflect_as(lpSimDisp->Do([=](SimDispWnd& Win) { Win.HideCursor(bHide); }));
REG_FUNC(BOOL, Resizeable, BOOL bEnable) reflect_as(lpSimDisp->Do([=](SimDispWnd &Win) { Win.Resizeable(bEnable); }));

REG_FUNC(void, SetOnClose, tSimDisp_OnClose lpfnOnClose) reflect_to(lpSimDisp->Do([=](SimDispWnd &Win) { Win.SetOnClose(lpfnOnClose); }));
REG_FUNC(void, SetOnMouse, tSimDisp_OnMouse lpfnOnMouse) reflect_to(lpSimDisp->Do([=](SimDispWnd &Win) { Win.SetOnMouse(lpfnOnMouse); }));
REG_FUNC(void, SetOnKey, tSimDisp_OnKey lpfnOnKey) reflect_to(lpSimDisp->Do([=](SimDispWnd &Win) { Win.SetOnKey(lpfnOnKey); }));
REG_FUNC(void, SetOnResize, tSimDisp_OnResize lpfnOnResize) reflect_to(lpSimDisp->Do([=](SimDispWnd &Win) { Win.SetOnResize(lpfnOnResize); }));

REG_FUNC(HWND, GetHWND, void) {
	HWND hWnd = O;
	lpSimDisp->Do([&](SimDispWnd &Win) { hWnd = Win; });
	return hWnd;
}
REG_FUNC(HANDLE, GetFileMapping, void) {
	HANDLE hFile = O;
	lpSimDisp->Do([&](SimDispWnd &Win) { hFile = Win.FileMapping(); });
	return hFile;
}

REG_FUNC(DWORD, ConsoleEnableShow, BOOL bEnableOpen) {
	lpSimDisp->Do([&](SimDispWnd &Win) { Win.ConsoleEnableShow(bEnableOpen); });
	return GetCurrentProcessId();
}
#pragma endregion

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (lpSimDisp) break;
			lpSimDisp = new SimDispClient;
			hinst = hinstDLL;
			break;
		case DLL_PROCESS_DETACH:
			if (!lpvReserved) break;
			if (!lpSimDisp) break;
			delete lpSimDisp;
			lpSimDisp = O;
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}
