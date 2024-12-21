#pragma once

#include "./wx/resource.h"

typedef struct {
	uint8_t Left : 1; // MK_LBUTTON
	uint8_t Middle : 1; // MK_MBUTTON
	uint8_t Right : 1; // MK_RBUTTON
	uint8_t Shift : 1; // MK_SHIFT
	uint8_t Control : 1; // MK_CONTROL
	uint8_t Leave : 1;
} tSimDisp_MouseKey;

typedef void(*tSimDisp_OnClose)(void);
typedef void(*tSimDisp_OnMouse)(int16_t xPos, int16_t yPos, int16_t zPos, tSimDisp_MouseKey MouseKeys);
typedef BOOL(*tSimDisp_OnResize)(uint16_t nSizeX, uint16_t nSizeY);

#ifdef DLL_EXPORTS
#	undef DLL_EXPORTS
#	define DLL_EXPORTS "SimDisp.inl"
#endif
#ifdef DLL_IMPORTS
#	undef  DLL_IMPORTS
#	define DLL_IMPORTS_DEF
#endif
#define DLL_IMPORTS "SimDisp.inl"
#define MOD_NAME SimDisp
#include "./wx/dll.inl"

#ifdef USE_AYXANDAR
class Ayxandar {
	uint16_t xMax = 0, yMax = 0;
	uint16_t xSize = 0, ySize = 0;
	uint32_t nPix = 0;
	uint32_t *lpBits = NULL;
public:
	inline void Init() {
		SimDisp::GetSizeMax(&xMax, &yMax);
		SimDisp::GetSize(&xSize, &ySize);
		nPix = xMax * yMax;
		lpBits = (uint32_t *)SimDisp::GetBuffer();
	}
	inline void Dot(WX::LPoint p, uint32_t rgb) {
#if _DEBUG
		assert(0 <= p.x && p.x <= xSize);
		assert(0 <= p.y && p.y <= ySize);
#endif
		lpBits[p.x + p.y * xMax] = rgb;
	}
	inline uint32_t Dot(WX::LPoint p) const {
#if _DEBUG
		assert(0 <= p.x && p.x <= xSize);
		assert(0 <= p.y && p.y <= ySize);
#endif
		return lpBits[p.x + p.y * xMax];
	}
	inline void Fill(uint32_t rgb) {
		auto pBits = lpBits;
		for (uint32_t n = 0; n < nPix; ++n)
			*pBits++ = rgb;
	}
	inline void Fill(uint32_t rgb, WX::LRect r) {
#if _DEBUG
		assert(0 <= r.left && r.right <= xSize);
		assert(0 <= r.top && r.bottom <= ySize);
#endif
		auto pBits = lpBits + r.left + r.top * xMax;
		for (int y = r.top; y <= r.bottom; ++y) {
			auto lpLine = pBits;
			for (int x = r.left; x <= r.right; ++x)
				*lpLine++ = rgb;
			pBits += xMax;
		}
	}
	inline void SetRect(WX::LRect r, const uint32_t *lpBits, uint32_t BytesPerLine) {
#if _DEBUG
		assert(0 <= r.left && r.right <= xSize);
		assert(0 <= r.top && r.bottom <= ySize);
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
	inline void GetRect(WX::LRect r, uint32_t *lpBits, uint32_t BytesPerLine) {
#if _DEBUG
		assert(0 <= r.left && r.right <= xSize);
		assert(0 <= r.top && r.bottom <= ySize);
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
