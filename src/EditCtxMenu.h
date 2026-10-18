// Super3D - 3D Scene Designer for Kids
// EditCtxMenu.h : 只读文本框右键菜单增强（复制 / 剪切 / 清除）+ Ctrl+X 剪切
// 用于编辑器"输出"页、"编译器错误"页，以及播放器"脚本输出"窗口。
#pragma once
#include <afxwin.h>

// 右键：复制（有选中复制选中，无选中复制全文）／剪切（复制并移除所选或全文）／清除
// 快捷键：Ctrl+X 剪切（只读输出框原生 WM_CUT 不生效，这里自行实现）
class CEditCtxMenu : public CEdit {
public:
    afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint pt) {
        if (pt.x == -1 && pt.y == -1) {   // 键盘触发（Shift+F10）：菜单显示在控件中心
            CRect rc; GetClientRect(&rc);
            pt = CPoint(rc.CenterPoint().x, rc.CenterPoint().y);
            ClientToScreen(&pt);
        }
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 2, L"剪切\tCtrl+X");
        menu.AppendMenu(MF_STRING, 1, L"复制\tCtrl+C");
        menu.AppendMenu(MF_STRING, 3, L"清除");
        UINT cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                       pt.x, pt.y, this, NULL);
        if (cmd == 1) DoCopy(false);                       // 复制
        else if (cmd == 2) DoCopy(true);                   // 剪切：复制并移除
        else if (cmd == 3) SetWindowText(_T(""));          // 清除
    }
    // Ctrl+X：剪切（与右键"剪切"一致）。控件只读，系统 WM_CUT 不生效，需自行实现。
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
        if (nChar == 'X' &&
            (::GetKeyState(VK_CONTROL) & 0x8000) &&
           !(::GetKeyState(VK_MENU) & 0x8000)) {
            DoCopy(true);
            return;
        }
        CEdit::OnKeyDown(nChar, nRepCnt, nFlags);
    }
    void DoCopy(bool cut) {
        CString t; GetWindowText(t);
        if (t.IsEmpty()) return;
        DWORD sel = GetSel();                        // 低字=起点，高字=终点
        int s = LOWORD(sel), e = HIWORD(sel);
        CString part = (s != e) ? t.Mid(s, e - s) : t;   // 有选中取选中，无选中取全文
        PutClipboard(part);
        if (cut) {                                     // 剪切：复制后从控件移除
            if (s != e) {
                CString rest = t.Left(s) + t.Mid(e);   // 删掉选中区间
                SetWindowText(rest);
                SetSel(s, s);
            } else {
                SetWindowText(_T(""));                 // 无选中：复制全文后清空
            }
        }
    }
    void PutClipboard(const CString& txt) {
        if (txt.IsEmpty()) return;
        int len = txt.GetLength();
        HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
        if (!h) return;
        wchar_t* p = (wchar_t*)::GlobalLock(h);
        if (p) { memcpy(p, txt.GetString(), (len + 1) * sizeof(wchar_t)); ::GlobalUnlock(h); }
        if (!::OpenClipboard(GetSafeHwnd())) { ::GlobalFree(h); return; }
        ::EmptyClipboard();
        if (!::SetClipboardData(CF_UNICODETEXT, h)) ::GlobalFree(h);
        ::CloseClipboard();
    }
    DECLARE_MESSAGE_MAP()   // 消息映射实现在 EditCtxMenu.cpp（避免多 TU 重复定义）
};
