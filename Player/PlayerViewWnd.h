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
// PlayerViewWnd.h : 播放器 3D 视口（只渲染 + 视角控制，无编辑交互）
#pragma once
#include "stdafx.h"
#include "Scene3D.h"

class CPlayerWnd;

class CPlayerViewWnd : public CWnd {
public:
    CPlayerViewWnd();
    virtual ~CPlayerViewWnd();

    BOOL CreateView(CWnd* parent, const CRect& rc);
    void SetOwner(CPlayerWnd* o) { m_owner = o; }
    void Repaint() { Invalidate(FALSE); }

protected:
    CPlayerWnd* m_owner;
    HDC m_hdc;
    HGLRC m_rc;
    int m_w, m_h;

    enum ViewMode { MODE_NONE = 0, MODE_ORBIT, MODE_PAN };
    int m_mode;
    CPoint m_last;

    void SetupGL();
    void RefreshOwner();

    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    DECLARE_MESSAGE_MAP()
};
