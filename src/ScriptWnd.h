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
// ScriptWnd.h : 独立浮动的“脚本”窗口（类似 Flash 的“动作”面板）
#pragma once
#include "stdafx.h"

class CScriptWnd : public CWnd {
public:
    CScriptWnd();
    virtual ~CScriptWnd();

    BOOL CreateScript(CWnd* owner);      // 创建独立浮动窗口
    void LayoutWnd();
    void SetText(const wchar_t* t);      // 设置脚本内容（不触发 change 回调）
    std::wstring GetText() const;
    void SetPerFrame(bool on);
    bool GetPerFrame() const;
    void SetOutput(const wchar_t* t);
    void ClearAll();                     // 清空脚本+勾选+输出
    CEdit* Edit() { return &m_edit; }

protected:
    CEdit m_edit, m_out;
    CButton m_chk;
    CStatic m_lbl;
    CWnd* m_owner;
    bool m_updating;
    CFont m_font;

    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnScriptChange();
    afx_msg void OnChkPerFrame();
    afx_msg void OnClose();
    virtual BOOL PreTranslateMessage(MSG* pMsg);   // 脚本编辑框 Ctrl+X/C/V/A
    DECLARE_MESSAGE_MAP()
};
