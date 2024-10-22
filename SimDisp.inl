
REG_FUNC(BOOL, Open, const wchar_t *lpTitle, uint16_t xSize, uint16_t ySize)
REG_FUNC(BOOL, Show, BOOL bShow)
REG_FUNC(void, Close, void)
REG_FUNC(void, UserClose, void)

REG_FUNC(void, GetSize, uint16_t *xSize, uint16_t *ySize)
REG_FUNC(void, GetSizeMax, uint16_t *xSize, uint16_t *ySize)
REG_FUNC(void*, GetBuffer, void)
REG_FUNC(BOOL, Flush, void)
REG_FUNC(BOOL, AutoFlush, BOOL bEnable)

REG_FUNC(BOOL, HideCursor, BOOL bHide)
REG_FUNC(BOOL, Resizeable, BOOL bEnable)

REG_FUNC(void, SetOnDestroy, tSimDisp_OnDestroy lpfnOnDestroy)
REG_FUNC(void, SetOnMouse, tSimDisp_OnMouse lpfnOnMouse)
REG_FUNC(void, SetOnResize, tSimDisp_OnResize lpfnOnResize)

#ifdef SIMDISP_HOST
REG_FUNC(HWND, GetHWND, void)
REG_FUNC(HANDLE, GetFileMapping, void)
#endif
