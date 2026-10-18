// Super3D - 3D Scene Designer for Kids
// Copyright (C) 2026 FMSer.CN
// Contact: 3781320752@qq.com
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
