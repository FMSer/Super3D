// PlayerApp.cpp : 播放器入口
#include "stdafx.h"
#include "PlayerApp.h"

CPlayerApp theApp;

BOOL CPlayerApp::InitInstance() {
    CWinApp::InitInstance();
    if (!m_wnd.CreateMain()) return FALSE;
    m_pMainWnd = &m_wnd;
    m_wnd.ShowWindow(SW_SHOW);
    m_wnd.UpdateWindow();
    // 优先：自身是否已打包（双击打包出的作品.exe）
    std::wstring err;
    if (!m_wnd.LoadSelfContained(err)) {
        // 其次：命令行参数（把 .s3d 文件拖到播放器图标上会自动带路径）
        if (__argc > 1) m_wnd.OpenFile(__wargv[1]);
    }
    return TRUE;
}
