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
// PlayerViewWnd.cpp : 播放器 3D 视口实现
#include "stdafx.h"
#include "PlayerViewWnd.h"
#include "PlayerWnd.h"
#include "Renderer.h"

BEGIN_MESSAGE_MAP(CPlayerViewWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

CPlayerViewWnd::CPlayerViewWnd() : m_owner(NULL), m_hdc(NULL), m_rc(NULL),
    m_w(0), m_h(0), m_mode(MODE_NONE) {}

CPlayerViewWnd::~CPlayerViewWnd() {}

BOOL CPlayerViewWnd::CreateView(CWnd* parent, const CRect& rc) {
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
    return CreateEx(0, cls, _T("播放视口"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, rc, parent, 0);
}

int CPlayerViewWnd::OnCreate(LPCREATESTRUCT lpcs) {
    if (CWnd::OnCreate(lpcs) == -1) return -1;
    m_hdc = ::GetDC(m_hWnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    int pf = ::ChoosePixelFormat(m_hdc, &pfd);
    if (pf == 0) { ::ReleaseDC(m_hWnd, m_hdc); m_hdc = NULL; return -1; }
    ::SetPixelFormat(m_hdc, pf, &pfd);
    m_rc = ::wglCreateContext(m_hdc);
    return 0;
}

void CPlayerViewWnd::OnDestroy() {
    if (m_rc) {
        ::wglMakeCurrent(NULL, NULL);
        ::wglDeleteContext(m_rc);
        m_rc = NULL;
    }
    if (m_hdc) {
        ::ReleaseDC(m_hWnd, m_hdc);
        m_hdc = NULL;
    }
    CWnd::OnDestroy();
}

BOOL CPlayerViewWnd::OnEraseBkgnd(CDC* pDC) {
    return TRUE; // 由 OpenGL 绘制
}

void CPlayerViewWnd::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    m_w = cx; m_h = cy;
}

void CPlayerViewWnd::SetupGL() {
    if (!m_rc || !m_hdc) return;
    ::wglMakeCurrent(m_hdc, m_rc);
}

void CPlayerViewWnd::OnPaint() {
    CPaintDC dc(this);
    if (!m_owner) return;
    if (m_rc && m_hdc && m_w > 0 && m_h > 0) {
        SetupGL();
        Renderer::Render(m_owner->Scene(), m_w, m_h);
        ::SwapBuffers(m_hdc);
        // 操作提示文字已移出场景：显示在窗口底部状态栏（见 CPlayerWnd::UpdateStatus）。
        // 不再用 GDI 叠加到 OpenGL 表面（SwapBuffers 每帧覆盖文字导致闪烁）。
    }
}

void CPlayerViewWnd::RefreshOwner() {
    if (m_owner) m_owner->ViewChanged();
}

void CPlayerViewWnd::OnLButtonDown(UINT nFlags, CPoint point) {
    SetFocus();
    SetCapture();
    m_mode = MODE_ORBIT;
    m_last = point;
    CWnd::OnLButtonDown(nFlags, point);
}

void CPlayerViewWnd::OnLButtonUp(UINT nFlags, CPoint point) {
    if (m_mode == MODE_ORBIT) {
        m_mode = MODE_NONE;
        if (GetCapture() == this) ReleaseCapture();
    }
    CWnd::OnLButtonUp(nFlags, point);
}

void CPlayerViewWnd::OnRButtonDown(UINT nFlags, CPoint point) {
    SetCapture();
    m_mode = MODE_PAN;
    m_last = point;
    CWnd::OnRButtonDown(nFlags, point);
}

void CPlayerViewWnd::OnRButtonUp(UINT nFlags, CPoint point) {
    if (m_mode == MODE_PAN) {
        m_mode = MODE_NONE;
        if (GetCapture() == this) ReleaseCapture();
    }
    CWnd::OnRButtonUp(nFlags, point);
}

void CPlayerViewWnd::OnMouseMove(UINT nFlags, CPoint point) {
    if (!m_owner) return;
    Camera& c = m_owner->Scene().cam;
    if (m_mode == MODE_ORBIT) {
        c.yaw -= (double)(point.x - m_last.x) * 0.5;
        c.pitch += (double)(point.y - m_last.y) * 0.5;
        if (c.pitch > 89.0) c.pitch = 89.0;
        if (c.pitch < -89.0) c.pitch = -89.0;
        m_last = point;
        Repaint();
    } else if (m_mode == MODE_PAN) {
        double k = c.dist * 0.0012;
        c.tx -= (double)(point.x - m_last.x) * k;
        c.ty += (double)(point.y - m_last.y) * k;
        m_last = point;
        Repaint();
    }
    CWnd::OnMouseMove(nFlags, point);
}

BOOL CPlayerViewWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
    if (!m_owner) return CWnd::OnMouseWheel(nFlags, zDelta, pt);
    Camera& c = m_owner->Scene().cam;
    c.dist *= (zDelta > 0) ? 0.9 : 1.1;
    if (c.dist < 1.5) c.dist = 1.5;
    if (c.dist > 500.0) c.dist = 500.0;
    Repaint();
    return TRUE;
}
