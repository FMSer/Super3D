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
// ScriptWnd.cpp : 独立浮动的“脚本”窗口
#include "stdafx.h"
#include "ScriptWnd.h"
#include "MainWnd.h"

#define ID_SW_EDIT   1201
#define ID_SW_OUT    1202
#define ID_SW_CHK    1203

BEGIN_MESSAGE_MAP(CScriptWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_EN_CHANGE(ID_SW_EDIT, &CScriptWnd::OnScriptChange)
    ON_BN_CLICKED(ID_SW_CHK, &CScriptWnd::OnChkPerFrame)
END_MESSAGE_MAP()

CScriptWnd::CScriptWnd() : m_owner(NULL), m_updating(false) {}
CScriptWnd::~CScriptWnd() {}

BOOL CScriptWnd::CreateScript(CWnd* owner) {
    m_owner = owner;
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1), NULL);
    CRect rc(0, 0, 420, 360);
    return CreateEx(WS_EX_APPWINDOW, cls, _T("脚本"), WS_POPUP | WS_CAPTION |
        WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, rc, owner, 0);
}

int CScriptWnd::OnCreate(LPCREATESTRUCT lpcs) {
    if (CWnd::OnCreate(lpcs) == -1) return -1;
    m_font.CreatePointFont(90, _T("微软雅黑"));
    m_lbl.Create(_T("脚本（在播放时按每帧运行）"), WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(0, 0, 10, 10), this, 0);
    m_lbl.SetFont(&m_font);
    m_chk.Create(_T("播放时每帧运行"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        CRect(0, 0, 10, 10), this, ID_SW_CHK);
    m_chk.SetFont(&m_font);
    m_edit.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
        ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_WANTRETURN,
        CRect(0, 0, 10, 10), this, ID_SW_EDIT);
    m_edit.SetFont(&m_font);
    m_out.Create(WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
        WS_VSCROLL | ES_READONLY,
        CRect(0, 0, 10, 10), this, ID_SW_OUT);
    m_out.SetFont(&m_font);
    return 0;
}

void CScriptWnd::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutWnd();
}

void CScriptWnd::LayoutWnd() {
    CRect rc; GetClientRect(&rc);
    if (rc.Width() < 40 || rc.Height() < 40) return;
    int M = 6;
    m_lbl.MoveWindow(M, M, 180, 18);
    m_chk.MoveWindow(M + 184, M, 150, 20);
    int outH = 90;
    int editTop = M + 26;
    m_edit.MoveWindow(M, editTop, rc.Width() - 2 * M, rc.Height() - editTop - outH - M);
    m_out.MoveWindow(M, rc.Height() - outH - M, rc.Width() - 2 * M, outH);
}

void CScriptWnd::SetText(const wchar_t* t) {
    m_updating = true;
    m_edit.SetWindowText(t);
    m_updating = false;
}
std::wstring CScriptWnd::GetText() const {
    CString s; m_edit.GetWindowText(s);
    return std::wstring(s.GetString());
}
void CScriptWnd::SetPerFrame(bool on) {
    m_chk.SetCheck(on ? BST_CHECKED : BST_UNCHECKED);
}
bool CScriptWnd::GetPerFrame() const {
    return m_chk.GetCheck() == BST_CHECKED;
}
void CScriptWnd::SetOutput(const wchar_t* t) { m_out.SetWindowText(t); }
void CScriptWnd::ClearAll() {
    m_updating = true;
    m_edit.SetWindowText(_T(""));
    m_out.SetWindowText(_T(""));
    m_chk.SetCheck(BST_UNCHECKED);
    m_updating = false;
}

void CScriptWnd::OnScriptChange() {
    if (m_updating || !m_owner) return;
    if (CMainWnd* main = (CMainWnd*)m_owner) main->ScriptTextChanged(GetText());
}
void CScriptWnd::OnChkPerFrame() {
    if (m_owner) ((CMainWnd*)m_owner)->PerFrameChanged(GetPerFrame());
}
void CScriptWnd::OnClose() { ShowWindow(SW_HIDE); }  // 关闭=隐藏，可再从菜单打开

// 脚本编辑框快捷键：Ctrl+X 剪切 / Ctrl+C 复制 / Ctrl+V 粘贴 / Ctrl+A 全选（独立顶层窗口需自处理）
BOOL CScriptWnd::PreTranslateMessage(MSG* pMsg) {
    if (pMsg->message == WM_KEYDOWN &&
        (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
        CWnd* f = GetFocus();
        if (f && f->GetSafeHwnd() == m_edit.GetSafeHwnd()) {
            switch (pMsg->wParam) {
                case 'X': m_edit.SendMessage(WM_CUT); return TRUE;
                case 'C': m_edit.SendMessage(WM_COPY); return TRUE;
                case 'V': m_edit.SendMessage(WM_PASTE); return TRUE;
                case 'A': m_edit.SetSel(0, -1); return TRUE;
            }
        }
    }
    return CWnd::PreTranslateMessage(pMsg);
}
