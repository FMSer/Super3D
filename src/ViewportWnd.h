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
// ViewportWnd.h : 3D 视口窗口（OpenGL + 鼠标交互）
#pragma once
#include "stdafx.h"
#include "Scene3D.h"
#include <utility>

class CMainWnd;

class CViewportWnd : public CWnd {
public:
    CViewportWnd();
    virtual ~CViewportWnd();

    BOOL CreateView(CWnd* parent, const CRect& rc);
    void SetOwner(CMainWnd* o) { m_owner = o; }
    CMainWnd* Owner() const { return m_owner; }
    void Repaint() { Invalidate(FALSE); }
    // 撤销/新建/打开后调用，清理可能已失效的曲线绘制状态（friend 以便 MainWnd 调用）
    friend class CMainWnd;
    void ValidateSpline();

protected:
    CMainWnd* m_owner;
    HDC m_hdc;
    HGLRC m_rc;
    int m_w, m_h;

    enum DragMode { DRAG_NONE = 0, DRAG_TRANSFORM, DRAG_ORBIT, DRAG_PAN, DRAG_MARQUEE };
    enum TransformMode { TMOVE = 0, TROTATE, TSCALE };
    int m_drag;
    int m_dragMode;                 // 本次拖动按哪种变换执行（TMOVE/TROTATE/TSCALE）
    CPoint m_last;
    CPoint m_dragStartPt;
    CPoint m_rDown;                 // 右键按下点（用于区分点击与旋转拖动）
    CPoint m_marqueeStart;          // 框选起点（屏幕）
    CPoint m_marqueeCur;            // 框选当前点（屏幕）
    bool m_marqueeShift;            // 框选时是否按住 Shift（追加模式）
    double m_dragStartDist;
    double m_startScale[3];
    Vec3 m_dragPlanePoint;      // 拖拽起点在相机平面上的交点
    Vec3 m_dragObjWorldPos;
    Vec3 m_dragObjStartLoc;     // 主选中拖动起始局部位置（多选一起平移的基准）
    Object3D* m_dragObj;
    std::vector<Object3D*> m_selDragObjs;   // 多选拖动：除主选中外一起移动的对象
    std::vector<Vec3> m_selDragStart;       // 对应各对象的起始局部位置

    // 多选统一外锚框（整体变换，类似组合元件）
    bool m_multiMode;                       // 正在多选外锚框缩放
    int  m_multiAxis;                       // 单轴缩放轴 0/1/2；-1 = 等比缩放
    Vec3 m_multiCenter;                     // 拖动起点外框中心（世界）
    std::vector<Object3D*> m_selObjs;       // 多选变换涉及的全部对象
    std::vector<Vec3> m_selStartWorld;      // 各对象起始世界中心
    std::vector<Vec3> m_selStartScale;      // 各对象起始 scale

    // 曲线（画柳条）交互状态
    Object3D* m_splineObj;       // 正在绘制中的曲线对象
    bool m_splineDrawing;        // 是否正在拖拽绘制曲线
    CPoint m_splineDown;         // 曲线工具按下点（判定单击=加锚点 / 拖拽=画弧线）
    int m_anchorIdx;             // 正在拖动的曲线锚点下标（-1 表示无）

    // 通用图元角点缩放（包围盒 8 角点锚点，拖动等比缩放）
    int m_cornerIndex;           // 正在拖动的角点下标（0..7，-1 无）
    Mat4 m_cornerInv;            // 拖动起点的显示世界矩阵逆
    Vec3 m_cornerBaseH;          // 未缩放局部正半宽（PrimitiveLocalBounds 的 mx）
    Vec3 m_cornerCenter;         // 对象中心世界坐标（等比缩放基准）
    double m_cornerLowestY;      // 拖动起点时组件世界最低点 y（作为缩放原点，保持不动）
    int m_faceIndex;             // 正在拖动的面中点（0..5，-1 无；单轴压扁/拉伸）

    // 网格编辑（Blender 式点/线/面）状态
    int m_emMode;                // 编辑模式：0=点 1=线 2=面
    std::vector<int> m_emSelV;   // 选中顶点（editVerts 下标）
    std::vector<std::pair<int, int>> m_emSelE;   // 选中边（顶点对，a<b）
    std::vector<int> m_emSelF;   // 选中面（editTris 中的三角序号）
    std::vector<std::pair<int, int>> m_emEdges;  // 唯一边表（a<b，进入编辑时构建）
    bool m_emDrag;               // 正在拖动编辑网格元素
    std::vector<int> m_emDragVerts;      // 拖动影响的顶点（去重）
    std::vector<Vec3> m_emDragStart;     // 拖动起点各顶点的局部坐标
    Mat4 m_emInv;                // 拖动起点显示世界矩阵逆（世界→局部）
    // 面挤出（Blender 式 E）：生成后沿平均法线轴拖出深度，单击确认
    bool m_emExtrude;            // 是否处于挤出拖动态（此时无需按住鼠标）
    Vec3 m_emExtrudeN;           // 挤出轴方向（局部单位法线）
    Vec3 m_emExtrudeC;           // 挤出轴上一点（选中区域中心，局部）
    double m_emExtrudeT0;        // 按下 E 时鼠标投影到法线轴的初始参数（深度归零基准）
    std::vector<int> m_emExtrudeTopF;    // 挤出完成后保持选中的新顶盖三角序号
    double m_rotStartRx, m_rotStartRy, m_rotStartRz;  // 旋转拖动起点的组旋转（子件公转补偿用）

    void StartSplineDraw(CPoint pt);
    void AddSplineAnchor(CPoint pt);
    void CancelSpline();
    void UpdateSplineDraw(CPoint pt);
    void FinishSpline();
    bool HitControlPoint(CPoint pt, Object3D* spline, int& idx) const;   // 命中锚点（射线距离）
    bool HitAABBCorner(CPoint pt, Object3D* o, int& corner) const;        // 命中包围盒角点锚点
    bool HitAABBFace(CPoint pt, Object3D* o, int& face) const;            // 命中包围盒面中点锚点
    bool MultiSelectionAABB(Vec3& mn, Vec3& mx) const;                    // 多选外包围盒（世界并集）
    bool HitMultiCorner(CPoint pt, const Vec3& mn, const Vec3& mx, int& corner) const; // 命中多选外框角点
    bool HitMultiFace(CPoint pt, const Vec3& mn, const Vec3& mx, int& face) const;     // 命中多选外框面中点
    void BeginMultiScaleData(const Vec3& center);                         // 记录多选整体缩放起点
    static void SetObjWorldPos(Object3D* o, const Vec3& pw);              // 设置对象世界中心
    // 网格编辑：进入/退出选择集维护与命中
    void BuildEditEdges();                                          // 由 editTris 建唯一边表
    void ClearEditSelection() { m_emSelV.clear(); m_emSelE.clear(); m_emSelF.clear(); }
    bool HitEditVertex(CPoint pt, int& vi) const;                   // 命中顶点（屏幕像素阈值）
    bool HitEditEdge(CPoint pt, int& ei) const;                     // 命中边（m_emEdges 下标）
    bool HitEditFace(CPoint pt, int& fi) const;                     // 命中三角（射线求交最近）
    void CollectEditDragVerts(std::vector<int>& out) const;         // 当前选中元素涉及的顶点（去重）
    void DeleteEditSelection();                                     // 删除选中元素（点→顶点删除，线→关联面删除，面→面删除）
    void ExtrudeEditSelection(double fixedDepth = 0.0);              // 面模式挤出：fixedDepth=0 进入法线拖动；非0 按值一次挤出
    double ExtrudeAxisDepth(CPoint point) const;                    // 鼠标点投影到挤出法线轴的有符号深度
    bool ProjectPoint(const Vec3& world, int& sx, int& sy) const;         // 世界→屏幕（命中用）

    // 顶部 Flash 式编辑导航栏
    int m_navH;                         // 导航条高度
    CRect m_navReturn;                  // 返回箭头区域
    CRect m_navRoot;                    // “场景1”选项卡
    std::vector<std::pair<CRect, Object3D*>> m_navItems;  // 各层级组选项卡

    void DrawNavBar(CDC& dc, int w);
    bool HitNav(CPoint pt);             // 命中导航条并跳转，返回是否已处理

    void SetupGL();
    Vec3 ScreenRay(int x, int y) const;
    bool RayPlane(const Vec3& org, const Vec3& dir, const Vec3& n, const Vec3& planePt, Vec3& out) const;
    Object3D* PickAt(int x, int y);   // 拾取，返回命中的元件/组（组按包围盒）
    void OnPick(int x, int y);
    void OnDragUpdate(int x, int y);
    void Nudge(int key);

    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    DECLARE_MESSAGE_MAP()
};
