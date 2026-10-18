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
// PropertiesPanel.cpp : 属性面板实现
#include "stdafx.h"
#include "PropertiesPanel.h"
#include "MainWnd.h"

enum {
    IDC_PROP_NAME = 2600,
    IDC_PROP_PX, IDC_PROP_PY, IDC_PROP_PZ,
    IDC_PROP_RX, IDC_PROP_RY, IDC_PROP_RZ,
    IDC_PROP_SX, IDC_PROP_SY, IDC_PROP_SZ,
    IDC_PROP_R, IDC_PROP_G, IDC_PROP_B, IDC_PROP_A,
    IDC_PROP_P1, IDC_PROP_P2, IDC_PROP_P3, IDC_PROP_P4,
    IDC_PROP_SEG,
    IDC_PROP_VIS,
    IDC_PROP_WIRE,
    IDC_PROP_SWATCH,
    IDC_PROP_FLIPH,
    IDC_PROP_FLIPV,
    IDC_PROP_TEXT,
    IDC_PROP_FONT,
    IDC_PROP_EDITMESH,
    IDC_PROP_REBUILD,
    IDC_PROP_EMMODE,
    IDC_PROP_EMDEL,
    IDC_PROP_EMEXTRUDE,
    IDC_PROP_EMEXTRUDEDEPTH
};
#define WM_PROP_SELALL (WM_APP + 0x131)

// 枚举系统字体，填充字体下拉列表（同一字体按多种字符集被多次枚举，用 FindStringExact 去重）
static int CALLBACK EnumFontProc(const LOGFONTW* lplf, const TEXTMETRICW*, DWORD, LPARAM lParam) {
    if (lplf->lfFaceName[0] == 0) return 1;
    CComboBox* cb = (CComboBox*)lParam;
    if (cb->FindStringExact(-1, lplf->lfFaceName) != CB_ERR) return 1;   // 已存在，跳过
    cb->AddString(lplf->lfFaceName);
    return 1;
}

BEGIN_MESSAGE_MAP(CPropertiesPanel, CWnd)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_PROP_NAME, IDC_PROP_SEG, &CPropertiesPanel::OnEditChange)   // 含名称框：输入即提交（名称实时改名，避免失焦时被刷新覆盖）
    ON_CONTROL_RANGE(EN_SETFOCUS, IDC_PROP_NAME, IDC_PROP_SEG, &CPropertiesPanel::OnEditFocus)
    ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_PROP_NAME, IDC_PROP_SEG, &CPropertiesPanel::OnEditKillFocus)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_PROP_TEXT, IDC_PROP_TEXT, &CPropertiesPanel::OnEditChange)   // 3D文本内容（在 SEG 范围之外，需单独挂接）
    ON_CONTROL_RANGE(EN_SETFOCUS, IDC_PROP_TEXT, IDC_PROP_TEXT, &CPropertiesPanel::OnEditFocus)
    ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_PROP_TEXT, IDC_PROP_TEXT, &CPropertiesPanel::OnEditKillFocus)
    ON_CBN_SELCHANGE(IDC_PROP_FONT, &CPropertiesPanel::OnFontSel)
    ON_CBN_EDITCHANGE(IDC_PROP_FONT, &CPropertiesPanel::OnFontSel)
    ON_CBN_SELCHANGE(IDC_PROP_EMMODE, &CPropertiesPanel::OnEmModeSel)
    ON_MESSAGE(WM_PROP_SELALL, &CPropertiesPanel::OnSelectAll)
    ON_BN_CLICKED(IDC_PROP_VIS, &CPropertiesPanel::OnVisClick)
    ON_BN_CLICKED(IDC_PROP_WIRE, &CPropertiesPanel::OnWireClick)
    ON_BN_CLICKED(IDC_PROP_SWATCH, &CPropertiesPanel::OnPickColor)
    ON_BN_CLICKED(IDC_PROP_FLIPH, &CPropertiesPanel::OnFlipH)
    ON_BN_CLICKED(IDC_PROP_FLIPV, &CPropertiesPanel::OnFlipV)
    ON_BN_CLICKED(IDC_PROP_EDITMESH, &CPropertiesPanel::OnEditMeshClick)
    ON_BN_CLICKED(IDC_PROP_REBUILD, &CPropertiesPanel::OnRebuildClick)
    ON_BN_CLICKED(IDC_PROP_EMDEL, &CPropertiesPanel::OnEmDelClick)
    ON_BN_CLICKED(IDC_PROP_EMEXTRUDE, &CPropertiesPanel::OnEmExtrudeClick)
    ON_WM_DRAWITEM()
    ON_WM_CTLCOLOR()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CPropertiesPanel::CPropertiesPanel() : m_owner(NULL), m_obj(NULL), m_bUpdating(false), m_skipID(0), m_lastEdit(0) {}

BOOL CPropertiesPanel::CreatePanel(CWnd* parent, const CRect& rc) {
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, NULL,
        (HBRUSH)(COLOR_WINDOW + 1), NULL);
    BOOL ok = CreateEx(WS_EX_CLIENTEDGE, cls, _T("属性"), WS_CHILD | WS_VISIBLE, rc, parent, 0);
    if (ok) SetupControls();
    return ok;
}

void CPropertiesPanel::SetupControls() {
    CFont* fnt = &m_owner->GetSmallFont();
    CRect rc;
    GetClientRect(&rc);
    int L = 6;
    int W = rc.Width() - 12;
    int eW = 44, eGap = 14;   // 加大间距，轴标签(Y/Z)不再被相邻输入框遮挡
    int x1 = L + 40;
    int x2 = x1 + eW + eGap;
    int x3 = x2 + eW + eGap;

    CreateLabel(m_lblName, _T("名称"), CRect(L, 6, L + 34, 26), this);
    CreateEdit(m_edtName, CRect(x1, 4, x1 + 150, 26), this, IDC_PROP_NAME);

    // 类型行：紧跟“名称”行之下
    CreateLabel(m_lblType, _T("类型"), CRect(L, 30, L + 34, 50), this);
    m_typeText.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(x1, 28, x1 + 150, 50), this, 0);

    CreateLabel(m_lblPos, _T("位置"), CRect(L, 54, L + 34, 74), this);
    CreateLabel(m_lblPosX, _T("X"), CRect(x1, 54, x1 + 12, 74), this);
    CreateEdit(m_edtPos[0], CRect(x1 + 12, 52, x1 + 12 + eW, 74), this, IDC_PROP_PX);
    CreateLabel(m_lblPosY, _T("Y"), CRect(x2, 54, x2 + 12, 74), this);
    CreateEdit(m_edtPos[1], CRect(x2 + 12, 52, x2 + 12 + eW, 74), this, IDC_PROP_PY);
    CreateLabel(m_lblPosZ, _T("Z"), CRect(x3, 54, x3 + 12, 74), this);
    CreateEdit(m_edtPos[2], CRect(x3 + 12, 52, x3 + 12 + eW, 74), this, IDC_PROP_PZ);

    CreateLabel(m_lblRot, _T("旋转"), CRect(L, 78, L + 34, 98), this);
    CreateLabel(m_lblRotX, _T("X"), CRect(x1, 78, x1 + 12, 98), this);
    CreateEdit(m_edtRot[0], CRect(x1 + 12, 76, x1 + 12 + eW, 98), this, IDC_PROP_RX);
    CreateLabel(m_lblRotY, _T("Y"), CRect(x2, 78, x2 + 12, 98), this);
    CreateEdit(m_edtRot[1], CRect(x2 + 12, 76, x2 + 12 + eW, 98), this, IDC_PROP_RY);
    CreateLabel(m_lblRotZ, _T("Z"), CRect(x3, 78, x3 + 12, 98), this);
    CreateEdit(m_edtRot[2], CRect(x3 + 12, 76, x3 + 12 + eW, 98), this, IDC_PROP_RZ);

    CreateLabel(m_lblScale, _T("缩放"), CRect(L, 102, L + 34, 122), this);
    CreateLabel(m_lblScaleX, _T("X"), CRect(x1, 102, x1 + 12, 122), this);
    CreateEdit(m_edtScale[0], CRect(x1 + 12, 100, x1 + 12 + eW, 122), this, IDC_PROP_SX);
    CreateLabel(m_lblScaleY, _T("Y"), CRect(x2, 102, x2 + 12, 122), this);
    CreateEdit(m_edtScale[1], CRect(x2 + 12, 100, x2 + 12 + eW, 122), this, IDC_PROP_SY);
    CreateLabel(m_lblScaleZ, _T("Z"), CRect(x3, 102, x3 + 12, 122), this);
    CreateEdit(m_edtScale[2], CRect(x3 + 12, 100, x3 + 12 + eW, 122), this, IDC_PROP_SZ);

    CreateLabel(m_lblColor, _T("颜色"), CRect(L, 126, L + 34, 146), this);
    CreateLabel(m_lblColorR, _T("R"), CRect(x1, 126, x1 + 12, 146), this);
    CreateEdit(m_edtColor[0], CRect(x1 + 12, 124, x1 + 12 + eW, 146), this, IDC_PROP_R);
    CreateLabel(m_lblColorG, _T("G"), CRect(x2, 126, x2 + 12, 146), this);
    CreateEdit(m_edtColor[1], CRect(x2 + 12, 124, x2 + 12 + eW, 146), this, IDC_PROP_G);
    CreateLabel(m_lblColorB, _T("B"), CRect(x3, 126, x3 + 12, 146), this);
    CreateEdit(m_edtColor[2], CRect(x3 + 12, 124, x3 + 12 + eW, 146), this, IDC_PROP_B);
    m_swatchColor = RGB(128, 128, 128);
    m_swatch.Create(_T(""), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        CRect(x3 + eW + 20, 126, x3 + eW + 36, 144), this, IDC_PROP_SWATCH);   // 色块缩小，避免右侧被边框遮挡
    m_swatch.SetFont(fnt);

    // 材质透明度：0-100%（100=不透明），紧跟"颜色"行之下；与上方各行动态完全一致（框 148-170/标签 150-170/行距 24）
    CreateLabel(m_lblAlpha, _T("透明"), CRect(L, 150, L + 34, 170), this);
    CreateLabel(m_lblAlphaVal, _T("A"), CRect(x1, 150, x1 + 12, 170), this);
    CreateEdit(m_edtAlpha, CRect(x1 + 12, 148, x1 + 12 + eW, 170), this, IDC_PROP_A);

    CreateLabel(m_lblSize, _T("尺寸"), CRect(L, 174, L + 34, 194), this);
    // 参数标签 + 输入框：与上方位置/旋转/颜色行完全同构（同列、同宽、同高、同字体）
    CreateLabel(m_lblParam[0], _T(""), CRect(x1, 174, x1 + 12, 194), this);
    CreateLabel(m_lblParam[1], _T(""), CRect(x2, 174, x2 + 12, 194), this);
    CreateLabel(m_lblParam[2], _T(""), CRect(x3, 174, x3 + 12, 194), this);
    CreateEdit(m_edtParam[0], CRect(x1 + 12, 172, x1 + 12 + eW, 194), this, IDC_PROP_P1 + 0);
    CreateEdit(m_edtParam[1], CRect(x2 + 12, 172, x2 + 12 + eW, 194), this, IDC_PROP_P1 + 1);
    CreateEdit(m_edtParam[2], CRect(x3 + 12, 172, x3 + 12 + eW, 194), this, IDC_PROP_P1 + 2);

    // 翻转：水平翻转（绕 Y 轴左右镜像） / 垂直翻转（绕 X 轴上下镜像）
    CreateLabel(m_lblFlip, _T("翻转"), CRect(L, 198, L + 34, 220), this);
    m_btnFlipH.Create(_T("水平翻转"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(x1, 196, x1 + 72, 220), this, IDC_PROP_FLIPH);
    m_btnFlipV.Create(_T("垂直翻转"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(x1 + 78, 196, x1 + 150, 220), this, IDC_PROP_FLIPV);
    m_btnFlipH.SetFont(fnt);
    m_btnFlipV.SetFont(fnt);

    // 3D文本：文字内容 / 字体（仅 PT_TEXT3D 显示）
    CreateLabel(m_lblText, _T("文字"), CRect(L, 222, L + 34, 242), this);
    CreateEdit(m_edtText, CRect(x1, 220, x1 + 150, 242), this, IDC_PROP_TEXT);
    CreateLabel(m_lblFont, _T("字体"), CRect(L, 246, L + 34, 266), this);
    // 字体下拉列表（CBS_DROPDOWN：可下拉选择，也允许输入自定义字体名）
    // 注意：CBS_DROPDOWN 的创建矩形高度必须给足，否则下拉列表无可展开空间；
    // 创建后 SetWindowPos 收缩为 22px 显示高度，再用 SetItemHeight 固定编辑框/列表项高度。
    m_cmbFont.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                     CBS_DROPDOWN | CBS_HASSTRINGS | CBS_SORT,
                     CRect(x1, 244, x1 + 150, 244 + 300), this, IDC_PROP_FONT);
    m_cmbFont.SetFont(fnt);
    m_cmbFont.SetItemHeight(-1, 21);           // 编辑框高度
    m_cmbFont.SetItemHeight(0, 18);            // 下拉列表项高度
    m_cmbFont.SetDroppedWidth(170);            // 下拉展开宽度
    m_cmbFont.SetWindowPos(NULL, x1, 244, 150, 22, SWP_NOZORDER | SWP_NOACTIVATE);
    HDC fdc = ::GetDC(NULL);                   // 屏幕 DC：枚举系统全部字体
    LOGFONTW flf;
    ::memset(&flf, 0, sizeof(flf));
    flf.lfCharSet = DEFAULT_CHARSET;
    ::EnumFontFamiliesExW(fdc, &flf, EnumFontProc, (LPARAM)&m_cmbFont, 0);
    ::ReleaseDC(NULL, fdc);
    m_edtText.ShowWindow(SW_HIDE);
    m_cmbFont.ShowWindow(SW_HIDE);
    m_lblText.ShowWindow(SW_HIDE);
    m_lblFont.ShowWindow(SW_HIDE);

    // 网格编辑（Blender 式点/线/面）：两行布局
    // 行1：网格 [编辑网格] [重建网格]   行2：模式 [点/线/面下拉] [删除选中]
    CreateLabel(m_lblMesh, _T("网格"), CRect(L, 222, L + 34, 244), this);
    m_btnEditMesh.Create(_T("编辑网格"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(x1, 220, x1 + 72, 244), this, IDC_PROP_EDITMESH);
    m_btnEditMesh.SetFont(fnt);
    m_btnRebuild.Create(_T("重建网格"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(x1 + 78, 220, x1 + 150, 244), this, IDC_PROP_REBUILD);
    m_btnRebuild.SetFont(fnt);
    CreateLabel(m_lblEmMode, _T("模式"), CRect(L, 246, L + 34, 268), this);
    m_cmbEmMode.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                       CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                       CRect(x1, 244, x1 + 200, 244 + 200), this, IDC_PROP_EMMODE);
    m_cmbEmMode.SetFont(fnt);
    m_cmbEmMode.AddString(_T("点"));
    m_cmbEmMode.AddString(_T("线"));
    m_cmbEmMode.AddString(_T("面"));
    m_cmbEmMode.SetCurSel(0);
    m_cmbEmMode.SetItemHeight(-1, 21);
    m_cmbEmMode.SetItemHeight(0, 18);
    m_cmbEmMode.SetWindowPos(NULL, x1, 244, 46, 22, SWP_NOZORDER | SWP_NOACTIVATE);
    m_btnEmDel.Create(_T("删除选中"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(x1 + 50, 244, x1 + 104, 268), this, IDC_PROP_EMDEL);
    m_btnEmDel.SetFont(fnt);
    // 挤出深度数值输入框（仅面模式）：输入值后点“挤出”按该值精确挤出；可负（向内）
    CreateEdit(m_edtEmExtrudeDepth, CRect(x1 + 108, 246, x1 + 152, 266), this, IDC_PROP_EMEXTRUDEDEPTH);
    m_edtEmExtrudeDepth.SetWindowText(_T("1"));
    m_edtEmExtrudeDepth.SetFont(fnt);
    m_btnEmExtrude.Create(_T("挤出"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(x1 + 156, 244, x1 + 200, 268), this, IDC_PROP_EMEXTRUDE);
    m_btnEmExtrude.SetFont(fnt);
    m_lblMesh.ShowWindow(SW_HIDE);
    m_btnEditMesh.ShowWindow(SW_HIDE);
    m_btnRebuild.ShowWindow(SW_HIDE);
    m_lblEmMode.ShowWindow(SW_HIDE);
    m_cmbEmMode.ShowWindow(SW_HIDE);
    m_btnEmDel.ShowWindow(SW_HIDE);
    m_edtEmExtrudeDepth.ShowWindow(SW_HIDE);
    m_btnEmExtrude.ShowWindow(SW_HIDE);

    // 最后两行：细分行（"细分"标签 + 单字提示"段" + 编辑框 + 勾选框，编辑框长宽与 X/Y/Z 一致），可见行（纯文本标签 + 勾选框）
    CreateLabel(m_lblWire, _T("细分"), CRect(L, 270, L + 28, 294), this);
    CreateLabel(m_lblSegHint, _T("段"), CRect(x1, 270, x1 + 12, 294), this);   // 与上方 X 轴标签同列对齐
    CreateEdit(m_edtSeg, CRect(x1 + 12, 270, x1 + 12 + eW, 292), this, IDC_PROP_SEG);   // 与 X 编辑框同列同宽
    m_chkWire.Create(_T(""), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        CRect(x1 + 60, 270, x1 + 80, 294), this, IDC_PROP_WIRE);
    CreateLabel(m_lblVisible, _T("可见"), CRect(L, 298, L + 28, 322), this);
    m_chkVisible.Create(_T(""), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        CRect(x1, 298, x1 + 20, 322), this, IDC_PROP_VIS);   // 勾选框左缘与"段"同列

    // 字体
    for (int i = 0; i < 3; i++) {
        m_edtPos[i].SetFont(fnt);
        m_edtRot[i].SetFont(fnt);
        m_edtScale[i].SetFont(fnt);
        m_edtColor[i].SetFont(fnt);
    }
    for (int i = 0; i < 3; i++) m_edtParam[i].SetFont(fnt);
    m_edtName.SetFont(fnt);
    m_edtSeg.SetFont(fnt);
    m_edtText.SetFont(fnt);
    m_edtAlpha.SetFont(fnt);
    m_chkWire.SetFont(fnt);
    m_chkVisible.SetFont(fnt);
    m_typeText.SetFont(fnt);
}

void CPropertiesPanel::CreateLabel(CStatic& st, const CString& text, const CRect& rc, CWnd* parent) {
    // SS_CENTERIMAGE：单行静态文本垂直居中于矩形，保证标签与同行编辑框文字垂直居中对齐
    st.Create(text, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, rc, parent, 0);
    st.SetFont(&m_owner->GetSmallFont());
}

void CPropertiesPanel::CreateEdit(CEdit& ed, const CRect& rc, CWnd* parent, UINT id) {
    ed.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_CENTER, rc, parent, id);
}

// 面板宽度约 250
void CPropertiesPanel::Bind(Object3D* o) {
    m_obj = o;
    m_skipID = 0;   // 切换对象时强制全部重新填充
    if (!o) {
        m_bUpdating = true;
        m_edtName.SetWindowText(_T(""));
        for (int i = 0; i < 3; i++) {
            m_edtPos[i].SetWindowText(_T(""));
            m_edtRot[i].SetWindowText(_T(""));
            m_edtScale[i].SetWindowText(_T(""));
            m_edtColor[i].SetWindowText(_T(""));
        }
        m_edtAlpha.SetWindowText(_T(""));
        for (int i = 0; i < 3; i++) m_edtParam[i].SetWindowText(_T(""));
        m_edtSeg.SetWindowText(_T(""));
        m_edtText.SetWindowText(_T(""));
        m_cmbFont.SetCurSel(-1);
        // 网格编辑控件随对象清空一起隐藏
        m_lblMesh.ShowWindow(SW_HIDE);
        m_btnEditMesh.ShowWindow(SW_HIDE);
        m_btnRebuild.ShowWindow(SW_HIDE);
        m_lblEmMode.ShowWindow(SW_HIDE);
        m_cmbEmMode.ShowWindow(SW_HIDE);
        m_btnEmDel.ShowWindow(SW_HIDE);
        m_edtEmExtrudeDepth.ShowWindow(SW_HIDE);
        m_btnEmExtrude.ShowWindow(SW_HIDE);
        m_chkVisible.SetCheck(0);
        m_typeText.SetWindowText(_T(""));
        m_btnFlipH.EnableWindow(FALSE);
        m_btnFlipV.EnableWindow(FALSE);
        m_bUpdating = false;
        return;
    }
    m_btnFlipH.EnableWindow(TRUE);
    m_btnFlipV.EnableWindow(TRUE);
    FillControls();
}

void CPropertiesPanel::FillControls() {
    if (!m_obj) return;
    m_bUpdating = true;
    CString s;
    // 正在编辑的框（m_skipID）跳过，避免每次击键被格式化值覆盖、打断输入
    if (m_skipID != IDC_PROP_NAME && GetFocus() != &m_edtName) m_edtName.SetWindowText(m_obj->name.c_str());
    if (m_skipID != IDC_PROP_PX) { s.Format(_T("%.2f"), m_obj->x); m_edtPos[0].SetWindowText(s); }
    if (m_skipID != IDC_PROP_PY) { s.Format(_T("%.2f"), m_obj->y); m_edtPos[1].SetWindowText(s); }
    if (m_skipID != IDC_PROP_PZ) { s.Format(_T("%.2f"), m_obj->z); m_edtPos[2].SetWindowText(s); }
    if (m_skipID != IDC_PROP_RX) { s.Format(_T("%.1f"), m_obj->rx); m_edtRot[0].SetWindowText(s); }
    if (m_skipID != IDC_PROP_RY) { s.Format(_T("%.1f"), m_obj->ry); m_edtRot[1].SetWindowText(s); }
    if (m_skipID != IDC_PROP_RZ) { s.Format(_T("%.1f"), m_obj->rz); m_edtRot[2].SetWindowText(s); }
    if (m_skipID != IDC_PROP_SX) { s.Format(_T("%.2f"), m_obj->sx); m_edtScale[0].SetWindowText(s); }
    if (m_skipID != IDC_PROP_SY) { s.Format(_T("%.2f"), m_obj->sy); m_edtScale[1].SetWindowText(s); }
    if (m_skipID != IDC_PROP_SZ) { s.Format(_T("%.2f"), m_obj->sz); m_edtScale[2].SetWindowText(s); }
    // 颜色显示：编辑态选中面/线有自定义色时显示该色（设色也只作用于选中元素），否则物体色
    double cr = m_obj->r, cg = m_obj->g, cb = m_obj->b;
    unsigned int selc;
    if (HasEditColorTarget() && GetSelCustomColor(selc))
        Object3D::UnpackEditColor(selc, cr, cg, cb);
    if (m_skipID != IDC_PROP_R) { s.Format(_T("%d"), (int)(cr * 255.0)); m_edtColor[0].SetWindowText(s); }
    if (m_skipID != IDC_PROP_G) { s.Format(_T("%d"), (int)(cg * 255.0)); m_edtColor[1].SetWindowText(s); }
    if (m_skipID != IDC_PROP_B) { s.Format(_T("%d"), (int)(cb * 255.0)); m_edtColor[2].SetWindowText(s); }
    if (m_skipID != IDC_PROP_A) { s.Format(_T("%d"), (int)(m_obj->alpha * 100.0)); m_edtAlpha.SetWindowText(s); }
    m_chkVisible.SetCheck(m_obj->visible ? BST_CHECKED : BST_UNCHECKED);
    m_typeText.SetWindowText(Object3D::TypeName(m_obj->type));
    // 参数区：固定槽位（原间距/尺寸），按类型显示对应数量（共 3 组）
    for (int i = 0; i < 3; i++) {
        bool show = false;
        switch (m_obj->type) {
            case PT_BOX: show = (i < 3); break;
            case PT_SPHERE: show = (i < 1); break;
            case PT_CYLINDER: show = (i < 3); break;   // 径/高/顶（截锥顶半径）
            case PT_CONE: show = (i < 2); break;
            case PT_TORUS: show = (i < 2); break;
            case PT_PLANE: show = (i < 1); break;
            case PT_SPLINE: show = (i < 2); break;   // 枝条粗 + 控制点数量
            case PT_LEAF: show = (i < 3); break;
            case PT_TEXT3D: show = (i < 2); break;
            case PT_TUBE: show = (i < 3); break;
            case PT_RTUBE: show = (i < 3); break;
            case PT_CAPSULE: show = (i < 2); break;    // 半径 / 圆柱段高度
            case PT_HEMISPHERE: show = (i < 3); break; // 半径 / 开口角 / 盖
            case PT_ROUNDBOX: show = (i < 3); break;   // 长 / 宽 / 高
            case PT_LATHE: show = (i < 1); break;      // 母线点数
            case PT_EXTRUDE: show = (i < 2); break;    // 厚度 / 截面点数
            default: show = false; break;
        }
        m_edtParam[i].ShowWindow(show ? SW_SHOW : SW_HIDE);
        m_lblParam[i].ShowWindow(show ? SW_SHOW : SW_HIDE);
        if (show && m_skipID != (UINT)(IDC_PROP_P1 + i)) {
            if (m_obj->type == PT_SPLINE && i == 1) s.Format(_T("%d"), (int)m_obj->ctrlPts.size());   // 控制点数量：始终显示真实数量
            else if (m_obj->type == PT_LATHE && i == 0) s.Format(_T("%d"), (int)m_obj->ctrlPts.size());   // 母线点数
            else if (m_obj->type == PT_EXTRUDE && i == 1) s.Format(_T("%d"), (int)m_obj->ctrlPts.size()); // 截面点数
            else s.Format(_T("%.2f"), m_obj->params[i]);
            m_edtParam[i].SetWindowText(s);
        }
    }
    // 参数标签文字
    const wchar_t* p1 = L"";
    const wchar_t* p2 = L"";
    const wchar_t* p3 = L"";
    switch (m_obj->type) {
        case PT_BOX: p1 = L"长"; p2 = L"宽"; p3 = L"高"; break;
        case PT_SPHERE: p1 = L"径"; break;
        case PT_CYLINDER: p1 = L"径"; p2 = L"高"; p3 = L"顶"; break;   // 底半径/高度/顶半径（<=径=圆柱）
        case PT_CONE: p1 = L"径"; p2 = L"高"; break;
        case PT_TORUS: p1 = L"径"; p2 = L"管"; break;
        case PT_PLANE: p1 = L"边"; break;
        case PT_SPLINE: p1 = L"粗"; p2 = L"控"; break;   // 枝条粗 / 控制点数量
        case PT_LEAF: p1 = L"长"; p2 = L"宽"; p3 = L"厚"; break;
        case PT_TEXT3D: p1 = L"厚"; p2 = L"高"; break;
        case PT_TUBE: p1 = L"外"; p2 = L"内"; p3 = L"高"; break;
        case PT_RTUBE: p1 = L"外"; p2 = L"内"; p3 = L"高"; break;
        case PT_CAPSULE: p1 = L"径"; p2 = L"段"; break;    // 半径 / 圆柱段高度
        case PT_HEMISPHERE: p1 = L"径"; p2 = L"角"; p3 = L"盖"; break; // 半径/开口角(度)/盖(0封闭1开口)
        case PT_ROUNDBOX: p1 = L"长"; p2 = L"宽"; p3 = L"高"; break;
        case PT_LATHE: p1 = L"点"; break;              // 母线点数（锚点编辑轮廓）
        case PT_EXTRUDE: p1 = L"厚"; p2 = L"点"; break; // 拉伸厚度 / 截面点数
    }
    m_lblParam[0].SetWindowText(p1);
    m_lblParam[1].SetWindowText(p2);
    m_lblParam[2].SetWindowText(p3);
    // 细分勾选框：始终显示，只同步勾选状态（"细分"文字标签已移除）；编辑框长宽与 X/Y/Z 一致
    if (m_owner) m_chkWire.SetCheck(m_owner->GetShowWire() ? BST_CHECKED : BST_UNCHECKED);
    m_edtSeg.ShowWindow(m_obj->type == PT_PLANE || m_obj->type == PT_NONE || m_obj->type == PT_TEXT3D || m_obj->type == PT_TUBE || m_obj->type == PT_BOOLEAN || m_obj->type == PT_EXTRUDE ? SW_HIDE : SW_SHOW);
    if (m_obj->type != PT_PLANE && m_obj->type != PT_NONE && m_obj->type != PT_TEXT3D && m_obj->type != PT_TUBE && m_obj->type != PT_BOOLEAN && m_obj->type != PT_EXTRUDE && m_skipID != IDC_PROP_SEG) {
        s.Format(_T("%d"), m_obj->segments);
        m_edtSeg.SetWindowText(s);
    }
    // 颜色块（颜料盒）：与 RGB 框一致（选中面/线有自定义色时显示该色）
    m_swatchColor = RGB(
        (BYTE)(cr * 255), (BYTE)(cg * 255), (BYTE)(cb * 255));
    m_swatch.Invalidate(FALSE);
    // 3D文本：文字内容 / 字体（仅 PT_TEXT3D 显示并绑定）
    bool isText = (m_obj->type == PT_TEXT3D);
    m_lblText.ShowWindow(isText ? SW_SHOW : SW_HIDE);
    m_lblFont.ShowWindow(isText ? SW_SHOW : SW_HIDE);
    m_edtText.ShowWindow(isText ? SW_SHOW : SW_HIDE);
    m_cmbFont.ShowWindow(isText ? SW_SHOW : SW_HIDE);
    if (isText) {
        if (m_skipID != IDC_PROP_TEXT) m_edtText.SetWindowText(m_obj->text.c_str());
        if (m_skipID != IDC_PROP_FONT) {
            int idx = m_cmbFont.FindStringExact(-1, m_obj->font.c_str());
            if (idx != CB_ERR) m_cmbFont.SetCurSel(idx);
            else m_cmbFont.SetWindowText(m_obj->font.c_str());   // 自定义字体名：直接显示
        }
    }
    // 网格编辑（Blender 式点/线/面，仅可编辑图元显示）
    bool isEditable = Object3D::EditableType(m_obj->type);
    bool isEditing = m_owner && m_owner->Scene().editMeshObj == m_obj;
    m_lblMesh.ShowWindow(isEditable ? SW_SHOW : SW_HIDE);
    m_btnEditMesh.ShowWindow(isEditable ? SW_SHOW : SW_HIDE);
    m_btnEditMesh.SetWindowText(isEditing ? _T("退出编辑") : _T("编辑网格"));
    // 重建网格 / 模式行：仅编辑态显示
    m_btnRebuild.ShowWindow(isEditing ? SW_SHOW : SW_HIDE);
    m_lblEmMode.ShowWindow(isEditing ? SW_SHOW : SW_HIDE);
    m_cmbEmMode.ShowWindow(isEditing ? SW_SHOW : SW_HIDE);
    m_btnEmDel.ShowWindow(isEditing ? SW_SHOW : SW_HIDE);
    // “挤出深度”输入框与“挤出”按钮仅在面模式编辑态可用
    bool faceMode = isEditing && m_owner->GetMeshEditMode() == 2;
    m_edtEmExtrudeDepth.ShowWindow(faceMode ? SW_SHOW : SW_HIDE);
    m_btnEmExtrude.ShowWindow(faceMode ? SW_SHOW : SW_HIDE);
    if (isEditing) {
        int mode = m_owner->GetMeshEditMode();
        m_cmbEmMode.SetCurSel((mode >= 0 && mode <= 2) ? mode : 0);
    }
    m_bUpdating = false;
}

void CPropertiesPanel::OnEditChange(UINT nID) {
    m_skipID = nID;   // 记录正在编辑的框
    if (m_bUpdating || !m_obj) return;
    ApplyControls(nID);
    if (m_owner) m_owner->NotifyPropertyEdit();   // 只更新对象+视口，不刷新面板文本
}

// 字体下拉列表选择/编辑变化
void CPropertiesPanel::OnFontSel() {
    if (m_bUpdating || !m_obj) return;
    ApplyControls(IDC_PROP_FONT);
    if (m_owner) m_owner->NotifyPropertyEdit();
}

// 进入 / 退出网格编辑态
void CPropertiesPanel::OnEditMeshClick() {
    if (!m_obj || m_bUpdating) return;
    if (!Object3D::EditableType(m_obj->type)) return;
    
    if (m_owner->Scene().editMeshObj == m_obj)
        m_owner->ExitMeshEdit();
    else
        m_owner->EnterMeshEdit(m_obj);
}

// 重建网格：丢弃编辑网格，回退到程序化生成的参数化网格
void CPropertiesPanel::OnRebuildClick() {
    if (!m_obj || m_bUpdating) return;
    
    m_owner->RebuildEditMesh(m_obj);
}

// 编辑模式（点/线/面）下拉选择变化
void CPropertiesPanel::OnEmModeSel() {
    if (m_bUpdating || !m_obj) return;
    int sel = m_cmbEmMode.GetCurSel();
    if (sel >= 0 && sel <= 2) {
        
        m_owner->SetMeshEditMode(sel);
    }
}

// 删除当前选中的点/线/面元素
void CPropertiesPanel::OnEmDelClick() {
    if (m_bUpdating || !m_obj) return;
    
    m_owner->DeleteEditSelection();
}

// 面模式挤出：读取“挤出深度”输入框的数值，按该值沿平均法线精确挤出（可负=向内）；
// 框为空或非数字时回退到鼠标交互拖动（与 E 键一致）。
void CPropertiesPanel::OnEmExtrudeClick() {
    if (m_bUpdating || !m_obj) return;
    CString s;
    m_edtEmExtrudeDepth.GetWindowText(s);
    CString t = s; t.Trim();
    if (t.IsEmpty()) { m_owner->ExtrudeEditSelection(); return; }
    // 仅允许数字、小数点、正负号、空白
    bool valid = true;
    for (int i = 0; i < t.GetLength(); i++) {
        TCHAR ch = t[i];
        if (!(ch == _T('.') || ch == _T('-') || ch == _T('+') || (ch >= _T('0') && ch <= _T('9')))) { valid = false; break; }
    }
    if (!valid) { m_owner->ExtrudeEditSelection(); return; }
    double d = _wtof(t);
    m_owner->ExtrudeEditSelectionDepth(d);
}

// 点击输入框获得焦点时全选文本，直接输入即可替换旧值（再次点击仍可定位光标）
// 用 PostMessage 延迟执行，确保点击的 caret 定位处理完后全选必然最后生效。
void CPropertiesPanel::OnEditFocus(UINT nID) {
    m_lastEdit = nID;   // 记住正在编辑的框（失焦时按它分类记录属性修改）
    PostMessage(WM_PROP_SELALL, (WPARAM)nID, 0);
}

// 编辑框失焦：属性修改提交完成（操作记录已移除，保留入口）
// 注意：名称框的 EN_KILLFOCUS 会被上面的 RANGE(55 行) 先匹配，这里统一分发；
// 不再单独注册 ON_EN_KILLFOCUS(IDC_PROP_NAME)。
void CPropertiesPanel::OnEditKillFocus(UINT nID) {
    if (nID == IDC_PROP_NAME) { OnNameKillFocus(); return; }
    if (!m_obj || m_bUpdating || !m_owner) return;
}

LRESULT CPropertiesPanel::OnSelectAll(WPARAM wParam, LPARAM) {
    CEdit* e = (CEdit*)GetDlgItem((UINT)wParam);
    if (e) e->SetSel(0, -1);
    return 0;
}

// 数值编辑框子类：首次点击全选、已聚焦再点允许定位
BEGIN_MESSAGE_MAP(CNumEdit, CEdit)
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

void CNumEdit::OnLButtonDown(UINT nFlags, CPoint point) {
    bool wasFocused = (::GetFocus() == m_hWnd);
    CEdit::OnLButtonDown(nFlags, point);   // 默认处理：SetFocus + 定位 caret
    if (!wasFocused) SetSel(0, -1);        // 首次聚焦：全选覆盖 caret，直接输入即替换
}

void CPropertiesPanel::OnVisClick() {
    if (!m_obj) return;
    m_obj->visible = (m_chkVisible.GetCheck() == BST_CHECKED);
    if (m_owner) { m_owner->OnPropChanged(); }
}

// 细分显示勾选框：切换选中图元网格线框的显示（全局开关）
void CPropertiesPanel::OnWireClick() {
    bool on = (m_chkWire.GetCheck() == BST_CHECKED);
    if (m_owner) { m_owner->SetShowWire(on); }
}

// 水平翻转：绕 Y 轴左右镜像（X 缩放取反）
void CPropertiesPanel::OnFlipH() {
    if (!m_obj || m_bUpdating) return;
    if (m_owner) {
        
        m_owner->PushUndo();
        m_obj->sx = -m_obj->sx;
        m_obj->dsx = -m_obj->dsx;
        m_owner->OnViewTransformChanged();
    }
    FillControls();
}

// 垂直翻转：绕 X 轴上下镜像（Y 缩放取反）
void CPropertiesPanel::OnFlipV() {
    if (!m_obj || m_bUpdating) return;
    if (m_owner) {
        
        m_owner->PushUndo();
        m_obj->sy = -m_obj->sy;
        m_obj->dsy = -m_obj->dsy;
        m_owner->OnViewTransformChanged();
    }
    FillControls();
}

void CPropertiesPanel::ApplyControls(UINT nID) {
    if (!m_obj) return;
    CString s;
    double orx = m_obj->rx, ory = m_obj->ry, orz = m_obj->rz;   // 应用前的旋转（组旋转补偿用）
    switch (nID) {
        case IDC_PROP_NAME: {    // 名称：实时提交（每键），与其它编辑框一致
            m_edtName.GetWindowText(s);
            if (!s.IsEmpty() && s != m_obj->name.c_str()) {
                m_obj->name = s.GetString();
                if (m_owner) m_owner->OnTreeRenamed();
            }
            break;
        }
        case IDC_PROP_PX: m_edtPos[0].GetWindowText(s); m_obj->x = _wtof(s); break;
        case IDC_PROP_PY: m_edtPos[1].GetWindowText(s); m_obj->y = _wtof(s); break;
        case IDC_PROP_PZ: m_edtPos[2].GetWindowText(s); m_obj->z = _wtof(s); break;
        case IDC_PROP_RX: m_edtRot[0].GetWindowText(s); m_obj->rx = _wtof(s); break;
        case IDC_PROP_RY: m_edtRot[1].GetWindowText(s); m_obj->ry = _wtof(s); break;
        case IDC_PROP_RZ: m_edtRot[2].GetWindowText(s); m_obj->rz = _wtof(s); break;
        case IDC_PROP_SX: m_edtScale[0].GetWindowText(s); m_obj->sx = _wtof(s); if (m_obj->sx < 0.01) m_obj->sx = 0.01; break;
        case IDC_PROP_SY: m_edtScale[1].GetWindowText(s); m_obj->sy = _wtof(s); if (m_obj->sy < 0.01) m_obj->sy = 0.01; break;
        case IDC_PROP_SZ: m_edtScale[2].GetWindowText(s); m_obj->sz = _wtof(s); if (m_obj->sz < 0.01) m_obj->sz = 0.01; break;
        case IDC_PROP_R: m_edtColor[0].GetWindowText(s); ApplyColorInput(0, _wtof(s) / 255.0); break;
        case IDC_PROP_G: m_edtColor[1].GetWindowText(s); ApplyColorInput(1, _wtof(s) / 255.0); break;
        case IDC_PROP_B: m_edtColor[2].GetWindowText(s); ApplyColorInput(2, _wtof(s) / 255.0); break;
        case IDC_PROP_A: {
            m_edtAlpha.GetWindowText(s);
            double a = _wtof(s) / 100.0;
            if (a < 0) a = 0; if (a > 1) a = 1;
            m_obj->alpha = a;
            break;
        }
        case IDC_PROP_P1:
            m_edtParam[0].GetWindowText(s);
            if (m_obj->type == PT_LATHE) {
                // 车削母线点数：取整限制 2..32，折线弧长重采样（不平滑，保棱角）
                int n = _ttoi(s);
                if (n < 2) n = 2;
                if (n > 32) n = 32;
                if (m_owner) m_owner->Scene().ResampleProfile(m_obj, n);
            } else {
                m_obj->params[0] = _wtof(s);
            }
            break;
        case IDC_PROP_P2:
            m_edtParam[1].GetWindowText(s);
            if (m_obj->type == PT_SPLINE) {
                // 控制点数量：取整限制 2..16，并沿原曲线路径重采样（保持形状）
                int n = _ttoi(s);
                if (n < 2) n = 2;
                if (n > 16) n = 16;
                m_obj->params[1] = (double)n;
                if (m_owner) m_owner->Scene().ResampleSpline(m_obj, n);
            } else if (m_obj->type == PT_EXTRUDE) {
                // 挤出截面点数：取整限制 3..32，闭合环弧长重采样
                int n = _ttoi(s);
                if (n < 3) n = 3;
                if (n > 32) n = 32;
                if (m_owner) m_owner->Scene().ResampleProfile(m_obj, n);
            } else {
                m_obj->params[1] = _wtof(s);
            }
            break;
        case IDC_PROP_P3: m_edtParam[2].GetWindowText(s); m_obj->params[2] = _wtof(s); break;
        case IDC_PROP_SEG: {
            m_edtSeg.GetWindowText(s);
            int v = _ttoi(s);
            if (v < 3) v = 3;
            if (v > 64) v = 64;
            m_obj->segments = v;
            break;
        }
        case IDC_PROP_TEXT: {
            m_edtText.GetWindowText(s);
            m_obj->text = s.GetString();
            m_obj->InvalidateMesh();
            break;
        }
        case IDC_PROP_FONT: {
            CString s;
            int sel = m_cmbFont.GetCurSel();
            if (sel != CB_ERR) m_cmbFont.GetLBText(sel, s);   // 下拉选中项
            else m_cmbFont.GetWindowText(s);                   // 直接输入的自定义字体名
            m_obj->font = s.GetString();
            m_obj->InvalidateMesh();
            break;
        }
    }
    // 图元尺寸/文字/字体变化 → 网格缓存失效
    if (nID >= IDC_PROP_P1 && nID <= IDC_PROP_P4) m_obj->InvalidateMesh();
    // 组（movieclip）旋转被直接改值：补偿直接子件自转，成员只公转、世界朝向不变
    if (m_obj->type == PT_NONE && !m_obj->children.empty() &&
        (m_obj->rx != orx || m_obj->ry != ory || m_obj->rz != orz))
        m_obj->CompensateChildRotations(Mat4::RotationXYZ(orx, ory, orz));
    if (nID == IDC_PROP_R || nID == IDC_PROP_G || nID == IDC_PROP_B) {
        // 颜料盒同步：编辑态选中面/线时显示选中元素的新色，否则物体色
        double r = m_obj->r, g = m_obj->g, b = m_obj->b;
        unsigned int c;
        if (HasEditColorTarget() && GetSelCustomColor(c))
            Object3D::UnpackEditColor(c, r, g, b);
        m_swatchColor = RGB(
            (BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
        m_swatch.Invalidate(FALSE);
    }
}

// ------------------------------------------------------------ 网格编辑态：选中面/线独立着色
// 本对象正在被网格编辑，且当前模式（线/面）下有选中的元素 → 颜色设置只作用于选中元素
bool CPropertiesPanel::HasEditColorTarget() {
    if (!m_owner || !m_obj) return false;
    if (m_owner->GetEditMeshObj() != m_obj) return false;
    int mode = m_owner->GetMeshEditMode();
    if (mode == 2) {
        std::vector<int> f;
        m_owner->GetEditSelFaces(f);
        return !f.empty();
    }
    if (mode == 1) {
        std::vector<std::pair<int, int> > e;
        m_owner->GetEditSelEdges(e);
        return !e.empty();
    }
    return false;
}

// 选中元素的首个自定义色（多选时取第一个带自定义色的；全无自定义色返回 false）
bool CPropertiesPanel::GetSelCustomColor(unsigned int& c) {
    if (!m_owner || !m_obj) return false;
    int mode = m_owner->GetMeshEditMode();
    if (mode == 2) {
        std::vector<int> f;
        m_owner->GetEditSelFaces(f);
        for (size_t i = 0; i < f.size(); i++)
            if (m_obj->GetEditTriColor(f[i], c)) return true;
        return false;
    }
    if (mode == 1) {
        std::vector<std::pair<int, int> > e;
        m_owner->GetEditSelEdges(e);
        for (size_t i = 0; i < e.size(); i++)
            if (m_obj->GetEditEdgeColor(e[i].first, e[i].second, c)) return true;
    }
    return false;
}

// 把颜色写到选中的面/线（不改动物体整体色）
void CPropertiesPanel::ApplySelColor(double r, double g, double b) {
    if (!m_owner || !m_obj) return;
    unsigned int c = Object3D::PackEditColor(r, g, b);
    int mode = m_owner->GetMeshEditMode();
    if (mode == 2) {
        std::vector<int> f;
        m_owner->GetEditSelFaces(f);
        if (!f.empty()) m_obj->SetEditTriColors(f, c);
    } else if (mode == 1) {
        std::vector<std::pair<int, int> > e;
        m_owner->GetEditSelEdges(e);
        if (!e.empty()) m_obj->SetEditEdgeColors(e, c);
    }
}

// R/G/B 单分量输入应用：编辑态选中面/线 → 以其现有色为基改该分量后写回选中元素；
// 否则按原逻辑改物体整体色
void CPropertiesPanel::ApplyColorInput(int comp, double v) {
    if (HasEditColorTarget()) {
        double r, g, b;
        unsigned int c;
        if (GetSelCustomColor(c)) {
            Object3D::UnpackEditColor(c, r, g, b);
        } else {
            r = m_obj->r; g = m_obj->g; b = m_obj->b;   // 未自定义过：以物体色为基
        }
        if (comp == 0) r = v;
        else if (comp == 1) g = v;
        else b = v;
        ApplySelColor(r, g, b);
    } else {
        if (comp == 0) m_obj->r = v;
        else if (comp == 1) m_obj->g = v;
        else m_obj->b = v;
    }
}

// 颜料盒点击：弹出标准颜色选择器，选色后应用到选中组件
// 网格编辑态选中面/线时：只改选中元素的颜色，不动物体整体色
void CPropertiesPanel::OnPickColor() {
    if (!m_obj) return;
    // 初始色：选中面/线有自定义色用之，否则物体色
    double cr = m_obj->r, cg = m_obj->g, cb = m_obj->b;
    unsigned int c0;
    if (GetSelCustomColor(c0)) Object3D::UnpackEditColor(c0, cr, cg, cb);
    COLORREF cur = RGB(
        (BYTE)(cr * 255), (BYTE)(cg * 255), (BYTE)(cb * 255));
    CColorDialog dlg(cur, CC_FULLOPEN | CC_RGBINIT);
    if (dlg.DoModal() == IDOK) {
        COLORREF c = dlg.GetColor();
        double r = GetRValue(c) / 255.0;
        double g = GetGValue(c) / 255.0;
        double b = GetBValue(c) / 255.0;
        if (HasEditColorTarget())
            ApplySelColor(r, g, b);   // 只改选中的面/线
        else { m_obj->r = r; m_obj->g = g; m_obj->b = b; }
        FillControls();                    // 更新 RGB 文本框与颜料盒
        if (m_owner) m_owner->NotifyPropertyEdit();
    }
}

// 颜料盒 owner-draw：填当前颜色
void CPropertiesPanel::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct) {
    if (lpDrawItemStruct->CtlID == (UINT)IDC_PROP_SWATCH) {
        CDC* dc = CDC::FromHandle(lpDrawItemStruct->hDC);
        CRect rc(lpDrawItemStruct->rcItem);
        dc->FillSolidRect(rc, m_swatchColor);
        dc->Draw3dRect(rc, RGB(90, 90, 90), RGB(90, 90, 90));
        return;
    }
    CWnd::OnDrawItem(nIDCtl, lpDrawItemStruct);
}

void CPropertiesPanel::OnNameKillFocus() {
    if (!m_obj) return;
    CString s;
    m_edtName.GetWindowText(s);
    if (!s.IsEmpty() && s != m_obj->name.c_str()) {
        m_obj->name = s.GetString();
        if (m_owner) m_owner->OnTreeRenamed();
    }
}

// 编辑框按回车：提交当前值（名称框无 EN_CHANGE，失焦才改名；回车不触发失焦，
// 默认会被系统响一声。这里回车=提交+焦点还给面板）
BOOL CPropertiesPanel::PreTranslateMessage(MSG* pMsg) {
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
        CWnd* fw = GetFocus();
        if (fw != NULL && fw == &m_edtName) {
            OnNameKillFocus();
            SetFocus();      // 焦点还给面板，避免系统提示音
            return TRUE;
        }
        if (fw != NULL && fw == &m_edtEmExtrudeDepth) {
            OnEmExtrudeClick();   // 输入深度后回车直接挤出
            SetFocus();
            return TRUE;
        }
    }
    return CWnd::PreTranslateMessage(pMsg);
}

HBRUSH CPropertiesPanel::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH hbr = CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
    if (pWnd == &m_swatch) {
        pDC->SetBkMode(TRANSPARENT);
        return (HBRUSH)m_swatchBrush.GetSafeHandle();
    }
    return hbr;
}

BOOL CPropertiesPanel::OnEraseBkgnd(CDC* pDC) {
    return FALSE; // 使用注册的窗口背景
}

void CPropertiesPanel::Refresh() {
    FillControls();
}
