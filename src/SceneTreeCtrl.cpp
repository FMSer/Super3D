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
// SceneTreeCtrl.cpp : 场景树实现
#include "stdafx.h"
#include "SceneTreeCtrl.h"
#include "MainWnd.h"

BEGIN_MESSAGE_MAP(CSceneTreeCtrl, CTreeCtrl)
    ON_NOTIFY_REFLECT(TVN_SELCHANGED, &CSceneTreeCtrl::OnSelChanged)
    ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, &CSceneTreeCtrl::OnEndLabelEdit)
    ON_NOTIFY_REFLECT(TVN_BEGINDRAG, &CSceneTreeCtrl::OnBeginDrag)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

CSceneTreeCtrl::CSceneTreeCtrl() : m_owner(NULL), m_bUpdating(false),
    m_dragItem(NULL), m_dragObj(NULL), m_dragImage(NULL),
    m_bDragging(false), m_lastDrop(NULL) {}

CSceneTreeCtrl::~CSceneTreeCtrl() {
    if (m_dragImage) {
        m_dragImage->DeleteImageList();
        delete m_dragImage;
        m_dragImage = NULL;
    }
}

BOOL CSceneTreeCtrl::CreateTree(CWnd* parent, const CRect& rc) {
    BOOL ok = Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                  TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_EDITLABELS | TVS_SHOWSELALWAYS,
                  rc, parent, 1001);
    if (ok) {
        SetFont(&m_owner->GetSmallFont());
    }
    return ok;
}

// 绘制 16x16 节点图标：0=组（橙文件夹），1=几何体（蓝方块）
void CSceneTreeCtrl::CreateIcons() {
    m_icons.Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 1);
    HDC hdc = ::GetDC(NULL);
    if (!hdc) { m_icons.Create(1, 1, ILC_COLOR32, 1, 0); return; }
    CDC* pDC = CDC::FromHandle(hdc);
    CBitmap bmp;
    if (bmp.CreateCompatibleBitmap(pDC, 16, 16)) {
        CDC mem;
        mem.CreateCompatibleDC(pDC);
        CBitmap* old = (CBitmap*)mem.SelectObject(&bmp);
        // 组：橙色小加号（可展开，替代文件夹方块）
        mem.FillSolidRect(0, 0, 16, 16, RGB(255, 0, 255)); // 粉色=透明色
        CPen pen(PS_SOLID, 2, RGB(220, 120, 0));
        mem.SelectObject(&pen);
        mem.MoveTo(3, 8); mem.LineTo(13, 8);
        mem.MoveTo(8, 3); mem.LineTo(8, 13);
        m_icons.Add(&bmp, RGB(255, 0, 255));
        // 几何体：蓝色小加号（替代黑色方块）
        mem.FillSolidRect(0, 0, 16, 16, RGB(255, 0, 255));
        CPen pen2(PS_SOLID, 2, RGB(40, 90, 210));
        mem.SelectObject(&pen2);
        mem.MoveTo(3, 8); mem.LineTo(13, 8);
        mem.MoveTo(8, 3); mem.LineTo(8, 13);
        m_icons.Add(&bmp, RGB(255, 0, 255));
        mem.SelectObject(old);
    }
    bmp.DeleteObject();
    ::ReleaseDC(NULL, hdc);
}

void CSceneTreeCtrl::Rebuild(Scene3D& s, const std::wstring& selName) {
    m_bUpdating = true;
    DeleteAllItems();
    std::vector<Object3D*> stack;
    for (size_t i = s.roots.size(); i > 0; i--)
        stack.push_back(s.roots[i - 1]);
    // 递归插入
    struct Item {
        Object3D* obj;
        HTREEITEM h;
        Item(Object3D* o, HTREEITEM x) : obj(o), h(x) {}
    };
    std::vector<Item> items;
    for (size_t i = 0; i < stack.size(); i++) {
        HTREEITEM h = InsertItem(stack[i]->name.c_str(), TVI_ROOT, TVI_LAST);
        items.push_back(Item(stack[i], h));
        SetItemData(h, (DWORD_PTR)stack[i]);
    }
    size_t idx = 0;
    while (idx < items.size()) {
        Object3D* o = items[idx].obj;
        HTREEITEM h = items[idx].h;
        idx++;
        for (size_t j = 0; j < o->children.size(); j++) {
            HTREEITEM ch = InsertItem(o->children[j]->name.c_str(), h, TVI_LAST);
            SetItemData(ch, (DWORD_PTR)o->children[j]);
            items.push_back(Item(o->children[j], ch));
        }
        if (o->name == selName) {
            SelectItem(h);
            Expand(h, TVE_EXPAND);
        }
    }
    m_bUpdating = false;
}

void CSceneTreeCtrl::OnSelChanged(NMHDR* pNMHDR, LRESULT* pResult) {
    NMTREEVIEW* p = (NMTREEVIEW*)pNMHDR;
    *pResult = 0;
    if (m_bUpdating || m_bDragging) return; // 拖拽期间不改选择
    HTREEITEM h = p->itemNew.hItem;
    if (!h || !m_owner) return;
    Object3D* o = (Object3D*)GetItemData(h);
    if (o) m_owner->SelectObject(o);
}

void CSceneTreeCtrl::OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult) {
    NMTVDISPINFO* p = (NMTVDISPINFO*)pNMHDR;
    *pResult = 0;
    if (!p->item.pszText || !p->item.pszText[0]) return;
    Object3D* o = (Object3D*)GetItemData(p->item.hItem);
    if (!o) return;
    o->name = p->item.pszText;
    *pResult = 1;
    if (m_owner) m_owner->OnTreeRenamed();
}

// 判断 child 是否在 parent 的子树上（不含自身）
bool CSceneTreeCtrl::IsDescendant(HTREEITEM parent, HTREEITEM child) const {
    for (HTREEITEM h = GetChildItem(parent); h; h = GetNextSiblingItem(h)) {
        if (h == child) return true;
        if (IsDescendant(h, child)) return true;
    }
    return false;
}

void CSceneTreeCtrl::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult) {
    *pResult = 0;
    NMTREEVIEW* p = (NMTREEVIEW*)pNMHDR;
    if (m_bDragging) return;
    m_dragItem = p->itemNew.hItem;
    if (!m_dragItem) return;
    m_dragObj = (Object3D*)GetItemData(m_dragItem);
    if (!m_dragObj) return;
    // 尝试创建拖拽影子；失败时仍继续（无影拖拽，靠目标高亮提示）
    m_dragImage = CreateDragImage(m_dragItem);
    if (m_dragImage) {
        m_dragImage->BeginDrag(0, CPoint(0, 0));
        CPoint pt = p->ptDrag;
        ClientToScreen(&pt);
        m_dragImage->DragEnter(NULL, pt);
    }
    SetCapture();
    ::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_SIZEALL)));
    m_bDragging = true;
    m_lastDrop = NULL;
}

void CSceneTreeCtrl::OnMouseMove(UINT nFlags, CPoint point) {
    if (!m_bDragging) { CTreeCtrl::OnMouseMove(nFlags, point); return; }
    if (m_dragImage) {
        CPoint pt = point;
        ClientToScreen(&pt);
        m_dragImage->DragMove(pt);
    }
    // 高亮可放入的组（无影子时同样有效）
    UINT flags = 0;
    HTREEITEM hTarget = HitTest(point, &flags);
    Object3D* targetObj = hTarget ? (Object3D*)GetItemData(hTarget) : NULL;
    bool valid = targetObj && targetObj->type == PT_NONE &&
                 targetObj != m_dragObj && !IsDescendant(m_dragItem, hTarget);
    HTREEITEM newDrop = valid ? hTarget : NULL;
    if (newDrop != m_lastDrop) {
        if (m_lastDrop) SetItemState(m_lastDrop, 0, TVIS_DROPHILITED);
        if (newDrop) SetItemState(newDrop, TVIS_DROPHILITED, TVIS_DROPHILITED);
        m_lastDrop = newDrop;
    }
}

void CSceneTreeCtrl::OnLButtonUp(UINT nFlags, CPoint point) {
    if (!m_bDragging) { CTreeCtrl::OnLButtonUp(nFlags, point); return; }
    m_bDragging = false;
    if (m_dragImage) {
        m_dragImage->DragLeave(NULL);
        m_dragImage->EndDrag();
    }
    ReleaseCapture();
    // 目标：优先点中项；若鼠标偏出控件/未点中，则用最后高亮的组
    UINT flags = 0;
    HTREEITEM hTarget = HitTest(point, &flags);
    if (!hTarget) hTarget = m_lastDrop;
    Object3D* targetObj = hTarget ? (Object3D*)GetItemData(hTarget) : NULL;
    if (targetObj && targetObj->type == PT_NONE &&
        targetObj != m_dragObj && m_dragObj && !IsDescendant(m_dragItem, hTarget)) {
        if (m_owner) m_owner->MoveIntoObject(m_dragObj, targetObj);
    }
    if (m_lastDrop) { SetItemState(m_lastDrop, 0, TVIS_DROPHILITED); m_lastDrop = NULL; }
    if (m_dragImage) {
        m_dragImage->DeleteImageList();
        delete m_dragImage;
        m_dragImage = NULL;
    }
    m_dragItem = NULL;
    m_dragObj = NULL;
}
