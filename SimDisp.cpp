
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

#include "./wx/window.h"
#include "./wx/control.h"
#include "./wx/dialog.h"
#include "./wx/realtime.h"
#include "./wx/file.h"

#define DLL_EXPORTS
#include "SimDisp.h"
#include "winerror.h"

#pragma region SimDisp

using namespace WX;

auto
&&d02_ = "02d"_nx,
&&d4_ = " 4d"_nx;

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

class BaseOf_Window(SiDiWindow) {
	SFINAE_Window(SiDiWindow);
private:
	LRect Border = 20;
	class BaseOf_Window(GraphPanel) {
		SFINAE_Window(GraphPanel);
		SiDiWindow &parent;
		CComPtr<ID2D1Factory> D2D_Factory;
		CComPtr<IWICImagingFactory> WIC_Factory;
		CComPtr<ID2D1HwndRenderTarget> RenderTarget;
		CComPtr<IWICBitmap> wicBitmap;
		CComPtr<ID2D1Bitmap> D2D_bitmap;
		class MyBitmap : public IWICBitmapLock {
			LSize size = { GetSystemMetrics(SM_CXSCREEN) + 1, GetSystemMetrics(SM_CYSCREEN) + 1 };
			File bmpBuff = File()
				.CreateMapping(size.cx * size.cy * 4)
				.Name(Cats(_T("SIDI"), GetCurrentProcessId()))
				.Security(InheritHandle);
			File::MapPointer ptr = bmpBuff.MapView();
			uint32_t *lpBits = ptr;
		public:
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
			inline HANDLE FileMapping() const reflect_as(bmpBuff);
			inline LSize Size() const reflect_as(size);
		} bmpItf;
	public:
		bool bMaskKeyboard = false,
			 bMaskMouse = false,
			 bMaskTouch = false,
			 bHideCursor = false;
	public:
		GraphPanel(SiDiWindow &parent) : parent(parent) {}
#pragma region Precreate
	private:
		WxClass() {
			xClass() {
				Styles(CStyle::Redraw);
				Cursor(IDC_ARROW);
			}
		};
		inline auto Create() {
			return super::Create()
				.Parent(parent)
				.Styles(WStyle::Child | WStyle::Visible);
		}
#pragma endregion
#pragma region Events
	private:
		inline int OnCreate(RefAs<CreateStruct *>) {
			assert(SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2D_Factory)));
			assert(SUCCEEDED(CoCreateInstance(
				CLSID_WICImagingFactory,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IWICImagingFactory,
				(void **)&WIC_Factory)));
			auto sz = Size();
			assert(SUCCEEDED(D2D_Factory->CreateHwndRenderTarget(
				D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE),
				D2D1::HwndRenderTargetProperties(self, D2D1::SizeU(sz.cx, sz.cy)),
				&RenderTarget)));
			assert(SUCCEEDED(RenderTarget->CreateSharedBitmap(IID_IWICBitmapLock, &bmpItf, O, &D2D_bitmap)));
			UINT size;
			LPBYTE lpBits;
			assert(SUCCEEDED(bmpItf.GetDataPointer(&size, &lpBits)));
			memset(lpBits, ~0, size);
			return 1;
		}
		inline void OnPaint() {
			auto &&ps = BeginPaint();
			if (RenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED)
				return;
			OnRender();
		}
		inline void OnSize(UINT state, int cx, int cy) {
			if (RenderTarget)
				RenderTarget->Resize(D2D1::SizeU(cx, cy));
		}
#pragma region Mouse
		bool OnSetCursor(HWND hwndCursor, UINT codeHitTest, UINT msg) {
			if (!bHideCursor)
				return false;
			SetCursor(nullptr);
			return true;
		}
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
		inline void OnRender() {
			if (RenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED)
				return;
			RenderTarget->BeginDraw();
			auto &&size = D2D_bitmap->GetSize();
			RenderTarget->DrawBitmap(
				D2D_bitmap,
				D2D1::RectF(0.0f, 0.0f, size.width, size.height)
			);
			assert(SUCCEEDED(RenderTarget->EndDraw()));
		}
		inline bool SaveToFile(LPCTSTR lpFilename) {
			try {
				::SaveToFile(
					DC(),
					Palette::Default(),
					ClipBitmap(
						Bitmap::Create(MaxSize()).Colors(Buffer()),
						Size()),
					File::Create(lpFilename).CreateAlways().Accesses(FileAccess::GenericWrite));
				return true;
			} catch (WX::Exception err) {
				parent.MsgBox(_T("Save to file error"), err.operator String(), MB::IconError);
			}
			return false;
		}
		inline void *Buffer() {
			UINT size;
			LPBYTE lpBits;
			assert(SUCCEEDED(bmpItf.GetDataPointer(&size, &lpBits)));
			return lpBits;
		}
		inline HANDLE FileMapping() reflect_as(bmpItf.FileMapping());
		inline LSize MaxSize() const reflect_as(bmpItf.Size());
	} panel = self;
#pragma region Precreate
	enum eMenu {
		IDM_PRINTSCREEN,
		IDM_EXIT,
		IDM_RESIZE,
		IDM_INVERT,
		IDM_HIDECURSOR,
		IDM_LOCKCURSOR,
		IDM_MASK_MOUSE,
		IDM_MASK_TOUCH,
		IDM_MASK_KEYBOARD,
		IDM_ABOUT
	};
	WX::Menu menu = WX::Menu::Create();
	WxClass() {
		xClass() {
			Styles(CStyle::Redraw);
			Cursor(IDC_ARROW);
			Background(SysColor::Window);
			auto makeIcon = [](WX::Icon &ico, int size) {
				Bitmap bmpColor = Bitmap().Create(size);
				Bitmap bmpMask = Bitmap::Create(size).BitsPerPixel(1);
				TayKit(WX::DC::CreateCompatible()(bmpColor)
					   .Fill(Brush::Black()), size / 2, size / 2);
				WX::DC::CreateCompatible()(bmpMask)
					.Fill(Brush::White())
					(Brush::Black()).DrawEllipse({ 0, 0, size, size });
				ico = ::Icon::Create(bmpColor, bmpMask);
			};
			makeIcon(Icon(), 128);
			makeIcon(IconSm(), 48);
		}
	};
	inline auto Create() {
		return super::Create()
			.Styles(WS::MinimizeBox | WS::Caption | WS::SysMenu | WS::ClipChildren)
			.Size(500)
			.Position(100)
			.Menu(menu
				  .Popup(_T("&Screen"), Menu::CreatePopup()
						 .String(_T("&Print Screen"), IDM_PRINTSCREEN)
						 .Separator()
						 .String(_T("&Resize Screen"), IDM_RESIZE, bResizeable)
						 .String(_T("&Invert Screen"), IDM_INVERT, bResizeable)
						 .Separator()
						 .Popup(_T("&Cursor Control"), Menu::CreatePopup()
								.Check(_T("&Hide Cursor On Screen"), IDM_HIDECURSOR, false)
								.Check(_T("&Lock Mouse On Screen"), IDM_LOCKCURSOR))
						 .Separator()
						 .String(_T("&Exit"), IDM_EXIT))
				  .Popup(_T("&Event"), Menu::CreatePopup()
						 .Check(_T("Mask &Mouse Event"), IDM_MASK_MOUSE, panel.bMaskMouse)
						 .Check(_T("Mask &Touch Event"), IDM_MASK_TOUCH, panel.bMaskTouch)
						 .Check(_T("Mask &Keyboard Event"), IDM_MASK_KEYBOARD, panel.bMaskKeyboard))
				  .Popup(_T("&Help"), Menu::CreatePopup()
						 .String(_T("&About"), IDM_ABOUT)));
	}
#pragma endregion
#pragma region Events
	StatusBar sbar;
	inline int OnCreate(RefAs<CreateStruct *> lpCreate) {
		assert(sbar.Create(self));
		auto rc = AdjustRect(lpCreate->Rect() + LSize(Border.bottom_right() + Border.top_left()) + LSize(1, 1 + sbar.Size().cy));
		assert(panel.Create()
			   .Position(Border.top_left())
			   .Size(lpCreate->Size()));
		Move(rc);
		return 1;
	}
	void(*pfnOnClose)() = O;
	inline void OnClose() {
		if (pfnOnClose)
			pfnOnClose();
		PostQuitMessage(0);
	}
	inline void OnSize(UINT state, int cx, int cy) {
		auto sz = Size();
		sbar.SetParts({ sz.cx / 6, 2 * sz.cx / 6, 3 * sz.cx / 6, 4 * sz.cx / 6, -1 });
		sz = panel.Size();
		sbar.Text(0, Cats(sz.cx, _T("x"), sz.cy));
		sbar.FixSize();
	}
	bool bLastIconic = false, bResizeable = false;
	tSimDisp_OnResize pfnOnResize = O;
	inline bool OnWindowPosChanging(RefAs<WindowPos *> pWndPos) { 
		if ((pWndPos->Flags() & SWP::NoSize) == SWP::NoSize)
			return false;
		if (bLastIconic) {
			bLastIconic = false;
			return false;
		}
		if (Iconic()) {
			bLastIconic = true;
			return false;
		}
		auto &&border = Size() - ClientSize() + Border.top_left() + Border.bottom_right();
		if (sbar.Enabled()) border.cy += sbar.Size().cy;
		LSize sz = pWndPos->Size() - border + LSize(1);
		if (sz.cx <= 0) (*pWndPos)->cx = border.cx + 1, sz.cx = 1;
		if (sz.cy <= 0) (*pWndPos)->cy = border.cy + 1, sz.cy = 1;
		if (pfnOnResize && sz != panel.Size())
			if (!pfnOnResize((uint16_t)sz.cx, (uint16_t)sz.cy)) {
				pWndPos->Flags(pWndPos->Flags() | SWP::NoSize | SWP::NoMove);
				return false;
			}
		panel.Size(sz);
		return true;
	}
	inline void OnCommand(int id, HWND hwndCtl, UINT codeNotify) {
		switch (id) {
			case IDM_PRINTSCREEN:
				PrintScreen();
				break;
			case IDM_EXIT:
				Destroy();
				break;
			case IDM_RESIZE:
				OnResizeBox();
				break;
			case IDM_INVERT:
				Resize(~panel.Size());
				break;
			case IDM_HIDECURSOR: HideCursor(!panel.bHideCursor); break;
			case IDM_LOCKCURSOR:
				break;
			case IDM_MASK_MOUSE: MaskMouse(!panel.bMaskMouse); break;
			case IDM_MASK_TOUCH: MaskTouch(!panel.bMaskTouch); break;
			case IDM_MASK_KEYBOARD: MaskKeyboard(!panel.bMaskKeyboard); break;
			case IDM_ABOUT:
				MsgBox(
					_T("About (" __DATE__ ")"),
					_T("Simulator Display for embedded OS test\n")
					_T("\tBy Nurtas Ihram"),
					MB::Ok | MB::IconInformation);
				break;
		}
	}
	UINT_PTR uIDTimer_FlushPriod = 0;
	inline void OnTimer(UINT uID) {
		if (uID == uIDTimer_FlushPriod)
			panel.OnRender();
	}
	tSimDisp_OnMouse pfnOnMouse = O;
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
		if (x < 0 || y < 0) {
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
	inline void OnResizeBox() {
		class BaseOf_Dialog(ResizeBox) {
			SFINAE_Dialog(ResizeBox);
		public:
			LSize size;
			ResizeBox(LSize size) : size(size) {}
#pragma region Precreate
		private:
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
			inline bool InitDialog() {
				Item(IDE_CX).Int(size.cx);
				Item(IDE_CY).Int(size.cy);
				return true;
			}
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
			inline void OnClose() reflect_to(End(IDCANCEL));
			inline void OnError(const Exception &err) {
				MsgBox(_T("Dialog error"), _T("Invalid input"), MB::IconError);
				End(IDCANCEL);
			}
#pragma endregion
		} rsBox = panel.Size();
		if (rsBox.Box(self) == IDOK)
			if (!Resize(rsBox.size))
				MsgBox(_T("Dialog error"), _T("Resize failed"), MB::IconError);
	}
#pragma endregion
public:
	inline bool PrintScreen(LPCTSTR lpFilename) reflect_as(panel.SaveToFile(lpFilename));
	inline bool PrintScreen() {
		ChooserFile cf;
		cf
			.File(Cats(ReplaceAll(SysTime(), _T(":"), _T("-")), _T(".bmp")))
			.Parent(self)
			.Styles(ChooserFile::Style::Explorer)
			.Title(_T("Printscreen to bitmap"))
			.Filter(_T("Bitmap file (*.bmp)\0*.bmp*\0\0"));
		if (!cf.SaveFile()) return false;
		if (PrintScreen(cf.File())) return true;
		MsgBox(_T("Print screen"), _T("Save bitmap failed!"), MB::Ok);
		return false;
	}
	inline void Resizeable(bool bResizeable = true) {
		this->bResizeable = bResizeable;
		if (menu) {
			menu(IDM_RESIZE).Enable(bResizeable);
			menu(IDM_INVERT).Enable(bResizeable);
		}
		if (self)
			Styles(bResizeable ? Styles() + WS::SizeBox : Styles() - WS::SizeBox);
	}
	inline bool Resize(LSize sz) {
		if (!bResizeable) return false;
		if (sz.cx == 0 || sz.cy == 0) return false;
		try {
			auto rc = AdjustRect(sz + LSize(Border.bottom_right() + Border.top_left()) + LSize(0, sbar.Size().cy));
			Size(rc);
			return true;
		} catch (...) {}
		return false;
	}
	inline void HideCursor(bool bHide) {
		panel.bHideCursor = bHide;
		if (menu)
			menu(IDM_HIDECURSOR).Check(bHide);
	}
	inline void MaskMouse(bool bMask) {
		panel.bMaskMouse = bMask;
		if (menu)
			menu(IDM_MASK_MOUSE).Check(bMask);
	}
	inline void MaskTouch(bool bMask) {
		panel.bMaskTouch = bMask;
		if (menu)
			menu(IDM_MASK_TOUCH).Check(bMask);
	}
	inline void MaskKeyboard(bool bMask) {
		panel.bMaskKeyboard = bMask;
		if (menu)
			menu(IDM_MASK_KEYBOARD).Check(bMask);
	}
	inline void SetOnResize(tSimDisp_OnResize lpfnOnResize) reflect_to(pfnOnResize = lpfnOnResize);
	inline void SetOnMouse(tSimDisp_OnMouse lpfnOnMouse) reflect_to(pfnOnMouse = lpfnOnMouse);
	inline void SetOnClose(tSimDisp_OnClose lpfnOnClose) reflect_to(pfnOnClose = lpfnOnClose);
	inline void Flush() reflect_to(panel.Message().OnPaint().Send());
	inline void AutoFlush(bool bEnable) {
		if (bEnable) {
			if (!uIDTimer_FlushPriod)
				uIDTimer_FlushPriod = SetTimer(1, 12);
			return;
		}
		if (uIDTimer_FlushPriod)
			KillTimer(uIDTimer_FlushPriod),
			uIDTimer_FlushPriod = 0;
	}
	inline void *Buffer() reflect_as(panel.Buffer());
	inline HANDLE FileMapping() reflect_as(panel.FileMapping());
	inline LSize ScreenSize() const reflect_as(panel.Size());
	inline LSize ScreenSizeMax() const reflect_as(panel.MaxSize());
};

#pragma endregion

class BaseOf_Thread(SiDiClient) {
	SFINAE_Thread(SiDiClient);
	using super = ThreadBase<SiDiClient>;
	LPCTSTR lpTitle = _T("SimDisp Display By Nurtas Ihram");
	uint16_t xSize = 400, ySize = 300;
	SiDiWindow *pSidi = O;
	Event evtInited = Event::Create();
	bool bError = false;
protected:
	inline void Start() {
		assert(SUCCEEDED(CoInitialize(NULL)));
		evtInited.Reset();
		bError = false;
		pSidi = new SiDiWindow;
		if (!pSidi->Create().Size({ xSize, ySize }).Caption(lpTitle))
			return;
		pSidi->Update();
		evtInited.Set();
		Message msg;
	_msg_loop:
		try {
			while (msg.Get()) {
				msg.Translate();
				msg.Dispatch();
			}
		} catch (Exception err) {
			if (err.LastError() == 298) goto _msg_loop; // Ignore 
			if (err.LastError() == 1400) goto _msg_loop; // Ignore 
			switch (pSidi->MsgBox(_T("Message process error"), 
								  err.operator String(),
								  MB::IconError | MB::AbortRetryIgnore)) {
				case IDIGNORE:
				case IDRETRY:
					goto _msg_loop;
				case IDABORT:
					break;
			}
		}
		CoUninitialize();
	}
#include <winerror.h>
	inline void Catch(const Exception &err) {
		bError = true;
		evtInited.Set();
		CoUninitialize();
		if (!pSidi) return;
		if (!*pSidi) return;
	}
public:
	~SiDiClient() reflect_to(Close());
	inline void Close() {
		if (pSidi) {
			delete pSidi;
			pSidi = O;
		}
		TerminateWait(1000);
	}
	inline void UserClose() {
		if (!self) return;
		if (!pSidi) {
			Terminate();
			return;
		}
		WaitForSignal();
		delete pSidi;
		pSidi = O;
	}
	inline bool Create(LPCTSTR lpTitle, uint16_t xSize, uint16_t ySize) {
		this->lpTitle = lpTitle;
		this->xSize = xSize;
		this->ySize = ySize;
		if (!super::Create().Security(InheritHandle))
			return false;
		evtInited.WaitForSignal();
		return !bError;
	}
	template<class AnyFunction>
	inline bool Do(const AnyFunction & fn_) {
		if (!pSidi) return false;
		try {
			fn_(*pSidi);
		} catch (Exception err) {
			pSidi->MsgBox(_T("Action error"), err.operator String(), MB::IconError);
			return false;
		}
		return true;
	}
	inline bool AutoFlush(bool bEnable) reflect_as(Do([=](SiDiWindow &Win) { Win.AutoFlush(bEnable); }));
} *lpSimDisp = O;

#pragma region Function Exportation

REG_FUNC(BOOL, Open, const wchar_t *lpTitle, uint16_t xSize, uint16_t ySize) reflect_as(lpSimDisp->Create(lpTitle, xSize, ySize));
REG_FUNC(BOOL, Show, BOOL bShow) reflect_as(lpSimDisp->Do([](SiDiWindow &Win) { Win.Show(); }));
REG_FUNC(void, Close, void) reflect_to(lpSimDisp->Close());
REG_FUNC(void, UserClose, void) reflect_to(lpSimDisp->UserClose());

REG_FUNC(void, GetSize, uint16_t *xSize, uint16_t *ySize) {
	lpSimDisp->Do([=](SiDiWindow &Win) {
		auto &&s = Win.ScreenSize();
		if (xSize)
			*xSize = (uint16_t)s.cx;
		if (ySize)
			*ySize = (uint16_t)s.cy;
	});
}
REG_FUNC(void, GetSizeMax, uint16_t *xSize, uint16_t *ySize) {
	lpSimDisp->Do([=](SiDiWindow &Win) {
		auto &&s = Win.ScreenSizeMax();
		if (xSize)
			*xSize = (uint16_t)s.cx;
		if (ySize)
			*ySize = (uint16_t)s.cy;
	});
}

REG_FUNC(void *, GetBuffer, void) {
	void *lpBuffer = O;
	lpSimDisp->Do([&](SiDiWindow &Win) { lpBuffer = Win.Buffer(); });
	return lpBuffer;
}
REG_FUNC(BOOL, Flush, void) reflect_as(lpSimDisp->Do([&](SiDiWindow &Win) { Win.Flush(); }));
REG_FUNC(BOOL, AutoFlush, BOOL bEnable) reflect_as(lpSimDisp->AutoFlush(bEnable));

REG_FUNC(BOOL, HideCursor, BOOL bHide) reflect_as(lpSimDisp->Do([=](SiDiWindow& Win) { Win.HideCursor(bHide); }));
REG_FUNC(BOOL, Resizeable, BOOL bEnable) reflect_as(lpSimDisp->Do([=](SiDiWindow &Win) { Win.Resizeable(bEnable); }));

REG_FUNC(void, SetOnClose, tSimDisp_OnClose lpfnOnClose) reflect_to(lpSimDisp->Do([=](SiDiWindow &Win) { Win.SetOnClose(lpfnOnClose); }));
REG_FUNC(void, SetOnMouse, tSimDisp_OnMouse lpfnOnMouse) reflect_to(lpSimDisp->Do([=](SiDiWindow &Win) { Win.SetOnMouse(lpfnOnMouse); }));
REG_FUNC(void, SetOnResize, tSimDisp_OnResize lpfnOnResize) reflect_to(lpSimDisp->Do([=](SiDiWindow &Win) { Win.SetOnResize(lpfnOnResize); }));

REG_FUNC(HWND, GetHWND, void) {
	HWND hWnd = O;
	lpSimDisp->Do([&](SiDiWindow &Win) { hWnd = Win; });
	return hWnd;
}
REG_FUNC(HANDLE, GetFileMapping, void) {
	HANDLE hFile = O;
	lpSimDisp->Do([&](SiDiWindow &Win) { hFile = Win.FileMapping(); });
	return hFile;
}
REG_FUNC(HANDLE, GetMailSlot, void) reflect_as(*lpSimDisp);

#pragma endregion

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (lpSimDisp) break;
			lpSimDisp = new SiDiClient;
			break;
		case DLL_PROCESS_DETACH:
			if (!lpvReserved)
				break;
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
