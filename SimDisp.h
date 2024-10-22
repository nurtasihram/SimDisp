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

typedef void(*tSimDisp_OnDestroy)(void);
typedef void(*tSimDisp_OnMouse)(int16_t xPos, int16_t yPos, int16_t zPos, tSimDisp_MouseKey MouseKeys);
typedef BOOL(*tSimDisp_OnResize)(uint16_t nSizeX, uint16_t nSizeY);

#ifdef     DLL_EXPORTS
#	undef  DLL_EXPORTS
#	define DLL_EXPORTS "SimDisp.inl"
#endif
#ifdef     DLL_IMPORTS
#	if     (DLL_IMPORTS + 1) > 1
#		define DLL_IMPORTS_DEF
#	endif
#	undef  DLL_IMPORTS
#	define DLL_IMPORTS "SimDisp.inl"
#	define MOD_NAME SimDisp
#endif
#include "./wx/dll.inl"

#ifdef AYXANDAR_MIRROR
class Ayxandar {
	uint16_t xMax = 0, yMax = 0;
	uint32_t nPix = 0, iPix = 0;
	uint32_t *lpBits = NULL;
	uint32_t *hpBits = NULL;
public:
	inline void Init() {
		SimDisp::GetSizeMax(&xMax, &yMax);
		nPix = xMax * yMax;
		iPix = nPix + xMax;
		lpBits = (uint32_t *)SimDisp::GetBuffer();
		hpBits = lpBits + nPix - xMax;
	}
	inline void Dot(WX::LPoint p, uint32_t rgb) { hpBits[p.x - p.y * xMax] = rgb; }
	inline uint32_t Dot(WX::LPoint p) const { return hpBits[p.x - p.y * xMax]; }
	inline void Fill(uint32_t rgb) {
		auto pBits = lpBits;
		for (uint32_t n = 0; n < nPix; ++n)
			*pBits++ = rgb;
	}
	inline void Fill(uint32_t rgb, WX::LRect r) {
		WX::LSize s = r.size();
		auto pBits = hpBits + r.left - r.top * xMax;
		for (int y = 0; y < s.cy; ++y) {
			auto lpLine = pBits;
			for (int x = 0; x < s.cx; ++x)
				*lpLine++ = rgb;
			pBits -= xMax;
		}
	}
	inline void GetRect(WX::LRect r, uint32_t *lpBits) {
		WX::LSize s = r.size();
		auto pBits = hpBits + r.left - r.top * xMax;
		for (int y = 0; y < s.cy; ++y) {
			auto lpLine = pBits;
			for (int x = 0; x < s.cx; ++x)
				*lpBits++ = *lpLine++;
			pBits -= xMax;
		}
	}
};
namespace SimDisp {
extern Ayxandar Ayx;
}
#elif defined(AYXANDAR)
class Ayxandar {
	uint16_t xMax = 0, yMax = 0;
	uint32_t nPix = 0;
	uint32_t *lpBits = NULL;
public:
	inline void Init() {
		SimDisp::GetSizeMax(&xMax, &yMax);
		nPix = xMax * yMax;
		lpBits = (uint32_t *)SimDisp::GetBuffer();
	}
	inline void Dot(WX::LPoint p, uint32_t rgb) { lpBits[p.x + p.y * xMax] = rgb; }
	inline uint32_t Dot(WX::LPoint p) const { return lpBits[p.x + p.y * xMax]; }
	inline void Fill(uint32_t rgb) {
		auto pBits = lpBits;
		for (uint32_t n = 0; n < nPix; ++n)
			*pBits++ = rgb;
	}
	inline void Fill(uint32_t rgb, WX::LRect r) {
		WX::LSize s = r.size();
		auto pBits = lpBits + r.left + r.top * xMax;
		for (int y = 0; y < s.cy; ++y) {
			auto lpLine = pBits;
			for (int x = 0; x < s.cx; ++x)
				*lpLine++ = rgb;
			pBits += xMax;
		}
	}
	inline void GetRect(WX::LRect r, uint32_t *lpBits) {
		WX::LSize s = r.size();
		auto pBits = this->lpBits + r.left + r.top * xMax;
		for (int y = 0; y < s.cy; ++y) {
			auto lpLine = pBits;
			for (int x = 0; x < s.cx; ++x)
				*lpBits++ = *lpLine++;
			pBits += xMax;
		}
	}
};
namespace SimDisp {
extern Ayxandar Ayx;
}
#endif
