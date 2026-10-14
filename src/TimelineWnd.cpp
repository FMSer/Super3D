// TimelineWnd.cpp : 时间轴实现（Flash 式：滚轮缩放 + 横向滚动 + 无限长）
#include "stdafx.h"
#include "TimelineWnd.h"
#include "MainWnd.h"

#define EDIT_NAME_ID 0x9000   // 就地编辑图层名的 CEdit 控件 ID

BEGIN_MESSAGE_MAP(CTimelineWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_EN_KILLFOCUS(EDIT_NAME_ID, OnNameEditEnd)
    ON_WM_MOUSEMOVE()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEWHEEL()
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
END_MESSAGE_MAP()

CTimelineWnd::CTimelineWnd() : m_owner(NULL), m_indentW(170), m_topH(12), m_hdrH(24), m_rowH(20),
    m_ppf(8.0), m_viewFrame(0.0), m_rowScroll(0), m_scrubbing(false), m_draggingKey(false),
    m_panning(false), m_draggingZoom(false), m_dragLayer(-1), m_dragIdx(0),
    m_nameEdit(NULL), m_editLayer(-1) {}

BOOL CTimelineWnd::CreateTimeline(CWnd* parent, const CRect& rc) {
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, NULL,
        (HBRUSH)(COLOR_WINDOW + 1), NULL);
    BOOL ok = CreateEx(WS_EX_CLIENTEDGE, cls, _T("时间轴"),
        WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_CLIPCHILDREN, rc, parent, 0);
    if (ok) m_font.CreatePointFont(90, _T("微软雅黑"));
    return ok;
}

BOOL CTimelineWnd::OnEraseBkgnd(CDC* pDC) { return TRUE; }

int CTimelineWnd::DepthOf(const Object3D* o) const {
    int d = 0;
    while (o->parent) { d++; o = o->parent; }
    return d;
}

int CTimelineWnd::TextW(const std::wstring& s) { return (int)s.size() * 13 + 4; }

void CTimelineWnd::CollectLayerObjects(int li, std::vector<Object3D*>& out) const {
    out.clear();
    if (!m_owner) return;
    Scene3D& scene = m_owner->Scene();
    std::vector<Object3D*> all;
    scene.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++)
        if (scene.RootLayerOf(all[i]) == li) out.push_back(all[i]);
}

void CTimelineWnd::LayoutRootChips(int li, int y0, std::vector<Object3D*>& objs, std::vector<CRect>& rects) const {
    objs.clear(); rects.clear();
    if (!m_owner) return;
    Scene3D& scene = m_owner->Scene();
    if (li < 0 || li >= (int)scene.layers.size()) return;
    int cx = 42 + TextW(scene.layers[li].name) + 8;   // 图层名之后开始排 chips
    for (size_t i = 0; i < scene.roots.size(); i++) {
        Object3D* r = scene.roots[i];
        if (scene.RootLayerOf(r) != li) continue;
        int tw = TextW(r->name);
        if (cx + 13 + tw > m_indentW - 4) break;      // 超出左列宽度则不再排
        objs.push_back(r);
        rects.push_back(CRect(cx - 2, y0 + 2, cx + 13 + tw, y0 + m_rowH - 2));
        cx += 13 + tw + 6;
    }
}

// 可见性“眼睛”图标：可见=蓝色实心+白瞳孔，隐藏=灰色+斜线
static void DrawEyeIcon(CDC& dc, int cx, int cy, bool visible) {
    COLORREF col = visible ? RGB(40, 90, 200) : RGB(150, 152, 156);
    CBrush b(col);
    CBrush* ob = dc.SelectObject(&b);
    dc.Ellipse(cx - 5, cy - 3, cx + 5, cy + 3);   // 眼体（实心）
    if (visible) {   // 白色瞳孔，更像睁开的眼睛
        CBrush w(RGB(255, 255, 255));
        CBrush* ow = dc.SelectObject(&w);
        dc.Ellipse(cx - 2, cy - 2, cx + 2, cy + 2);
        dc.SelectObject(ow);
    } else {         // 隐藏：灰色斜线
        dc.SelectObject(ob);
        CPen sp(PS_SOLID, 1, col);
        CPen* osp = dc.SelectObject(&sp);
        dc.MoveTo(cx - 5, cy - 3);
        dc.LineTo(cx + 5, cy + 3);
        dc.SelectObject(osp);
    }
    dc.SelectObject(ob);
}

// 锁定“锁”图标：锁定=实心，未锁定=空心（用线条勾勒）
static void DrawLockIcon(CDC& dc, int cx, int cy, bool locked) {
    COLORREF col = locked ? RGB(120, 60, 30) : RGB(160, 162, 166);
    CRect ring(cx - 3, cy - 4, cx + 4, cy + 1);   // 锁环
    CRect body(cx - 4, cy, cx + 5, cy + 5);       // 锁身
    if (locked) {
        dc.FillSolidRect(&ring, col);
        dc.FillSolidRect(&body, col);
    } else {
        CPen pen(PS_SOLID, 1, col);
        CPen* op = dc.SelectObject(&pen);
        dc.MoveTo(ring.left, ring.top);         dc.LineTo(ring.right - 1, ring.top);
        dc.MoveTo(ring.right - 1, ring.top);    dc.LineTo(ring.right - 1, ring.bottom);
        dc.MoveTo(ring.right - 1, ring.bottom); dc.LineTo(ring.left, ring.bottom);
        dc.MoveTo(ring.left, ring.bottom);      dc.LineTo(ring.left, ring.top);
        dc.MoveTo(body.left, body.top);         dc.LineTo(body.right - 1, body.top);
        dc.MoveTo(body.right - 1, body.top);    dc.LineTo(body.right - 1, body.bottom);
        dc.MoveTo(body.right - 1, body.bottom); dc.LineTo(body.left, body.bottom);
        dc.MoveTo(body.left, body.bottom);      dc.LineTo(body.left, body.top);
        dc.SelectObject(op);
    }
}

// 帧 → 像素（基于可视范围）
int CTimelineWnd::XFromFrame(int f) const {
    return m_indentW + (int)((f - m_viewFrame) * m_ppf);
}

// 像素 → 帧（不限上界，允许超出 totalFrames，实现“无限长”）
int CTimelineWnd::FrameFromX(int x) const {
    double f = m_viewFrame + (x - m_indentW) / m_ppf;
    int fi = (int)f;
    if (fi < 0) fi = 0;
    return fi;
}

// 当前可视宽度可容纳的帧数
double CTimelineWnd::VisibleFrames() const {
    CRect rc;
    GetClientRect(&rc);
    int w = rc.Width() - m_indentW;
    if (w < 1) w = 1;
    return w / m_ppf;
}

// 逻辑内容长度：总帧数 / 当前播放头 / 所有关键帧中最远帧，取最大
double CTimelineWnd::ContentLength() const {
    if (!m_owner) return 120.0;
    Scene3D& scene = m_owner->Scene();
    double L = scene.totalFrames;
    if (scene.frame + 1 > L) L = scene.frame + 1;
    for (size_t i = 0; i < scene.layers.size(); i++) {
        const std::vector<LayerKey>& ks = scene.layers[i].keys;
        for (size_t k = 0; k < ks.size(); k++)
            if (ks[k].frame + 1 > L) L = ks[k].frame + 1;
    }
    L += 200;   // 预留 200 帧空白，时间轴可继续向后延伸（无限长）
    return L;
}

// 播放头移到总帧数之外时，自动扩展总帧数（时间轴无限长）
void CTimelineWnd::EnsureLengthForFrame(int f) {
    if (!m_owner) return;
    Scene3D& scene = m_owner->Scene();
    if (f >= scene.totalFrames) scene.totalFrames = f + 1;
}

void CTimelineWnd::SetViewFrame(double vf) {
    if (vf < 0) vf = 0;
    m_viewFrame = vf;
}

// 右上角缩放调节条区域（标尺行右侧的一小段水平条）
void CTimelineWnd::ZoomBarRect(CRect& r) const {
    CRect rc;
    GetClientRect(&rc);
    int w = rc.Width() - m_indentW;
    int bw = (w > 150 ? 110 : w - 40);
    if (bw < 60) bw = 60;
    // 位于时间轴最顶部的独立工具行，不与标尺/刻度重叠
    r = CRect(rc.right - bw - 8, 1, rc.right - 8, m_topH - 1);
}

// 调节条水平位置 → 每帧像素（缩放级别）
double CTimelineWnd::PpfFromZoomX(int x) const {
    CRect zb;
    ZoomBarRect(zb);
    double t = (double)(x - zb.left) / (zb.Width() > 1 ? zb.Width() : 1);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    const double minP = 0.8, maxP = 40.0;
    return minP + (maxP - minP) * t;
}

// 绘制右上角缩放调节条
void CTimelineWnd::DrawZoomBar(CDC& mem) const {
    CRect zb;
    ZoomBarRect(zb);
    if (zb.Width() <= 0) return;
    mem.FillSolidRect(&zb, RGB(202, 209, 219));
    mem.DrawEdge(&zb, EDGE_ETCHED, BF_RECT);
    // 滑块（当前缩放级别对应位置）
    const double minP = 0.8, maxP = 40.0;
    double ppf = m_ppf;
    if (ppf < minP) ppf = minP;
    if (ppf > maxP) ppf = maxP;
    int inW = zb.Width() - 8;
    int tx = zb.left + 4 + (int)((ppf - minP) / (maxP - minP) * inW);
    CRect th(tx - 4, zb.top + 2, tx + 4, zb.bottom - 2);
    mem.FillSolidRect(&th, RGB(80, 105, 145));
    mem.DrawEdge(&th, EDGE_SUNKEN, BF_RECT);
}

// 同步横向滚动条（范围 = 内容长度 - 可视帧数）
void CTimelineWnd::UpdateScroll() {
    CRect rc;
    GetClientRect(&rc);
    int w = rc.Width() - m_indentW;
    double vis = (w > 0 ? w : 1) / m_ppf;
    double len = ContentLength();
    int max = 0;
    if (len > vis) max = (int)(len - vis + 0.999);
    int pos = (int)m_viewFrame;
    if (pos > max) { pos = max; SetViewFrame(pos); }

    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = max;
    si.nPage = (UINT)(vis > 1 ? vis : 1);
    si.nPos = pos;
    SetScrollInfo(SB_HORZ, &si, TRUE);

    // 垂直滚动：范围 = 图层总行数 - 可视行数
    if (!m_owner) return;
    int totalRows = (int)m_owner->Scene().layers.size();
    int rowTop = m_topH + m_hdrH;
    int sbH = GetSystemMetrics(SM_CYHSCROLL);
    int visibleRows = (rc.Height() - sbH - rowTop) / m_rowH;
    if (visibleRows < 1) visibleRows = 1;
    int vmax = totalRows - visibleRows;
    if (vmax < 0) vmax = 0;
    if (m_rowScroll > vmax) m_rowScroll = vmax;
    if (m_rowScroll < 0) m_rowScroll = 0;
    SCROLLINFO vi;
    vi.cbSize = sizeof(vi);
    vi.fMask = SIF_PAGE | SIF_RANGE | SIF_POS | SIF_DISABLENOSCROLL;
    vi.nMin = 0;
    vi.nMax = vmax;
    vi.nPage = (UINT)visibleRows;
    vi.nPos = m_rowScroll;
    SetScrollInfo(SB_VERT, &vi, TRUE);
}

void CTimelineWnd::OnPaint() {
    CPaintDC dc(this);
    if (!m_owner) return;
    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    // 底部留出横向滚动条的高度
    int sbH = GetSystemMetrics(SM_CYHSCROLL);
    int bot = rc.Height() - sbH;
    if (bot < m_topH + m_hdrH) bot = m_topH + m_hdrH;

    Scene3D& scene = m_owner->Scene();
    UpdateScroll();

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* oldBmp = mem.SelectObject(&bmp);

    mem.FillSolidRect(&rc, RGB(255, 255, 255));

    double startFrame = m_viewFrame;
    double endFrame = m_viewFrame + (rc.Width() - m_indentW) / m_ppf;

    // 顶部工具行（放缩放调节条）底色，不加底边线（便于数字上移不受阻）
    mem.FillSolidRect(0, 0, rc.Width(), m_topH, RGB(235, 238, 242));

    // 标尺（从顶部工具行之下开始）
    int rulerTop = m_topH;
    int rulerBot = m_topH + m_hdrH;
    mem.FillSolidRect(0, rulerTop, rc.Width(), m_hdrH, RGB(228, 231, 236));
    mem.DrawEdge(CRect(0, rulerTop, rc.Width(), rulerBot), EDGE_ETCHED, BF_BOTTOM);
    mem.SetBkMode(TRANSPARENT);
    CFont* oldFont = mem.SelectObject(&m_font);

    // 刻度步长随缩放自适应（默认约每 5 帧一个数字），始终标出第 0 帧与步长倍数帧
    double want = 40.0 / m_ppf;
    static const int steps[] = { 1, 2, 5, 10, 15, 20, 30, 40, 50, 60, 80, 100 };
    int step = 5;
    for (int i = 0; i < (int)(sizeof(steps) / sizeof(steps[0])); i++) {
        if (steps[i] <= want + 0.001) step = steps[i];
        else break;
    }

    int first = (int)startFrame;
    if (first < 0) first = 0;
    first = first - (first % step);

    // 网格线（贯穿行区）：每 step 个内部帧
    for (int f = first; (double)f <= endFrame + 0.5; f += step) {
        int x = m_indentW + (int)((f - m_viewFrame) * m_ppf);
        if (x < m_indentW - 4) continue;
        CPen gPen(PS_SOLID, 1, RGB(236, 240, 246));
        CPen* oldG = mem.SelectObject(&gPen);
        mem.MoveTo(x, rulerBot);
        mem.LineTo(x, bot);
        mem.SelectObject(oldG);
    }
    // 刻度小竖线 + 数字：帧号为 0,5,10,15…（与代码/脚本中的 frame 完全一致，起始帧=0）
    // 每个内部帧都画刻度小竖线，带数字的帧（0,5,10,15…）刻度稍长并居中显示数字
    for (int f = 0; (double)f <= endFrame + 0.5; f++) {
        int num = f;
        int x = m_indentW + (int)((f - m_viewFrame) * m_ppf);
        if (x < m_indentW - 4) continue;
        bool labeled = (num == 0 || num % 5 == 0);
        mem.MoveTo(x, rulerBot - (labeled ? 6 : 3));
        mem.LineTo(x, rulerBot);
        if (labeled) {
            CString t;
            t.Format(_T("%d"), num);
            // 数字以刻度线为中心居中，Y 向上、不压刻度
            int oldAlign = mem.SetTextAlign(TA_CENTER);
            mem.TextOut(x, rulerTop - 2, t);
            mem.SetTextAlign(oldAlign);
        }
    }
    mem.TextOut(6, rulerTop + 3, _T("时间轴"));

    // 右上角缩放调节条（位于顶部工具行，不遮挡标尺数字）
    DrawZoomBar(mem);

    // 图层行（从垂直滚动偏移 m_rowScroll 开始绘制可视部分）：每行 = 一个图层
    int rowTop = m_topH + m_hdrH;
    int firstRow = m_rowScroll;
    const std::vector<Layer>& L = scene.layers;
    for (size_t li = (size_t)firstRow; li < L.size(); li++) {
        int y0 = rowTop + ((int)li - firstRow) * m_rowH;
        if (y0 >= bot) break;
        CRect row(0, y0, rc.Width(), y0 + m_rowH);
        if ((int)li == scene.curLayer)
            mem.FillSolidRect(&row, RGB(255, 244, 204));     // 当前活动图层：浅黄高亮
        else if (li % 2 == 1)
            mem.FillSolidRect(&row, RGB(245, 246, 248));
        int cy = y0 + m_rowH / 2;
        DrawEyeIcon(mem, 11, cy, L[li].visible);             // 图层可见性
        DrawLockIcon(mem, 30, cy, L[li].locked);             // 图层锁定
        mem.SetBkMode(TRANSPARENT);
        mem.TextOut(42, y0 + 3, L[li].name.c_str());         // 图层名
        // 关键帧（该图层的图层级关键帧，作用于本图层所有元件）
        const std::vector<LayerKey>& ks = L[li].keys;
        for (size_t q = 0; q < ks.size(); q++) {
            int x = m_indentW + (int)((ks[q].frame - m_viewFrame) * m_ppf);
            if (x < m_indentW - 6 || x > rc.Width() + 6) continue;
            POINT pts[4];
            pts[0].x = x;      pts[0].y = cy - 6;
            pts[1].x = x + 5;  pts[1].y = cy;
            pts[2].x = x;      pts[2].y = cy + 6;
            pts[3].x = x - 5;  pts[3].y = cy;
            COLORREF col = (ks[q].frame == scene.frame ? RGB(220, 60, 60) : RGB(40, 90, 200));
            if (ks[q].objs.empty()) {
                // 空关键帧：该帧图层无元件 → 空心菱形轮廓（Flash 式）
                CPen pen(PS_SOLID, 1, col);
                CPen* oldPen = mem.SelectObject(&pen);
                CBrush* oldB = (CBrush*)mem.SelectStockObject(NULL_BRUSH);
                mem.Polygon(pts, 4);
                mem.SelectObject(oldB);
                mem.SelectObject(oldPen);
            } else {
                // 实心关键帧：该帧图层有元件
                CBrush br(col);
                CBrush* oldBr = mem.SelectObject(&br);
                mem.Polygon(pts, 4);
                mem.SelectObject(oldBr);
            }
            // 有脚本的关键帧：右上角加橙色小三角标记
            if (!ks[q].script.empty()) {
                CBrush sb(RGB(240, 120, 0));
                CBrush* oldSb = mem.SelectObject(&sb);
                POINT sp[3];
                sp[0].x = x + 5; sp[0].y = cy - 6;
                sp[1].x = x + 9; sp[1].y = cy - 6;
                sp[2].x = x + 5; sp[2].y = cy - 2;
                mem.Polygon(sp, 3);
                mem.SelectObject(oldSb);
            }
        }
    }

    // 播放头
    int phX = m_indentW + (int)((scene.frame - m_viewFrame) * m_ppf);
    if (phX >= m_indentW - 1 && phX <= rc.Width() + 1) {
        CPen pen(PS_SOLID, 2, RGB(220, 40, 40));
        CPen* oldPen = mem.SelectObject(&pen);
        mem.MoveTo(phX, 0);
        mem.LineTo(phX, bot);
        mem.SelectObject(oldPen);
        // 播放头顶部小三角（在标尺行上沿）
        int h0 = m_topH;
        POINT hd[3];
        hd[0].x = phX;     hd[0].y = h0;
        hd[1].x = phX - 5; hd[1].y = h0 + 6;
        hd[2].x = phX + 5; hd[2].y = h0 + 6;
        CBrush hbr(RGB(220, 40, 40));
        mem.Polygon(hd, 3);
    }
    mem.SelectObject(oldFont);

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(oldBmp);
}

// 滚轮：Flash 式横向滚动时间轴（暂时去掉缩放）
BOOL CTimelineWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
    if (!m_owner) return CWnd::OnMouseWheel(nFlags, zDelta, pt);
    if (nFlags & MK_CONTROL) {   // Ctrl+滚轮：横向滚动
        double vis = VisibleFrames();
        double delta = (zDelta > 0 ? -vis : vis) * 0.1;
        SetViewFrame(m_viewFrame + delta);
        UpdateScroll();
        Invalidate(FALSE);
        return TRUE;
    }
    m_rowScroll -= (zDelta > 0 ? 3 : -3);   // 默认垂直滚动元件行
    UpdateScroll();
    Invalidate(FALSE);
    return TRUE;
}

// 垂直滚动条：滚动元件行
void CTimelineWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
    if (!m_owner) { CWnd::OnVScroll(nSBCode, nPos, pScrollBar); return; }
    switch (nSBCode) {
        case SB_LINEUP:   m_rowScroll -= 1; break;
        case SB_LINEDOWN: m_rowScroll += 1; break;
        case SB_PAGEUP:   m_rowScroll -= 8; break;
        case SB_PAGEDOWN: m_rowScroll += 8; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;
            if (GetScrollInfo(SB_VERT, &si, SIF_TRACKPOS))
                m_rowScroll = si.nTrackPos;
            break;
        }
        default: break;
    }
    UpdateScroll();
    Invalidate(FALSE);
}

// 横向滚动条
void CTimelineWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
    if (!m_owner) { CWnd::OnHScroll(nSBCode, nPos, pScrollBar); return; }
    double vis = VisibleFrames();
    switch (nSBCode) {
        case SB_LINELEFT:   SetViewFrame(m_viewFrame - 5); break;
        case SB_LINERIGHT:  SetViewFrame(m_viewFrame + 5); break;
        case SB_PAGELEFT:   SetViewFrame(m_viewFrame - vis * 0.8); break;
        case SB_PAGERIGHT:  SetViewFrame(m_viewFrame + vis * 0.8); break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;
            if (GetScrollInfo(SB_HORZ, &si, SIF_TRACKPOS))
                SetViewFrame(si.nTrackPos);
            break;
        }
        default: break;
    }
    UpdateScroll();
    Invalidate(FALSE);
}

void CTimelineWnd::OnLButtonDown(UINT nFlags, CPoint point) {
    if (!m_owner) return;
    Scene3D& scene = m_owner->Scene();

    // 右上角缩放调节条：按下即拖动
    CRect zb;
    ZoomBarRect(zb);
    if (zb.PtInRect(point)) {
        m_draggingZoom = true;
        m_scrubbing = false;
        m_panning = false;
        SetCapture();
        m_ppf = PpfFromZoomX(point.x);
        UpdateScroll();
        Invalidate(FALSE);
        return;
    }

    // 标尺区：点击/拖动播放头（可连续拖动定位帧）
    if (point.y < m_topH + m_hdrH) {
        m_scrubbing = true;
        m_panning = false;
        SetCapture();
        int f = FrameFromX(point.x);
        EnsureLengthForFrame(f);
        m_owner->SetCurrentFrame(f);
        return;
    }

    int rowTop = m_topH + m_hdrH;
    int rel = (point.y - rowTop) / m_rowH;
    int rowIdx = rel + m_rowScroll;   // 加上垂直滚动偏移
    if (rowIdx < 0 || (size_t)rowIdx >= scene.layers.size()) {
        // 行区空白处：横向平移（按住左键拖动）
        if (point.x < m_indentW) return;
        m_panning = true;
        m_scrubbing = false;
        m_panStart = point;
        m_panStartView = m_viewFrame;
        SetCapture();
        return;
    }
    int li = rowIdx;
    int oy = rowTop + (rowIdx - m_rowScroll) * m_rowH;

    if (point.x < m_indentW) {
        // 眼睛列（图层可见性切换：图层隐藏则整层隐藏）
        if (point.x < 20) {
            scene.layers[li].visible = !scene.layers[li].visible;
            m_owner->NotifyTimelineChanged();
            return;
        }
        // 锁定列（图层锁定切换：锁定后该层元件不可选中/拖动）
        if (point.x < 38) {
            scene.layers[li].locked = !scene.layers[li].locked;
            m_owner->NotifyTimelineChanged();
            return;
        }
        // 图层名/空白：设为当前图层
        m_owner->SetCurrentLayerIdx(li);
        return;
    }
    // 命中图层关键帧菱形 → 开始拖拽
    const std::vector<LayerKey>& ks = scene.layers[li].keys;
    for (size_t q = 0; q < ks.size(); q++) {
        int x = m_indentW + (int)((ks[q].frame - m_viewFrame) * m_ppf);
        if (point.x >= x - 6 && point.x <= x + 6 &&
            point.y >= oy && point.y <= oy + m_rowH) {
            m_draggingKey = true;
            m_dragLayer = li;
            m_dragIdx = q;
            SetCapture();
            m_owner->PushUndo();
            return;
        }
    }
    // 行区普通点击：定位当前帧，并把该图层设为当前
    if (scene.curLayer != li) scene.curLayer = li;
    int f = FrameFromX(point.x);
    EnsureLengthForFrame(f);
    m_owner->SetCurrentFrame(f);
}

void CTimelineWnd::StartNameEdit(int li) {
    if (!m_owner) return;
    Scene3D& scene = m_owner->Scene();
    EndNameEdit();   // 先提交可能存在的上一次编辑
    int rowTop = m_topH + m_hdrH;
    int oy = rowTop + (li - m_rowScroll) * m_rowH;
    int w = TextW(scene.layers[li].name) + 10;
    if (w < 40) w = 40;
    CRect rc(42, oy + 1, 42 + w, oy + m_rowH - 1);
    m_nameEdit = new CEdit();
    if (!m_nameEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, rc, this, EDIT_NAME_ID)) {
        delete m_nameEdit; m_nameEdit = NULL; return;
    }
    m_nameEdit->SetFont(&m_font);
    m_nameEdit->SetWindowText(scene.layers[li].name.c_str());
    m_nameEdit->SetSel(0, -1);
    m_nameEdit->SetFocus();
    m_editLayer = li;
}

void CTimelineWnd::EndNameEdit() {
    CEdit* e = m_nameEdit;
    if (!e) return;
    m_nameEdit = NULL;
    if (m_owner && m_editLayer >= 0) {
        CString s;
        e->GetWindowText(s);
        if (!s.IsEmpty() &&
            s != m_owner->Scene().layers[m_editLayer].name.c_str())
            m_owner->Scene().layers[m_editLayer].name = s.GetString();
    }
    m_editLayer = -1;
    e->DestroyWindow();
    delete e;
    Invalidate(FALSE);
}

void CTimelineWnd::OnNameEditEnd() { EndNameEdit(); }

void CTimelineWnd::OnLButtonDblClk(UINT nFlags, CPoint point) {
    if (!m_owner) { CWnd::OnLButtonDblClk(nFlags, point); return; }
    Scene3D& scene = m_owner->Scene();
    int rowTop = m_topH + m_hdrH;
    int rel = (point.y - rowTop) / m_rowH;
    int li = rel + m_rowScroll;
    if (li < 0 || (size_t)li >= scene.layers.size()) { CWnd::OnLButtonDblClk(nFlags, point); return; }
    // 双击图层名列（x∈[42, m_indentW)，避开眼睛/锁定列）→ 就地改图层名
    if (point.x >= 42 && point.x < m_indentW) {
        StartNameEdit(li);
        return;
    }
    CWnd::OnLButtonDblClk(nFlags, point);
}

void CTimelineWnd::OnMouseMove(UINT nFlags, CPoint point) {
    if (m_draggingZoom) {
        m_ppf = PpfFromZoomX(point.x);
        UpdateScroll();
        Invalidate(FALSE);
        return;
    }
    if (m_scrubbing) {
        int f = FrameFromX(point.x);
        EnsureLengthForFrame(f);
        m_owner->SetCurrentFrame(f);
        Invalidate(FALSE);
        return;
    }
    if (m_panning) {
        SetViewFrame(m_panStartView - (point.x - m_panStart.x) / m_ppf);
        UpdateScroll();
        Invalidate(FALSE);
        return;
    }
    if (!m_draggingKey || m_dragLayer < 0) return;
    Scene3D& scene = m_owner->Scene();
    // 拖动关键帧期间 keys 可能被增删而改变，防止 m_dragIdx 越界
    if (m_dragIdx >= scene.layers[m_dragLayer].keys.size()) {
        m_draggingKey = false;
        m_dragLayer = -1;
        return;
    }
    int f = FrameFromX(point.x);
    if (f < 0) f = 0;
    int oldF = scene.layers[m_dragLayer].keys[m_dragIdx].frame;
    if (oldF != f) {
        scene.MoveLayerKey(m_dragLayer, oldF, f);   // 值保留、自动排序去重
        m_owner->OnTimelineKeyMoved();
    }
    Invalidate(FALSE);
}

void CTimelineWnd::OnLButtonUp(UINT nFlags, CPoint point) {
    if (m_draggingZoom) {
        m_draggingZoom = false;
        ReleaseCapture();
    }
    if (m_scrubbing || m_panning) {
        m_scrubbing = false;
        m_panning = false;
        ReleaseCapture();
    }
    if (m_draggingKey) {
        m_draggingKey = false;
        m_dragLayer = -1;
        ReleaseCapture();
    }
}

void CTimelineWnd::OnRButtonUp(UINT nFlags, CPoint point) {
    if (!m_owner) return;
    Scene3D& scene = m_owner->Scene();
    int rowTop = m_topH + m_hdrH;
    int rel = (point.y - rowTop) / m_rowH;
    int rowIdx = rel + m_rowScroll;
    if (rowIdx < 0 || (size_t)rowIdx >= scene.layers.size()) return;
    int li = rowIdx;
    int oy = rowTop + (rowIdx - m_rowScroll) * m_rowH;

    // 命中图层关键帧菱形 → 记录帧号，供"删除关键帧"使用（不直接删除）
    int hitKeyFrame = -1;
    const std::vector<LayerKey>& ks = scene.layers[li].keys;
    for (size_t q = 0; q < ks.size(); q++) {
        int x = m_indentW + (int)((ks[q].frame - m_viewFrame) * m_ppf);
        if (point.x >= x - 6 && point.x <= x + 6 &&
            point.y >= oy && point.y <= oy + m_rowH) {
            hitKeyFrame = ks[q].frame;
        }
    }
    int ctxFrame = FrameFromX(point.x);

    // 右键菜单：关键帧操作（插入/删除）+ 图层操作
    CPoint sp = point;
    ClientToScreen(&sp);
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 4100, L"在此处插入关键帧");
    menu.AppendMenu(MF_STRING, 4102, L"在此处插入空白关键帧");
    if (hitKeyFrame >= 0)
        menu.AppendMenu(MF_STRING, 4101, L"删除此关键帧");
    if (hitKeyFrame >= 0)
        menu.AppendMenu(MF_STRING, 4105, L"编辑此关键帧脚本…");
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 4010, L"设为当前图层");
    Object3D* sel = m_owner->GetSelected();
    bool canMove = sel && sel->parent == NULL && scene.RootLayerOf(sel) != li;
    menu.AppendMenu((canMove ? MF_STRING : MF_STRING | MF_GRAYED), 4020, L"将选中元件移到此图层");
    if (hitKeyFrame >= 0 && sel && sel->parent == NULL) {
        menu.AppendMenu(MF_STRING, 4103, L"把选中元件加入此关键帧");
        menu.AppendMenu(MF_STRING, 4104, L"从此关键帧移除选中元件");
    }
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 4011, L"上移一层（更上层）");
    menu.AppendMenu(MF_STRING, 4012, L"下移一层（更下层）");
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 4013, L"置顶（最上层）");
    menu.AppendMenu(MF_STRING, 4014, L"置底（最底层）");
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, 4030, L"新建图层");
    bool layerEmpty = true;
    for (size_t i = 0; i < scene.roots.size(); i++)
        if (scene.RootLayerOf(scene.roots[i]) == li) { layerEmpty = false; break; }
    menu.AppendMenu((scene.layers.size() > 1 && layerEmpty ? MF_STRING : MF_STRING | MF_GRAYED),
                    4031, L"删除空图层");
    int id = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_RETURNCMD, sp.x, sp.y, this);
    switch (id) {
        case 4100:   // 插入图层关键帧：对当前右键图层在右键帧插入
            m_owner->InsertLayerKeyAtFrame(li, ctxFrame);
            break;
        case 4102:   // 插入空白关键帧：该帧该层置空
            m_owner->InsertEmptyLayerKeyAtFrame(li, ctxFrame);
            break;
        case 4101:   // 删除图层关键帧
            m_owner->DeleteLayerKeyAt(li, hitKeyFrame);
            break;
        case 4105:   // 编辑此关键帧脚本（进入该帧时执行一次；空白关键帧也可挂）
            m_owner->EditKeyFrameScript(li, hitKeyFrame);
            break;
        case 4103: m_owner->AddObjectToKeyAt(li, hitKeyFrame); break;
        case 4104: m_owner->RemoveObjectFromKeyAt(li, hitKeyFrame); break;
        case 4010: m_owner->SetCurrentLayerIdx(li); break;
        case 4020: m_owner->MoveObjectToLayer(sel, li); break;
        case 4011: m_owner->MoveLayerByIndex(li, -1); break;
        case 4012: m_owner->MoveLayerByIndex(li, 1); break;
        case 4013: m_owner->MoveLayerByIndex(li, 2); break;
        case 4014: m_owner->MoveLayerByIndex(li, 3); break;
        case 4030: m_owner->AddNewLayer(); break;
        case 4031: m_owner->DeleteEmptyLayer(li); break;
        default: break;
    }
}
