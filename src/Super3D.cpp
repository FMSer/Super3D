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
// Super3D.cpp : 应用程序入口
#include "stdafx.h"
#include "Super3D.h"
#include "MainWnd.h"
#include "ProjectIO.h"

CSuper3DApp theApp;

CSuper3DApp::CSuper3DApp() {}

BOOL CSuper3DApp::InitInstance() {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES;
    ::InitCommonControlsEx(&icc);

    // 无人值守导出（不显示窗口）：
    //   Super3D.exe /exporthtml <输出.html> [输入.s3d]
    if (__argc >= 3 && _wcsicmp(__wargv[1], L"/exporthtml") == 0) {
        CMainWnd tmp;
        std::wstring err;
        if (__argc >= 4 && ProjectIO::LoadProject(tmp.Scene(), __wargv[3], err) == false) {
            ::MessageBoxW(NULL, err.c_str(), L"Super3D", MB_ICONWARNING);
        }
        bool ok = tmp.ExportToFile(__wargv[2]);
        if (!ok) ::MessageBoxW(NULL, L"导出失败：无法写入文件。", L"Super3D", MB_ICONWARNING);
        return FALSE;
    }

    // 布尔差自动验证：/testbool <输出.s3d> [模式]
    //   模式0=盒−管贯穿 1=盒−管半插 2=盒−管贴顶 3=盒−管横穿(X)
    //   模式4=圆柱−管贯穿 5=球−管贯穿 6=盒−圆柱贯穿 7=盒−盒贯穿
    //   模式8=圆锥−管贯穿 9=圆环−管贯穿 10=圆柱−管贴顶 11=圆柱−管半插
    if (__argc >= 3 && _wcsicmp(__wargv[1], L"/testbool") == 0) {
        int mode = (__argc >= 4) ? _wtoi(__wargv[3]) : 0;
        Scene3D sc;
        sc.fps = 24; sc.totalFrames = 60; sc.loopPlay = true;
        Object3D* A = NULL; Object3D* B = NULL;
        int aType = PT_BOX, bType = PT_TUBE;
        if (mode == 4 || mode == 10 || mode == 11) { aType = PT_CYLINDER; bType = PT_TUBE; }
        else if (mode == 5) { aType = PT_SPHERE; bType = PT_TUBE; }
        else if (mode == 6) { aType = PT_BOX; bType = PT_CYLINDER; }
        else if (mode == 7) { aType = PT_BOX; bType = PT_BOX; }
        else if (mode == 8) { aType = PT_CONE; bType = PT_TUBE; }
        else if (mode == 9) { aType = PT_TORUS; bType = PT_TUBE; }
        else if (mode == 12 || mode == 13) { aType = PT_CYLINDER; bType = PT_TUBE; }   // 并集/交集
        else if (mode == 14) { aType = PT_CYLINDER; bType = PT_TUBE; }                 // 原始叠加（不执行布尔）
        else if (mode == 17) { aType = PT_SPHERE; bType = PT_CYLINDER; }               // 球−圆柱（差集）
        else if (mode == 18) { aType = PT_BOX; bType = PT_CYLINDER; }                  // 盒−截锥(圆台)贯穿（差集）：应成喇叭孔
        A = sc.AddObject(aType, L"目标", NULL);
        if (aType == PT_BOX)      { A->params[0] = 4.0; A->params[1] = 4.0; A->params[2] = 4.0; }
        else if (aType == PT_CYLINDER) { A->params[0] = 2.0; A->params[1] = 4.0; }
        else if (aType == PT_SPHERE)   { A->params[0] = 2.0; }
        else if (aType == PT_CONE)     { A->params[0] = 2.0; A->params[1] = 4.0; }
        else if (aType == PT_TORUS)    { A->params[0] = 2.0; A->params[1] = 0.8; }
        A->y = 2.0;
        A->r = 0.92; A->g = 0.62; A->b = 0.28;
        B = sc.AddObject(bType, L"孔体", NULL);
        if (bType == PT_BOX)          { B->params[0] = 2.0; B->params[1] = 2.0; B->params[2] = 2.0; }
        else if (bType == PT_CYLINDER) { B->params[0] = 1.0; B->params[1] = 6.0; }
        else { B->params[0] = 2.0; B->params[1] = 1.2; B->params[2] = 6.0; }   // 方管
        B->r = 0.62; B->g = 0.45; B->b = 0.85;
        if (mode == 0)      { B->y = 3.0; }        // 贯穿（管底共面A底）
        else if (mode == 1) { B->params[2] = 2.0; B->y = 3.5; }   // 半插：管底在A内 y=0.5
        else if (mode == 2) { B->params[2] = 2.0; B->y = 3.0; }   // 贴顶：管底共面A顶 y=2
        else if (mode == 3) { B->y = 2.0; B->rz = 90.0; }         // 横穿：管沿X贯穿A
        else if (mode == 6) { B->params[1] = 6.0; B->y = 3.0; }   // 圆柱贯穿
        else if (mode == 7) { B->y = 2.0; }                       // 立方体贯穿
        else if (mode == 10) { B->params[2] = 2.0; B->y = 5.0; }  // 圆柱−管贴顶：管底共面圆柱顶 y=4
        else if (mode == 11) { B->params[2] = 2.0; B->y = 4.5; }  // 圆柱−管半插：管底在圆柱内 0.5
        else if (mode == 12 || mode == 13) { B->y = 3.0; }        // 圆柱−管贯穿（并集/交集）
        else if (mode == 14) { B->y = 3.0; }                      // 原始叠加
        else if (mode == 17) { B->y = 3.0; }                      // 圆柱贯穿球（差集）
        else if (mode == 18) {                                    // 圆台：底径1.6、顶径0.3、高8，居中贯穿盒
            B->params[0] = 1.6; B->params[1] = 8.0; B->params[2] = 0.3; B->segments = 48; B->y = 2.0;
        }
        else                { B->y = 3.0; }                       // 管贯穿（4/5/8/9）
        if (mode == 14) {
            // 原始叠加：保留 A、B 两个体（用于教学示意图）
        } else {
            A->RebuildBooleanMesh(A, B, (mode == 12) ? 1 : (mode == 13) ? 2 : 0);
            A->type = PT_BOOLEAN;
            sc.DeleteObject(B);
        }
        sc.scriptPerFrame = false;
        std::wstring err;
        if (!ProjectIO::SaveProject(sc, __wargv[2], err)) {
            ::MessageBoxW(NULL, err.c_str(), L"Super3D", MB_ICONWARNING);
        }
        return FALSE;
    }

    CMainWnd* pWnd = new CMainWnd();
    m_pMainWnd = pWnd;
    if (!pWnd->CreateMain()) {
        delete pWnd;
        return FALSE;
    }
    pWnd->ShowWindow(SW_SHOWMAXIMIZED);   // 主对话框启动即最大化（Flash/Animate 式全屏工作区）
    pWnd->UpdateWindow();
    return TRUE;
}
