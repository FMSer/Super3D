// TimelineWnd.h : Flash 式时间轴控件（滚轮缩放 / 横向滚动 / 无限长）
#pragma once
#include "stdafx.h"
#include "Scene3D.h"

class CMainWnd;

class CTimelineWnd : public CWnd {
public:
    CTimelineWnd();
    void SetOwner(CMainWnd* o) { m_owner = o; }
    BOOL CreateTimeline(CWnd* parent, const CRect& rc);
    void SetSceneChanged() { Invalidate(FALSE); }

protected:
    CMainWnd* m_owner;
    int m_indentW;          // 左侧元件名列宽
    int m_topH;             // 顶部缩放调节条工具行高
    int m_hdrH;             // 标尺行高
    int m_rowH;             // 每行高
    double m_ppf;           // 每帧像素数（缩放级别）
    double m_viewFrame;     // 可视区域最左侧帧（支持小数值）
    int m_rowScroll;        // 垂直滚动：可视区起始行索引（元件行过多时上下滚动）
    bool m_scrubbing;       // 正在标尺上拖拽播放头
    bool m_draggingKey;     // 正在拖拽关键帧菱形
    bool m_panning;         // 正在标尺空白处横向平移
    bool m_draggingZoom;    // 正在拖动右上角缩放调节条
    CPoint m_panStart;      // 平移起点（屏幕）
    double m_panStartView;  // 平移起点时的 m_viewFrame
    int m_dragLayer;        // 正在拖拽图层关键帧的图层索引；-1=无拖拽
    size_t m_dragIdx;       // m_dragLayer 所指图层 keys 内的索引
    CFont m_font;
    CEdit* m_nameEdit;      // 就地编辑图层名的 CEdit；NULL=未编辑
    int m_editLayer;        // 正在编辑图层名的图层索引

    // 可视范围换算
    int FrameFromX(int x) const;
    int XFromFrame(int f) const;
    double VisibleFrames() const;
    // 逻辑内容长度（总帧数/当前帧/最远关键帧 取最大）
    double ContentLength() const;
    // 让播放头移到帧 f 之外时自动扩展时间轴长度
    void EnsureLengthForFrame(int f);
    void UpdateScroll();
    void SetViewFrame(double vf);
    void ZoomBarRect(CRect& r) const;   // 右上角缩放调节条区域
    double PpfFromZoomX(int x) const;   // 调节条 x → 每帧像素
    void DrawZoomBar(CDC& mem) const;   // 绘制调节条
    int DepthOf(const Object3D* o) const;
    static int TextW(const std::wstring& s);   // 估算文本宽度：s.size()*13+4（中文9pt≈12px，取13保守；绘制与命中都用它，保证一致）
    void CollectLayerObjects(int li, std::vector<Object3D*>& out) const;   // 根属于图层 li 的全部元件（含组内子元件）
    void LayoutRootChips(int li, int y0, std::vector<Object3D*>& objs, std::vector<CRect>& rects) const;  // 图层行左侧根元件 chips 布局（与绘制完全一致）

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);   // 双击图层名就地改名
    afx_msg void OnNameEditEnd();                               // 图层名编辑失焦提交
    void StartNameEdit(int li);
    void EndNameEdit();
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    DECLARE_MESSAGE_MAP()
};
