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
// ViewportWnd.cpp : 3D 视口实现
#include "stdafx.h"
#include "ViewportWnd.h"
#include "MainWnd.h"
#include "Renderer.h"
#include <gl/gl.h>
#include <gl/glu.h>
#include <map>
#include <algorithm>

BEGIN_MESSAGE_MAP(CViewportWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MBUTTONDOWN()
    ON_WM_MBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_KEYDOWN()
    ON_WM_SETFOCUS()
END_MESSAGE_MAP()

CViewportWnd::CViewportWnd() : m_owner(NULL), m_hdc(NULL), m_rc(NULL), m_w(0), m_h(0),
    m_drag(DRAG_NONE), m_dragStartDist(1.0), m_dragObj(NULL), m_navH(28),
    m_splineObj(NULL), m_splineDrawing(false), m_splineDown(0, 0), m_anchorIdx(-1), m_cornerIndex(-1), m_faceIndex(-1),
    m_emMode(0), m_emDrag(false), m_emExtrude(false), m_emExtrudeT0(0.0),
    m_multiMode(false), m_multiAxis(-1),
    m_rotStartRx(0), m_rotStartRy(0), m_rotStartRz(0) {
    m_startScale[0] = m_startScale[1] = m_startScale[2] = 1.0;
}

CViewportWnd::~CViewportWnd() {}

BOOL CViewportWnd::CreateView(CWnd* parent, const CRect& rc) {
    // 必须带 CS_DBLCLKS，否则窗口收不到双击消息（双击进入组编辑将失效）
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        ::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
    return CreateEx(WS_EX_CLIENTEDGE, cls, _T("视口"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, rc, parent, 0);
}

int CViewportWnd::OnCreate(LPCREATESTRUCT lpcs) {
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

void CViewportWnd::OnDestroy() {
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

BOOL CViewportWnd::OnEraseBkgnd(CDC* pDC) {
    return TRUE; // 由 OpenGL 绘制
}

void CViewportWnd::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    m_w = cx; m_h = cy;
}

void CViewportWnd::SetupGL() {
    if (!m_rc || !m_hdc) return;
    ::wglMakeCurrent(m_hdc, m_rc);
    // 启用垂直同步（VSync），消除拖动元件时高频重绘产生的撕裂/闪烁感
    typedef BOOL(APIENTRY *PFNwglSwapIntervalEXT)(int);
    PFNwglSwapIntervalEXT swapInt = (PFNwglSwapIntervalEXT)::wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInt) swapInt(1);
}

void CViewportWnd::OnPaint() {
    CPaintDC dc(this);
    if (!m_owner) return;
    if (m_rc && m_hdc && m_w > 0 && m_h > 0) {
        SetupGL();
        Renderer::Render(m_owner->Scene(), m_w, m_h);
        // 网格编辑态：画点/线/面编辑叠加层（顶点/边框/选中高亮，沿用当前相机矩阵）
        if (m_owner->Scene().editMeshObj)
            Renderer::DrawEditMeshOverlay(m_owner->Scene(), m_emMode, m_emSelV, m_emSelE, m_emSelF);
        ::SwapBuffers(m_hdc);
        // 框选：在场景之上叠加虚线选框（GDI，仅框选期间显示）
        if (m_drag == DRAG_MARQUEE) {
            CPen pen(PS_DOT, 1, RGB(70, 130, 255));
            CPen* oldPen = dc.SelectObject(&pen);
            CBrush* oldBr = (CBrush*)dc.SelectStockObject(NULL_BRUSH);
            int x1 = (m_marqueeStart.x < m_marqueeCur.x) ? m_marqueeStart.x : m_marqueeCur.x;
            int y1 = (m_marqueeStart.y < m_marqueeCur.y) ? m_marqueeStart.y : m_marqueeCur.y;
            int x2 = (m_marqueeStart.x > m_marqueeCur.x) ? m_marqueeStart.x : m_marqueeCur.x;
            int y2 = (m_marqueeStart.y > m_marqueeCur.y) ? m_marqueeStart.y : m_marqueeCur.y;
            dc.Rectangle(x1, y1, x2, y2);
            dc.SelectObject(oldBr);
            dc.SelectObject(oldPen);
        }
        // 文字全部移出场景（场景与文本分开）：编辑导航栏/选中名由主窗口用 GDI 画在视口外，
        // 底部操作提示移到底部状态栏。不再在 SwapBuffers 之后向场景叠加 GDI 文字，
        // 否则下一帧缓冲交换时文字被冲掉，快速拖放/缩放时明显闪烁。
        // 曲线的黄色锚点由 Renderer 用 OpenGL 在世界坐标绘制（与曲线同一相机，贴合无错位）
    }
}

Vec3 CViewportWnd::ScreenRay(int x, int y) const {
    Camera& c = m_owner->Scene().cam;
    Vec3 eye = Renderer::CameraEye(c);
    Vec3 target(c.tx, c.ty, c.tz);
    Vec3 fwd = Normalize(target - eye);
    Vec3 right = Normalize(Cross(fwd, Vec3(0, 1, 0)));
    Vec3 up = Cross(right, fwd);
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);
    double aspect = m_h > 0 ? (double)m_w / (double)m_h : 1.0;
    double px = ((double)x / m_w * 2.0 - 1.0) * tanF * aspect;
    double py = (1.0 - (double)y / m_h * 2.0) * tanF;
    return Normalize(fwd + right * px + up * py);
}

bool CViewportWnd::RayPlane(const Vec3& org, const Vec3& dir,
                            const Vec3& n, const Vec3& planePt, Vec3& out) const {
    double denom = Dot(dir, n);
    if (std::fabs(denom) < 1e-9) return false;
    double t = Dot(planePt - org, n) / denom;
    if (t < 0) return false;
    out = org + dir * t;
    return true;
}

Object3D* CViewportWnd::PickAt(int x, int y) {
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 dir = ScreenRay(x, y);
    Object3D* hit = NULL;
    double t = 0;
    if (m_owner->Scene().PickObject(eye, dir, hit, t))
        return hit;
    return NULL;
}

void CViewportWnd::OnPick(int x, int y) {
    Object3D* hit = PickAt(x, y);
    if (hit)
        m_owner->SelectObject(hit);
    else
        m_owner->SelectObject(NULL);
}

void CViewportWnd::OnLButtonDown(UINT nFlags, CPoint point) {
    SetFocus();
    // 挤出交互中：移动鼠标已实时调整深度，单击=确认（保留新顶盖选中）
    if (m_emExtrude) {
        m_emExtrude = false;
        m_emDrag = false;
        m_drag = DRAG_NONE;
        m_dragObj = NULL;
        m_emDragVerts.clear();
        m_emDragStart.clear();
        ReleaseCapture();
        BuildEditEdges();
        m_owner->OnViewTransformChanged();
        Invalidate(FALSE);
        return;
    }
    // 编辑导航栏已移出视口：由主窗口绘制在视口上方并处理点击（←返回 / 场景1 / 层级选项卡）
    int tool = m_owner->GetTool();
    // 画曲线工具：单击逐个添加控制点（多控制点曲线），按住拖拽快速画弧线（3点）
    if (tool == CMainWnd::TOOL_SPLINE) {
        if (m_splineObj && !m_splineDrawing) {   // 锚点会话中：本次单击追加锚点
            AddSplineAnchor(point);
            return;
        }
        StartSplineDraw(point);
        m_splineDown = point;
        return;
    }
    // 网格编辑态（点/线/面）：Ctrl/Shift 都用于元素多选，不参与对象级多选，交给后面的编辑逻辑
    Object3D* emObjPre = m_owner->Scene().editMeshObj;
    bool emActive = (tool == CMainWnd::TOOL_SELECT && emObjPre && !emObjPre->locked &&
                     !m_owner->Scene().LayerLocked(emObjPre));
    // Ctrl+左键：多选 / 取消选择（不触发拖动）；网格编辑态下交由后面的元素选择逻辑处理
    if ((nFlags & MK_CONTROL) && !emActive) {
        Object3D* picked = PickAt(point.x, point.y);
        if (picked) {
            if (m_owner->InSelection(picked))
                m_owner->RemoveFromSelection(picked);
            else
                m_owner->AddToSelection(picked, true);
        } else {
            m_owner->ClearSelection();
        }
        return;
    }
    // 网格编辑态（Blender 式点/线/面）：命中元素 → 选择/反选并开始拖动；未命中 → 清空编辑选择。
    // 编辑态吞掉常规拾取/对象拖动（退出编辑用 Esc 或属性面板按钮），中键平移/右键旋转不受影响。
    Object3D* emObj = m_owner->Scene().editMeshObj;
    if (tool == CMainWnd::TOOL_SELECT && emObj && !emObj->locked && !m_owner->Scene().LayerLocked(emObj)) {
        bool hit = false;
        bool addSel = (nFlags & (MK_SHIFT | MK_CONTROL)) != 0;   // Ctrl 或 Shift：元素加选/减选
        if (m_emMode == 0) {                        // 点模式
            int vi = -1;
            if (HitEditVertex(point, vi)) {
                hit = true;
                std::vector<int>::iterator it = std::find(m_emSelV.begin(), m_emSelV.end(), vi);
                if (addSel) {
                    if (it != m_emSelV.end()) m_emSelV.erase(it);
                    else m_emSelV.push_back(vi);
                } else if (it == m_emSelV.end()) {
                    m_emSelV.clear();
                    m_emSelV.push_back(vi);
                }
            }
        } else if (m_emMode == 1) {                 // 线模式
            int ei = -1;
            if (HitEditEdge(point, ei)) {
                hit = true;
                std::pair<int, int> e = m_emEdges[ei];
                std::vector<std::pair<int, int> >::iterator it = std::find(m_emSelE.begin(), m_emSelE.end(), e);
                if (addSel) {
                    if (it != m_emSelE.end()) m_emSelE.erase(it);
                    else m_emSelE.push_back(e);
                } else if (it == m_emSelE.end()) {
                    m_emSelE.clear();
                    m_emSelE.push_back(e);
                }
            }
        } else {                                    // 面模式（按四边形选择）
            int fi = -1;
            if (HitEditFace(point, fi)) {
                hit = true;
                // 把命中的三角扩展为所属四边形的全部三角
                std::vector<int> qt;
                emObj->GetQuadTris(fi, qt);
                if (addSel) {
                    for (size_t q = 0; q < qt.size(); q++) {
                        std::vector<int>::iterator it = std::find(m_emSelF.begin(), m_emSelF.end(), qt[q]);
                        if (it != m_emSelF.end()) m_emSelF.erase(it);
                        else m_emSelF.push_back(qt[q]);
                    }
                } else {
                    // 与点/线模式一致：未选中 → 排他选中；已全选 → 保持不变（再次点击不取消）
                    bool allSel = qt.size() > 0;
                    for (size_t q = 0; q < qt.size(); q++)
                        if (std::find(m_emSelF.begin(), m_emSelF.end(), qt[q]) == m_emSelF.end()) { allSel = false; break; }
                    if (!allSel) {
                        m_emSelF.clear();
                        for (size_t q = 0; q < qt.size(); q++) m_emSelF.push_back(qt[q]);
                    }
                }
            }
        }
        if (hit && addSel) {
            // Ctrl/Shift 加选/减选：只改选择集，不进入拖动，避免误移元素；松手后刷新属性面板
            Invalidate(FALSE);
            m_owner->OnViewTransformChanged();
            return;
        }
        if (hit) {
            // 命中即进入拖动（新选中的元素可立即拖，Blender 式）
            SetCapture();
            m_drag = DRAG_TRANSFORM;
            m_dragMode = TMOVE;
            m_dragObj = emObj;
            m_emDrag = true;
            m_last = point;
            CollectEditDragVerts(m_emDragVerts);
            m_emDragStart.clear();
            m_emDragStart.reserve(m_emDragVerts.size());
            for (size_t i = 0; i < m_emDragVerts.size(); i++)
                m_emDragStart.push_back(emObj->editVerts[m_emDragVerts[i]]);
            m_emInv = emObj->DisplayWorldMatrix().Inverse();
            Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
            Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
            Vec3 fwd = Normalize(target - eye);
            Vec3 center = emObj->DisplayWorldMatrix().TransformPoint(Vec3(0, 0, 0));
            if (!RayPlane(eye, ScreenRay(point.x, point.y), fwd, center, m_dragPlanePoint))
                m_dragPlanePoint = center;
            m_owner->PushUndo();
        } else if (!addSel) {
            ClearEditSelection();
        }
        Invalidate(FALSE);
        return;
    }
    // 平移视角：使用“平移视角”工具，或按住 Shift 再左键拖动
    if (m_owner->GetTool() == CMainWnd::TOOL_PAN || (nFlags & MK_SHIFT)) {
        SetCapture();
        m_drag = DRAG_PAN;
        m_last = point;
        return;
    }
    // 选择工具下，点中选中曲线/车削/挤出的黄色锚点 → 拖动锚点调整轮廓（不整条变黄）
    Object3D* selNow = m_owner->GetSelected();
    if (tool == CMainWnd::TOOL_SELECT && selNow &&
        (selNow->type == PT_SPLINE || selNow->type == PT_LATHE || selNow->type == PT_EXTRUDE) &&
        !selNow->locked && !m_owner->Scene().LayerLocked(selNow)) {
        int idx = -1;
        if (HitControlPoint(point, selNow, idx)) {
            SetCapture();
            m_drag = DRAG_TRANSFORM;
            m_dragMode = TMOVE;
            m_dragObj = selNow;
            m_anchorIdx = idx;
            m_last = point;
            Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
            Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
            Vec3 fwd = Normalize(target - eye);
            m_dragPlanePoint = selNow->DisplayWorldMatrix().TransformPoint(selNow->ctrlPts[idx]);
            m_owner->PushUndo();
            return;
        }
    }
    // 多选统一外锚框：选中数 >1 时共用一个外包围盒（角点=整体等比缩放、面中点=整体单轴缩放），
    // 不再触发单个元件的锚点缩放；点中锚框以外仍走整体移动
    if (m_owner->SelectionCount() > 1 && !m_owner->Scene().editMeshObj &&
        (tool == CMainWnd::TOOL_SELECT || tool == CMainWnd::TOOL_SCALE)) {
        Vec3 mN, mX;
        if (MultiSelectionAABB(mN, mX)) {
            int corner = -1, face = -1;
            if (HitMultiCorner(point, mN, mX, corner)) {
                SetCapture();
                m_drag = DRAG_TRANSFORM;
                m_dragMode = TSCALE;
                m_multiMode = true;
                m_multiAxis = -1;               // 等比
                m_dragStartPt = point;
                BeginMultiScaleData(Vec3((mN.x + mX.x) * 0.5, (mN.y + mX.y) * 0.5, (mN.z + mX.z) * 0.5));
                m_last = point;
                m_owner->PushUndo();
                return;
            }
            if (HitMultiFace(point, mN, mX, face)) {
                SetCapture();
                m_drag = DRAG_TRANSFORM;
                m_dragMode = TSCALE;
                m_multiMode = true;
                m_multiAxis = face / 2;         // 0=x 1=y 2=z（单轴）
                m_dragStartPt = point;
                BeginMultiScaleData(Vec3((mN.x + mX.x) * 0.5, (mN.y + mX.y) * 0.5, (mN.z + mX.z) * 0.5));
                m_last = point;
                m_owner->PushUndo();
                return;
            }
        }
    }
    // 选择/缩放工具下，点中选中图元/组合元件的包围盒角点锚点 → 拖动调整大小（非均匀缩放）；多选时走上面的统一外锚框
    Object3D* selNow2 = m_owner->GetSelected();
    if (m_owner->SelectionCount() <= 1 &&
        (tool == CMainWnd::TOOL_SELECT || tool == CMainWnd::TOOL_SCALE) && selNow2 &&
        selNow2->type != PT_SPLINE && selNow2->type != PT_LATHE && selNow2->type != PT_EXTRUDE &&
        !selNow2->locked && !m_owner->Scene().LayerLocked(selNow2)) {
        int corner = -1;
        if (HitAABBCorner(point, selNow2, corner)) {
            Vec3 mnL, mxL;
            bool okBox = (selNow2->type == PT_NONE) ?
                Object3D::GroupWorldAABB(selNow2, mnL, mxL) :
                Object3D::PrimitiveLocalBounds(selNow2, mnL, mxL);
            if (okBox) {
                SetCapture();
                m_drag = DRAG_TRANSFORM;
                m_dragMode = TMOVE;
                m_dragObj = selNow2;
                m_cornerIndex = corner;
                m_dragStartPt = point;
                m_cornerInv = selNow2->DisplayWorldMatrix().Inverse();
                m_cornerBaseH = Vec3(mxL.x, mxL.y, mxL.z);
                m_cornerLowestY = selNow2->dy + mnL.y * selNow2->sy;   // 缩放原点=当前最低点
                m_startScale[0] = selNow2->sx; m_startScale[1] = selNow2->sy; m_startScale[2] = selNow2->sz;
                m_last = point;
                Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
                Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
                Vec3 fwd = Normalize(target - eye);
                Vec3 center = selNow2->DisplayWorldMatrix().TransformPoint(Vec3(0, 0, 0));
                m_cornerCenter = center;
                if (RayPlane(eye, ScreenRay(point.x, point.y), fwd, center, m_dragPlanePoint)) {
                    m_owner->PushUndo();
                } else {
                    m_cornerIndex = -1;
                    m_drag = DRAG_NONE;
                }
                return;
            }
        }
    }
    // 选择/缩放工具下，点中选中图元/组合元件的蓝色面中点锚点 → 单轴压扁/拉伸（圆柱拖顶面只变高、拖侧面只变粗细）
    Object3D* selNow3 = m_owner->GetSelected();
    if (m_owner->SelectionCount() <= 1 &&
        (tool == CMainWnd::TOOL_SELECT || tool == CMainWnd::TOOL_SCALE) && selNow3 &&
        selNow3->type != PT_SPLINE && selNow3->type != PT_LATHE && selNow3->type != PT_EXTRUDE &&
        !selNow3->locked && !m_owner->Scene().LayerLocked(selNow3)) {
        int face = -1;
        if (HitAABBFace(point, selNow3, face)) {
            Vec3 mnL, mxL;
            bool okBox = (selNow3->type == PT_NONE) ?
                Object3D::GroupWorldAABB(selNow3, mnL, mxL) :
                Object3D::PrimitiveLocalBounds(selNow3, mnL, mxL);
            if (okBox) {
                SetCapture();
                m_drag = DRAG_TRANSFORM;
                m_dragMode = TMOVE;
                m_dragObj = selNow3;
                m_faceIndex = face;
                m_dragStartPt = point;
                m_cornerInv = selNow3->DisplayWorldMatrix().Inverse();
                m_cornerBaseH = Vec3(mxL.x, mxL.y, mxL.z);
                m_cornerLowestY = selNow3->dy + mnL.y * selNow3->sy;   // 缩放原点=当前最低点
                m_startScale[0] = selNow3->sx; m_startScale[1] = selNow3->sy; m_startScale[2] = selNow3->sz;
                m_last = point;
                Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
                Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
                Vec3 fwd = Normalize(target - eye);
                Vec3 center = selNow3->DisplayWorldMatrix().TransformPoint(Vec3(0, 0, 0));
                m_cornerCenter = center;
                if (RayPlane(eye, ScreenRay(point.x, point.y), fwd, center, m_dragPlanePoint)) {
                    m_owner->PushUndo();
                } else {
                    m_faceIndex = -1;
                    m_drag = DRAG_NONE;
                }
                return;
            }
        }
    }
    SetCapture();
    m_last = point;
    tool = m_owner->GetTool();
    Object3D* picked = PickAt(point.x, point.y);
    // 框选：选择工具 + 空白处按下（未点中对象、非网格编辑态、非曲线绘制）→ 拉出选框；
    // 位移超过阈值才框选，单击空白仍保持原“清空选中”行为（在 OnLButtonUp 统一处理）
    if (tool == CMainWnd::TOOL_SELECT && !picked &&
        !m_owner->Scene().editMeshObj && !m_splineObj) {
        m_drag = DRAG_MARQUEE;
        m_marqueeStart = point;
        m_marqueeCur = point;
        m_marqueeShift = (nFlags & MK_SHIFT) != 0;
        return;
    }
    // 变换工具（移动/旋转/缩放）下：空白处保留当前选中，直接拖动已选对象（不因空白按下清空选中，
    // 否则拖不动已选对象，且后续撤销会因选中状态被破坏而崩溃）；只有点中其他对象才切换选中。
    // 选择工具下：空白处清空选中（原行为）。
    bool toolXform = (tool == CMainWnd::TOOL_MOVE || tool == CMainWnd::TOOL_ROTATE || tool == CMainWnd::TOOL_SCALE);
    if (picked) {
        // 点击的对象已在多选集合中 → 保留多选、仅切换主选中（否则 SelectObject 会清空多选集合，
        // 导致 Ctrl+A 全选后无法整组拖动）；点击新对象 → 单选
        if (m_owner->InSelection(picked)) {
            if (m_owner->GetSelected() != picked)
                m_owner->AddToSelection(picked, true);
        } else {
            m_owner->SelectObject(picked);
        }
    } else if (!toolXform)
        m_owner->SelectObject(NULL);
    Object3D* sel = m_owner->GetSelected();
    // 可拖动的条件：移动/旋转/缩放工具下选中任意对象；
    // 选择工具下，点中“当前已选中”的对象也直接拖动（像 Scratch 拖角色）。
    // 但无论哪种工具，只有“当前编辑层级的直接子级”才能被拖动，
    // 否则（如场景根层却选中了组内的深层子元件）禁止拖动，避免误拖子元件。
    bool editable = sel && m_owner->Scene().IsDirectEditChild(sel) && !sel->locked && !m_owner->Scene().LayerLocked(sel);
    bool canDrag = false;
    if (tool == CMainWnd::TOOL_MOVE)      { canDrag = (editable); m_dragMode = TMOVE; }
    else if (tool == CMainWnd::TOOL_ROTATE){ canDrag = (editable); m_dragMode = TROTATE; }
    else if (tool == CMainWnd::TOOL_SCALE) { canDrag = (editable); m_dragMode = TSCALE; }
    else if (tool == CMainWnd::TOOL_SELECT && picked && picked == sel && editable) {
        canDrag = true; m_dragMode = TMOVE;
    }
    if (canDrag && sel) {
        m_drag = DRAG_TRANSFORM;
        m_dragObj = sel;
        m_dragStartPt = point;
        m_dragObjStartLoc = Vec3(sel->x, sel->y, sel->z);
        // 多选拖动：若当前有多选且主选中在集合内，其余选中对象随主选中一起平移（同编辑层级直接子级）
        m_selDragObjs.clear();
        m_selDragStart.clear();
        {
            std::vector<Object3D*> selSet;
            m_owner->GetSelectionSet(selSet);
            for (size_t i = 0; i < selSet.size(); i++) {
                Object3D* o = selSet[i];
                if (!o || o == sel) continue;
                m_selDragObjs.push_back(o);
                m_selDragStart.push_back(Vec3(o->x, o->y, o->z));
            }
        }
        Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
        Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
        Vec3 fwd = Normalize(target - eye);
        Vec3 worldPos = sel->DisplayWorldMatrix().TransformPoint(Vec3(0, 0, 0));
        if (RayPlane(eye, ScreenRay(point.x, point.y), fwd, worldPos, m_dragPlanePoint)) {
            m_dragObjWorldPos = worldPos;
            m_dragStartDist = sqrt((double)((point.x - m_dragStartPt.x) * (point.x - m_dragStartPt.x) +
                                           (point.y - m_dragStartPt.y) * (point.y - m_dragStartPt.y)));
            m_startScale[0] = sel->sx; m_startScale[1] = sel->sy; m_startScale[2] = sel->sz;
            m_rotStartRx = sel->rx; m_rotStartRy = sel->ry; m_rotStartRz = sel->rz;
            m_owner->PushUndo();
        } else {
            m_drag = DRAG_NONE;
        }
    }
}

void CViewportWnd::OnDragUpdate(int x, int y) {
    Object3D* sel = m_dragObj;
    if (!sel || !m_owner) return;
    // 兜底：只允许拖动“当前编辑层级的直接子级”，否则即使误设了子元件也不移动
    if (!m_owner->Scene().IsDirectEditChild(sel)) return;
    if (m_dragMode == TMOVE) {
        Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
        Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
        Vec3 fwd = Normalize(target - eye);
        Vec3 p;
        if (RayPlane(eye, ScreenRay(x, y), fwd, m_dragPlanePoint, p)) {
            Vec3 delta = p - m_dragPlanePoint;
            Vec3 newWorld = m_dragObjWorldPos + delta;
            Vec3 loc;
            if (sel->parent) {
                Mat4 invP = sel->parent->DisplayWorldMatrix().Inverse();
                loc = invP.TransformPoint(newWorld);
            } else {
                loc = newWorld;
            }
            sel->x = loc.x; sel->y = loc.y; sel->z = loc.z;
            // 多选一起平移：其余选中对象相对各自起点施加与主选中相同的局部位移
            Vec3 dLoc = loc - m_dragObjStartLoc;
            for (size_t i = 0; i < m_selDragObjs.size() && i < m_selDragStart.size(); i++) {
                Object3D* o = m_selDragObjs[i];
                if (!o) continue;
                o->x = m_selDragStart[i].x + dLoc.x;
                o->y = m_selDragStart[i].y + dLoc.y;
                o->z = m_selDragStart[i].z + dLoc.z;
            }
        }
    } else if (m_dragMode == TROTATE) {
        int dx = x - m_last.x;
        int dy = y - m_last.y;
        sel->ry += dx * 0.6;
        sel->rx += dy * 0.6;
        // 组（movieclip）整体旋转：补偿直接子件自转，成员只绕组原点公转、自身角度不变
        if (sel->type == PT_NONE && !sel->children.empty())
            sel->CompensateChildRotations(Mat4::RotationXYZ(m_rotStartRx, m_rotStartRy, m_rotStartRz));
    } else if (m_dragMode == TSCALE) {
        // 线性位移驱动：按鼠标到对象中心投影距离的增量线性缩放，单击(位移0)不变、拖动全程平滑。
        // 修复原实现：m_dragStartDist 按下时恒为 0（起点即自身）被兜底成 1.0 → f=原始像素距离，
        // 拖几像素即放大数倍、上限 20 更失控（“缩放变很快很大”）。
        double f = 1.0;
        int cxp, cyp;
        if (ProjectPoint(m_dragObjWorldPos, cxp, cyp)) {
            double startDist = sqrt((double)((m_dragStartPt.x - cxp) * (m_dragStartPt.x - cxp) +
                                            (m_dragStartPt.y - cyp) * (m_dragStartPt.y - cyp)));
            double curDist = sqrt((double)((x - cxp) * (x - cxp) + (y - cyp) * (y - cyp)));
            double delta = curDist - startDist;   // 向外拖为正(放大)，向中心拖为负(缩小)
            double ref = startDist; if (ref < 40.0) ref = 40.0;
            f = 1.0 + delta / ref * 0.5;
        }
        if (f < 0.05) f = 0.05;
        if (f > 5) f = 5;
        sel->sx = m_startScale[0] * f;
        sel->sy = m_startScale[1] * f;
        sel->sz = m_startScale[2] * f;
    }
    m_last = CPoint(x, y);
    m_owner->OnViewTransformChanged(true);   // 拖动中：只更新场景，不刷新文本（避免闪烁）
}

void CViewportWnd::OnMouseMove(UINT nFlags, CPoint point) {
    if (m_splineDrawing && m_splineObj) {
        UpdateSplineDraw(point);
        return;
    }
    if (m_drag == DRAG_MARQUEE) {
        m_marqueeCur = point;
        Invalidate(FALSE);
        return;
    }
    if (m_drag == DRAG_TRANSFORM && m_anchorIdx >= 0 && m_dragObj &&
        (m_dragObj->type == PT_SPLINE || m_dragObj->type == PT_LATHE || m_dragObj->type == PT_EXTRUDE)) {
        // 拖动黄色锚点调整曲线曲度/车削母线/挤出截面
        Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
        Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
        Vec3 fwd = Normalize(target - eye);
        Vec3 p;
        if (RayPlane(eye, ScreenRay(point.x, point.y), fwd, m_dragPlanePoint, p)) {
            if (m_anchorIdx < (int)m_dragObj->ctrlPts.size()) {
                Mat4 inv = m_dragObj->DisplayWorldMatrix().Inverse();
                m_dragObj->ctrlPts[m_anchorIdx] = inv.TransformPoint(p);
            }
        }
        m_last = point;
        Invalidate(FALSE);
        return;
    }
    if (m_drag == DRAG_TRANSFORM && m_emExtrude && m_dragObj) {
        // 挤出交互：鼠标移动投影到平均法线轴，新顶盖沿轴平移（深度），无需按住鼠标
        double t = ExtrudeAxisDepth(point) - m_emExtrudeT0;
        for (size_t i = 0; i < m_emDragVerts.size() && i < m_emDragStart.size(); i++) {
            int vi = m_emDragVerts[i];
            if (vi >= 0 && vi < (int)m_dragObj->editVerts.size())
                m_dragObj->editVerts[vi] = m_emDragStart[i] + m_emExtrudeN * t;
        }
        m_last = point;
        m_owner->OnViewTransformChanged(true);
        Invalidate(FALSE);
        return;
    }
    if (m_drag == DRAG_TRANSFORM && m_emDrag && m_dragObj) {
        // 拖动编辑网格顶点：相机平面位移 → 世界位移 → 对象局部位移（就地改 editVerts）
        Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
        Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
        Vec3 fwd = Normalize(target - eye);
        Vec3 p;
        if (RayPlane(eye, ScreenRay(point.x, point.y), fwd, m_dragPlanePoint, p)) {
            Mat4 inv = m_dragObj->DisplayWorldMatrix().Inverse();
            Vec3 dLocal = inv.TransformPoint(p) - inv.TransformPoint(m_dragPlanePoint);
            for (size_t i = 0; i < m_emDragVerts.size() && i < m_emDragStart.size(); i++) {
                int vi = m_emDragVerts[i];
                if (vi >= 0 && vi < (int)m_dragObj->editVerts.size())
                    m_dragObj->editVerts[vi] = m_emDragStart[i] + dLocal;
            }
        }
        m_last = point;
        m_owner->OnViewTransformChanged(true);   // 拖动中：只更新场景，不刷新文本（避免闪烁）
        Invalidate(FALSE);
        return;
    }
    if (m_drag == DRAG_TRANSFORM && m_dragObj && m_dragObj->type != PT_SPLINE && m_faceIndex >= 0) {
        // 拖动蓝色面中点锚点 → 只缩放该轴（单轴压扁/拉伸），其他轴不变，避免意外压成平面
        Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
        Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
        Vec3 fwd = Normalize(target - eye);
        Vec3 p;
        if (RayPlane(eye, ScreenRay(point.x, point.y), fwd, m_cornerCenter, p)) {
            Vec3 loc = m_cornerInv.TransformPoint(p);
            int axis = m_faceIndex / 2;          // 0=x 1=y 2=z
            // 线性位移驱动：沿“锚点到中心方向”的像素位移增量线性缩放，单击不变、拖动平滑
            double ns = m_startScale[axis];
            int cxp, cyp;
            if (ProjectPoint(m_cornerCenter, cxp, cyp)) {
                double sx0 = m_dragStartPt.x - cxp, sy0 = m_dragStartPt.y - cyp;
                double bl = std::sqrt(sx0 * sx0 + sy0 * sy0);   // 按下时锚点到中心的像素距离
                double ref = bl; if (ref < 30.0) ref = 30.0;
                double dirx = (bl > 1.0) ? sx0 / bl : 0.0;
                double diry = (bl > 1.0) ? sy0 / bl : 0.0;
                double dx = point.x - m_dragStartPt.x, dy = point.y - m_dragStartPt.y;
                double proj = dx * dirx + dy * diry;            // 沿锚点方向位移，向外为正
                ns = m_startScale[axis] + proj / ref * 0.5;
                if (ns < 0.05) ns = 0.05;
                if (ns > 20) ns = 20;
            }
            if (axis == 0) m_dragObj->sx = ns;
            else if (axis == 1) m_dragObj->sy = ns;
            else m_dragObj->sz = ns;
            // 以拖动起点时的最低点为缩放原点：保持最低点不动（在水平面上则向上长，埋入地下则以最低点为锚）
            Vec3 mnL, mxL;
            if (Object3D::PrimitiveLocalBounds(m_dragObj, mnL, mxL)) {
                m_dragObj->dy = m_cornerLowestY - mnL.y * m_dragObj->sy;
                m_dragObj->y = m_dragObj->dy;
            }
        }
        m_last = point;
        m_owner->OnViewTransformChanged(true);   // 拖动中：只更新场景，不刷新文本（避免闪烁）
        return;
    }
    if (m_drag == DRAG_TRANSFORM && m_multiMode && !m_selObjs.empty()) {
        // 多选统一外锚框整体缩放：等比(m_multiAxis<0) / 单轴(m_multiAxis=0/1/2)。
        // 每个选中对象的世界中心绕“外框中心”缩放，scale 同乘系数 → 如同组合元件整体缩放
        double f = 1.0;
        int cxp, cyp;
        if (ProjectPoint(m_multiCenter, cxp, cyp)) {
            double startDist = std::sqrt((double)((m_dragStartPt.x - cxp) * (m_dragStartPt.x - cxp) +
                                                  (m_dragStartPt.y - cyp) * (m_dragStartPt.y - cyp)));
            double curDist = std::sqrt((double)((point.x - cxp) * (point.x - cxp) +
                                                (point.y - cyp) * (point.y - cyp)));
            double delta = curDist - startDist;      // 向外拖为正(放大)，向中心拖为负(缩小)
            double ref = startDist; if (ref < 40.0) ref = 40.0;
            f = 1.0 + delta / ref * 0.5;
        }
        if (f < 0.05) f = 0.05;
        if (f > 5) f = 5;
        if (m_multiAxis < 0) {
            for (size_t i = 0; i < m_selObjs.size(); i++) {
                Object3D* o = m_selObjs[i];
                if (!o) continue;
                const Vec3& c = m_multiCenter;
                const Vec3& sw = m_selStartWorld[i];
                SetObjWorldPos(o, Vec3(c.x + (sw.x - c.x) * f, c.y + (sw.y - c.y) * f, c.z + (sw.z - c.z) * f));
                o->sx = m_selStartScale[i].x * f;
                o->sy = m_selStartScale[i].y * f;
                o->sz = m_selStartScale[i].z * f;
            }
        } else {
            int ax = m_multiAxis;
            for (size_t i = 0; i < m_selObjs.size(); i++) {
                Object3D* o = m_selObjs[i];
                if (!o) continue;
                Vec3 sw = m_selStartWorld[i];
                Vec3 nw = sw;
                if (ax == 0) nw.x = m_multiCenter.x + (sw.x - m_multiCenter.x) * f;
                else if (ax == 1) nw.y = m_multiCenter.y + (sw.y - m_multiCenter.y) * f;
                else nw.z = m_multiCenter.z + (sw.z - m_multiCenter.z) * f;
                SetObjWorldPos(o, nw);
                if (ax == 0) o->sx = m_selStartScale[i].x * f;
                else if (ax == 1) o->sy = m_selStartScale[i].y * f;
                else o->sz = m_selStartScale[i].z * f;
            }
        }
        m_last = point;
        m_owner->OnViewTransformChanged(true);   // 拖动中：只更新场景，不刷新文本（避免闪烁）
        return;
    }
    if (m_drag == DRAG_TRANSFORM && m_dragObj && m_dragObj->type != PT_SPLINE && m_cornerIndex >= 0) {
        // 拖动角点锚点 → 整体等比缩放（压扁/拉伸请用蓝色面中点锚点），
        // 且从起点按约 20% 灵敏度缓变，避免单击/微动时急剧缩放
        Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
        Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
        Vec3 fwd = Normalize(target - eye);
        Vec3 p;
        if (RayPlane(eye, ScreenRay(point.x, point.y), fwd, m_cornerCenter, p)) {
            // 线性位移驱动：按鼠标到中心距离的增量线性缩放，单击(位移0)不变，拖动全程平滑
            double f = 1.0;
            int cxp, cyp;
            if (ProjectPoint(m_cornerCenter, cxp, cyp)) {
                double startDist = std::sqrt((double)((m_dragStartPt.x - cxp) * (m_dragStartPt.x - cxp) +
                                                      (m_dragStartPt.y - cyp) * (m_dragStartPt.y - cyp)));
                double curDist = std::sqrt((double)((point.x - cxp) * (point.x - cxp) +
                                                    (point.y - cyp) * (point.y - cyp)));
                double delta = curDist - startDist;      // 向外拖为正(放大)，向中心拖为负(缩小)
                double ref = startDist; if (ref < 40.0) ref = 40.0;
                f = 1.0 + delta / ref * 0.5;
            }
            if (f < 0.05) f = 0.05;
            if (f > 5) f = 5;
            m_dragObj->sx = m_startScale[0] * f;
            m_dragObj->sy = m_startScale[1] * f;
            m_dragObj->sz = m_startScale[2] * f;
            // 以拖动起点时的最低点为缩放原点：保持最低点不动（在水平面上则向上长，埋入地下则以最低点为锚）
            Vec3 mnL, mxL;
            if (Object3D::PrimitiveLocalBounds(m_dragObj, mnL, mxL)) {
                m_dragObj->dy = m_cornerLowestY - mnL.y * m_dragObj->sy;
                m_dragObj->y = m_dragObj->dy;
            }
        }
        m_last = point;
        m_owner->OnViewTransformChanged(true);   // 拖动中：只更新场景，不刷新文本（避免闪烁）
        return;
    }
    if (m_drag == DRAG_TRANSFORM) {
        OnDragUpdate(point.x, point.y);
        return;
    }
    if (m_drag == DRAG_ORBIT) {
        Camera& c = m_owner->Scene().cam;
        c.yaw -= (point.x - m_last.x) * 0.4;
        c.pitch += (point.y - m_last.y) * 0.4;
        if (c.pitch > 89) c.pitch = 89;
        if (c.pitch < -89) c.pitch = -89;
        m_last = point;
        Invalidate(FALSE);
        return;
    }
    if (m_drag == DRAG_PAN) {
        Camera& c = m_owner->Scene().cam;
        Vec3 eye = Renderer::CameraEye(c);
        Vec3 target(c.tx, c.ty, c.tz);
        Vec3 fwd = Normalize(target - eye);
        Vec3 right = Normalize(Cross(fwd, Vec3(0, 1, 0)));
        Vec3 up = Cross(right, fwd);
        double k = c.dist / 600.0;
        int dx = point.x - m_last.x;
        int dy = point.y - m_last.y;
        c.tx += (-right.x * dx - up.x * dy) * k;
        c.ty += (-right.y * dx - up.y * dy) * k;
        c.tz += (-right.z * dx - up.z * dy) * k;
        m_last = point;
        Invalidate(FALSE);
    }
}

void CViewportWnd::OnLButtonUp(UINT nFlags, CPoint point) {
    if (m_drag == DRAG_MARQUEE) {
        // 位移超过阈值 = 框选：选中选框内对象（普通=替换选中；Shift=追加/反选）
        bool moved = (std::abs(point.x - m_marqueeStart.x) + std::abs(point.y - m_marqueeStart.y) > 6);
        if (moved) {
            CRect rc((m_marqueeStart.x < m_marqueeCur.x) ? m_marqueeStart.x : m_marqueeCur.x,
                     (m_marqueeStart.y < m_marqueeCur.y) ? m_marqueeStart.y : m_marqueeCur.y,
                     (m_marqueeStart.x > m_marqueeCur.x) ? m_marqueeStart.x : m_marqueeCur.x,
                     (m_marqueeStart.y > m_marqueeCur.y) ? m_marqueeStart.y : m_marqueeCur.y);
            std::vector<Object3D*> kids;
            m_owner->Scene().EditChildren(kids);
            std::vector<Object3D*> hits;
            for (size_t i = 0; i < kids.size(); i++) {
                Object3D* o = kids[i];
                if (!o || o->locked || m_owner->Scene().LayerLocked(o)) continue;
                Vec3 wc = o->DisplayWorldMatrix().TransformPoint(Vec3(0, 0, 0));
                int sx, sy;
                if (!ProjectPoint(wc, sx, sy)) continue;   // 投影失败（在相机背后）不参与
                if (rc.PtInRect(CPoint(sx, sy))) hits.push_back(o);
            }
            if (!hits.empty()) {
                if (m_marqueeShift) {
                    // Shift 追加/反选：在选中集中的移除，不在的加入；一次性提交避免逐条刷新闪烁
                    std::vector<Object3D*> selSet;
                    m_owner->GetSelectionSet(selSet);
                    for (size_t i = 0; i < hits.size(); i++) {
                        bool in = false;
                        for (size_t j = 0; j < selSet.size(); j++)
                            if (selSet[j] == hits[i]) { in = true; break; }
                        if (in) {
                            for (size_t j = 0; j < selSet.size(); j++)
                                if (selSet[j] == hits[i]) { selSet.erase(selSet.begin() + j); break; }
                        } else {
                            selSet.push_back(hits[i]);
                        }
                    }
                    Object3D* primary = m_owner->GetSelected();
                    if (!primary && !selSet.empty()) primary = selSet[0];
                    m_owner->SetSelectionSet(selSet, primary);
                } else {
                    m_owner->SetSelectionSet(hits, hits[0]);   // 批量替换：只重建一次场景树
                }
                m_owner->OnViewTransformChanged();   // 刷新属性面板/状态栏
            } else if (!m_marqueeShift) {
                m_owner->SelectObject(NULL);         // 框到空白：清空选中
            }
        } else {
            m_owner->SelectObject(NULL);             // 单击空白：清空选中（原行为）
        }
        m_drag = DRAG_NONE;
        ReleaseCapture();
        Invalidate(FALSE);
        return;
    }
    if (m_splineDrawing) {
        // 单击（按下未拖）：转入锚点会话继续加锚点；拖拽：生成弧线并结束
        if (abs(point.x - m_splineDown.x) + abs(point.y - m_splineDown.y) <= 6) {
            m_splineDrawing = false;
            ReleaseCapture();
            return;
        }
        FinishSpline();
        return;
    }
    if (m_drag == DRAG_TRANSFORM && m_dragObj) {
        // 操作记录：拖拽变换完成（移动/旋转/缩放）
        const wchar_t* opName = L"移动";
        if (m_dragMode == TROTATE) opName = L"旋转";
        else if (m_dragMode == TSCALE) opName = L"缩放";
        
        m_dragObj = NULL;
    }
    m_anchorIdx = -1;
    m_emDrag = false;
    m_emDragVerts.clear();
    m_emDragStart.clear();
    m_cornerIndex = -1;
    m_faceIndex = -1;
    m_multiMode = false;
    m_multiAxis = -1;
    if (!m_selObjs.empty()) {
        
    }
    m_selObjs.clear();
    m_selStartWorld.clear();
    m_selStartScale.clear();
    m_drag = DRAG_NONE;
    ReleaseCapture();
    m_owner->OnViewTransformChanged();   // 拖动结束：完整刷新属性面板/状态栏一次，显示最终值
}

// 双击：命中组→进入该元件内部编辑；空白→退回上一层（Flash 式元件导航）
void CViewportWnd::OnLButtonDblClk(UINT nFlags, CPoint point) {
    SetFocus();
    // 曲线锚点会话：双击结束，生成保留全部控制点的曲线
    if (m_splineObj && !m_splineDrawing) {
        FinishSpline();
        return;
    }
    if (HitNav(point)) return;
    Object3D* hit = PickAt(point.x, point.y);
    if (hit && hit->type == PT_NONE && hit->HasChildren())
        m_owner->EnterGroup(hit);
    else
        m_owner->ExitGroup();
}

// 顶部 Flash 式编辑导航栏：← 返回 + “场景1” + 各级元件选项卡，当前层高亮
void CViewportWnd::DrawNavBar(CDC& dc, int w) {
    m_navReturn = CRect(0, 0, 0, 0);
    m_navRoot = CRect(0, 0, 0, 0);
    m_navItems.clear();
    dc.FillSolidRect(0, 0, w, m_navH, RGB(235, 238, 242));
    bool inside = m_owner && m_owner->Scene().InsideGroup();
    int x = 6;
    int y = (m_navH - 20) / 2;
    if (inside) {
        CString s = _T("←");
        CSize sz = dc.GetTextExtent(s);
        int wd = sz.cx + 14;
        CRect rc(x, y, x + wd, y + 20);
        dc.SetTextColor(RGB(0, 0, 0));
        dc.TextOut(x + 5, y + 2, s);
        m_navReturn = rc;
        x += wd;
    }
    {
        CString s = _T("场景1");
        CSize sz = dc.GetTextExtent(s);
        int wd = sz.cx + 16;
        CRect rc(x, y, x + wd, y + 20);
        dc.SetTextColor(RGB(0, 0, 0));
        if (inside) dc.TextOut(x + 8, y + 2, s);   // 根层是当前层：加粗显示，不画底色
        else { CFont* cF = dc.GetCurrentFont(); LOGFONT lf; cF->GetLogFont(&lf); lf.lfWeight = FW_BOLD;
               CFont bf; bf.CreateFontIndirect(&lf); CFont* oF = dc.SelectObject(&bf);
               dc.TextOut(x + 8, y + 2, s); dc.SelectObject(oF); }
        m_navRoot = rc;
        x += wd;
    }
    if (m_owner) {
        std::vector<Object3D*> chain = m_owner->EditPath();
        Object3D* cur = m_owner->Scene().editRoot;
        for (size_t i = 0; i < chain.size(); i++) {
            CString s = chain[i]->name.c_str();
            CSize sz = dc.GetTextExtent(s);
            int wd = sz.cx + 16;
            CRect rc(x, y, x + wd, y + 20);
            bool curThis = (chain[i] == cur);
            dc.SetTextColor(RGB(0, 0, 0));
            if (curThis) { CFont* cF = dc.GetCurrentFont(); LOGFONT lf; cF->GetLogFont(&lf); lf.lfWeight = FW_BOLD;
                           CFont bf; bf.CreateFontIndirect(&lf); CFont* oF = dc.SelectObject(&bf);
                           dc.TextOut(x + 8, y + 2, s); dc.SelectObject(oF); }
            else dc.TextOut(x + 8, y + 2, s);
            m_navItems.push_back(std::make_pair(rc, chain[i]));
            x += wd;
        }
    }
}

// 点击导航条：返回/进入指定层级。返回 true 表示已处理（不再走选择）
bool CViewportWnd::HitNav(CPoint pt) {
    if (!m_owner || pt.y >= m_navH) return false;
    if (m_owner->Scene().InsideGroup() && m_navReturn.PtInRect(pt)) {
        m_owner->ExitGroup();
        return true;
    }
    if (m_navRoot.PtInRect(pt)) {
        m_owner->SetEditLevel(NULL);
        return true;
    }
    for (size_t i = 0; i < m_navItems.size(); i++) {
        if (m_navItems[i].first.PtInRect(pt)) {
            m_owner->SetEditLevel(m_navItems[i].second);
            return true;
        }
    }
    return false;
}

void CViewportWnd::OnRButtonDown(UINT nFlags, CPoint point) {
    SetFocus();
    // 挤出交互中：右键=取消（深度归零，保留平面顶盖）
    if (m_emExtrude) {
        Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
        if (o) for (size_t i = 0; i < m_emDragVerts.size() && i < m_emDragStart.size(); i++)
            o->editVerts[m_emDragVerts[i]] = m_emDragStart[i];
        m_emExtrude = false;
        m_emDrag = false;
        m_drag = DRAG_NONE;
        m_dragObj = NULL;
        m_emDragVerts.clear();
        m_emDragStart.clear();
        ReleaseCapture();
        BuildEditEdges();
        m_owner->OnViewTransformChanged();
        Invalidate(FALSE);
        return;
    }
    // 框选进行中按右键：取消框选，避免与旋转视角冲突
    if (m_drag == DRAG_MARQUEE) {
        m_drag = DRAG_NONE;
        ReleaseCapture();
        Invalidate(FALSE);
        return;
    }
    SetCapture();
    m_drag = DRAG_ORBIT;
    m_last = point;
    m_rDown = point;
}

void CViewportWnd::OnRButtonUp(UINT nFlags, CPoint point) {
    bool wasClick = (std::abs(point.x - m_rDown.x) < 4 && std::abs(point.y - m_rDown.y) < 4);
    m_drag = DRAG_NONE;
    ReleaseCapture();
    // 右键点击（未拖动）→ 弹出“转换为 MovieClip”等操作菜单
    if (wasClick && m_owner) {
        CPoint sp = point;
        ClientToScreen(&sp);
        m_owner->ShowObjectMenu(sp);
    }
}

void CViewportWnd::OnMButtonDown(UINT nFlags, CPoint point) {
    SetCapture();
    m_drag = DRAG_PAN;
    m_last = point;
}

void CViewportWnd::OnMButtonUp(UINT nFlags, CPoint point) {
    m_drag = DRAG_NONE;
    ReleaseCapture();
}

BOOL CViewportWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
    Camera& c = m_owner->Scene().cam;
    double f = (zDelta > 0) ? 0.88 : 1.14;
    c.dist *= f;
    if (c.dist < 2.0) c.dist = 2.0;
    if (c.dist > 400.0) c.dist = 400.0;
    Invalidate(FALSE);
    return TRUE;
}

void CViewportWnd::Nudge(int key) {
    // 多选一起微调：收集当前选中集合（主选中 + Ctrl 多选）
    std::vector<Object3D*> items;
    m_owner->GetSelectionSet(items);
    if (items.empty()) return;
    // 过滤不可编辑的（锁定/非直接子级/锁定图层）
    std::vector<Object3D*> movable;
    for (size_t i = 0; i < items.size(); i++) {
        Object3D* o = items[i];
        if (o && m_owner->Scene().IsDirectEditChild(o) && !o->locked && !m_owner->Scene().LayerLocked(o))
            movable.push_back(o);
    }
    if (movable.empty()) return;
    m_owner->PushUndo();
    double dx = 0, dy = 0, dz = 0;
    switch (key) {
        case VK_LEFT: dx = -0.5; break;
        case VK_RIGHT: dx = 0.5; break;
        case VK_UP: dz = -0.5; break;
        case VK_DOWN: dz = 0.5; break;
        case VK_PRIOR: dy = 0.5; break;   // PageUp
        case VK_NEXT: dy = -0.5; break;    // PageDown
        default: return;
    }
    for (size_t i = 0; i < movable.size(); i++) {
        movable[i]->x += dx;
        movable[i]->y += dy;
        movable[i]->z += dz;
    }
    m_owner->OnViewTransformChanged();
}

void CViewportWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    // 网格编辑态：Del=删除选中元素，Esc=退出编辑，1/2/3=切点/线/面模式，E=面挤出
    if (m_owner && m_owner->Scene().editMeshObj) {
        // 挤出交互中：Esc 取消（深度归零，保留平面顶盖），左键确认在 OnLButtonDown
        if (m_emExtrude && nChar == VK_ESCAPE) {
            Object3D* exo = m_owner->Scene().editMeshObj;
            if (exo) for (size_t i = 0; i < m_emDragVerts.size() && i < m_emDragStart.size(); i++)
                exo->editVerts[m_emDragVerts[i]] = m_emDragStart[i];
            m_emExtrude = false;
            m_emDrag = false;
            m_drag = DRAG_NONE;
            m_emDragVerts.clear();
            m_emDragStart.clear();
            ReleaseCapture();
            BuildEditEdges();
            m_owner->OnViewTransformChanged();
            Invalidate(FALSE);
            return;
        }
        switch (nChar) {
            case VK_DELETE: m_owner->DeleteEditSelection(); return;
            case VK_ESCAPE: m_owner->ExitMeshEdit(); return;
            case 'E': if (m_emMode == 2 && !m_emExtrude) ExtrudeEditSelection(); return;
            case '1': case '2': case '3': {
                int nm = nChar - '1';
                if (nm != m_emMode) m_owner->SetMeshEditMode(nm);   // 统一走刷新（含按钮显隐）
                return;
            }
        }
    }
    // 曲线锚点会话：Enter 结束 / Esc 取消
    if (m_splineObj && !m_splineDrawing) {
        if (nChar == VK_RETURN) { FinishSpline(); return; }
        if (nChar == VK_ESCAPE) { CancelSpline(); return; }
    }
    switch (nChar) {
        case VK_DELETE: m_owner->DoDelete(); return;
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_PRIOR: case VK_NEXT:
            if (!ctrl) { Nudge(nChar); return; }
            break;
    }
    if (ctrl) {
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        switch (nChar) {
            case 'A': m_owner->SelectAllObjects(); return;
            case 'Z': if (shift) m_owner->DoRedo(); else m_owner->DoUndo(); return;
            case 'Y': m_owner->DoRedo(); return;
            case 'D': m_owner->DoDuplicate(); return;
            case 'S': m_owner->DoSave(); return;
            case 'O': m_owner->DoOpen(); return;
            case 'N': m_owner->DoNew(); return;
        }
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CViewportWnd::OnSetFocus(CWnd* pOldWnd) {
    CWnd::OnSetFocus(pOldWnd);
}

// ------------------------------------------------------------ 曲线（画柳条）交互
// 世界坐标 → 屏幕坐标（与 ScreenRay 互逆；编辑态用正交投影）
bool CViewportWnd::ProjectPoint(const Vec3& world, int& sx, int& sy) const {
    Camera& c = m_owner->Scene().cam;
    Vec3 eye = Renderer::CameraEye(c);
    Vec3 target(c.tx, c.ty, c.tz);
    Vec3 fwd = Normalize(target - eye);
    Vec3 right = Normalize(Cross(fwd, Vec3(0, 1, 0)));
    Vec3 up = Cross(right, fwd);
    Vec3 v = world - eye;
    double z = Dot(v, fwd);
    if (z <= 0.01) return false;
    double x = Dot(v, right);
    double y = Dot(v, up);
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);
    double aspect = m_h > 0 ? (double)m_w / (double)m_h : 1.0;
    sx = (int)((x / (z * tanF * aspect) + 1.0) * 0.5 * m_w);
    sy = (int)((0.5 - 0.5 * y / (z * tanF)) * m_h);   // ndc∈[-1,1] → 屏幕，0.5 系数不能少
    return true;
}

// 画曲线工具：按住左键，起点落在相机前方平面
void CViewportWnd::StartSplineDraw(CPoint pt) {
    if (!m_owner) return;
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
    Vec3 fwd = Normalize(target - eye);
    Vec3 p;
    // 画在“过注视点 target 的相机前方平面”上，避免落在焦平面（z≈0）导致锚点难以点选
    if (!RayPlane(eye, ScreenRay(pt.x, pt.y), fwd, target, p)) return;
    SetCapture();
    Object3D* parent = m_owner->Scene().editRoot;
    if (!parent && m_owner->GetSelected() && m_owner->GetSelected()->type == PT_NONE)
        parent = m_owner->GetSelected();
    Object3D* o = m_owner->Scene().AddObject(PT_SPLINE, m_owner->Scene().AutoName(PT_SPLINE), parent);
    // 关键帧语义：新曲线入当前帧关键帧（与按钮添加图元一致），
    // 否则该层已有（空）关键帧时曲线创建后不显示，表现为"曲线无法添加"
    if (o && !o->parent) m_owner->Scene().AddNewObjectToCurrentKey(o);
    o->ctrlPts.clear();
    o->ctrlPts.push_back(p);
    m_owner->PushUndo();
    m_owner->SelectObject(o);   // 会话中即选中：已添加的锚点（黄色浮点）实时可见
    m_splineObj = o;
    m_splineDrawing = true;
    m_last = pt;
    Invalidate(FALSE);
}

// 拖动中：终点跟随鼠标
void CViewportWnd::UpdateSplineDraw(CPoint pt) {
    if (!m_splineObj) return;
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
    Vec3 fwd = Normalize(target - eye);
    Vec3 p;
    if (RayPlane(eye, ScreenRay(pt.x, pt.y), fwd, target, p)) {
        if (m_splineObj->ctrlPts.size() >= 2) m_splineObj->ctrlPts.pop_back();
        m_splineObj->ctrlPts.push_back(p);
    }
    m_last = pt;
    Invalidate(FALSE);
}

// 结束曲线：拖拽模式在两端之间插入中点控制点（带下垂弧度），
// 单击锚点模式（双击/回车结束）保留全部控制点（多控制点曲线）。结束后选中该曲线以便拖锚点调曲度。
void CViewportWnd::FinishSpline() {
    Object3D* o = m_splineObj;
    bool wasDrag = m_splineDrawing;
    m_splineDrawing = false;
    m_splineObj = NULL;
    if (o && o->ctrlPts.size() < 2) {
        m_owner->Scene().DeleteObject(o);
        m_owner->RefreshAll();
        ReleaseCapture();
        return;
    }
    if (o && o->ctrlPts.size() >= 2 && wasDrag) {
        Vec3 a = o->ctrlPts.front();
        Vec3 b = o->ctrlPts.back();
        Vec3 mid = (a + b) * 0.5;
        mid.y += 0.4;   // 中点略抬高，形成下垂的柳条弧线
        o->ctrlPts.insert(o->ctrlPts.begin() + 1, mid);
    }
    if (m_owner) {
        m_owner->SetTool(CMainWnd::TOOL_SELECT);
        if (o) m_owner->SelectObject(o);
    }
    ReleaseCapture();
}

// 锚点会话：单击追加一个控制点（在“过注视点 target 的相机前方平面”上，与起点一致）
void CViewportWnd::AddSplineAnchor(CPoint pt) {
    if (!m_splineObj) return;
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 target(m_owner->Scene().cam.tx, m_owner->Scene().cam.ty, m_owner->Scene().cam.tz);
    Vec3 fwd = Normalize(target - eye);
    Vec3 p;
    if (!RayPlane(eye, ScreenRay(pt.x, pt.y), fwd, target, p)) return;
    m_splineObj->ctrlPts.push_back(p);
    m_last = pt;
    Invalidate(FALSE);
}

// Esc：取消曲线会话，删除未完成的曲线对象
void CViewportWnd::CancelSpline() {
    if (m_splineObj) m_owner->Scene().DeleteObject(m_splineObj);
    m_splineObj = NULL;
    m_splineDrawing = false;
    m_owner->RefreshAll();
    ReleaseCapture();
}

// 命中选中曲线/车削/挤出的某个黄色锚点（屏幕距离阈值约 12 像素）
bool CViewportWnd::HitControlPoint(CPoint pt, Object3D* spline, int& idx) const {
    if (!spline || (spline->type != PT_SPLINE && spline->type != PT_LATHE && spline->type != PT_EXTRUDE) ||
        spline->ctrlPts.empty()) return false;
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 ray = ScreenRay(pt.x, pt.y);
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);
    double bestPix = 1e18;
    int bestIdx = -1;
    for (size_t i = 0; i < spline->ctrlPts.size(); i++) {
        Vec3 w = spline->DisplayWorldMatrix().TransformPoint(spline->ctrlPts[i]);
        Vec3 e2w = w - eye;
        double t = Dot(e2w, ray);
        if (t < 0) continue;                 // 眼睛后方，跳过
        Vec3 closest = eye + ray * t;
        Vec3 diff = w - closest;
        double dist = std::sqrt(Dot(diff, diff));
        // 把世界距离换算成近似屏幕像素（横向/垂向单位像素一致，aspect 抵消）
        double pxPerUnit = (double)m_h / (t * tanF);
        double pix = dist * pxPerUnit;
        if (pix < bestPix) { bestPix = pix; bestIdx = (int)i; }
    }
    if (bestIdx < 0 || bestPix > 20.0) return false;
    idx = bestIdx;
    return true;
}

// 命中选中图元的包围盒角点锚点（8 个角点中最近的一个，射线距离换算成屏幕像素）。
// 组合元件（PT_NONE）用世界包围盒直接命中，与渲染一致
bool CViewportWnd::HitAABBCorner(CPoint pt, Object3D* o, int& corner) const {
    if (!o) return false;
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 ray = ScreenRay(pt.x, pt.y);
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);
    if (o->type == PT_NONE) {
        Vec3 mn, mx;
        if (!Object3D::GroupWorldAABB(o, mn, mx)) return false;
        double bestPix = 1e18;
        int bestIdx = -1;
        for (int c = 0; c < 8; c++) {
            Vec3 w((c & 1) ? mx.x : mn.x, (c & 2) ? mx.y : mn.y, (c & 4) ? mx.z : mn.z);
            Vec3 e2w = w - eye;
            double t = Dot(e2w, ray);
            if (t < 0) continue;
            Vec3 closest = eye + ray * t;
            Vec3 diff = w - closest;
            double dist = std::sqrt(Dot(diff, diff));
            double pxPerUnit = (double)m_h / (t * tanF);
            double pix = dist * pxPerUnit;
            if (pix < bestPix) { bestPix = pix; bestIdx = c; }
        }
        if (bestIdx < 0 || bestPix > 20.0) return false;
        corner = bestIdx;
        return true;
    }
    if (!Object3D::IsPrimitive(o->type)) return false;
    Vec3 mnL, mxL;
    if (!Object3D::PrimitiveLocalBounds(o, mnL, mxL)) return false;
    Mat4 wm = o->DisplayWorldMatrix();
    double bestPix = 1e18;
    int bestIdx = -1;
    for (int c = 0; c < 8; c++) {
        double cx = (c & 1) ? mxL.x : mnL.x;
        double cy = (c & 2) ? mxL.y : mnL.y;
        double cz = (c & 4) ? mxL.z : mnL.z;
        Vec3 w = wm.TransformPoint(Vec3(cx, cy, cz));
        Vec3 e2w = w - eye;
        double t = Dot(e2w, ray);
        if (t < 0) continue;
        Vec3 closest = eye + ray * t;
        Vec3 diff = w - closest;
        double dist = std::sqrt(Dot(diff, diff));
        double pxPerUnit = (double)m_h / (t * tanF);
        double pix = dist * pxPerUnit;
        if (pix < bestPix) { bestPix = pix; bestIdx = c; }
    }
    if (bestIdx < 0 || bestPix > 20.0) return false;
    corner = bestIdx;
    return true;
}

// 命中选中图元的包围盒面中点锚点（6 个面中最近的一个，射线距离换算成屏幕像素）。
// 组合元件（PT_NONE）用世界包围盒直接命中
bool CViewportWnd::HitAABBFace(CPoint pt, Object3D* o, int& face) const {
    if (!o) return false;
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 ray = ScreenRay(pt.x, pt.y);
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);
    if (o->type == PT_NONE) {
        Vec3 mn, mx;
        if (!Object3D::GroupWorldAABB(o, mn, mx)) return false;
        double bestPix = 1e18;
        int bestIdx = -1;
        for (int f = 0; f < 6; f++) {
            Vec3 w((f == 0) ? mx.x : (f == 1 ? mn.x : (mn.x + mx.x) * 0.5),
                   (f == 2) ? mx.y : (f == 3 ? mn.y : (mn.y + mx.y) * 0.5),
                   (f == 4) ? mx.z : (f == 5 ? mn.z : (mn.z + mx.z) * 0.5));
            Vec3 e2w = w - eye;
            double t = Dot(e2w, ray);
            if (t < 0) continue;
            Vec3 closest = eye + ray * t;
            Vec3 diff = w - closest;
            double dist = std::sqrt(Dot(diff, diff));
            double pxPerUnit = (double)m_h / (t * tanF);
            double pix = dist * pxPerUnit;
            if (pix < bestPix) { bestPix = pix; bestIdx = f; }
        }
        if (bestIdx < 0 || bestPix > 20.0) return false;
        face = bestIdx;
        return true;
    }
    if (!Object3D::IsPrimitive(o->type)) return false;
    Vec3 mnL, mxL;
    if (!Object3D::PrimitiveLocalBounds(o, mnL, mxL)) return false;
    Mat4 wm = o->DisplayWorldMatrix();
    double bestPix = 1e18;
    int bestIdx = -1;
    for (int f = 0; f < 6; f++) {
        double fx = (f == 0) ? mxL.x : (f == 1 ? mnL.x : 0.0);
        double fy = (f == 2) ? mxL.y : (f == 3 ? mnL.y : 0.0);
        double fz = (f == 4) ? mxL.z : (f == 5 ? mnL.z : 0.0);
        Vec3 w = wm.TransformPoint(Vec3(fx, fy, fz));
        Vec3 e2w = w - eye;
        double t = Dot(e2w, ray);
        if (t < 0) continue;
        Vec3 closest = eye + ray * t;
        Vec3 diff = w - closest;
        double dist = std::sqrt(Dot(diff, diff));
        double pxPerUnit = (double)m_h / (t * tanF);
        double pix = dist * pxPerUnit;
        if (pix < bestPix) { bestPix = pix; bestIdx = f; }
    }
    if (bestIdx < 0 || bestPix > 20.0) return false;
    face = bestIdx;
    return true;
}

// 对象世界包围盒（图元用本地包围盒+显示矩阵变换；组用 GroupWorldAABB）——多选外框用
static bool ObjWorldAABB(const Object3D* o, Vec3& mn, Vec3& mx) {
    if (!o) return false;
    if (Object3D::IsPrimitive(o->type)) {
        Vec3 mnL, mxL;
        if (!Object3D::PrimitiveLocalBounds(o, mnL, mxL)) return false;
        Mat4 wm = o->DisplayWorldMatrix();
        bool any = false;
        for (int i = 0; i < 8; i++) {
            Vec3 p(mnL.x, mnL.y, mnL.z);
            if (i & 1) p.x = mxL.x;
            if (i & 2) p.y = mxL.y;
            if (i & 4) p.z = mxL.z;
            Vec3 w = wm.TransformPoint(p);
            if (!any) { mn = mx = w; any = true; }
            else {
                if (w.x < mn.x) mn.x = w.x; if (w.x > mx.x) mx.x = w.x;
                if (w.y < mn.y) mn.y = w.y; if (w.y > mx.y) mx.y = w.y;
                if (w.z < mn.z) mn.z = w.z; if (w.z > mx.z) mx.z = w.z;
            }
        }
        return any;
    }
    return Object3D::GroupWorldAABB(o, mn, mx);
}

// 多选外包围盒：当前选中对象世界包围盒的并集（选中数 >1 时由调用方使用）
bool CViewportWnd::MultiSelectionAABB(Vec3& mn, Vec3& mx) const {
    if (!m_owner) return false;
    std::vector<Object3D*> selSet;
    m_owner->GetSelectionSet(selSet);
    bool any = false;
    mn = Vec3(1e18, 1e18, 1e18);
    mx = Vec3(-1e18, -1e18, -1e18);
    for (size_t i = 0; i < selSet.size(); i++) {
        Vec3 b0, b1;
        if (!ObjWorldAABB(selSet[i], b0, b1)) continue;
        if (!any) { mn = b0; mx = b1; any = true; }
        else {
            if (b0.x < mn.x) mn.x = b0.x; if (b1.x > mx.x) mx.x = b1.x;
            if (b0.y < mn.y) mn.y = b0.y; if (b1.y > mx.y) mx.y = b1.y;
            if (b0.z < mn.z) mn.z = b0.z; if (b1.z > mx.z) mx.z = b1.z;
        }
    }
    return any;
}

// 命中多选外框的 8 角点（世界 AABB，与渲染同基准）
bool CViewportWnd::HitMultiCorner(CPoint pt, const Vec3& mn, const Vec3& mx, int& corner) const {
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 ray = ScreenRay(pt.x, pt.y);
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);
    double bestPix = 1e18;
    int bestIdx = -1;
    for (int c = 0; c < 8; c++) {
        Vec3 w((c & 1) ? mx.x : mn.x, (c & 2) ? mx.y : mn.y, (c & 4) ? mx.z : mn.z);
        Vec3 e2w = w - eye;
        double t = Dot(e2w, ray);
        if (t < 0) continue;
        Vec3 closest = eye + ray * t;
        Vec3 diff = w - closest;
        double dist = std::sqrt(Dot(diff, diff));
        double pxPerUnit = (double)m_h / (t * tanF);
        double pix = dist * pxPerUnit;
        if (pix < bestPix) { bestPix = pix; bestIdx = c; }
    }
    if (bestIdx < 0 || bestPix > 20.0) return false;
    corner = bestIdx;
    return true;
}

// 命中多选外框的 6 面中点（世界 AABB）
bool CViewportWnd::HitMultiFace(CPoint pt, const Vec3& mn, const Vec3& mx, int& face) const {
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 ray = ScreenRay(pt.x, pt.y);
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);
    double bestPix = 1e18;
    int bestIdx = -1;
    for (int f = 0; f < 6; f++) {
        Vec3 w((f == 0) ? mx.x : (f == 1 ? mn.x : (mn.x + mx.x) * 0.5),
               (f == 2) ? mx.y : (f == 3 ? mn.y : (mn.y + mx.y) * 0.5),
               (f == 4) ? mx.z : (f == 5 ? mn.z : (mn.z + mx.z) * 0.5));
        Vec3 e2w = w - eye;
        double t = Dot(e2w, ray);
        if (t < 0) continue;
        Vec3 closest = eye + ray * t;
        Vec3 diff = w - closest;
        double dist = std::sqrt(Dot(diff, diff));
        double pxPerUnit = (double)m_h / (t * tanF);
        double pix = dist * pxPerUnit;
        if (pix < bestPix) { bestPix = pix; bestIdx = f; }
    }
    if (bestIdx < 0 || bestPix > 20.0) return false;
    face = bestIdx;
    return true;
}

// 把对象世界中心设置到指定世界坐标（父级矩阵逆变换回局部坐标）
void CViewportWnd::SetObjWorldPos(Object3D* o, const Vec3& pw) {
    if (!o) return;
    Mat4 wp = o->parent ? o->parent->WorldMatrix() : Mat4();
    Vec3 lp = wp.Inverse().TransformPoint(pw);
    o->x = lp.x; o->y = lp.y; o->z = lp.z;
}

// 开始多选整体缩放：记录全部选中对象起点（世界中心+scale）与外框中心
void CViewportWnd::BeginMultiScaleData(const Vec3& center) {
    m_selObjs.clear();
    m_selStartWorld.clear();
    m_selStartScale.clear();
    if (!m_owner) return;
    std::vector<Object3D*> selSet;
    m_owner->GetSelectionSet(selSet);
    for (size_t i = 0; i < selSet.size(); i++) {
        Object3D* o = selSet[i];
        if (!o) continue;
        m_selObjs.push_back(o);
        Vec3 wc = o->DisplayWorldMatrix().TransformPoint(Vec3(0, 0, 0));
        m_selStartWorld.push_back(wc);
        m_selStartScale.push_back(Vec3(o->sx, o->sy, o->sz));
    }
    m_multiCenter = center;
}

// ------------------------------------------------------------ 网格编辑（点/线/面）
// 由 editTris 建唯一边表（顶点对 a<b），进入编辑/删除元素后调用
void CViewportWnd::BuildEditEdges() {
    m_emEdges.clear();
    Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
    if (!o || o->editTris.empty()) return;
    size_t nv = o->editVerts.size();
    std::map<long long, bool> seen;
    for (size_t t = 0; t + 2 < o->editTris.size(); t += 3) {
        for (int j = 0; j < 3; j++) {
            int a = o->editTris[t + j], b = o->editTris[t + (j + 1) % 3];
            if (a == b) continue;
            long long key = (a < b) ? (long long)a * (long long)nv + b : (long long)b * (long long)nv + a;
            if (seen.find(key) != seen.end()) continue;
            if (o->IsDiagEdge(a, b)) continue;   // 跳过三角剖分斜线，只保留网格线
            seen[key] = true;
            m_emEdges.push_back((a < b) ? std::make_pair(a, b) : std::make_pair(b, a));
        }
    }
}

// 命中编辑网格顶点（射线距离换算屏幕像素，阈值 20；返回 editVerts 下标）
bool CViewportWnd::HitEditVertex(CPoint pt, int& vi) const {
    vi = -1;
    Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
    if (!o || o->editVerts.empty()) return false;
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 ray = ScreenRay(pt.x, pt.y);
    Mat4 wm = o->DisplayWorldMatrix();
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);
    double bestPix = 20.0;
    int best = -1;
    for (size_t i = 0; i < o->editVerts.size(); i++) {
        Vec3 w = wm.TransformPoint(o->editVerts[i]);
        Vec3 e2w = w - eye;
        double t = Dot(e2w, ray);
        if (t < 0) continue;
        Vec3 closest = eye + ray * t;
        Vec3 diff = w - closest;
        double dist = std::sqrt(Dot(diff, diff));
        double pxPerUnit = (double)m_h / (t * tanF);
        double pix = dist * pxPerUnit;
        if (pix <= bestPix) { bestPix = pix; best = (int)i; }
    }
    if (best < 0) return false;
    vi = best;
    return true;
}

// 命中编辑网格边（两端点投影成屏幕线段，点线距离阈值 12 像素；返回 m_emEdges 下标）
// 在阈值内优先选离相机最近（深度最小）的边，避免背面的边在屏幕上投影更近而被误选
bool CViewportWnd::HitEditEdge(CPoint pt, int& ei) const {
    ei = -1;
    Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
    if (!o || m_emEdges.empty()) return false;
    Mat4 wm = o->DisplayWorldMatrix();
    Camera& c = m_owner->Scene().cam;
    Vec3 eye = Renderer::CameraEye(c);
    Vec3 dir = ScreenRay(pt.x, pt.y);
    Vec3 fwd = Normalize(Vec3(c.tx, c.ty, c.tz) - eye);
    double tanF = std::tan(45.0 * kDeg2Rad * 0.5);

    // 射线与网格最近交点的射线参数（仿射变换保持参数值，= 世界距离）。
    // 用于遮挡剔除：最近点明显在正面交点之后的边（背面边）不参与拾取。
    double tHit = 1e18;
    if (!o->editTris.empty()) {
        Mat4 inv = wm.Inverse();
        Vec3 org = inv.TransformPoint(eye);
        Vec3 dirL = inv.TransformPoint(eye + dir) - org;
        for (size_t t = 0; t + 2 < o->editTris.size(); t += 3) {
            double hitT;
            if (RayTriangleHit(org, dirL, o->editVerts[o->editTris[t]], o->editVerts[o->editTris[t + 1]],
                               o->editVerts[o->editTris[t + 2]], hitT)) {
                if (hitT > 0 && hitT < tHit) tHit = hitT;
            }
        }
    }

    double bestDist = 14.0;
    double bestDepth = 1e18;
    int bestIdx = -1;
    for (size_t i = 0; i < m_emEdges.size(); i++) {
        Vec3 w1 = wm.TransformPoint(o->editVerts[m_emEdges[i].first]);
        Vec3 w2 = wm.TransformPoint(o->editVerts[m_emEdges[i].second]);
        // 射线与线段最近点（sRay = 射线参数，u = 线段参数）
        Vec3 d(w2.x - w1.x, w2.y - w1.y, w2.z - w1.z);
        Vec3 W(eye.x - w1.x, eye.y - w1.y, eye.z - w1.z);
        double b = Dot(dir, d), cc = Dot(d, d), e = Dot(dir, W), f = Dot(d, W);
        double sRay, u;
        double denom = cc - b * b;
        if (denom < 1e-12) { u = 0.0; sRay = -e; }
        else {
            u = (f - e * b) / denom;
            if (u < 0) u = 0; if (u > 1) u = 1;
            sRay = u * b - e;
            if (sRay < 0) { sRay = 0; u = cc > 1e-12 ? f / cc : 0.0; if (u < 0) u = 0; if (u > 1) u = 1; }
        }
        if (sRay < 0.01) continue;
        // 遮挡：边最近点比正面交点还靠后（超过约 18px 的深度容差）→ 背面边，跳过
        if (tHit < 1e17) {
            double slack = (2.0 * sRay * tanF / (double)m_h) * 18.0;
            if (sRay > tHit + slack) continue;
        }
        int sx1, sy1, sx2, sy2;
        if (!ProjectPoint(w1, sx1, sy1)) continue;
        if (!ProjectPoint(w2, sx2, sy2)) continue;
        double dx = (double)(sx2 - sx1), dy = (double)(sy2 - sy1);
        double len2 = dx * dx + dy * dy;
        double uu = (len2 < 1e-9) ? 0.0 : (((double)pt.x - sx1) * dx + ((double)pt.y - sy1) * dy) / len2;
        if (uu < 0) uu = 0;
        if (uu > 1) uu = 1;
        double px = sx1 + uu * dx - pt.x;
        double py = sy1 + uu * dy - pt.y;
        double dist = std::sqrt(px * px + py * py);
        if (dist > 14.0) continue;
        // 边中点到相机的视线深度（沿 fwd 方向），越小越靠前
        Vec3 mid((w1.x + w2.x) * 0.5, (w1.y + w2.y) * 0.5, (w1.z + w2.z) * 0.5);
        double depth = Dot(mid - eye, fwd);
        // 屏幕距离主导：背面边已被上面的遮挡剔除过滤，深度只在距离几乎相同
        // （1px 内，如共顶点的几条边）时作决胜，避免旁边一条边抢走点击目标
        if (bestIdx < 0 || dist < bestDist - 1.0 ||
            (dist <= bestDist + 1.0 && depth < bestDepth)) {
            bestDist = dist; bestDepth = depth; bestIdx = (int)i;
        }
    }
    if (bestIdx < 0) return false;
    ei = bestIdx;
    return true;
}

// 命中编辑网格三角面（局部射线求交，取视线最近者；返回 editTris 中的三角序号）
bool CViewportWnd::HitEditFace(CPoint pt, int& fi) const {
    fi = -1;
    Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
    if (!o || o->editTris.empty()) return false;
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 dir = ScreenRay(pt.x, pt.y);
    Mat4 inv = o->DisplayWorldMatrix().Inverse();
    Vec3 org = inv.TransformPoint(eye);
    Vec3 dirL = inv.TransformPoint(eye + dir) - org;
    double bestT = 1e18;
    int best = -1;
    for (size_t t = 0; t + 2 < o->editTris.size(); t += 3) {
        double hitT;
        if (RayTriangleHit(org, dirL, o->editVerts[o->editTris[t]], o->editVerts[o->editTris[t + 1]],
                           o->editVerts[o->editTris[t + 2]], hitT)) {
            if (hitT > 0 && hitT < bestT) { bestT = hitT; best = (int)(t / 3); }
        }
    }
    if (best < 0) return false;
    fi = best;
    return true;
}

// 当前选中元素涉及的顶点（去重）：点=选中顶点；线=边两端点；面=三角三顶点
void CViewportWnd::CollectEditDragVerts(std::vector<int>& out) const {
    out.clear();
    std::map<int, bool> seen;
    if (m_emMode == 0) {
        for (size_t i = 0; i < m_emSelV.size(); i++) seen[m_emSelV[i]] = true;
    } else if (m_emMode == 1) {
        for (size_t i = 0; i < m_emSelE.size(); i++) {
            seen[m_emSelE[i].first] = true;
            seen[m_emSelE[i].second] = true;
        }
    } else {
        Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
        if (!o) return;
        for (size_t i = 0; i < m_emSelF.size(); i++) {
            int t = m_emSelF[i];
            if (t < 0 || t >= (int)o->editTris.size() / 3) continue;
            for (int j = 0; j < 3; j++) seen[o->editTris[t * 3 + j]] = true;
        }
    }
    for (std::map<int, bool>::iterator it = seen.begin(); it != seen.end(); ++it)
        out.push_back(it->first);
}

// 删除选中元素：点模式删顶点（含关联三角）；线模式删共享该边的三角；面模式删选中三角。
// 删除后重建边表并清空选择（顶点删除会重排索引）
void CViewportWnd::DeleteEditSelection() {
    Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
    if (!o || o->editVerts.empty()) return;
    if (m_emMode == 0 && !m_emSelV.empty()) {
        m_owner->PushUndo();
        o->DeleteEditVerts(m_emSelV);
    } else if (m_emMode == 1 && !m_emSelE.empty()) {
        m_owner->PushUndo();
        std::vector<int> kill;
        for (size_t i = 0; i < m_emSelE.size(); i++) {
            int a = m_emSelE[i].first, b = m_emSelE[i].second;
            for (size_t t = 0; t + 2 < o->editTris.size(); t += 3) {
                int c0 = o->editTris[t], c1 = o->editTris[t + 1], c2 = o->editTris[t + 2];
                bool hasA = (c0 == a || c1 == a || c2 == a);
                bool hasB = (c0 == b || c1 == b || c2 == b);
                if (hasA && hasB) kill.push_back((int)(t / 3));
            }
        }
        o->DeleteEditTris(kill);
    } else if (m_emMode == 2 && !m_emSelF.empty()) {
        m_owner->PushUndo();
        o->DeleteEditTris(m_emSelF);
    } else {
        return;
    }
    ClearEditSelection();
    BuildEditEdges();
    m_owner->OnViewTransformChanged();
    Invalidate(FALSE);
}

// 鼠标点到“挤出法线轴”的最近点参数（局部坐标，沿轴有符号距离）；射线与轴平行时退回相机平面
double CViewportWnd::ExtrudeAxisDepth(CPoint point) const {
    Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
    if (!o) return 0.0;
    Mat4 inv = o->DisplayWorldMatrix().Inverse();
    Vec3 eye = Renderer::CameraEye(m_owner->Scene().cam);
    Vec3 dirW = ScreenRay(point.x, point.y);
    Vec3 ol = inv.TransformPoint(eye);
    Vec3 dl = inv.TransformPoint(eye + dirW) - ol;
    if (Length(dl) > 1e-12) dl = Normalize(dl);
    Vec3 n = m_emExtrudeN;
    Vec3 r = ol - m_emExtrudeC;
    double a = Dot(dl, dl), b = Dot(dl, n), c = Dot(n, n);
    double e1 = -Dot(dl, r), e2 = -Dot(n, r);
    double det = a * c - b * b;
    if (fabs(det) > 1e-8) return (b * e1 - a * e2) / det;   // 射线与法线轴最近点的沿轴参数
    return Dot(-r, n);
}

// 面模式挤出：生成顶盖与侧壁。fixedDepth=0 → 进入沿法线轴的鼠标拖动（单击确认）；
// fixedDepth 非 0 → 直接按该深度一次挤出（属性面板输入，可负=向内）。
void CViewportWnd::ExtrudeEditSelection(double fixedDepth) {
    Object3D* o = m_owner ? m_owner->Scene().editMeshObj : NULL;
    if (!o || !o->HasEditMesh() || m_emMode != 2 || m_emSelF.empty()) return;
    std::vector<int> selF = m_emSelF;
    std::vector<int> newTopVerts, newTopFaces;
    Vec3 avgN;
    m_owner->PushUndo();
    o->ExtrudeEditFaces(selF, newTopVerts, newTopFaces, avgN);
    if (newTopVerts.empty() || newTopFaces.empty()) return;

    // 选中新顶盖（保持面模式），并记录新顶盖顶点起点（0 偏移时的局部坐标）
    m_emSelV.clear();
    m_emSelE.clear();
    m_emSelF = newTopFaces;
    std::vector<Vec3> startPos;
    startPos.reserve(newTopVerts.size());
    Vec3 center(0, 0, 0);
    for (size_t i = 0; i < newTopVerts.size(); i++) {
        startPos.push_back(o->editVerts[newTopVerts[i]]);
        center = center + o->editVerts[newTopVerts[i]];
    }
    if (newTopVerts.size() > 0) center = center * (1.0 / (double)newTopVerts.size());
    if (Length(avgN) < 1e-9) avgN = Vec3(0, 0, 1);

    // 数值输入：直接按深度一次挤出，不进入拖动
    if (fixedDepth != 0.0) {
        Vec3 off = avgN * fixedDepth;
        for (size_t i = 0; i < newTopVerts.size(); i++)
            o->editVerts[newTopVerts[i]] = startPos[i] + off;
        BuildEditEdges();
        m_owner->OnViewTransformChanged();
        Invalidate(FALSE);
        return;
    }

    // 交互：把新顶盖顶点送入法线轴拖动
    m_emDragVerts = newTopVerts;
    m_emDragStart = startPos;
    m_emExtrudeN = avgN;
    m_emExtrudeC = center;
    m_emExtrudeTopF = newTopFaces;
    m_emExtrude = true;
    m_emDrag = true;
    m_drag = DRAG_TRANSFORM;
    m_dragMode = TMOVE;
    m_dragObj = o;
    m_emInv = o->DisplayWorldMatrix().Inverse();

    // 记录当前鼠标在轴上的初始参数，保证挤出从深度 0 开始
    CPoint cur;
    GetCursorPos(&cur);
    ScreenToClient(&cur);
    m_emExtrudeT0 = ExtrudeAxisDepth(cur);

    BuildEditEdges();
    SetCapture();
    m_owner->OnViewTransformChanged();
    Invalidate(FALSE);
}

// 撤销/新建/打开后清理可能已失效的曲线状态与网格编辑态
void CViewportWnd::ValidateSpline() {
    if (m_splineObj && (!m_owner || !m_owner->Scene().ContainsObject(m_splineObj)))
        m_splineObj = NULL;
    m_splineDrawing = false;
    // 网格编辑态失效：撤销/装载后 editMeshObj 指针已不在场景中 → 退出编辑态
    if (m_owner && m_owner->Scene().editMeshObj &&
        !m_owner->Scene().ContainsObject(m_owner->Scene().editMeshObj))
        m_owner->Scene().editMeshObj = NULL;
    if (!m_owner || !m_owner->Scene().editMeshObj) {
        ClearEditSelection();
        m_emEdges.clear();
        m_emDrag = false;
        m_emExtrude = false;
        m_emDragVerts.clear();
        m_emDragStart.clear();
    }
}
