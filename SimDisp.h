#pragma once

#include "./wx/resource.h"

/// @brief SimDisp鼠鍵結構
struct tSimDisp_MouseKey {
	uint8_t Left : 1; // MK_LBUTTON
	uint8_t Middle : 1; // MK_MBUTTON
	uint8_t Right : 1; // MK_RBUTTON
	uint8_t Shift : 1; // MK_SHIFT
	uint8_t Control : 1; // MK_CONTROL
	uint8_t Leave : 1;
};

/// @brief 鼠標事件類
/// @param xPos 繪圖面板内光標x
/// @param yPos 繪圖面板内光標y
/// @param zPos 鼠標滾輪數
/// @param MouseKeys 鼠鍵結構
typedef void(*tSimDisp_OnMouse)(int16_t xPos, int16_t yPos, int16_t zPos, tSimDisp_MouseKey MouseKeys);

/// @brief 鍵盤事件類
/// @param vk 鍵盤按鍵值
/// @param bPressed 是否按下
typedef void(*tSimDisp_OnKey)(UINT vk, BOOL bPressed);

/// @brief 窗體關閉事件類
typedef void(*tSimDisp_OnClose)(void);

/// @brief 窗體重設尺寸響應事件類
/// @param nSizeX 繪圖面板新尺寸x
/// @param nSizeY 繪圖面板新尺寸y
/// @return 是否接受新尺寸
typedef BOOL(*tSimDisp_OnResize)(uint16_t nSizeX, uint16_t nSizeY);

#define DLL_INL_LIST "SimDisp.inl"
#define MOD_NAME SimDisp
#include "./wx/dll.inl"

#ifdef USE_AYXANDAR
#	undef USE_AYXANDAR
/// @brief Ayxandar内存繪圖類
class Ayxandar {
	uint16_t xMax = 0, yMax = 0;
	uint16_t xSize = 0, ySize = 0;
	uint32_t nPix = 0;
	uint32_t *lpBits = NULL;
public:

	/// @brief 初始化繪圖類
	inline void Init() {
		SimDisp::GetSizeMax(&xMax, &yMax);
		SimDisp::GetSize(&xSize, &ySize);
		nPix = xMax * yMax;
		lpBits = (uint32_t *)SimDisp::GetColors();
	}

	/// @brief 繪製圖元
	/// @param x 圖元x位置
	/// @param y 圖元y位置
	/// @param rgb 圖元RGB888色彩值
	inline void Dot(WX::LPoint p, uint32_t rgb) {
#if _DEBUG
		assertl(0 <= p.x && p.x <= xSize);
		assertl(0 <= p.y && p.y <= ySize);
#endif
		lpBits[p.x + p.y * xMax] = rgb;
	}

	/// @brief 獲取圖元
	/// @param x 圖元x位置
	/// @param y 圖元y位置
	/// @return 圖元RGB888色彩值
	inline uint32_t Dot(WX::LPoint p) const {
#if _DEBUG
		assertl(0 <= p.x && p.x <= xSize);
		assertl(0 <= p.y && p.y <= ySize);
#endif
		return lpBits[p.x + p.y * xMax];
	}

	/// @brief 完全填充銀幕
	/// @param rgb 填充RGB888色彩值
	inline void Fill(uint32_t rgb) {
		auto pBits = lpBits;
		for (uint32_t n = 0; n < nPix; ++n)
			*pBits++ = rgb;
	}

	/// @brief 區域填充銀幕
	/// @param rgb 填充RGB888色彩值
	/// @param r 填充區域
	inline void Fill(uint32_t rgb, WX::LRect r) {
#if _DEBUG
		assertl(0 <= r.left && r.right <= xSize);
		assertl(0 <= r.top && r.bottom <= ySize);
#endif
		auto pBits = lpBits + r.left + r.top * xMax;
		for (int y = r.top; y <= r.bottom; ++y) {
			auto lpLine = pBits;
			for (int x = r.left; x <= r.right; ++x)
				*lpLine++ = rgb;
			pBits += xMax;
		}
	}

	/// @brief 設置緩衝區
	/// @param r 設置區域
	/// @param lpBits 色彩值（逐行排列）
	/// @param BytesPerLine 每行字節數
	inline void SetRect(WX::LRect r, const uint32_t *lpBits, uint32_t BytesPerLine) {
#if _DEBUG
		assertl(0 <= r.left && r.right <= xSize);
		assertl(0 <= r.top && r.bottom <= ySize);
#endif
		auto pBits = this->lpBits + r.left + r.top * xMax;
		for (int y = r.top; y <= r.bottom; ++y) {
			auto lpSrc = lpBits;
			auto lpDst = pBits;
			for (int x = r.left; x <= r.right; ++x)
				*lpDst++ = *lpSrc++;
			(uint8_t *&)lpBits += BytesPerLine;
			pBits += xMax;
		}
	}

	/// @brief 獲取緩衝區
	/// @param r 獲取區域
	/// @param lpBits 色彩值（逐行排列）
	/// @param BytesPerLine 每行字節數
	inline void GetRect(WX::LRect r, uint32_t *lpBits, uint32_t BytesPerLine) {
#if _DEBUG
		assertl(0 <= r.left && r.right <= xSize);
		assertl(0 <= r.top && r.bottom <= ySize);
#endif
		auto pBits = this->lpBits + r.left + r.top * xMax;
		for (int y = r.top; y <= r.bottom; ++y) {
			auto lpSrc = pBits;
			auto lpDst = lpBits;
			for (int x = r.left; x <= r.right; ++x)
				*lpDst++ = *lpSrc++;
			(uint8_t *&)lpBits += BytesPerLine;
			pBits += xMax;
		}
	}
};
namespace SimDisp {
extern Ayxandar Ayx;
}
#endif
