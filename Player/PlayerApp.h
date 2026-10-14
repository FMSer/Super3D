// PlayerApp.h : 播放器应用程序类
#pragma once
#include "stdafx.h"
#include "PlayerWnd.h"

class CPlayerApp : public CWinApp {
public:
    CPlayerApp() {}
    virtual BOOL InitInstance();
    CPlayerWnd m_wnd;
};
