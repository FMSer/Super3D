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
    DECLARE_MESSAGE_MAP()
};
