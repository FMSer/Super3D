// SceneTreeCtrl.h : 场景树控件
#pragma once
#include "stdafx.h"
#include "Scene3D.h"

class CMainWnd;

class CSceneTreeCtrl : public CTreeCtrl {
public:
    CSceneTreeCtrl();
    ~CSceneTreeCtrl();
    void SetOwner(CMainWnd* o) { m_owner = o; }
    BOOL CreateTree(CWnd* parent, const CRect& rc);
    void Rebuild(Scene3D& s, const std::wstring& selName);
    bool m_bUpdating;

protected:
    CMainWnd* m_owner;
    HTREEITEM m_dragItem;
    Object3D* m_dragObj;
    CImageList* m_dragImage;
    bool m_bDragging;
    HTREEITEM m_lastDrop;
    CImageList m_icons;          // 树节点图标（0=组 1=几何体）
    void CreateIcons();
    bool IsDescendant(HTREEITEM parent, HTREEITEM child) const;

    afx_msg void OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()
};
