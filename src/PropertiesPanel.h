// PropertiesPanel.h : 属性面板
#pragma once
#include "stdafx.h"
#include "Scene3D.h"

class CMainWnd;

// 数值编辑框子类：首次点击（未聚焦）全选旧文本，直接输入即替换；已聚焦再点击可定位到具体位置。
class CNumEdit : public CEdit {
public:
    CNumEdit() {}
protected:
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()
};

class CPropertiesPanel : public CWnd {
public:
    CPropertiesPanel();
    void SetOwner(CMainWnd* o) { m_owner = o; }
    BOOL CreatePanel(CWnd* parent, const CRect& rc);
    void Bind(Object3D* o);
    void Refresh();          // 从对象重新取值填框
    Object3D* m_obj;
    bool m_bUpdating;
    UINT m_skipID;          // 正在编辑的编辑框 ID（刷新时跳过，避免打断输入）

protected:
    CMainWnd* m_owner;
    CBrush m_swatchBrush;

    CStatic m_lblName, m_lblPos, m_lblRot, m_lblScale, m_lblColor, m_lblType, m_lblSize, m_lblSeg, m_lblFlip;
    CStatic m_lblPosX, m_lblPosY, m_lblPosZ;
    CStatic m_lblRotX, m_lblRotY, m_lblRotZ;
    CStatic m_lblScaleX, m_lblScaleY, m_lblScaleZ;
    CStatic m_lblColorR, m_lblColorG, m_lblColorB;
    CStatic m_lblParam[4];
    CStatic m_typeText;
    CButton m_swatch;           // 颜料盒：显示当前颜色，点击弹出颜色选择器
    COLORREF m_swatchColor;     // 颜料盒当前显示色

    CEdit m_edtName;
    CNumEdit m_edtPos[3], m_edtRot[3], m_edtScale[3], m_edtColor[3];
    CNumEdit m_edtParam[4];
    CNumEdit m_edtSeg;
    CButton m_chkVisible;
    CButton m_chkWire;      // 细分显示勾选框（控制选中图元网格线框的显示）
    CButton m_btnFlipH, m_btnFlipV;  // 水平翻转 / 垂直翻转
    CStatic m_lblText, m_lblFont;    // 3D文本：文字内容 / 字体
    CEdit m_edtText;
    CComboBox m_cmbFont;             // 字体下拉列表
    // 网格编辑（Blender 式点/线/面）：进入编辑 / 重建网格 / 编辑模式 / 删除选中
    CStatic m_lblMesh;                       // "网格" 行标签
    CButton m_btnEditMesh;                   // 编辑网格 / 退出编辑 按钮
    CButton m_btnRebuild;                    // 重建网格 按钮（编辑态显示）
    CStatic m_lblEmMode;                     // "模式" 行标签（编辑态显示）
    CComboBox m_cmbEmMode;                   // 点/线/面 下拉
    CButton m_btnEmDel;                      // 删除选中 按钮

    void CreateLabel(CStatic& st, const CString& text, const CRect& rc, CWnd* parent);
    void CreateEdit(CEdit& ed, const CRect& rc, CWnd* parent, UINT id);
    void SetupControls();
    void FillControls();
    void ApplyControls(UINT nID);

    // 网格编辑态：选中面/线独立着色（不改动物体整体色）
    bool HasEditColorTarget();                // 编辑态且当前模式有选中的面/线
    bool GetSelCustomColor(unsigned int& c);  // 选中元素的首个自定义色（无=false）
    void ApplySelColor(double r, double g, double b);   // 颜色写到选中的面/线
    void ApplyColorInput(int comp, double v); // R/G/B 分量输入应用（自动分流整体/选中）

    afx_msg void OnEditChange(UINT nID);
    afx_msg void OnEditFocus(UINT nID);
    afx_msg void OnFontSel();       // 字体下拉选择变化
    afx_msg LRESULT OnSelectAll(WPARAM wParam, LPARAM lParam);
    afx_msg void OnVisClick();
    afx_msg void OnWireClick();
    afx_msg void OnFlipH();
    afx_msg void OnFlipV();
    afx_msg void OnEditMeshClick(); // 进入/退出网格编辑
    afx_msg void OnRebuildClick();  // 重建网格（回退到程序化生成）
    afx_msg void OnEmModeSel();     // 编辑模式下拉选择变化
    afx_msg void OnEmDelClick();    // 删除选中元素
    afx_msg void OnPickColor();
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnNameKillFocus();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};
