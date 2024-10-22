#pragma once

enum class SIDI_MSG : UINT {
	SIDI_NULL = WM_USER + 10,
#define REG_FUNC(ret, name, ...) name,
#include "SimDisp.inl"
#undef REG_FUNC
	SIDI_EXIT
};
