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
