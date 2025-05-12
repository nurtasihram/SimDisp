
REG_FUNC(BOOL, Open, const wchar_t *lpTitle, uint16_t xSize, uint16_t ySize)
REG_FUNC(BOOL, Show, BOOL bShow)
REG_FUNC(void, Close, void)
REG_FUNC(void, UserClose, void)
REG_FUNC(BOOL, IsRunning, void)

REG_FUNC(void, GetSize, uint16_t *xSize, uint16_t *ySize)
REG_FUNC(void, GetSizeMax, uint16_t *xSize, uint16_t *ySize)
REG_FUNC(void *, GetColors, void)

REG_FUNC(BOOL, HideCursor, BOOL bHide)
REG_FUNC(BOOL, Resizeable, BOOL bEnable)

REG_FUNC(void, SetOnClose, tSimDisp_OnClose lpfnOnClose)
REG_FUNC(void, SetOnMouse, tSimDisp_OnMouse lpfnOnMouse)
REG_FUNC(void, SetOnKey, tSimDisp_OnKey lpfnOnKey)
REG_FUNC(void, SetOnResize, tSimDisp_OnResize lpfnOnResize)

REG_FUNC(HWND, GetHWND, void)
REG_FUNC(DWORD, ConsoleEnableShow, BOOL bEnableOpen)

#ifdef SIMDISP_HOST
REG_FUNC(HANDLE, GetFileMapping, void)
#endif
