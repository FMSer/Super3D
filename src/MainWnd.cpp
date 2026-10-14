// MainWnd.cpp : 主窗口实现
#include "stdafx.h"
#include "MainWnd.h"
#include "Examples.h"
#include "ProjectIO.h"
#include "Renderer.h"
#include <map>
#include <algorithm>
#pragma comment(lib, "msimg32.lib")   // TransparentBlt 透明位图

// 从引擎错误文本 "第N行：描述" 提取行号与描述（解析/运行错误统一格式）
static int ParseErrLine(const std::wstring& raw, std::wstring& desc) {
    int line = 1;
    if (raw.size() >= 2 && raw[0] == L'第') {
        const wchar_t* p = raw.c_str() + 1;
        int v = 0;
        while (*p >= L'0' && *p <= L'9') { v = v * 10 + (*p - L'0'); p++; }
        if (*p == L'行' && p[1] == L'：') {
            line = v;
            desc = raw.substr((size_t)(p - raw.c_str()) + 2);
            return line;
        }
    }
    desc = raw;
    return line;
}

enum {
    IDC_BTN_NEW = 2000, IDC_BTN_OPEN, IDC_BTN_SAVE, IDC_BTN_UNDO,
    IDC_BTN_GROUP, IDC_BTN_UNGROUP, IDC_BTN_BOX, IDC_BTN_SPHERE,
    IDC_BTN_CYLINDER, IDC_BTN_CONE, IDC_BTN_TORUS, IDC_BTN_PLANE,
    IDC_BTN_TOOL_SELECT, IDC_BTN_TOOL_MOVE, IDC_BTN_TOOL_ROTATE, IDC_BTN_TOOL_SCALE,
    IDC_BTN_TOOL_PAN, IDC_BTN_TOOL_SPLINE,
    IDC_BTN_LEAF,
    IDC_BTN_TEXT3D,
    IDC_BTN_TUBE,
    IDC_BTN_RTUBE,
    IDC_BTN_DUPLICATE, IDC_BTN_CUT, IDC_BTN_DELETE,
    IDC_BTN_ADDKEY, IDC_BTN_DELKEY,
    IDC_BTN_PLAY, IDC_BTN_STOP,
    IDC_BTN_EX0, IDC_BTN_EX1, IDC_BTN_EX2, IDC_BTN_EX3, IDC_BTN_EX4, IDC_BTN_EX5,
    IDC_BTN_RESETVIEW, IDC_BTN_RUN, IDC_BTN_DEBUG, IDC_BTN_EXPORT, IDC_BTN_HELP,
    IDC_BTN_SUPERDOC,   // 帮助菜单：SuperScript 文档（打开 SuperScript参考.html）
    IDC_BTN_DOC_CLASS,  // 文件菜单：文档类脚本（整个工程的初始化脚本）
    IDC_OPEN_SCRIPT = 2100,
    IDC_CMD_TOMC,         // 右键：转换为 MovieClip
    IDC_CMD_PASTE,        // 右键：粘贴（剪贴板）
    IDC_CMD_BOOLEAN,      // 右键：布尔差集（A−B 挖孔）
    IDC_CMD_BOOLUNION,    // 右键：布尔并集（A∪B）
    IDC_CMD_BOOLINTER,    // 右键：布尔交集（A∩B）
    // 元件对齐
    IDC_ALIGN_LEFT, IDC_ALIGN_HCENTER, IDC_ALIGN_RIGHT,
    IDC_ALIGN_TOP, IDC_ALIGN_VCENTER, IDC_ALIGN_BOTTOM
};

struct ToolBtn { UINT id; const wchar_t* label; int row; };
static const ToolBtn kToolBtns[] = {
    { IDC_BTN_NEW, L"新建", 0 },   { IDC_BTN_OPEN, L"打开", 0 },  { IDC_BTN_SAVE, L"保存", 0 },
    { IDC_BTN_UNDO, L"撤销", 0 },
    { IDC_BTN_TOOL_SELECT, L"选择", 0 }, { IDC_BTN_TOOL_MOVE, L"移动", 0 },
    { IDC_BTN_TOOL_ROTATE, L"旋转", 0 }, { IDC_BTN_TOOL_SCALE, L"缩放", 0 },
    { IDC_BTN_TOOL_PAN, L"平移视角", 0 },
    { IDC_BTN_TOOL_SPLINE, L"曲线", 0 },
    { IDC_BTN_DUPLICATE, L"复制", 0 },   { IDC_BTN_CUT, L"剪切", 0 }, { IDC_BTN_DELETE, L"删除", 0 },
    { IDC_BTN_ADDKEY, L"插关键帧", 0 },  { IDC_BTN_DELKEY, L"删关键帧", 0 },
    { IDC_BTN_PLAY, L"播放", 0 },        { IDC_BTN_STOP, L"停止", 0 },
    { IDC_BTN_RESETVIEW, L"重置视角", 0 },
    { IDC_BTN_GROUP, L"组", 1 },         { IDC_BTN_UNGROUP, L"打散", 1 },
    { IDC_BTN_BOX, L"立方体", 1 },
    { IDC_BTN_SPHERE, L"球", 1 },        { IDC_BTN_CYLINDER, L"圆柱", 1 },
    { IDC_BTN_CONE, L"圆锥", 1 },        { IDC_BTN_TORUS, L"圆环", 1 },
    { IDC_BTN_PLANE, L"平面", 1 },
    { IDC_BTN_LEAF, L"绿叶", 1 },
    { IDC_BTN_TEXT3D, L"文本", 1 },
    { IDC_BTN_TUBE, L"方管", 1 },
    { IDC_BTN_RTUBE, L"圆管", 1 },
    { IDC_BTN_EX0, L"House", 1 },       { IDC_BTN_EX1, L"Windmill", 1 },
    { IDC_BTN_EX2, L"SolarSystem", 1 }, { IDC_BTN_EX3, L"BounceBall", 1 },
    { IDC_BTN_EX4, L"DocClass", 1 },    { IDC_BTN_EX5, L"TankBattle", 1 },
    { IDC_BTN_RUN, L"运行脚本", 1 },  { IDC_BTN_DEBUG, L"调试", 1 },
    { IDC_BTN_EXPORT, L"导出", 1 },
    { IDC_BTN_HELP, L"帮助", 1 }
};
static const int kBtnCount = sizeof(kToolBtns) / sizeof(kToolBtns[0]);

// 左侧图标工具栏（Flash 常用工具，竖排；其余功能进菜单）
// 按功能分三组：图元（基本体/曲面装饰）、工具（选择/变换/视角）、控制（播放/停止），
// 组与组之间在布局时留出加大的间隔并绘制分隔线（见 OnSize / OnEraseBkgnd）
// 顺序：图元组（立方体…文本，圆管紧随圆环、方管在后）在前，其次是选择/移动/旋转/缩放/平移，
//       最后是播放/停止
static const UINT kSideBarIds[] = {
    // 图元组（基本体在前，圆管/方管相邻，曲面装饰在后）
    IDC_BTN_BOX, IDC_BTN_SPHERE, IDC_BTN_CYLINDER, IDC_BTN_CONE, IDC_BTN_TORUS,
    IDC_BTN_RTUBE, IDC_BTN_TUBE, IDC_BTN_PLANE, IDC_BTN_TOOL_SPLINE, IDC_BTN_LEAF, IDC_BTN_TEXT3D,
    // 工具组（选择/移动/旋转/缩放/平移）
    IDC_BTN_TOOL_SELECT, IDC_BTN_TOOL_MOVE, IDC_BTN_TOOL_ROTATE, IDC_BTN_TOOL_SCALE,
    IDC_BTN_TOOL_PAN,
    // 控制组
    IDC_BTN_PLAY, IDC_BTN_STOP
};
// 与 kSideBarIds 一一对应：true = 该项是某一功能组的第一项（前面画加大的分组间隔+分隔线）
static const bool kSideGroupStart[] = {
    true, false, false, false, false, false, false, false, false, false, false,
    true, false, false, false, false,
    true, false
};
// 与 kSideBarIds 一一对应的悬停提示文本
static const wchar_t* kSideTips[] = {
    L"添加立方体", L"添加球体", L"添加圆柱", L"添加圆锥", L"添加圆环",
    L"添加圆管", L"添加方管", L"添加平面", L"绘制曲线", L"添加绿叶", L"添加文本",
    L"选择工具", L"移动工具", L"旋转工具",
    L"缩放工具", L"平移视角",
    L"播放", L"停止"
};
static const int kSideCount = sizeof(kSideBarIds) / sizeof(kSideBarIds[0]);
static bool IsSideBar(UINT id) {
    for (int i = 0; i < kSideCount; i++) if (kSideBarIds[i] == id) return true;
    return false;
}

// 文档 Tab 控件（owner-draw）：文字左侧，右端固定画"×"关闭按钮；WM_LBUTTONDOWN 判定右端 18px 通知父窗口关闭
BEGIN_MESSAGE_MAP(CDocTabCtrl, CTabCtrl)
    ON_WM_LBUTTONDOWN()
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()

void CDocTabCtrl::DrawItem(LPDRAWITEMSTRUCT lp) {
    CDC dc; dc.Attach(lp->hDC);
    CRect rc = lp->rcItem;
    bool sel = (lp->itemState & ODS_SELECTED) != 0;
    // 统一系统按钮底色，选中项仅加细边框区分（不变色，保持原 Tab 外观）
    dc.FillRect(&rc, &CBrush(::GetSysColor(COLOR_BTNFACE)));
    wchar_t buf[256];
    TCITEM ti; ti.mask = TCIF_TEXT; ti.pszText = buf; ti.cchTextMax = 256;
    SendMessage(TCM_GETITEM, lp->itemID, (LPARAM)&ti);
    CString txt(buf);
    {   // 修剪尾随空格：宽度计算已含留白，绘制时去掉使显示干净
        int n = txt.GetLength();
        while (n > 0 && txt[n - 1] == L' ') n--;
        if (n < txt.GetLength()) txt = txt.Left(n);
    }
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(::GetSysColor(COLOR_BTNTEXT));
    dc.SelectObject(GetFont());
    if (sel) dc.FrameRect(&rc, &CBrush(::GetSysColor(COLOR_BTNSHADOW)));
    CRect tr = rc; tr.left += 4; tr.right -= 18;   // 预留右侧"×"区
    dc.DrawText(txt, &tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    // 右端画"×"
    CRect xr = rc; xr.right -= 2;
    int cx = xr.right - 7, cy = rc.CenterPoint().y;
    dc.MoveTo(cx - 3, cy - 3); dc.LineTo(cx + 3, cy + 3);
    dc.MoveTo(cx + 3, cy - 3); dc.LineTo(cx - 3, cy + 3);
    dc.Detach();
}

void CDocTabCtrl::OnLButtonDown(UINT nFlags, CPoint point) {
    // 命中标签右端 18px"×"区：直接请求父窗口关闭该文档，不切换选中
    TCHITTESTINFO hi = { point, TCHT_ONITEM };
    int idx = HitTest(&hi);
    if (idx >= 0) {
        CRect r; GetItemRect(idx, &r);
        if (point.x >= r.right - 18) {
            GetParent()->SendMessage(WM_USER + 55, (WPARAM)idx, 0);
            return;   // 吃掉本次按下，不交给基类切换选中
        }
    }
    CTabCtrl::OnLButtonDown(nFlags, point);
}

BEGIN_MESSAGE_MAP(CMainWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_EN_CHANGE(9007, &CMainWnd::OnDocEdtChange)
    ON_CONTROL(LBN_DBLCLK, 9401, &CMainWnd::OnAutoDblClk)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(2201, &CMainWnd::OnDocMenuUndo)
    ON_COMMAND(2202, &CMainWnd::OnDocMenuCut)
    ON_COMMAND(2203, &CMainWnd::OnDocMenuCopy)
    ON_COMMAND(2204, &CMainWnd::OnDocMenuPaste)
    ON_COMMAND(2205, &CMainWnd::OnDocMenuSelectAll)
    ON_COMMAND(2206, &CMainWnd::OnDocMenuFormat)
    ON_WM_DESTROY()
    ON_NOTIFY(TCN_SELCHANGE, 9000, &CMainWnd::OnTabSelChange)
    ON_NOTIFY(TCN_SELCHANGE, 9010, &CMainWnd::OnDocTabSelChange)
    ON_NOTIFY_EX(TTN_GETDISPINFO, 0, &CMainWnd::OnDocTabTip)
    ON_MESSAGE(WM_USER + 55, &CMainWnd::OnDocTabCloseMsg)
    ON_COMMAND(IDC_BTN_NEW, &CMainWnd::OnNew)
    ON_COMMAND(IDC_BTN_OPEN, &CMainWnd::OnOpen)
    ON_COMMAND(IDC_BTN_SAVE, &CMainWnd::OnSave)
    ON_COMMAND(IDC_BTN_UNDO, &CMainWnd::OnUndo)
    ON_COMMAND(IDC_BTN_GROUP, &CMainWnd::OnGroup)
    ON_COMMAND(IDC_BTN_UNGROUP, &CMainWnd::OnUngroup)
    ON_COMMAND(IDC_BTN_BOX, &CMainWnd::OnBox)
    ON_COMMAND(IDC_BTN_SPHERE, &CMainWnd::OnSphere)
    ON_COMMAND(IDC_BTN_CYLINDER, &CMainWnd::OnCylinder)
    ON_COMMAND(IDC_BTN_CONE, &CMainWnd::OnCone)
    ON_COMMAND(IDC_BTN_TORUS, &CMainWnd::OnTorus)
    ON_COMMAND(IDC_BTN_PLANE, &CMainWnd::OnPlane)
    ON_COMMAND(IDC_BTN_LEAF, &CMainWnd::OnLeaf)
    ON_COMMAND(IDC_BTN_TEXT3D, &CMainWnd::OnText3D)
    ON_COMMAND(IDC_BTN_TUBE, &CMainWnd::OnTube)
    ON_COMMAND(IDC_BTN_RTUBE, &CMainWnd::OnRTube)
    ON_COMMAND(IDC_BTN_TOOL_SPLINE, &CMainWnd::OnToolSpline)
    ON_COMMAND(IDC_BTN_TOOL_SELECT, &CMainWnd::OnToolSelect)
    ON_COMMAND(IDC_BTN_TOOL_MOVE, &CMainWnd::OnToolMove)
    ON_COMMAND(IDC_BTN_TOOL_ROTATE, &CMainWnd::OnToolRotate)
    ON_COMMAND(IDC_BTN_TOOL_SCALE, &CMainWnd::OnToolScale)
    ON_COMMAND(IDC_BTN_TOOL_PAN, &CMainWnd::OnToolPan)
    ON_COMMAND(IDC_BTN_DUPLICATE, &CMainWnd::OnDuplicate)
    ON_COMMAND(IDC_BTN_CUT, &CMainWnd::OnCut)
    ON_COMMAND(IDC_BTN_DELETE, &CMainWnd::OnDelete)
    ON_COMMAND(IDC_BTN_ADDKEY, &CMainWnd::OnAddKey)
    ON_COMMAND(IDC_BTN_DELKEY, &CMainWnd::OnDelKey)
    ON_COMMAND(IDC_BTN_PLAY, &CMainWnd::OnPlay)
    ON_COMMAND(IDC_BTN_STOP, &CMainWnd::OnStop)
    ON_COMMAND(IDC_BTN_EX0, &CMainWnd::OnEx0)
    ON_COMMAND(IDC_BTN_EX1, &CMainWnd::OnEx1)
    ON_COMMAND(IDC_BTN_EX2, &CMainWnd::OnEx2)
    ON_COMMAND(IDC_BTN_EX3, &CMainWnd::OnEx3)
    ON_COMMAND(IDC_BTN_EX4, &CMainWnd::OnEx4)
    ON_COMMAND(IDC_BTN_EX5, &CMainWnd::OnEx5)
    ON_COMMAND(IDC_BTN_RESETVIEW, &CMainWnd::OnResetView)
    ON_COMMAND(IDC_BTN_RUN, &CMainWnd::OnRun)
    ON_COMMAND(IDC_BTN_DEBUG, &CMainWnd::OnDebug)
    ON_COMMAND(IDC_BTN_EXPORT, &CMainWnd::OnExport)
    ON_COMMAND(IDC_BTN_HELP, &CMainWnd::OnHelp)
    ON_COMMAND(IDC_BTN_SUPERDOC, &CMainWnd::OnSuperScriptDoc)
    ON_COMMAND(IDC_BTN_DOC_CLASS, &CMainWnd::OnDocClassScript)
    ON_COMMAND(IDC_OPEN_SCRIPT, &CMainWnd::ShowScriptWindow)
    ON_COMMAND(IDC_CMD_TOMC, &CMainWnd::OnWrapToMovieClip)
    ON_COMMAND(IDC_CMD_PASTE, &CMainWnd::OnPaste)
    ON_COMMAND(IDC_CMD_BOOLEAN, &CMainWnd::OnBooleanHole)
    ON_COMMAND(IDC_CMD_BOOLUNION, &CMainWnd::OnBooleanUnion)
    ON_COMMAND(IDC_CMD_BOOLINTER, &CMainWnd::OnBooleanInter)
    ON_COMMAND(IDC_ALIGN_LEFT, &CMainWnd::OnAlignLeft)
    ON_COMMAND(IDC_ALIGN_HCENTER, &CMainWnd::OnAlignHCenter)
    ON_COMMAND(IDC_ALIGN_RIGHT, &CMainWnd::OnAlignRight)
    ON_COMMAND(IDC_ALIGN_TOP, &CMainWnd::OnAlignTop)
    ON_COMMAND(IDC_ALIGN_VCENTER, &CMainWnd::OnAlignVCenter)
    ON_COMMAND(IDC_ALIGN_BOTTOM, &CMainWnd::OnAlignBottom)
    ON_WM_DRAWITEM()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
END_MESSAGE_MAP()

CMainWnd::CMainWnd() : m_selected(NULL), m_clipboard(NULL), m_tool(TOOL_SELECT), m_playing(false),
    m_tlStopped(false), m_syntaxDirty(false), m_bUpdatingScript(false), m_showWire(false), m_navInside(false),
    m_hIconSmall(NULL), m_hIconLarge(NULL), m_lastKeyScriptFrame(-1),
    m_docScriptMode(false), m_docSSOpen(true) {}

CMainWnd::~CMainWnd() {
    if (m_clipboard) delete m_clipboard;
}

BOOL CMainWnd::CreateMain() {
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1), NULL);
    CRect rc(0, 0, 1280, 800);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (rc.Width() > sw - 60) rc.right = sw - 60;
    if (rc.Height() > sh - 100) rc.bottom = sh - 100;
    // 说明：顶层窗口统一使用 CreateEx；部分环境下 CWnd::Create(空父窗口) 会失败
    BOOL ok = CreateEx(0, cls, _T("Super3D"),
        WS_OVERLAPPEDWINDOW, rc, NULL, 0);
    if (ok) CenterWindow();
    return ok;
}

// 主窗口图标：运行时用 GDI 画一个等距立方体（不引入资源文件）。
// 颜色位图为 32bpp DIB：洋红作色键填充，画完按色键写 alpha（键=全透明，其余=不透明），
// 背景真正透明不露黑底；1bpp 掩码与剪影保持一致仅作兜底。
// 个性图标：金顶/青左/紫右三色等距立方体 + 顶面蓝色小写 "ss"（Super Script 缩写），象征 3D 创作与脚本。
static HICON CreateCubeIcon(int size) {
    HDC sdc = ::GetDC(NULL);
    CDC dcS;
    dcS.Attach(sdc);
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;      // 顶向下，可直接按行读写像素
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    CBitmap col;
    // MFC CBitmap 无 CreateDIBSection 成员，直接调 Win32 API 再 Attach（析构自动删除）
    HBITMAP hcol = ::CreateDIBSection(dcS.GetSafeHdc(), &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hcol || !bits) {
        dcS.Detach();
        ::ReleaseDC(NULL, sdc);
        return NULL;
    }
    col.Attach(hcol);
    CDC dcC;
    dcC.CreateCompatibleDC(&dcS);
    CBitmap* oldC = dcC.SelectObject(&col);
    CBitmap mask;
    mask.CreateBitmap(size, size, 1, 1, NULL);
    CDC dcM;
    dcM.CreateCompatibleDC(&dcS);
    CBitmap* oldM = dcM.SelectObject(&mask);

    // 立方体几何：七点等距投影（T顶 L左 R右 M中 BL左下 BR右下 B底）
    // 再放大：水平占宽约 78%、垂直占高约 86%，贴近图标边缘但保留少量留白
    int cx = size / 2, t = size / 14, b = size - 1 - t;
    int half = (b - t) / 4, l = size / 9, r = size - l;
    CPoint T(cx, t), Lp(l, t + half), R(r, t + half), M(cx, t + 2 * half),
           BL(l, b - half), BR(r, b - half), B(cx, b);

    // 掩码：白=透明，黑=不透明。先全白，再画黑色剪影六边形（与 alpha 通道一致）
    dcM.FillSolidRect(0, 0, size, size, RGB(255, 255, 255));
    CBrush brM(RGB(0, 0, 0));
    CPen penM(PS_SOLID, 1, RGB(0, 0, 0));
    CBrush* obM = dcM.SelectObject(&brM);
    CPen* opM = dcM.SelectObject(&penM);
    POINT sil[6] = { T, R, BR, B, BL, Lp };
    dcM.Polygon(sil, 6);
    dcM.SelectObject(obM);
    dcM.SelectObject(opM);

    // 颜色位图：洋红作色键 → 画三个个性色面 → 顶面 "ss" 文字 → 高光 → 按色键写 alpha
    dcC.FillSolidRect(0, 0, size, size, RGB(255, 0, 255));
    // 边线深墨蓝，比纯黑更柔和
    CPen penE(PS_SOLID, 1, RGB(18, 22, 40));
    CPen* opC = dcC.SelectObject(&penE);
    CBrush brT(RGB(255, 206, 58));     // 顶面：活力金
    CBrush brL(RGB(0, 168, 156));     // 左面：青绿
    CBrush brR(RGB(123, 44, 191));   // 右面：深紫
    POINT ptTop[4] = { T, R, M, Lp };
    POINT ptLeft[4] = { Lp, M, B, BL };
    POINT ptRight[4] = { R, M, B, BR };
    CBrush* obC = dcC.SelectObject(&brT); dcC.Polygon(ptTop, 4);
    dcC.SelectObject(&brL); dcC.Polygon(ptLeft, 4);
    dcC.SelectObject(&brR); dcC.Polygon(ptRight, 4);

    // 顶面高光带：沿顶面顶边偏下画一条更亮的金色窄带，增加立体光泽
    CBrush brHi(RGB(255, 235, 130));
    int hi = size / 28;
    if (hi < 1) hi = 1;
    POINT ptHi[4] = {
        CPoint(T.x, T.y + hi),
        CPoint(R.x - hi, R.y + hi),
        CPoint(M.x, M.y - hi),
        CPoint(Lp.x + hi, Lp.y + hi)
    };
    dcC.SelectObject(&brHi);
    dcC.SelectObject(::GetStockObject(NULL_PEN));
    dcC.Polygon(ptHi, 4);

    // 顶面黑色小写 "ss"（Super Script 缩写）：以顶面菱形中心（cx, t+half）略偏上构造文字框，
    // 水平用顶面宽度、垂直用字高，确保文字水平居中并相对上表面中心上移；白色描边增强金色顶面上的可读性
    {
        CFont fSS;
        int fh = (M.y - T.y) * 80 / 100;
        if (fh < 5) fh = 5;
        fSS.CreateFont(-fh, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, _T("微软雅黑"));
        CFont* oldF = dcC.SelectObject(&fSS);
        dcC.SetBkMode(TRANSPARENT);
        // 以菱形中心偏上为准构造文字框：宽 = 顶面水平宽，垂直中心 = t + half*0.9，高 = 字高
        int cy = t + half * 9 / 10;
        int w = (R.x - Lp.x);
        CRect rcTop(cx - w / 2, cy - fh / 2 - 1, cx + w / 2, cy + fh / 2 + 1);
        // 白描边：四向偏移各画一次白字，再画黑色主体（金色顶面上对比更强）
        dcC.SetTextColor(RGB(255, 255, 255));
        int off = 1;
        CRect r1 = rcTop; r1.OffsetRect(-off, 0); dcC.DrawText(_T("ss"), r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        CRect r2 = rcTop; r2.OffsetRect(off, 0); dcC.DrawText(_T("ss"), r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        CRect r3 = rcTop; r3.OffsetRect(0, -off); dcC.DrawText(_T("ss"), r3, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        CRect r4 = rcTop; r4.OffsetRect(0, off); dcC.DrawText(_T("ss"), r4, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        dcC.SetTextColor(RGB(10, 10, 10));
        dcC.DrawText(_T("ss"), rcTop, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        dcC.SelectObject(oldF);
    }

    dcC.SelectObject(obC);
    dcC.SelectObject(opC);
    ::GdiFlush();
    DWORD* px = (DWORD*)bits;          // 内存字节序 B G R A
    for (int i = 0; i < size * size; i++) {
        if ((px[i] & 0xFFFFFF) == 0xFF00FF) px[i] = 0;     // 色键 → 全透明
        else px[i] |= 0xFF000000;                          // 图形 → 不透明
    }

    ICONINFO ii;
    memset(&ii, 0, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmMask = (HBITMAP)mask;
    ii.hbmColor = (HBITMAP)col;
    HICON h = ::CreateIconIndirect(&ii);   // 内部复制位图，局部 CBitmap 析构即可
    dcC.SelectObject(oldC);
    dcM.SelectObject(oldM);
    dcS.Detach();
    ::ReleaseDC(NULL, sdc);
    return h;
}

int CMainWnd::OnCreate(LPCREATESTRUCT lpcs) {
    if (CWnd::OnCreate(lpcs) == -1) return -1;

    m_fontBtn.CreatePointFont(85, _T("微软雅黑"));
    m_fontSmall.CreatePointFont(80, _T("微软雅黑"));
    {   // 导航栏当前层加粗字（基于小字体加粗）
        LOGFONT lf; memset(&lf, 0, sizeof(lf));
        m_fontSmall.GetLogFont(&lf);
        lf.lfWeight = FW_BOLD;
        m_navBoldFont.CreateFontIndirect(&lf);
    }

    // 工具栏按钮：先用普通文本按钮显示中文标签（替代图标），其余按钮隐藏并进入菜单
    for (int i = 0; i < kBtnCount; i++) {
        DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
        m_btn[i].Create(kToolBtns[i].label, style, CRect(0, 0, 10, 10), this, kToolBtns[i].id);
        m_btn[i].SetFont(&m_fontBtn);
    }

    // 左侧工具栏标题（水平居中）
    m_toolbarTitle.Create(L"工具栏", WS_CHILD | WS_VISIBLE | SS_CENTER,
        CRect(0, 0, 10, 10), this);
    m_toolbarTitle.SetFont(&m_fontBtn);

    // 左侧图标工具栏悬停提示
    m_tooltip.Create(this, TTS_ALWAYSTIP);
    m_tooltip.Activate(TRUE);
    for (int i = 0; i < kSideCount; i++) {
        int idx = -1;
        for (int j = 0; j < kBtnCount; j++) if (kToolBtns[j].id == kSideBarIds[i]) { idx = j; break; }
        if (idx >= 0) m_tooltip.AddTool(&m_btn[idx], kSideTips[i]);
    }

    m_viewport.SetOwner(this);
    m_viewport.CreateView(this, CRect(0, 0, 10, 10));

    m_tree.SetOwner(this);
    m_tree.CreateTree(this, CRect(0, 0, 10, 10));

    m_props.SetOwner(this);
    m_props.CreatePanel(this, CRect(0, 0, 10, 10));

    // 顶部面板 Tab：时间轴 | 输出 | 编译器错误（仿 Flash 底部面板，先建 Tab 再嵌时间轴）
    m_tab.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), this, 9000);
    m_tab.SetFont(&m_fontSmall);
    m_tab.InsertItem(0, L"时间轴");
    m_tab.InsertItem(1, L"输出");
    m_tab.InsertItem(2, L"编译器错误");
    m_tab.SetCurSel(0);
    m_traceEdt.Create(WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                      CRect(0, 0, 10, 10), &m_tab, 9001);
    m_traceEdt.SetFont(&m_fontSmall);
    m_traceStat.Create(L"", WS_CHILD | SS_LEFT, CRect(0, 0, 10, 10), &m_tab, 9002);
    m_traceStat.SetFont(&m_fontSmall);
    m_errEdt.Create(WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                    CRect(0, 0, 10, 10), &m_tab, 9003);
    m_errEdt.SetFont(&m_fontSmall);
    m_errStat.Create(L"", WS_CHILD | SS_LEFT, CRect(0, 0, 10, 10), &m_tab, 9004);
    m_errStat.SetFont(&m_fontSmall);

    m_timeline.SetOwner(this);
    m_timeline.CreateTimeline(this, CRect(0, 0, 10, 10));
    m_timeline.SetParent(&m_tab);   // 时间轴嵌入顶部面板 Tab 页1（消息仍发给 MainWnd）

    // 文档 Tab：工程.s3d | 工程.ss（时间轴下面、场景导航"场景1"上面，仿 Animate 文档标签）
    m_fontMono.CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          FIXED_PITCH | FF_MODERN, _T("Courier New"));
    m_docTab.Create(WS_CHILD | WS_VISIBLE | TCS_OWNERDRAWFIXED | TCS_TOOLTIPS, CRect(0, 0, 10, 10), this, 9010);
    m_docTab.SetFont(&m_fontSmall);
    m_docTab.InsertItem(0, L".s3d");
    m_docTab.InsertItem(1, L".ss");
    m_docTab.SetCurSel(0);
    AfxInitRichEdit();   // .ss 编辑器用 RICHEDIT（AS3 式关键字着色）
    m_docEdt.Create(WS_CHILD | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
                    ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
                    CRect(0, 0, 10, 10), this, 9007);
    m_docEdt.SetEventMask(m_docEdt.GetEventMask() | ENM_CHANGE);   // 必须启用 EN_CHANGE，否则输入不触发自动补全/着色
    m_docEdt.SetFont(&m_fontMono);
    m_docEdt.SetBackgroundColor(FALSE, RGB(255, 255, 255));
    // .ss 自动补全候选列表（弹出式，仿 AS3 代码提示下拉框）
    // 注意：CListBox::Create 内部走 CWnd::Create，其断言禁止 WS_POPUP（wincore.cpp:777）；
    // 弹出窗口必须用 CreateEx 直接创建 LISTBOX，且创建时 nID 须为 0（WS_POPUP 下 nID 被当作
    // 菜单句柄传入 CreateWindowEx，非 0 会导致创建失败），创建成功后再补设控件 ID 以保留
    // LBN_DBLCLK 通知路由；LBS_NOINTEGRALHEIGHT 仅适用于 owner-draw 列表，不使用
    m_autoList.CreateEx(0, _T("LISTBOX"), NULL,
                        WS_POPUP | WS_VSCROLL | LBS_NOTIFY | WS_BORDER,
                        CRect(0, 0, 10, 10), this, 0);
    m_autoList.SetDlgCtrlID(9401);
    m_autoList.SetFont(&m_fontMono);
    m_autoVisible = false;
    m_autoInserted = false;
    m_docScriptMode = false;
    UpdateDocTabs();

    // 独立“脚本”浮动窗口（仿 Flash 动作面板）
    m_scriptWnd.CreateScript(this);
    m_scriptWnd.SetFont(&m_fontSmall);

    m_status.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(0, 0, 10, 10), this, 0);
    m_status.SetFont(&m_fontSmall);

    // 加载工具栏图标表（exe 同目录 toolbar.png），失败则回退 GDI 绘制
    m_iconLoaded = false;
    CString exeDir;
    ::GetModuleFileName(NULL, exeDir.GetBuffer(MAX_PATH), MAX_PATH);
    exeDir.ReleaseBuffer();
    int pos = exeDir.ReverseFind(_T('\\'));
    if (pos >= 0) exeDir = exeDir.Left(pos);
    CString iconPath = exeDir + _T("\\toolbar.png");
    if (SUCCEEDED(m_iconSheet.Load(iconPath)) && m_iconSheet.GetWidth() > 0) {
        // 把接近白色的背景像素统一成纯白，便于 TransparentBlt 做透明（内存级快速处理）
        int iw = m_iconSheet.GetWidth(), ih = m_iconSheet.GetHeight();
        int pitch = m_iconSheet.GetPitch();
        int bpp = m_iconSheet.GetBPP() / 8;
        if (pitch && (bpp == 3 || bpp == 4)) {
            BYTE* base = (BYTE*)m_iconSheet.GetBits();
            for (int y = 0; y < ih; y++) {
                BYTE* line = base + (INT_PTR)y * pitch;
                for (int x = 0; x < iw; x++) {
                    BYTE* px = line + (INT_PTR)x * bpp;   // BGRA / BGR: [0]=B [1]=G [2]=R
                    if (px[0] > 232 && px[1] > 232 && px[2] > 232) { px[0] = 255; px[1] = 255; px[2] = 255; }
                }
            }
            m_iconLoaded = true;
        }
    }

    // 初始：新建空文件（不再加载小房子示例）
    m_script.SetPlaybackHook(&CMainWnd::PlaybackHook, this);
    m_scene.Clear();
    m_selected = NULL;
    m_selName.clear();
    m_scene.fps = 24;
    m_scene.totalFrames = 120;
    m_scene.loopPlay = true;
    SetScriptEditText(L"");
    m_scriptWnd.SetPerFrame(false);
    m_scene.SetFrame(0);
    // 主窗口图标：运行时绘制的等距立方体（小图标 32px 位图、大图标 48px 位图，系统缩放显示更清晰）
    m_hIconSmall = CreateCubeIcon(32);
    m_hIconLarge = CreateCubeIcon(48);
    if (m_hIconSmall) SetIcon(m_hIconSmall, FALSE);
    if (m_hIconLarge) SetIcon(m_hIconLarge, TRUE);
    BuildMenu();
    RefreshAll();
    return 0;
}

void CMainWnd::OnDestroy() {
    if (m_playing) KillTimer(1);
    if (m_hIconSmall) { ::DestroyIcon(m_hIconSmall); m_hIconSmall = NULL; }
    if (m_hIconLarge) { ::DestroyIcon(m_hIconLarge); m_hIconLarge = NULL; }
    CWnd::OnDestroy();
}

void CMainWnd::SetScriptEditText(const std::wstring& text) {
    m_scriptWnd.SetText(text.c_str());
    m_script.SetText(text);
}

// ------------------------------------------------------------ 布局（Flash 风格）
void CMainWnd::Layout() {
    CRect rc;
    GetClientRect(&rc);
    int W = rc.Width(), H = rc.Height();
    if (W < 200 || H < 200) return;

    const int TOP_H = 210, TOOL_W = 76, RIGHT_W = 252, STATUS_H = 44, M = 6;
    const int btnH = 24, btnGap = 3;
    int bottomTop = H - STATUS_H - 6;

    // 顶部：时间轴占据顶部（仿 Flash，位于场景上方）；左侧留出图标工具栏列，避免重叠
    int toolRight = M + TOOL_W;   // 工具栏列右边界
    int contentY = 4;
    int contentH = TOP_H - 6;
    if (contentH < 20) contentH = 20;
    // 顶部面板 Tab（时间轴 | 输出 | 编译器错误）
    m_tab.MoveWindow(toolRight, contentY, W - toolRight - M, contentH);
    // Tab 页区域（调整标签栏后的客户区）
    CRect tabRc(toolRight, contentY, toolRight + (W - toolRight - M), contentY + contentH);
    CRect pageRc = tabRc;
    m_tab.SendMessage(TCM_ADJUSTRECT, FALSE, (LPARAM)&pageRc);
    int px = pageRc.left - tabRc.left, py = pageRc.top - tabRc.top;
    int pw = pageRc.Width(), ph = pageRc.Height();
    m_timeline.MoveWindow(px, py, pw, ph);
    m_traceEdt.MoveWindow(px, py, pw, ph - 22);
    m_traceStat.MoveWindow(px + 4, py + ph - 20, 200, 18);
    m_errEdt.MoveWindow(px, py, pw, ph - 22);
    m_errStat.MoveWindow(px + 4, py + ph - 20, 200, 18);
    ShowTabPage(m_tab.GetCurSel());

    // 文档 Tab（工程.s3d | 工程.ss）：位于时间轴面板下面、场景导航"场景1"上面（仿 Animate 文档标签）
    m_docTab.MoveWindow(toolRight, 4 + TOP_H, W - toolRight - M, 26);

    // 场景区起点（文档 Tab 之下）；左侧工具栏从顶部开始，与时间轴顶端对齐
    int viewTop = 4 + TOP_H + 26;
    if (viewTop > bottomTop - 80) viewTop = bottomTop - 80;

    // 视口外编辑导航栏（GDI，场景与文本分开）：位于视口正上方，不叠加在场景 GL 上
    const int NAV_H = 26;
    if (viewTop + NAV_H > bottomTop - 40) viewTop = bottomTop - 40 - NAV_H;
    m_navRect.SetRect(TOOL_W + M, viewTop, W - TOOL_W - RIGHT_W - 3 * M, viewTop + NAV_H);

    // 左侧：图标工具栏（竖排单列），顶端与时间轴对齐；顶部显示"工具栏"标题，下方按 kSideBarIds 顺序排列按钮
    // 功能组之间留出加大的间隔（分组划分），间隔中央绘制一条分隔浅线（OnEraseBkgnd 画）
    const int titleH = 20;
    int x = M, y = 4;
    m_toolbarTitle.MoveWindow(x, y, TOOL_W, titleH);
    y += titleH + 14;   // 标题与第1个按钮间距
    const int divGap = 12;             // 分组间隔（在 btnGap 之上追加）
    m_sideDivs.clear();
    for (int i = 0; i < kSideCount; i++) {
        if (i > 0 && kSideGroupStart[i]) {
            int divY = y + divGap / 2;
            m_sideDivs.push_back(CRect(x + 6, divY, x + TOOL_W - 10, divY));
            y += divGap;
        }
        for (int j = 0; j < kBtnCount; j++) {
            if (kToolBtns[j].id == kSideBarIds[i]) {
                m_btn[j].ShowWindow(SW_SHOW);
                m_btn[j].MoveWindow(x, y, TOOL_W - 4, btnH);
                y += btnH + btnGap;
                break;
            }
        }
    }
    for (int i = 0; i < kBtnCount; i++)
        if (!IsSideBar(kToolBtns[i].id)) m_btn[i].ShowWindow(SW_HIDE);

    // 文档 Tab 切换：.ss 激活时脚本编辑器占视口区域，隐藏场景编辑（顶部面板与工具栏保留）
    m_docEdt.MoveWindow(TOOL_W + M, 4 + TOP_H + 26, W - TOOL_W - 2 * M, bottomTop - (4 + TOP_H + 26));
    if (m_docScriptMode) {
        m_viewport.ShowWindow(SW_HIDE);
        m_tree.ShowWindow(SW_HIDE);
        m_props.ShowWindow(SW_HIDE);
        m_navRect.SetRectEmpty();   // 脚本模式不绘制"场景1"导航
        m_docEdt.ShowWindow(SW_SHOW);
    } else {
        m_docEdt.ShowWindow(SW_HIDE);
        m_viewport.ShowWindow(SW_SHOW);
        m_tree.ShowWindow(SW_SHOW);
        m_props.ShowWindow(SW_SHOW);
    }

    // 右侧：属性栏（上）+ 场景树（下）
    int rightX = W - RIGHT_W - M;
    int rightW = RIGHT_W;
    int split = viewTop + (bottomTop - viewTop) * 66 / 100;
    if (split < viewTop + 300) split = viewTop + 300;
    if (split > bottomTop - 80) split = bottomTop - 80;
    m_props.MoveWindow(rightX, viewTop, rightW, split - viewTop);
    m_tree.MoveWindow(rightX, split + 4, rightW, bottomTop - split - 4);

    // 中间：场景视口（下移导航栏高度，导航栏在视口外、场景上方）
    m_viewport.MoveWindow(TOOL_W + M, viewTop + NAV_H, W - TOOL_W - RIGHT_W - 3 * M, bottomTop - (viewTop + NAV_H));

    // 状态栏
    m_status.MoveWindow(0, H - STATUS_H, W, STATUS_H);

    RefreshNavBar();   // 布局完成后重算视口外导航栏并请求重画
}

// 视口外编辑导航栏：重算各导航项矩形（←返回 / 场景1 / 各级组选项卡），并请求重画
void CMainWnd::RefreshNavBar() {
    m_navReturn.SetRectEmpty();
    m_navRoot.SetRectEmpty();
    m_navItems.clear();
    m_navInside = m_scene.InsideGroup();
    if (m_navRect.Width() <= 0 || m_navRect.Height() <= 0) return;
    CClientDC dc(this);
    CFont* old = dc.SelectObject(&m_fontSmall);
    int x = m_navRect.left + 6;
    int y = m_navRect.top + 3;
    int h = m_navRect.Height() - 6;
    {
        CString s = _T("场景1");
        CSize sz = dc.GetTextExtent(s);
        int wd = sz.cx + 16 + 15;   // 15：名称前图标占位
        m_navRoot = CRect(x, y, x + wd, y + h);
        x += wd;
    }
    std::vector<Object3D*> chain = EditPath();
    for (size_t i = 0; i < chain.size(); i++) {
        CString s = chain[i]->name.c_str();
        CSize sz = dc.GetTextExtent(s);
        int wd = sz.cx + 16 + 15;   // 15：名称前图标占位
        CRect rc(x, y, x + wd, y + h);
        m_navItems.push_back(std::make_pair(rc, chain[i]));
        x += wd;
    }
    dc.SelectObject(old);
    // bErase 必须为 TRUE：导航栏只画在 OnEraseBkgnd 里，FALSE 不会触发 WM_ERASEBKGND，
    // 进入/退出组后选项卡就不会重画（看起来像没出现）
    InvalidateRect(m_navRect, TRUE);
}

// 导航栏名称前的小图标（Flash 风格，纯 GDI 手绘，不引入资源文件）：
// scene=true 画场景拍板；否则画元件胶片夹
static void DrawNavIcon(CDC* pDC, int x, int y, bool scene) {
    const COLORREF ink = RGB(96, 101, 110);      // 图标主体深灰
    const COLORREF paper = RGB(240, 241, 245);   // 细节亮色
    CBrush br(ink), brW(paper);
    CPen pen(PS_SOLID, 1, ink), penW(PS_SOLID, 1, paper);
    CBrush* ob = pDC->SelectObject(&br);
    CPen* op = pDC->SelectObject(&pen);
    if (scene) {
        // 场景：拍板（斜顶盖 + 底盒 + 白色斜刻痕）
        POINT lid[4] = { {x, y + 7}, {x + 2, y + 2}, {x + 12, y + 2}, {x + 12, y + 7} };
        pDC->Polygon(lid, 4);
        pDC->FillRect(CRect(x, y + 7, x + 12, y + 12), &br);
        pDC->SelectObject(&penW);
        pDC->MoveTo(x + 4, y + 2); pDC->LineTo(x + 2, y + 7);
        pDC->MoveTo(x + 9, y + 2); pDC->LineTo(x + 7, y + 7);
    } else {
        // 元件(MovieClip)：圆角方块 + 2×2 白点（胶片片孔）
        pDC->RoundRect(x, y, x + 12, y + 12, 4, 4);
        pDC->FillRect(CRect(x + 3, y + 3, x + 5, y + 5), &brW);
        pDC->FillRect(CRect(x + 7, y + 3, x + 9, y + 5), &brW);
        pDC->FillRect(CRect(x + 3, y + 7, x + 5, y + 9), &brW);
        pDC->FillRect(CRect(x + 7, y + 7, x + 9, y + 9), &brW);
    }
    pDC->SelectObject(ob);
    pDC->SelectObject(op);
}

// 主窗口背景：先擦除整个客户区（清除子窗口 resize/移动后旧内容残留在主窗口背景的痕迹），
// 再画视口外导航栏（GDI，原生清晰、不闪）
BOOL CMainWnd::OnEraseBkgnd(CDC* pDC) {
    CRect rc;
    GetClientRect(&rc);
    CBrush bgAll(RGB(240, 241, 245));
    pDC->FillRect(&rc, &bgAll);
    // 左侧工具栏功能分组分隔线（浅灰，位于组间隔中央；布局时记录矩形）
    if (!m_sideDivs.empty()) {
        CPen pen(PS_SOLID, 1, RGB(205, 208, 215));
        CPen* oldPen = pDC->SelectObject(&pen);
        for (size_t i = 0; i < m_sideDivs.size(); i++) {
            pDC->MoveTo(m_sideDivs[i].left, m_sideDivs[i].top);
            pDC->LineTo(m_sideDivs[i].right, m_sideDivs[i].bottom);
        }
        pDC->SelectObject(oldPen);
    }
    if (m_navRect.Width() > 0 && m_navRect.Height() > 0) {
        CBrush bg(RGB(235, 238, 242));
        pDC->FillRect(m_navRect, &bg);
        CFont* old = pDC->SelectObject(&m_fontSmall);
        int oldBk = pDC->SetBkMode(TRANSPARENT);
        Object3D* cur = m_scene.editRoot;
        bool rootCur = !m_navInside;
        pDC->SetTextColor(RGB(0, 0, 0));
        if (!m_navRoot.IsRectEmpty()) {
            DrawNavIcon(pDC, m_navRoot.left + 3, m_navRoot.top + (m_navRoot.Height() - 12) / 2, true);
            CRect trc = m_navRoot; trc.left += 15;
            pDC->SelectObject(rootCur ? &m_navBoldFont : &m_fontSmall);
            pDC->DrawText(_T("场景1"), trc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        for (size_t i = 0; i < m_navItems.size(); i++) {
            bool curThis = (m_navItems[i].second == cur);
            DrawNavIcon(pDC, m_navItems[i].first.left + 3, m_navItems[i].first.top + (m_navItems[i].first.Height() - 12) / 2, false);
            CRect trc = m_navItems[i].first; trc.left += 15;
            pDC->SelectObject(curThis ? &m_navBoldFont : &m_fontSmall);
            pDC->DrawText(m_navItems[i].second->name.c_str(), trc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        pDC->SetBkMode(oldBk);
        pDC->SelectObject(old);
    }
    return TRUE;   // 已处理背景
}

// 命中视口外导航栏：←返回 / 场景1回根 / 层级项跳转
void CMainWnd::OnLButtonUp(UINT nFlags, CPoint point) {
    if (m_autoVisible) {   // 点击候选列表外任意处：关闭自动补全
        CRect rc; m_autoList.GetWindowRect(&rc);
        CPoint sp = point; ClientToScreen(&sp);
        if (!rc.PtInRect(sp)) HideAutoComplete();
    }
    if (m_navRect.PtInRect(point)) {
        if (m_navInside && m_navReturn.PtInRect(point)) { ExitGroup(); return; }
        if (m_navRoot.PtInRect(point)) { SetEditLevel(NULL); return; }
        for (size_t i = 0; i < m_navItems.size(); i++)
            if (m_navItems[i].first.PtInRect(point)) { SetEditLevel(m_navItems[i].second); return; }
    }
    CWnd::OnLButtonUp(nFlags, point);
}

// 播放期间把键盘状态喂给脚本引擎（Key.isDown 查询用）
// 文档类脚本常驻整个运行周期：无论主脚本开关，都接收键盘状态与事件
void CMainWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
    if (m_playing) {
        if (m_scene.scriptPerFrame) m_script.SetKeyDown((int)nChar);
        m_docScript.SetKeyDown((int)nChar);
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}
void CMainWnd::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) {
    if (m_playing) {
        if (m_scene.scriptPerFrame) m_script.SetKeyUp((int)nChar);
        m_docScript.SetKeyUp((int)nChar);
    }
    CWnd::OnKeyUp(nChar, nRepCnt, nFlags);
}

BOOL CMainWnd::PreTranslateMessage(MSG* pMsg) {
    if (m_tooltip.m_hWnd) m_tooltip.RelayEvent(pMsg);
    // 播放期间拦截键盘消息喂给脚本引擎（无论哪个子窗口有焦点都能生效）
    if (m_playing) {
        if (m_scene.scriptPerFrame) {
            if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
                m_script.SetKeyDown((int)pMsg->wParam);
            else if (pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP)
                m_script.SetKeyUp((int)pMsg->wParam);
        }
        // 文档类常驻：全程接收键盘状态（AS3 文档类生命周期）
        if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
            m_docScript.SetKeyDown((int)pMsg->wParam);
        else if (pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP)
            m_docScript.SetKeyUp((int)pMsg->wParam);
    }
    // .ss 自动补全列表键盘：↑↓ 选择、回车/Tab 插入、Esc 关闭、光标移动/退格关闭
    if (m_autoVisible && pMsg->message == WM_KEYDOWN) {
        switch (pMsg->wParam) {
            case VK_UP: { int i = m_autoList.GetCurSel(); if (i > 0) m_autoList.SetCurSel(i - 1); return TRUE; }
            case VK_DOWN: { int i = m_autoList.GetCurSel(); int n = m_autoList.GetCount(); if (i < n - 1) m_autoList.SetCurSel(i + 1); return TRUE; }
            case VK_RETURN:
            case VK_TAB: InsertAutoItem(); return TRUE;
            case VK_ESCAPE: HideAutoComplete(); return TRUE;
            case VK_LEFT: case VK_RIGHT: case VK_HOME: case VK_END:
            case VK_PRIOR: case VK_NEXT: case VK_BACK:
                HideAutoComplete(); break;
            default: break;   // 普通字符：交给 RichEdit 输入，EN_CHANGE 更新过滤
        }
    }
    // .ss 编辑器回车：自动缩进（继承上一行缩进；行尾 { 加一级；行首 } 减一级）。
    // 不做焦点/句柄判断——RichEdit 键盘消息的 hwnd 在某些焦点时序下取不到编辑框，
    // 而 .ss 模式下用户按回车基本都在编辑代码，无条件拦截保证必生效
    if (m_docScriptMode && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
        AutoIndentReturn();
        return TRUE;
    }
    // .ss 文档类编辑页：Ctrl+A/C/V/X/Z 显式作用于脚本编辑器。
    // CEdit 原生不支持 Ctrl+A（会响蜂鸣），需自处理；其余显式发送命令保证焦点不在编辑框时也生效
    if (m_docScriptMode && pMsg->message == WM_KEYDOWN &&
        (GetKeyState(VK_CONTROL) & 0x8000)) {
        switch (pMsg->wParam) {
            case 'A': m_docEdt.SetFocus(); m_docEdt.SetSel(0, -1); return TRUE;
            case 'C': m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_COPY); return TRUE;
            case 'V': m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_PASTE); return TRUE;
            case 'X': m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_CUT); return TRUE;
            case 'Z': m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_UNDO); return TRUE;
        }
    }
    // 全局快捷键：Ctrl+F 格式化 .ss / Ctrl+S 保存 / Ctrl+O 打开 / Ctrl+N 新建（帮助文档已声明；.ss 页保存当前文件）
    if (pMsg->message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) &&
        !(GetKeyState(VK_MENU) & 0x8000)) {
        switch (pMsg->wParam) {
            case 'F': DoFormatDoc(); return TRUE;
            case 'S': DoSave(); return TRUE;
            case 'O': DoOpen(); return TRUE;
            case 'N': DoNew(); return TRUE;
        }
    }
    // Ctrl+A：全局全选场景元件（视口/场景树/时间轴等焦点均生效）；
    // 焦点在文本框/编辑框（属性面板数值框、脚本编辑区）时保留系统全选文本，不拦截
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'A' &&
        (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
        CWnd* f = GetFocus();
        if (!f || !f->IsKindOf(RUNTIME_CLASS(CEdit))) {
            SelectAllObjects();
            return TRUE;
        }
    }
    return CWnd::PreTranslateMessage(pMsg);
}

// ------------------------------------------------------------ 顶部菜单（Flash 风格）
void CMainWnd::BuildMenu() {
    CMenu top;
    top.CreateMenu();
    auto popup = [](CMenu& m, std::initializer_list<std::pair<UINT, const wchar_t*>> items,
                    bool separator = false) {
        m.CreatePopupMenu();
        for (auto& it : items) m.AppendMenu(MF_STRING, it.first, it.second);
        if (separator) m.AppendMenu(MF_SEPARATOR);
    };
    {   // 文件
        CMenu f; popup(f, { {IDC_BTN_NEW, L"新建"}, {IDC_BTN_OPEN, L"打开"},
                            {IDC_BTN_SAVE, L"保存"}, {IDC_BTN_EXPORT, L"导出"},
                            {IDC_BTN_DOC_CLASS, L"文档类脚本…"} }, true);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("文件(&F)")); f.Detach();
    }
    {   // 编辑
        CMenu f; popup(f, { {IDC_BTN_UNDO, L"撤销"}, {IDC_BTN_DUPLICATE, L"复制"},
                            {IDC_BTN_CUT, L"剪切"}, {IDC_BTN_DELETE, L"删除"},
                            {IDC_BTN_GROUP, L"组合"},
                            {IDC_BTN_UNGROUP, L"打散"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("编辑(&E)")); f.Detach();
    }
    {   // 修改
        CMenu f; popup(f, { {IDC_ALIGN_LEFT, L"左对齐"}, {IDC_ALIGN_HCENTER, L"水平居中"},
                            {IDC_ALIGN_RIGHT, L"右对齐"}, {IDC_ALIGN_TOP, L"顶对齐"},
                            {IDC_ALIGN_VCENTER, L"垂直居中"}, {IDC_ALIGN_BOTTOM, L"底对齐"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("修改(&M)")); f.Detach();
    }
    {   // 视图
        CMenu f; popup(f, { {IDC_BTN_RESETVIEW, L"重置视角"}, {IDC_BTN_TOOL_PAN, L"平移视角"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("视图(&V)")); f.Detach();
    }
    {   // 插入
        CMenu f; popup(f, { {IDC_BTN_BOX, L"立方体"}, {IDC_BTN_SPHERE, L"球"},
                            {IDC_BTN_CYLINDER, L"圆柱"}, {IDC_BTN_CONE, L"圆锥"},
                            {IDC_BTN_TORUS, L"圆环"}, {IDC_BTN_PLANE, L"平面"},
                            {IDC_BTN_LEAF, L"绿叶"}, {IDC_BTN_TOOL_SPLINE, L"曲线"},
                            {IDC_BTN_TEXT3D, L"文本"}, {IDC_BTN_TUBE, L"方管"},
                            {IDC_BTN_RTUBE, L"圆管"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("插入(&I)")); f.Detach();
    }
    {   // 示例
        CMenu f; popup(f, { {IDC_BTN_EX0, L"House"}, {IDC_BTN_EX1, L"Windmill"},
                            {IDC_BTN_EX2, L"SolarSystem"}, {IDC_BTN_EX3, L"BounceBall"},
                            {IDC_BTN_EX4, L"DocClass"}, {IDC_BTN_EX5, L"TankBattle"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("示例(&S)")); f.Detach();
    }
    {   // 控制
        CMenu f; popup(f, { {IDC_BTN_PLAY, L"播放"}, {IDC_BTN_STOP, L"停止"},
                            {IDC_BTN_RUN, L"运行脚本"}, {IDC_BTN_DEBUG, L"调试"},
                            {IDC_BTN_ADDKEY, L"插入关键帧"}, {IDC_BTN_DELKEY, L"删除关键帧"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("控制(&O)")); f.Detach();
    }
    {   // 窗口
        CMenu f; popup(f, { {IDC_OPEN_SCRIPT, L"脚本窗口"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("窗口(&W)")); f.Detach();
    }
    {   // 帮助：使用说明 + SuperScript 文档
        CMenu f; popup(f, { {IDC_BTN_HELP, L"使用说明"}, {IDC_BTN_SUPERDOC, L"SuperScript文档"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("帮助(&H)")); f.Detach();
    }
    SetMenu(&top);
    top.Detach();   // 菜单句柄由窗口接管
}

// ------------------------------------------------------------ 左侧图标工具栏绘制
void CMainWnd::DrawToolIcon(CDC* dc, CRect rc, UINT id, bool active) {
    int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
    // 浅色工具栏：普通/激活 两套配色，统一 2px 描边，图元用明暗三面填充，图标严格居中
    COLORREF line = active ? RGB(55, 105, 195) : RGB(64, 68, 76);
    COLORREF f1 = active ? RGB(110, 150, 215) : RGB(146, 150, 158);   // 深面
    COLORREF f2 = active ? RGB(150, 185, 238) : RGB(172, 176, 184);   // 中面
    COLORREF f3 = active ? RGB(192, 214, 246) : RGB(198, 202, 208);   // 浅面
    COLORREF leaf = active ? RGB(70, 150, 70) : RGB(88, 158, 88);     // 绿叶描边
    COLORREF leafLite = active ? RGB(160, 210, 160) : RGB(182, 216, 182); // 绿叶叶肉
    COLORREF bgFill = active ? RGB(215, 230, 250) : RGB(244, 244, 244);
    CPen pen(PS_SOLID, 2, line);
    CPen* old = dc->SelectObject(&pen);

    auto fillPoly = [&](const POINT* p, int n, COLORREF fill) {
        CBrush br(fill); CBrush* ob = dc->SelectObject(&br);
        dc->Polygon(p, n);
        dc->SelectObject(ob);
    };
    auto fillRect = [&](int l, int t, int r, int b, COLORREF fill) {
        CBrush br(fill); CBrush* ob = dc->SelectObject(&br);
        dc->Rectangle(l, t, r, b);
        dc->SelectObject(ob);
    };
    auto fillEllipse = [&](int l, int t, int r, int b, COLORREF fill) {
        CBrush br(fill); CBrush* ob = dc->SelectObject(&br);
        dc->Ellipse(l, t, r, b);
        dc->SelectObject(ob);
    };

    switch (id) {
    case IDC_BTN_TOOL_SELECT: {   // 选择：实心鼠标指针
        POINT a[] = { {cx - 12, cy - 11}, {cx + 9, cy - 5}, {cx - 3, cy - 4}, {cx - 5, cy + 9}, {cx - 10, cy + 7}, {cx - 8, cy - 4} };
        fillPoly(a, 6, line);
        break;
    }
    case IDC_BTN_TOOL_MOVE: {     // 移动：十字四箭头
        dc->MoveTo(cx, cy - 12); dc->LineTo(cx, cy + 12);
        dc->MoveTo(cx - 12, cy); dc->LineTo(cx + 12, cy);
        dc->MoveTo(cx, cy - 12); dc->LineTo(cx - 4, cy - 6); dc->MoveTo(cx, cy - 12); dc->LineTo(cx + 4, cy - 6);
        dc->MoveTo(cx, cy + 12); dc->LineTo(cx - 4, cy + 6); dc->MoveTo(cx, cy + 12); dc->LineTo(cx + 4, cy + 6);
        dc->MoveTo(cx - 12, cy); dc->LineTo(cx - 6, cy - 4); dc->MoveTo(cx - 12, cy); dc->LineTo(cx - 6, cy + 4);
        dc->MoveTo(cx + 12, cy); dc->LineTo(cx + 6, cy - 4); dc->MoveTo(cx + 12, cy); dc->LineTo(cx + 6, cy + 4);
        break;
    }
    case IDC_BTN_TOOL_ROTATE: {   // 旋转：环形箭头
        dc->Arc(cx - 13, cy - 9, cx + 13, cy + 9, cx + 13, cy, cx - 13, cy);
        dc->MoveTo(cx + 11, cy - 4); dc->LineTo(cx + 12, cy - 9); dc->LineTo(cx + 7, cy - 8);
        dc->MoveTo(cx - 11, cy + 5); dc->LineTo(cx - 12, cy + 9); dc->LineTo(cx - 7, cy + 3);
        break;
    }
    case IDC_BTN_TOOL_SCALE: {    // 缩放：放大镜
        dc->Ellipse(cx - 9, cy - 11, cx + 6, cy + 4);
        dc->MoveTo(cx + 5, cy + 3); dc->LineTo(cx + 11, cy + 9);
        break;
    }
    case IDC_BTN_TOOL_PAN: {      // 平移：四向箭头
        dc->MoveTo(cx, cy - 12); dc->LineTo(cx, cy + 12);
        dc->MoveTo(cx - 12, cy); dc->LineTo(cx + 12, cy);
        dc->MoveTo(cx, cy - 12); dc->LineTo(cx - 4, cy - 6); dc->MoveTo(cx, cy - 12); dc->LineTo(cx + 4, cy - 6);
        dc->MoveTo(cx, cy + 12); dc->LineTo(cx - 4, cy + 6); dc->MoveTo(cx, cy + 12); dc->LineTo(cx + 4, cy + 6);
        dc->MoveTo(cx - 12, cy); dc->LineTo(cx - 6, cy - 4); dc->MoveTo(cx - 12, cy); dc->LineTo(cx - 6, cy + 4);
        dc->MoveTo(cx + 12, cy); dc->LineTo(cx + 6, cy - 4); dc->MoveTo(cx + 12, cy); dc->LineTo(cx + 6, cy + 4);
        break;
    }
    case IDC_BTN_TOOL_SPLINE: {   // 曲线：S 形波浪
        POINT p[] = { {cx - 16, cy + 4}, {cx - 9, cy - 9}, {cx - 3, cy - 9}, {cx + 5, cy + 9}, {cx + 11, cy + 9}, {cx + 16, cy - 2} };
        dc->PolyBezier(p, 6);
        break;
    }
    case IDC_BTN_BOX: {           // 立方体：三个明暗面
        POINT top[] = { {cx - 15, cy - 6}, {cx - 3, cy - 11}, {cx + 15, cy - 11}, {cx + 3, cy - 6} };
        fillPoly(top, 4, f3);
        POINT front[] = { {cx - 15, cy - 6}, {cx + 3, cy - 6}, {cx + 3, cy + 7}, {cx - 15, cy + 7} };
        fillPoly(front, 4, f2);
        POINT right[] = { {cx + 3, cy - 6}, {cx + 15, cy - 11}, {cx + 15, cy + 2}, {cx + 3, cy + 7} };
        fillPoly(right, 4, f1);
        dc->MoveTo(cx - 15, cy - 6); dc->LineTo(cx - 3, cy - 11); dc->LineTo(cx + 15, cy - 11);
        dc->LineTo(cx + 15, cy + 2); dc->LineTo(cx + 3, cy + 7); dc->LineTo(cx - 15, cy + 7); dc->LineTo(cx - 15, cy - 6);
        dc->MoveTo(cx + 3, cy - 6); dc->LineTo(cx + 15, cy - 11);
        dc->MoveTo(cx + 3, cy - 6); dc->LineTo(cx + 3, cy + 7);
        break;
    }
    case IDC_BTN_SPHERE: {        // 球：高光+阴影
        fillEllipse(cx - 11, cy - 11, cx + 11, cy + 11, f2);
        fillEllipse(cx - 8, cy - 8, cx - 2, cy - 3, RGB(255, 255, 255));
        fillEllipse(cx + 1, cy + 1, cx + 10, cy + 10, f1);
        dc->Ellipse(cx - 11, cy - 11, cx + 11, cy + 11);
        break;
    }
    case IDC_BTN_CYLINDER: {      // 圆柱：顶面+侧面+底弧
        fillRect(cx - 15, cy - 2, cx + 15, cy + 7, f2);
        fillEllipse(cx - 15, cy - 11, cx + 15, cy - 2, f3);
        dc->MoveTo(cx - 15, cy - 6); dc->LineTo(cx - 15, cy + 7);
        dc->MoveTo(cx + 15, cy - 6); dc->LineTo(cx + 15, cy + 7);
        dc->Arc(cx - 15, cy + 3, cx + 15, cy + 11, cx + 15, cy + 6, cx - 15, cy + 6);
        dc->Ellipse(cx - 15, cy - 11, cx + 15, cy - 2);
        break;
    }
    case IDC_BTN_CONE: {          // 圆锥：三角+底弧
        POINT body[] = { {cx - 15, cy - 3}, {cx + 15, cy - 3}, {cx, cy + 10} };
        fillPoly(body, 3, f2);
        fillEllipse(cx - 15, cy - 3, cx + 15, cy + 3, f3);
        dc->MoveTo(cx - 15, cy - 3); dc->LineTo(cx, cy + 10); dc->LineTo(cx + 15, cy - 3);
        dc->Arc(cx - 15, cy - 3, cx + 15, cy + 3, cx + 15, cy - 3, cx - 15, cy - 3);
        break;
    }
    case IDC_BTN_TORUS: {         // 圆环：外环+内孔
        fillEllipse(cx - 16, cy - 10, cx + 16, cy + 10, f2);
        fillEllipse(cx - 5, cy - 3, cx + 5, cy + 3, bgFill);
        dc->Ellipse(cx - 16, cy - 10, cx + 16, cy + 10);
        dc->Ellipse(cx - 5, cy - 3, cx + 5, cy + 3);
        break;
    }
    case IDC_BTN_PLANE: {         // 平面：填充+对角线
        fillRect(cx - 16, cy - 6, cx + 16, cy + 6, f3);
        dc->MoveTo(cx - 16, cy - 6); dc->LineTo(cx + 16, cy + 6);
        break;
    }
    case IDC_BTN_LEAF: {          // 绿叶：叶形+叶脉
        POINT lp[] = { {cx - 14, cy + 8}, {cx - 5, cy - 10}, {cx + 12, cy - 7}, {cx + 15, cy - 3}, {cx + 11, cy + 2}, {cx - 2, cy + 11} };
        fillPoly(lp, 6, leafLite);
        dc->MoveTo(cx - 14, cy + 8); dc->LineTo(cx - 5, cy - 10); dc->LineTo(cx + 12, cy - 7);
        dc->LineTo(cx + 15, cy - 3); dc->LineTo(cx + 11, cy + 2); dc->LineTo(cx - 2, cy + 11); dc->LineTo(cx - 14, cy + 8);
        dc->MoveTo(cx - 10, cy + 4); dc->LineTo(cx - 1, cy - 5);   // 主叶脉
        break;
    }
    case IDC_BTN_TEXT3D: {        // 3D文本：立体字母 A（前面+厚度面+中横）
        POINT a[8] = { {cx-11,cy+9},{cx-4,cy-9},{cx+2,cy-9},{cx+9,cy+9},
                       {cx+3,cy+9},{cx+1,cy+3},{cx-3,cy+3},{cx-5,cy+9} };
        POINT t[8];
        for (int i = 0; i < 8; i++) { t[i].x = a[i].x - 3; t[i].y = a[i].y + 4; }
        fillPoly(t, 8, f1);                     // 厚度面（先画，深色）
        for (int i = 1; i < 8; i++) { dc->MoveTo(a[i]); dc->LineTo(t[i]); }  // 厚度连接边
        fillPoly(a, 8, f2);                     // 前面
        dc->Polygon(a, 8);
        fillRect(cx - 5, cy - 1, cx + 3, cy + 3, f3);   // A 中横（亮）
        break;
    }
    case IDC_BTN_TUBE: {         // 方形空管：外方柱+顶面，中间内孔（中空）
        fillRect(cx - 16, cy - 5, cx + 16, cy + 8, f2);      // 外柱前面
        dc->Rectangle(cx - 16, cy - 5, cx + 16, cy + 8);
        fillRect(cx - 16, cy - 13, cx + 16, cy - 5, f3);     // 顶面
        dc->Rectangle(cx - 16, cy - 13, cx + 16, cy - 5);
        fillRect(cx - 7, cy - 2, cx + 7, cy + 5, bgFill);    // 内孔（背景色，中空）
        dc->Rectangle(cx - 7, cy - 2, cx + 7, cy + 5);
        break;
    }
    case IDC_BTN_RTUBE: {        // 圆形空管：外圆筒+椭圆顶面，中间圆孔（中空）
        fillEllipse(cx - 16, cy + 1, cx + 16, cy + 11, f1);  // 底弧（深色暗示筒身）
        fillRect(cx - 16, cy - 1, cx + 16, cy + 6, f2);      // 筒身
        fillEllipse(cx - 16, cy - 9, cx + 16, cy + 3, f3);   // 顶面（椭圆）
        fillEllipse(cx - 7, cy - 4, cx + 7, cy + 2, bgFill); // 顶面内孔（背景色，中空）
        break;
    }
    case IDC_BTN_GROUP: {         // 组合：嵌套双框
        dc->Rectangle(cx - 16, cy - 8, cx - 2, cy + 4);
        dc->Rectangle(cx - 4, cy - 4, cx + 15, cy + 8);
        break;
    }
    case IDC_BTN_UNGROUP: {       // 打散：裂开两半
        dc->Rectangle(cx - 16, cy - 6, cx - 3, cy + 7);
        dc->Rectangle(cx + 3, cy - 6, cx + 16, cy + 7);
        dc->MoveTo(cx, cy - 8); dc->LineTo(cx, cy + 8);
        break;
    }
    case IDC_BTN_DUPLICATE: {     // 复制：重叠双框
        dc->Rectangle(cx - 14, cy - 7, cx - 1, cy + 5);
        dc->Rectangle(cx + 2, cy - 6, cx + 15, cy + 7);
        break;
    }
    case IDC_BTN_CUT: {           // 剪切：剪刀
        dc->MoveTo(cx - 12, cy - 9); dc->LineTo(cx - 12, cy - 2);
        dc->MoveTo(cx - 12, cy - 2); dc->LineTo(cx + 12, cy + 8);
        dc->MoveTo(cx - 12, cy - 9); dc->LineTo(cx + 12, cy - 8);
        dc->Ellipse(cx - 16, cy - 12, cx - 9, cy - 5);   // 左手柄（圆圈）
        dc->Ellipse(cx + 9, cy - 11, cx + 16, cy - 4);   // 右手柄
        break;
    }
    case IDC_BTN_DELETE: {        // 删除：垃圾桶
        dc->Rectangle(cx - 10, cy - 1, cx + 10, cy + 8);
        dc->MoveTo(cx - 12, cy - 5); dc->LineTo(cx + 12, cy - 5);
        dc->MoveTo(cx - 6, cy - 5); dc->LineTo(cx - 5, cy - 9);
        dc->MoveTo(cx - 5, cy - 9); dc->LineTo(cx + 5, cy - 9);
        dc->MoveTo(cx + 5, cy - 9); dc->LineTo(cx + 6, cy - 5);
        break;
    }
    case IDC_BTN_UNDO: {          // 撤销：左弯箭头
        dc->MoveTo(cx + 12, cy - 6); dc->LineTo(cx + 12, cy + 2);
        dc->Arc(cx - 6, cy - 9, cx + 12, cy + 9, cx + 12, cy + 2, cx - 6, cy + 2);
        dc->MoveTo(cx - 8, cy + 2); dc->LineTo(cx - 1, cy - 5);
        dc->MoveTo(cx - 8, cy + 2); dc->LineTo(cx - 1, cy + 8);
        break;
    }
    case IDC_BTN_PLAY: {          // 播放：实心三角
        POINT p[] = { {cx - 8, cy - 10}, {cx + 10, cy}, {cx - 8, cy + 10} };
        fillPoly(p, 3, line);
        break;
    }
    case IDC_BTN_STOP:            // 停止：实心方块
        fillRect(cx - 7, cy - 7, cx + 7, cy + 7, line);
        break;
    default: break;
    }
    dc->SelectObject(old);
}

// 图标按钮 owner-draw
void CMainWnd::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct) {
    if (!IsSideBar((UINT)nIDCtl)) { CWnd::OnDrawItem(nIDCtl, lpDrawItemStruct); return; }
    CDC* dc = CDC::FromHandle(lpDrawItemStruct->hDC);
    CRect rc(lpDrawItemStruct->rcItem);
    // 只高亮“当前工具”（选择/移动/旋转/缩放/平移/曲线），保证单选
    bool active = false;
    if ((UINT)nIDCtl == IDC_BTN_TOOL_SELECT && m_tool == TOOL_SELECT) active = true;
    if ((UINT)nIDCtl == IDC_BTN_TOOL_MOVE && m_tool == TOOL_MOVE) active = true;
    if ((UINT)nIDCtl == IDC_BTN_TOOL_ROTATE && m_tool == TOOL_ROTATE) active = true;
    if ((UINT)nIDCtl == IDC_BTN_TOOL_SCALE && m_tool == TOOL_SCALE) active = true;
    if ((UINT)nIDCtl == IDC_BTN_TOOL_PAN && m_tool == TOOL_PAN) active = true;
    if ((UINT)nIDCtl == IDC_BTN_TOOL_SPLINE && m_tool == TOOL_SPLINE) active = true;
    COLORREF bg = active ? RGB(215, 230, 250) : RGB(244, 244, 244);
    dc->FillSolidRect(rc, bg);
    DrawToolIcon(dc, rc, (UINT)nIDCtl, active);
    // 当前工具加粗边框
    if (active) {
        CPen pen(PS_SOLID, 2, RGB(70, 120, 200));
        CPen* old = dc->SelectObject(&pen);
        dc->SelectObject(GetStockObject(NULL_BRUSH));
        dc->Rectangle(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
        dc->SelectObject(old);
    }
}


void CMainWnd::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    Layout();
}

// ------------------------------------------------------------ 播放
// 帧推进按真实时间累积（16ms 定时器高频采样键盘，动画仍按 fps 定义速度走，
// 与 HTML 导出 requestAnimationFrame 的丝滑手感对齐）
void CMainWnd::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == 2) {   // .ss 编辑器语法着色防抖定时器（停止输入 150ms 后重扫）
        KillTimer(2);
        if (m_syntaxDirty) { m_syntaxDirty = false; ApplyDocSyntax(); }
        return;
    }
    if (nIDEvent != 1) { CWnd::OnTimer(nIDEvent); return; }
    if (!m_playing) return;
    // 键盘状态高频采样（60Hz 定时器下按键更跟手）
    // 文档类常驻整个运行周期：无论主脚本开关，都采样并分发事件；主脚本按需采样
    {
        static const int s_keys[] = { 37, 38, 39, 40, 32, 65, 68, 87, 83, 81, 69 };
        static bool s_prev[11] = { false };   // 边沿检测：0→1 触发 keyDown，1→0 触发 keyUp
        for (size_t i = 0; i < sizeof(s_keys) / sizeof(s_keys[0]); i++) {
            int k = s_keys[i];
            bool down = (::GetAsyncKeyState(k) & 0x8000) != 0;
            if (down) {
                m_docScript.SetKeyDown(k);
                if (m_scene.scriptPerFrame) m_script.SetKeyDown(k);
            } else {
                m_docScript.SetKeyUp(k);
                if (m_scene.scriptPerFrame) m_script.SetKeyUp(k);
            }
            if (down && !s_prev[i]) {
                m_docScript.FireStageEvent(L"keyDown", k);
                if (m_scene.scriptPerFrame) m_script.FireStageEvent(L"keyDown", k);
            } else if (!down && s_prev[i]) {
                m_docScript.FireStageEvent(L"keyUp", k);
                if (m_scene.scriptPerFrame) m_script.FireStageEvent(L"keyUp", k);
            }
            s_prev[i] = down;
        }
    }
    // 按真实时间累积推进动画帧（保持 fps 定义速度）
    static DWORD lastT = 0, acc = 0;
    DWORD now = ::GetTickCount();
    if (lastT == 0) lastT = now;
    DWORD dt = now - lastT;
    if (dt > 200) dt = 200;   // 防止暂停/恢复后一次补过多帧
    lastT = now;
    acc += dt;
    int frameMs = m_scene.fps > 0 ? 1000 / m_scene.fps : 40;
    bool advanced = false;
    while (acc >= (DWORD)frameMs) {
        acc -= frameMs;
        int cur = m_scene.frame;   // 时间轴停止（脚本 stop()）时沿用当前帧
        if (!m_tlStopped) {
            int nf = m_scene.frame + 1;
            // 播放结束点 = 最后一图层关键帧（而非固定的 totalFrames=120）
            int endFrame = m_scene.LastLayerKeyFrame();
            if (endFrame < 1) endFrame = m_scene.totalFrames - 1;   // 无关键帧时沿用总帧数
            if (nf > endFrame) {
                if (m_scene.loopPlay) nf = 0;
                else { nf = endFrame; Stop(); break; }
            }
            // 先更新关键帧插值，再运行逐帧脚本（脚本写显示值），最后统一渲染一次
            m_scene.SetFrame(nf);
            // 关键帧脚本（Flash 式帧动作）：进入新关键帧时执行一次（含空白关键帧）
            if (nf != m_lastKeyScriptFrame) {
                m_lastKeyScriptFrame = nf;
                ExecuteKeyFrameScripts(nf);
            }
            cur = nf;
        }
        // 文档类脚本（AS3 文档类）：整个运行周期每帧驱动（onEnterFrame / addEventListener 回调，
        // 顶层代码仅第一帧一次），不依赖主脚本开关；stop() 只停时间轴，脚本照常每帧执行
        if (!m_scene.docClass.empty()) {
            m_docScript.RunFrame(&m_scene, cur);
            // 持久化文档类写入的显示值（onEnterFrame 里 dx+=speed 等）：
            // 否则 scriptPerFrame=0 / 无关键帧时，下一帧 SetFrame 的 SyncDisplayToLocal
            // 会用旧本地值把增量覆盖回初始（"只动一下"）
            m_scene.CopyLocalFromDisplayAll();
            if (m_docScript.LastRunError()) {
                // 文档类运行错误 → Flash 式编译器错误框（自动弹出）
                std::wstring desc; int ln = ParseErrLine(m_docScript.GetLog(), desc);
                std::wstring path = m_scene.projectPath;
                if (path.empty()) path = L"（未命名工程）";
                wchar_t b[512];
                swprintf(b, 512, L"%ls，第%d行，第1列 1002: %ls", path.c_str(), ln, desc.c_str());
                std::vector<std::wstring> errs; errs.push_back(b);
                ShowCompileErrors(errs);
                Stop();
                break;
            } else if (!m_docScript.GetLog().empty()) {
                m_scriptWnd.SetOutput(m_docScript.GetLog().c_str());
                m_traceLines.push_back(m_docScript.GetLog());
            }
        }
        if (m_scene.scriptPerFrame) {
            m_script.RunFrame(&m_scene, cur);
            if (m_script.LastRunError()) {
                m_scriptWnd.SetOutput(m_script.GetLog().c_str());
                Stop();
                break;
            } else if (!m_script.GetLog().empty()) {
                m_scriptWnd.SetOutput(m_script.GetLog().c_str());
                m_traceLines.push_back(m_script.GetLog());
            }
        }
        advanced = true;
        if (m_tlStopped) break;   // stop()：时间轴停在当前帧，仅执行脚本不再推进
    }
    if (advanced) {
        m_viewport.Repaint();
        m_timeline.Invalidate(FALSE);
    }
    UpdateStatus();
}

void CMainWnd::Play() {
    if (m_playing) return;
    m_lastKeyScriptFrame = -1;   // 重新播放：关键帧脚本从头开始执行
    m_traceLines.clear();        // 重新播放：trace 输出从头收集
    // 每次编译/运行：输出页与编译器错误页清空重置（旧信息不保留）
    m_traceEdt.SetWindowText(_T(""));
    m_traceStat.SetWindowText(_T(""));
    m_errEdt.SetWindowText(_T(""));
    m_errStat.SetWindowText(_T(""));
    // 文档类脚本：播放开始时执行一次（Flash 文档类初始化）
    if (!m_scene.docClass.empty()) {
        m_docScript.SetText(m_scene.docClass);
        std::wstring err;
        if (!m_docScript.EnsureParsed(err)) {
            // 文档类语法错误 → Flash 式编译器错误框（自动弹出）
            std::wstring desc; int ln = ParseErrLine(err, desc);
            std::wstring path = m_scene.projectPath;
            if (path.empty()) path = L"（未命名工程）";
            wchar_t b[512];
            swprintf(b, 512, L"%ls，第%d行，第1列 1001: %ls", path.c_str(), ln, desc.c_str());
            std::vector<std::wstring> errs; errs.push_back(b);
            ShowCompileErrors(errs);
        } else {
            m_docScript.RunOnce(&m_scene);
            // 收集文档类 main() 的 trace 输出（调试/播放时在"输出"页显示）
            if (!m_docScript.GetLog().empty()) m_traceLines.push_back(m_docScript.GetLog());
            m_scene.SyncDisplayToLocalAll();
            m_viewport.Repaint();
        }
    }
    m_tlStopped = false;   // 重新播放：时间轴恢复推进（脚本 stop() 状态清除）
    m_playing = true;
    // 播放开始：先执行当前帧的关键帧脚本（起始帧=0，帧号与时间轴/脚本 frame 完全一致）
    ExecuteKeyFrameScripts(m_scene.frame);
    SetTimer(1, 16, NULL);   // 16ms 定时器：键盘采样更跟手，帧推进由时间累积控制
    UpdateStatus();
}

void CMainWnd::Stop() {
    if (!m_playing) return;
    m_playing = false;
    KillTimer(1);
    m_script.ClearKeys();
    m_docScript.ClearKeys();
    ShowTraceOutput();       // 停止播放：统一显示本次播放收集的 trace 输出
    UpdateStatus();
}

// Flash 式"输出"页：播放/调试期间收集的 trace 输出统一写入顶部面板"输出"页并自动切换
void CMainWnd::ShowTraceOutput() {
    if (m_traceLines.empty()) return;
    std::wstring body;
    for (size_t i = 0; i < m_traceLines.size(); i++) body += m_traceLines[i];  // 每行自带 \r\n
    wchar_t b[128];
    swprintf(b, 128, L"输出 %d 条", (int)m_traceLines.size());
    m_traceEdt.SetWindowText(body.c_str());
    m_traceStat.SetWindowText(b);
    // 本次无错误时才自动切到"输出"页（有错误时错误页优先）
    CString errTxt; m_errEdt.GetWindowText(errTxt);
    if (errTxt.IsEmpty()) { m_tab.SetCurSel(1); ShowTabPage(1); }
}

void CMainWnd::SetCurrentFrame(int f) {
    if (f < 0) f = 0;
    if (f >= m_scene.totalFrames) m_scene.totalFrames = f + 1;   // 时间轴无限长：播放头移出时自动扩展
    m_scene.SetFrame(f);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

void CMainWnd::PlaybackHook(int cmd, double arg, void* user) {
    CMainWnd* w = (CMainWnd*)user;
    if (!w) return;
    switch (cmd) {
        case 1: // play()：恢复时间轴推进（未在播放时启动播放）
            w->m_tlStopped = false;
            if (!w->m_playing) w->Play();
            break;
        case 2: // stop()：时间轴停在当前帧，类代码/帧代码照常每帧执行（Flash 语义）
            w->m_tlStopped = true;
            break;
        case 3: w->SetCurrentFrame((int)arg); break;
    }
}

// ------------------------------------------------------------ 选择与刷新
void CMainWnd::SelectObject(Object3D* o) {
    if (m_selected == o) {
        // 仍要保证视口高亮一致
        m_viewport.Repaint();
        m_timeline.Invalidate(FALSE);
        return;
    }
    std::vector<Object3D*> all;
    m_scene.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) all[i]->selected = false;
    m_selSet.clear();   // 单选会清空多选集合
    m_selected = o;
    m_selName = o ? o->name : L"";
    if (o) {
        o->selected = true;
        m_selSet.push_back(o);   // 单选也入集合：m_selSet 按点选顺序维护（先选者在下标 0，布尔 A/B 依赖此顺序）
    }
    m_tree.Rebuild(m_scene, m_selName);
    m_props.Bind(m_selected);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

// 双击组：进入该元件内部，编辑其子元件（Flash 式）
void CMainWnd::EnterGroup(Object3D* g) {
    if (!g || g->type != PT_NONE || !g->HasChildren()) return;
    SetEditLevel(g);
}

// 直接跳转到指定组编辑；g==NULL 回到主场景（根层）
void CMainWnd::SetEditLevel(Object3D* g) {
    if (g && g->type != PT_NONE) return;
    Stop();
    m_scene.editRoot = g;
    SelectObject(NULL);
    RefreshAll();
}

// 从根到当前编辑层的组链（不含根），用于导航栏
std::vector<Object3D*> CMainWnd::EditPath() const {
    std::vector<Object3D*> chain;
    for (Object3D* p = m_scene.editRoot; p; p = p->parent) chain.push_back(p);
    size_t n = chain.size();
    for (size_t i = 0; i < n / 2; i++) {
        Object3D* t = chain[i]; chain[i] = chain[n - 1 - i]; chain[n - 1 - i] = t;
    }
    return chain;
}

// 双击空白：退出当前元件编辑，回到上一层
void CMainWnd::ExitGroup() {
    if (!m_scene.InsideGroup()) return;
    Stop();
    m_scene.editRoot = m_scene.editRoot->parent;
    // 退回到的父级若是图元（没有子元件可编辑），继续向上直到组或根层
    while (m_scene.editRoot && m_scene.editRoot->type != PT_NONE)
        m_scene.editRoot = m_scene.editRoot->parent;
    SelectObject(NULL);
    RefreshAll();
}

// ------------------------------------------------------------ Ctrl 多选
void CMainWnd::RefreshSelectionUI() {
    m_tree.Rebuild(m_scene, m_selName);
    m_props.Bind(m_selected);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

void CMainWnd::ClearSelection() {
    std::vector<Object3D*> all;
    m_scene.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) all[i]->selected = false;
    m_selSet.clear();
    m_selected = NULL;
    m_selName.clear();
    RefreshSelectionUI();
}

void CMainWnd::AddToSelection(Object3D* o, bool makePrimary) {
    if (!o) return;
    if (!InSelection(o)) {
        o->selected = true;
        m_selSet.push_back(o);
    }
    if (makePrimary) {
        m_selected = o;
        m_selName = o->name;
    }
    RefreshSelectionUI();
}

void CMainWnd::RemoveFromSelection(Object3D* o) {
    if (!o) return;
    for (size_t i = 0; i < m_selSet.size(); i++) {
        if (m_selSet[i] == o) { m_selSet.erase(m_selSet.begin() + i); break; }
    }
    o->selected = false;
    if (m_selected == o) {
        m_selected = m_selSet.empty() ? NULL : m_selSet.back();
        m_selName = m_selected ? m_selected->name : L"";
    }
    RefreshSelectionUI();
}

bool CMainWnd::InSelection(Object3D* o) const {
    for (size_t i = 0; i < m_selSet.size(); i++)
        if (m_selSet[i] == o) return true;
    return false;
}

int CMainWnd::SelectionCount() const { return (int)m_selSet.size(); }

// 输出当前多选集合（m_selSet + 主选中，去重）——供视口多选拖动等使用
void CMainWnd::GetSelectionSet(std::vector<Object3D*>& out) const {
    out.clear();
    for (size_t i = 0; i < m_selSet.size(); i++)
        if (m_selSet[i]) out.push_back(m_selSet[i]);
    if (m_selected) {
        bool have = false;
        for (size_t i = 0; i < out.size(); i++)
            if (out[i] == m_selected) { have = true; break; }
        if (!have) out.push_back(m_selected);
    }
}

// Ctrl+A：全选当前编辑层级的直接子级（组内编辑选组内成员，根层选所有根元件）
void CMainWnd::SelectAllObjects() {
    Stop();
    // 直接构建多选集合，避免逐个 AddToSelection 重复刷新树
    {
        std::vector<Object3D*> all;
        m_scene.CollectAll(all);
        for (size_t i = 0; i < all.size(); i++) all[i]->selected = false;
    }
    m_selSet.clear();
    m_selected = NULL;
    std::vector<Object3D*> items;
    if (m_scene.editRoot) items = m_scene.editRoot->children;
    else items = m_scene.roots;
    for (size_t i = 0; i < items.size(); i++) {
        Object3D* o = items[i];
        if (!o) continue;
        o->selected = true;
        m_selSet.push_back(o);
        m_selected = o;              // 最后一个作为主选中
    }
    m_selName = m_selected ? m_selected->name : L"";
    RefreshSelectionUI();
}

// 把当前选中的若干元件转换为一个 MovieClip（组），保持各自世界位置
static bool WorldAABB(Object3D* o, Vec3& mn, Vec3& mx);   // 对象世界包围盒（图元/组通用），定义在下方

void CMainWnd::WrapToMovieClip() {
    Stop();
    // 收集所有处于选中态的元件（含普通单击选中的主选中），保证一个都不漏
    std::vector<Object3D*> items;
    {
        std::vector<Object3D*> all;
        m_scene.CollectAll(all);
        for (size_t i = 0; i < all.size(); i++)
            if (all[i]->selected) items.push_back(all[i]);
    }
    if (items.empty() && m_selected) items.push_back(m_selected);
    if (items.empty()) return;
    PushUndo();
    // Flash 式 MovieClip：组有自己的三维坐标系，原点(0,0,0)就在组合体包围盒中心。
    // 成员坐标换算为相对中心的局部坐标（世界位置不变），
    // 这样旋转/缩放组时整体绕自身中心变换，而不是绕世界原点各自转圈。
    Vec3 mn(1e18, 1e18, 1e18), mx(-1e18, -1e18, -1e18);
    bool hasBox = false;
    for (size_t i = 0; i < items.size(); i++) {
        Vec3 b0, b1;
        if (!WorldAABB(items[i], b0, b1)) continue;
        hasBox = true;
        if (b0.x < mn.x) mn.x = b0.x;
        if (b0.y < mn.y) mn.y = b0.y;
        if (b0.z < mn.z) mn.z = b0.z;
        if (b1.x > mx.x) mx.x = b1.x;
        if (b1.y > mx.y) mx.y = b1.y;
        if (b1.z > mx.z) mx.z = b1.z;
    }
    Vec3 c = hasBox ? Vec3((mn.x + mx.x) * 0.5, (mn.y + mx.y) * 0.5, (mn.z + mx.z) * 0.5)
                    : Vec3(0, 0, 0);
    wchar_t buf[64];
    swprintf(buf, 64, L"元件%d", m_scene.CountType(PT_NONE) + 1);
    Object3D* g = m_scene.AddObject(PT_NONE, buf, m_scene.editRoot);
    // 中心换算到组父级（当前编辑层）的局部坐标，作为组原点
    if (m_scene.editRoot) {
        Mat4 inv = m_scene.editRoot->WorldMatrix().Inverse();
        c = inv.TransformPoint(c);
    }
    g->x = c.x; g->y = c.y; g->z = c.z;
    // 成员移入组并保持世界位置（组仅平移、无旋转缩放，位置/关键帧换算完全精确）
    for (size_t i = 0; i < items.size(); i++)
        m_scene.MoveInto(items[i], g);
    m_scene.SyncDisplayToLocalAll();
    ClearSelection();
    SelectObject(g);
    UpdateStatus();
}

// 视口右键菜单
void CMainWnd::ShowObjectMenu(CPoint screenPt) {
    CMenu menu;
    menu.CreatePopupMenu();
    UINT selEn = (m_selected || SelectionCount() > 0) ? 0 : MF_GRAYED;
    menu.AppendMenu(MF_STRING | selEn, IDC_BTN_DUPLICATE, L"复制");
    menu.AppendMenu(MF_STRING | selEn, IDC_BTN_CUT, L"剪切");
    menu.AppendMenu(MF_STRING | (m_clipboard ? 0 : MF_GRAYED), IDC_CMD_PASTE, L"粘贴");
    menu.AppendMenu(MF_STRING | selEn, IDC_BTN_DELETE, L"删除");
    UINT ungroupEn = (m_selected && m_selected->type == PT_NONE) ? 0 : MF_GRAYED;
    menu.AppendMenu(MF_STRING | ungroupEn, IDC_BTN_UNGROUP, L"打散");
    // 布尔运算：任意图元/布尔结果(A) 与 孔体(B)，单选/多选均可（自动找重叠配对元件）
    bool canBool = false;
    for (size_t i = 0; i < m_selSet.size(); i++) {
        int t = m_selSet[i]->type;
        if ((t >= PT_BOX && t <= PT_RTUBE) || t == PT_BOOLEAN) { canBool = true; break; }
    }
    UINT boolEn = canBool ? 0 : MF_GRAYED;
    menu.AppendMenu(MF_STRING | boolEn, IDC_CMD_BOOLEAN, L"布尔差集(A−B挖孔)");
    menu.AppendMenu(MF_STRING | boolEn, IDC_CMD_BOOLUNION, L"布尔并集(A∪B合并)");
    menu.AppendMenu(MF_STRING | boolEn, IDC_CMD_BOOLINTER, L"布尔交集(A∩B共体)");
    menu.AppendMenu(MF_STRING | selEn, IDC_CMD_TOMC, L"转换为 MovieClip");
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, screenPt.x, screenPt.y, this);
}

void CMainWnd::OnWrapToMovieClip() { WrapToMovieClip(); }

// 把对象的世界坐标位置写入本地平移（考虑父级矩阵），仅改平移保持旋转/缩放
static void SetWorldPos(Object3D* o, const Vec3& pw) {
    Mat4 wp = o->parent ? o->parent->WorldMatrix() : Mat4();
    Vec3 lp = wp.Inverse().TransformPoint(pw);
    o->x = lp.x; o->y = lp.y; o->z = lp.z;
}

// 对象的世界包围盒：图元（非组）用本地包围盒 + 显示矩阵变换；组用 GroupWorldAABB
static bool WorldAABB(Object3D* o, Vec3& mn, Vec3& mx) {
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

// 布尔运算：元件(A) op 孔体(B)。A 可为任意图元，B 为凸体（立方体/球/圆柱/圆锥/方管）。
// op: 0=差集A−B(挖孔) 1=并集A∪B 2=交集A∩B
void CMainWnd::OnBooleanHole() { OnBooleanOp(0); }
void CMainWnd::OnBooleanUnion() { OnBooleanOp(1); }
void CMainWnd::OnBooleanInter() { OnBooleanOp(2); }
void CMainWnd::OnBooleanOp(int op) {
    auto isHole = [](int t) {
        return t == PT_BOX || t == PT_SPHERE || t == PT_CYLINDER || t == PT_CONE || t == PT_TUBE || t == PT_RTUBE || t == PT_BOOLEAN;
    };
    auto isTarget = [](int t) { return (t >= PT_BOX && t <= PT_RTUBE) || t == PT_BOOLEAN; };   // 布尔结果可再次作为目标
    Object3D* A = NULL; Object3D* B = NULL;
    // 规则：已在选中的物体为 A（先选中者），按 Ctrl 追加选中的另一物体为 B。
    // m_selSet 按点选顺序维护：下标 0 = 先选中的物体，其后为 Ctrl 追加的物体。
    if (m_selSet.size() >= 2 && isTarget(m_selSet[0]->type)) {
        A = m_selSet[0];
        for (size_t i = 1; i < m_selSet.size() && !B; i++)
            if (m_selSet[i] != A && isHole(m_selSet[i]->type)) B = m_selSet[i];
    }
    if (!A || !B) {   // 回退：按集合顺序自动配对（先目标后孔体）
        A = NULL; B = NULL;
        for (size_t i = 0; i < m_selSet.size(); i++) {
            int t = m_selSet[i]->type;
            if (!A && isTarget(t)) A = m_selSet[i];
            else if (!B && isHole(t) && m_selSet[i] != A) B = m_selSet[i];
        }
    }
    if (A && !B) {   // 只选了目标元件：找与之重叠的第一个孔体
        Vec3 am, ax; if (!WorldAABB(A, am, ax)) { A = NULL; }
        std::vector<Object3D*> all; m_scene.CollectAll(all);
        for (size_t i = 0; i < all.size(); i++) {
            Object3D* o = all[i];
            if (o == A || !isHole(o->type)) continue;
            Vec3 bm, bx; if (!WorldAABB(o, bm, bx)) continue;
            if (am.x <= bx.x && ax.x >= bm.x && am.y <= bx.y && ax.y >= bm.y && am.z <= bx.z && ax.z >= bm.z) { B = o; break; }
        }
    } else if (!A && B) {   // 只选了孔体：找与之重叠的第一个目标元件
        Vec3 bm, bx; if (!WorldAABB(B, bm, bx)) { B = NULL; }
        std::vector<Object3D*> all; m_scene.CollectAll(all);
        for (size_t i = 0; i < all.size(); i++) {
            Object3D* o = all[i];
            if (o == B || !isTarget(o->type)) continue;
            Vec3 am, ax; if (!WorldAABB(o, am, ax)) continue;
            if (am.x <= bx.x && ax.x >= bm.x && am.y <= bx.y && ax.y >= bm.y && am.z <= bx.z && ax.z >= bm.z) { A = o; break; }
        }
    }
    if (!A || !B || A == B) {
        MessageBox(_T("布尔运算需要目标元件与孔体。\r\n请多选两个元件，或单选一个元件并确保它与另一个元件在空间上重叠。"),
            _T("Super3D"), MB_ICONINFORMATION);
        return;
    }
    Stop();
    PushUndo();
    // A 转为布尔结果：先在 A 仍为图元时生成网格（BuildBooleanMesh 在 A 为图元时执行），再改类型
    A->RebuildBooleanMesh(A, B, op);
    A->type = PT_BOOLEAN;
    // 删除孔体 B，选中改为 A
    RemoveFromSelection(B);
    if (m_selected == B) m_selected = A;
    m_scene.RemoveObjectFromAllLayerKeys(B);
    m_scene.DeleteObject(B);
    SelectObject(A);
    RefreshAll();
}

void CMainWnd::OnAlign(int mode) {
    // 收集待对齐对象：优先 Ctrl 多选集合；不足则补入当前编辑层级中已标记选中的元件
    std::vector<Object3D*> sel = m_selSet;
    if (sel.size() < 2) {
        std::vector<Object3D*> kids;
        m_scene.EditChildren(kids);
        for (Object3D* k : kids)
            if (k->selected && !InSelection(k)) sel.push_back(k);
    }
    if (sel.size() < 2) {
        MessageBox(_T("对齐需要至少两个元件。\r\n请先用 Ctrl+单击 多选两个及以上的元件，再执行对齐。"),
            _T("Super3D"), MB_ICONINFORMATION);
        return;
    }
    struct Item { Object3D* o; Vec3 mn, mx; };
    std::vector<Item> items;
    for (Object3D* o : sel) {
        Vec3 mn, mx;
        if (!WorldAABB(o, mn, mx)) continue;   // 世界包围盒，与渲染同基准
        items.push_back({ o, mn, mx });
    }
    if (items.size() < 2) {
        MessageBox(_T("无法计算所选元件的包围盒，对齐取消。"), _T("Super3D"), MB_ICONINFORMATION);
        return;
    }
    // 以第一个选中的元件为基准，其他元件对齐到它
    const Item& ref = items[0];
    double refCX = (ref.mn.x + ref.mx.x) * 0.5, refCY = (ref.mn.y + ref.mx.y) * 0.5;
    PushUndo();
    for (Item& it : items) {
        Vec3 pw = it.o->WorldMatrix().TransformPoint(Vec3(0, 0, 0));   // 对象世界位置
        double cx = (it.mn.x + it.mx.x) * 0.5, cy = (it.mn.y + it.mx.y) * 0.5;
        switch (mode) {
            case 0: pw.x += ref.mn.x - it.mn.x; break;   // 左对齐：对齐到基准元件左边
            case 1: pw.x += refCX - cx;       break;     // 水平居中：对齐到基准元件中心
            case 2: pw.x += ref.mx.x - it.mx.x; break;   // 右对齐：对齐到基准元件右边
            case 3: pw.y += ref.mx.y - it.mx.y; break;   // 顶对齐：对齐到基准元件顶边
            case 4: pw.y += refCY - cy;       break;     // 垂直居中：对齐到基准元件中心
            case 5: pw.y += ref.mn.y - it.mn.y; break;   // 底对齐：对齐到基准元件底边
            default: continue;
        }
        SetWorldPos(it.o, pw);
    }
    m_scene.SyncDisplayToLocalAll();
    RefreshAll();
}
void CMainWnd::OnAlignLeft()    { OnAlign(0); }
void CMainWnd::OnAlignHCenter() { OnAlign(1); }
void CMainWnd::OnAlignRight()   { OnAlign(2); }
void CMainWnd::OnAlignTop()     { OnAlign(3); }
void CMainWnd::OnAlignVCenter() { OnAlign(4); }
void CMainWnd::OnAlignBottom()  { OnAlign(5); }

void CMainWnd::RefreshAll() {    m_selected = m_scene.FindByName(m_selName);
    if (m_selected) m_selected->selected = true;
    m_tree.Rebuild(m_scene, m_selName);
    m_props.Bind(m_selected);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    RefreshNavBar();     // 编辑层级变化时刷新视口外导航栏
    UpdateStatus();
}

// 时间轴图标切换（可见性/锁定）后的轻量刷新：不插入关键帧
void CMainWnd::NotifyTimelineChanged() {
    m_props.Bind(m_selected);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

void CMainWnd::UpdateStatus() {
    CString s;
    const wchar_t* toolName = L"选择";
    switch (m_tool) {
        case TOOL_MOVE: toolName = L"移动"; break;
        case TOOL_ROTATE: toolName = L"旋转"; break;
        case TOOL_SCALE: toolName = L"缩放"; break;
        case TOOL_PAN: toolName = L"平移视角"; break;
        case TOOL_SPLINE: toolName = L"画曲线"; break;
    }
    CString sel = m_selected ? m_selected->name.c_str() : _T("（无）");
    // 当前编辑层级路径
    std::wstring path = L"场景";
    if (m_scene.editRoot) {
        std::vector<Object3D*> chain;
        for (Object3D* p = m_scene.editRoot; p; p = p->parent) chain.push_back(p);
        for (size_t i = chain.size(); i > 0; i--)
            path += std::wstring(L" > ") + chain[i - 1]->name;
    }
    s.Format(_T("帧 %d/%d    工具：%s    编辑：%s    选中：%s    %s\n左键：点选/拖拽  右键：旋转  滚轮：缩放  Shift+左键：平移  Ctrl+点选：多选  右键菜单：转换为元件"),
        m_scene.frame, m_scene.totalFrames, toolName,
        path.c_str(), sel.GetString(), m_playing ? _T("播放中…") : _T(""));
    m_status.SetWindowText(s);
}

// ------------------------------------------------------------ 撤销
void CMainWnd::PushUndo() {
    UndoSnap s;
    s.text = ProjectIO::SerializeToString(m_scene, true);   // 文本不含布尔网格体
    // 布尔网格缓存按对象名二进制侧挂（memcpy 级，远比序列化成文本快）
    std::vector<Object3D*> all;
    m_scene.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) {
        Object3D* o = all[i];
        if (o->type == PT_BOOLEAN && o->_meshValid && !o->_meshVerts.empty())
            s.boolMesh[o->name] = std::make_pair(o->_meshVerts, o->_meshNorms);
    }
    m_undo.push_back(s);
    if (m_undo.size() > 30) m_undo.pop_front();
}

void CMainWnd::MoveIntoObject(Object3D* o, Object3D* g) {
    if (!o || !g) return;
    PushUndo();
    if (m_scene.MoveInto(o, g)) {
        m_scene.SyncDisplayToLocalAll();
        SelectObject(o);
        RefreshAll();
        UpdateStatus();
    }
}

// 时间轴层级调整：-1上移 1下移 2置顶 3置底
void CMainWnd::MoveLayer(Object3D* o, int dir) {
    if (!o) return;
    PushUndo();
    if (m_scene.MoveLayer(o, dir)) {
        m_scene.SyncDisplayToLocalAll();
        RefreshAll();
        UpdateStatus();
    } else {
        m_undo.pop_back();   // 未发生移动则撤销入栈
    }
}

void CMainWnd::SetCurrentLayerIdx(int idx) {
    if (m_scene.SetCurrentLayer(idx)) {
        m_timeline.Invalidate(FALSE);
        UpdateStatus();
    }
}

void CMainWnd::MoveObjectToLayer(Object3D* o, int layerIdx) {
    if (!o) return;
    PushUndo();
    if (m_scene.MoveToLayer(o, layerIdx)) {
        if (o && o->parent == NULL) m_scene.AddNewObjectToCurrentKey(o);
        m_scene.SyncDisplayToLocalAll();
        RefreshAll();
        UpdateStatus();
    } else {
        m_undo.pop_back();
    }
}

void CMainWnd::MoveLayerByIndex(int layerIdx, int dir) {
    PushUndo();
    if (m_scene.MoveLayerOrder(layerIdx, dir)) {
        m_scene.SyncDisplayToLocalAll();
        RefreshAll();
        UpdateStatus();
    } else {
        m_undo.pop_back();
    }
}

void CMainWnd::AddNewLayer() {
    PushUndo();
    m_scene.AddLayer();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

void CMainWnd::DeleteEmptyLayer(int idx) {
    PushUndo();
    if (m_scene.DeleteLayer(idx)) {
        RefreshAll();
        UpdateStatus();
    } else {
        m_undo.pop_back();
    }
}

void CMainWnd::DoUndo() {
    if (m_undo.empty()) return;
    Stop();
    UndoSnap snap = m_undo.back();
    m_undo.pop_back();
    // 撤销只恢复场景编辑内容（对象/关键帧/脚本），不改变相机视角：
    // 相机属于视图状态，视角操作（右键旋转/滚轮/平移）不入撤销栈，
    // 反序列化 scene.Clear() 会 cam.Reset()、文本快照又恢复旧相机 → 这里覆盖回当前相机。
    Camera camKeep = m_scene.cam;
    int frameKeep = m_scene.frame;   // 播放头同属视图状态，撤销不跳帧
    std::wstring err;
    if (!ProjectIO::DeserializeFromString(m_scene, snap.text, err)) {
        m_scene.cam = camKeep;
        AfxMessageBox(err.c_str(), MB_ICONWARNING);
        return;
    }
    m_scene.cam = camKeep;
    // 文本快照不含布尔网格体：按对象名把二进制网格挂回
    std::vector<Object3D*> all;
    m_scene.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) {
        Object3D* o = all[i];
        if (o->type != PT_BOOLEAN) continue;
        std::map<std::wstring, std::pair<std::vector<Vec3>, std::vector<Vec3> > >::iterator it = snap.boolMesh.find(o->name);
        if (it != snap.boolMesh.end()) {
            o->_meshVerts = it->second.first;
            o->_meshNorms = it->second.second;
            o->_meshValid = !o->_meshVerts.empty();
            o->_meshRev++;
        }
    }
    m_scene.editRoot = NULL;
    m_scene.editMeshObj = NULL;   // 撤销快照重建了全部对象，编辑态指针一律失效
    m_scene.frame = frameKeep;
    m_scene.SetFrame(frameKeep);
    // 反序列化 scene.Clear() 已删除所有旧对象，选中的旧对象已悬空，必须重置，
    // 否则 RefreshAll() 访问悬空 m_selected（组合的 children 已释放）触发 _Orphan_all 崩溃。
    m_selected = NULL;
    m_selName.clear();
    m_script.SetText(m_scene.script);
    SetScriptEditText(m_scene.script);
    m_scriptWnd.SetPerFrame(m_scene.scriptPerFrame);
    m_viewport.ValidateSpline();
    RefreshAll();
    UpdateStatus();
}

// ------------------------------------------------------------ 元件操作
void CMainWnd::AddPrimitive(int type) {
    Stop();
    PushUndo();
    Object3D* parent = m_scene.editRoot;   // 新增元件落在当前编辑容器内
    if (!parent && m_selected && m_selected->type == PT_NONE) parent = m_selected;
    Object3D* o = m_scene.AddObject(type, m_scene.AutoName(type), parent);
    Scene3D::SetStandingY(o);
    if (o && o->parent == NULL) m_scene.AddNewObjectToCurrentKey(o);
    SelectObject(o);
    RefreshAll();
}

void CMainWnd::DoDelete() {
    // 网格编辑态：Del 键删除当前选中的点/线/面，而不是整个元件
    if (m_scene.editMeshObj) {
        m_viewport.DeleteEditSelection();
        return;
    }
    if (!m_selected && SelectionCount() == 0) return;
    Stop();
    PushUndo();
    // 多选删除：把 Ctrl 多选集合 + 主选中一起删除（全选后 Del 一键清空当前层级）
    std::vector<Object3D*> items;
    GetSelectionSet(items);
    // 若父级也在待删集合中，子级由父级删除时级联删除（避免重复删已释放指针）
    std::vector<Object3D*> toDelete;
    for (size_t i = 0; i < items.size(); i++) {
        Object3D* o = items[i];
        if (!o) continue;
        bool parentInSet = false;
        for (size_t j = 0; j < items.size(); j++)
            if (items[j] && items[j] == o->parent) { parentInSet = true; break; }
        if (!parentInSet) toDelete.push_back(o);
    }
    for (size_t i = 0; i < toDelete.size(); i++) {
        Object3D* o = toDelete[i];
        if (!o) continue;
        m_scene.RemoveObjectFromAllLayerKeys(o);
        m_scene.DeleteObject(o);
    }
    ClearSelection();
    m_selName.clear();
    RefreshAll();
    UpdateStatus();
}

void CMainWnd::DoDuplicate() {   // 复制：把选中元件暂存到剪贴板（不生成副本，右键"粘贴"才粘贴出来）
    if (!m_selected) return;
    if (m_clipboard) delete m_clipboard;
    m_clipboard = m_selected->CloneDeep(m_selected->name, false);
    UpdateStatus();
}

void CMainWnd::OnCut() {          // 剪切：暂存剪贴板并从场景移除
    if (!m_selected) return;
    if (m_clipboard) delete m_clipboard;
    m_clipboard = m_selected->CloneDeep(m_selected->name, false);
    Stop();
    PushUndo();
    m_scene.RemoveObjectFromAllLayerKeys(m_selected);
    m_scene.DeleteObject(m_selected);
    m_selected = NULL;
    m_selName.clear();
    RefreshAll();
    UpdateStatus();
}

void CMainWnd::OnPaste() {        // 粘贴：把剪贴板克隆生成副本加入当前编辑层级
    if (!m_clipboard) return;
    Stop();
    PushUndo();
    Object3D* c = m_clipboard->CloneDeep(m_clipboard->name, true);
    c->x += 1.5;                  // 与原件错开，便于看清是副本
    c->parent = NULL;
    c->layer = m_scene.curLayer;  // 粘贴到当前选中图层（不继承原件的图层号）
    if (m_scene.InsideGroup() && m_scene.editRoot) {
        m_scene.editRoot->children.push_back(c);
        c->parent = m_scene.editRoot;
    } else {
        m_scene.roots.push_back(c);
        m_scene.SortRootsByLayer();
    }
    if (c->parent == NULL) m_scene.AddNewObjectToCurrentKey(c);
    SelectObject(c);
    RefreshAll();
}

// ------------------------------------------------------------ Blender 式网格编辑（点/线/面）

// 进入网格编辑态：把当前程序化参数网格固化为可编辑网格（editVerts/editTris）
void CMainWnd::EnterMeshEdit(Object3D* o) {
    if (!o || !Object3D::EditableType(o->type)) return;
    if (m_scene.editMeshObj == o) return;
    Stop();
    PushUndo();
    if (!o->HasEditMesh())
        o->BuildEditMeshFromCore();   // 首次进入：焊接生成顶点表/三角表
    m_scene.editMeshObj = o;
    SelectObject(o);
    m_viewport.ClearEditSelection();
    m_viewport.BuildEditEdges();
    RefreshAll();
}

// 退出网格编辑态：编辑结果已随拖动/删除时的 PushUndo 入撤销栈，这里只清理状态
void CMainWnd::ExitMeshEdit() {
    if (!m_scene.editMeshObj) return;
    m_scene.editMeshObj = NULL;
    m_viewport.ClearEditSelection();
    RefreshAll();
}

// 重建网格：丢弃编辑网格，回退到程序化参数网格（参数改动重新生效）
void CMainWnd::RebuildEditMesh(Object3D* o) {
    if (!o) return;
    Stop();
    PushUndo();
    o->ClearEditMesh();
    if (m_scene.editMeshObj == o) {
        m_scene.editMeshObj = NULL;
        m_viewport.ClearEditSelection();
    }
    RefreshAll();
}

// 删除编辑态当前选中的点/线/面（转发视口处理，视口内已有 PushUndo）
void CMainWnd::DeleteEditSelection() {
    if (m_scene.editMeshObj) m_viewport.DeleteEditSelection();
}

// 切换编辑模式（0点 1线 2面）：切换后清空选择（不同元素类型不通用）
void CMainWnd::SetMeshEditMode(int m) {
    if (m < 0 || m > 2) m = 0;
    m_viewport.m_emMode = m;
    m_viewport.ClearEditSelection();
    m_viewport.Repaint();
}

void CMainWnd::DoNew() {
    Stop();
    PushUndo();
    m_scene.Clear();
    m_selected = NULL;
    m_selName.clear();
    m_scene.fps = 24;
    m_scene.totalFrames = 120;
    m_scene.loopPlay = true;
    SetScriptEditText(L"");
    m_scriptWnd.SetPerFrame(false);
    m_scriptWnd.SetOutput(_T(""));
    m_docScriptMode = false;            // 新工程回到场景编辑
    m_docSSOpen = true;                 // 新工程恢复 .ss 标签
    SetDocText(L"");        // 新工程暂无文档类（首次切 .ss 标签时自动填模板）
    m_docTab.SetCurSel(0);              // 文档 Tab 回到 .s3d
    m_titleName.clear();                // 新工程无显示名
    m_viewport.ValidateSpline();
    RefreshAll();
    UpdateTitle();
    UpdateDocTabs();
    Layout();
}

// ------------------------------------------------------------ 关键帧
void CMainWnd::OnAddKey() {
    PushUndo();
    m_scene.InsertLayerKey(m_scene.curLayer, m_scene.frame);
    m_scene.SetFrame(m_scene.frame);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

void CMainWnd::OnDelKey() {
    if (!m_scene.LayerHasKeyAt(m_scene.curLayer, m_scene.frame)) return;
    PushUndo();
    m_scene.RemoveLayerKey(m_scene.curLayer, m_scene.frame);
    m_scene.SetFrame(m_scene.frame);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
}

void CMainWnd::DeleteLayerKeyAt(int layerIdx, int frame) {
    if (layerIdx < 0 || layerIdx >= (int)m_scene.layers.size()) return;
    PushUndo();
    m_scene.RemoveLayerKey(layerIdx, frame);
    m_scene.SetFrame(m_scene.frame);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

// 时间轴右键"在此处插入关键帧"：对指定图层在指定帧插入图层关键帧（不依赖播放头、无需选中元件）
void CMainWnd::InsertLayerKeyAtFrame(int layerIdx, int frame) {
    if (layerIdx < 0 || layerIdx >= (int)m_scene.layers.size()) return;
    if (frame < 0) frame = 0;
    PushUndo();
    // 若当前选中的是根元件且不在目标图层，则先把它移到目标图层
    if (m_selected && m_selected->parent == NULL && m_scene.RootLayerOf(m_selected) != layerIdx)
        m_scene.MoveToLayer(m_selected, layerIdx);
    m_scene.InsertLayerKey(layerIdx, frame);
    m_scene.SetFrame(frame);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

// 时间轴右键"插入空白关键帧"：该帧该层置为空（无元件），之后该层在该帧起空白直到下一有关键帧
void CMainWnd::InsertEmptyLayerKeyAtFrame(int layerIdx, int frame) {
    if (layerIdx < 0 || layerIdx >= (int)m_scene.layers.size()) return;
    if (frame < 0) frame = 0;
    PushUndo();
    m_scene.InsertEmptyLayerKey(layerIdx, frame);
    m_scene.SetFrame(frame);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

// 关键帧内容编辑：把选中元件加入/移出指定关键帧
void CMainWnd::AddObjectToKeyAt(int layerIdx, int frame) {
    if (!m_selected) return;
    PushUndo();
    m_scene.AddObjectToLayerKey(layerIdx, frame, m_selected);
    m_scene.SetFrame(m_scene.frame);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}
void CMainWnd::RemoveObjectFromKeyAt(int layerIdx, int frame) {
    if (!m_selected) return;
    PushUndo();
    m_scene.RemoveObjectFromLayerKey(layerIdx, frame, m_selected);
    m_scene.SetFrame(m_scene.frame);
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

// ------------------------------------------------------------ 变更通知
// 修改元件（拖拽/属性面板/方向键）后统一入口：
// 若当前帧已有该元件的关键帧，则更新该关键帧值（编辑才保留、切帧不变回）；
// 若当前帧无关键帧，只同步本地显示值、不自动创建关键帧（关键帧需手动插入）。
// 不能整体用 SetFrame——它会对关键帧插值并覆盖刚改的本地值。
void CMainWnd::OnViewTransformChanged(bool light) {
    if (m_selected) {
        m_scene.UpdateLayerKeySnap(m_selected, m_scene.frame);   // 更新当前帧图层关键帧快照
        m_selected->SyncDisplayToLocal();
    }
    // 多选变换（拖动/方向键微调等）：其余选中对象也同步显示坐标，
    // 渲染用 display(dx/dy/dz)，只改本地 x/y/z 不同步则画面不动
    for (size_t i = 0; i < m_selSet.size(); i++) {
        Object3D* o = m_selSet[i];
        if (o && o != m_selected) o->SyncDisplayToLocal();
    }
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    if (!light) {   // 拖动中(light)不刷新属性面板/状态栏：其文本每帧 SetWindowText 重绘会闪烁；
                    // 拖动中只更新场景，松手时再完整刷新一次显示最终值。
        m_props.Refresh();
        UpdateStatus();
    }
}

void CMainWnd::OnPropChanged() {
    OnViewTransformChanged();
}

// 属性面板编辑中（每次击键）：只更新对象+视口，绝不刷新面板文本，
// 避免覆盖正在输入的框导致“改不动 / 变出 20.20 这种”。
// 若当前帧已有该元件关键帧则更新它；无关键帧则不自动创建；仅用本地值同步显示。
void CMainWnd::NotifyPropertyEdit() {
    if (m_selected) {
        m_scene.UpdateLayerKeySnap(m_selected, m_scene.frame);
        m_selected->SyncDisplayToLocal();
    }
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

void CMainWnd::OnTimelineKeyMoved() {
    m_scene.SetFrame(m_scene.frame);
    m_viewport.Repaint();
    UpdateStatus();
}

void CMainWnd::OnTreeRenamed() {
    m_selName = m_selected ? m_selected->name : L"";
    m_tree.Rebuild(m_scene, m_selName);
    UpdateStatus();
}

// ------------------------------------------------------------ 脚本
void CMainWnd::RunScriptOnce() {
    // 先执行文档类（若有），再执行主场景脚本——与播放时一致
    if (!m_scene.docClass.empty()) {
        m_docScript.SetText(m_scene.docClass);
        std::wstring err;
        if (!m_docScript.EnsureParsed(err)) {
            m_scriptWnd.SetOutput(err.c_str());
        } else {
            m_docScript.RunOnce(&m_scene);
        }
    }
    m_script.RunOnce(&m_scene);
    m_scriptWnd.SetOutput(m_script.GetLog().c_str());
    // 关键帧脚本：模拟"播放到当前帧"，先执行当前帧的帧动作（写入显示值），
    // 使调试模式下关键帧脚本（如 ball.x=100）同样生效
    ExecuteKeyFrameScripts(m_scene.frame);
    // 单次运行修改的是本地编辑值，同步到显示值才能在视口中看到效果
    m_scene.SyncDisplayToLocalAll();
    m_props.Refresh();
    m_viewport.Repaint();
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

// 独立脚本窗口回调：脚本内容改变
void CMainWnd::ScriptTextChanged(const std::wstring& text) {
    m_scene.script = text;
    m_script.SetText(m_scene.script);
    std::wstring err;
    if (m_scene.script.empty()) { UpdateStatus(); return; }
    if (!m_script.EnsureParsed(err)) {
        CString s;
        s.Format(_T("脚本语法错误：%s"), err.c_str());
        m_status.SetWindowText(s);
    } else {
        UpdateStatus();
    }
}

// 独立脚本窗口回调：每帧运行勾选
void CMainWnd::PerFrameChanged(bool on) {
    m_scene.scriptPerFrame = on;
}

// 窗口菜单【脚本窗口】：打开当前播放头所在帧（当前图层）的关键帧/空白关键帧脚本。
// 与时间轴右键"编辑此关键帧脚本…"一致：仅当该帧有关键帧条目（实心或空白关键帧）时打开；
// 当前帧无关键帧条目则提示，不自动创建（与右键菜单行为相同）。
void CMainWnd::ShowScriptWindow() {
    bool hasKey = false;
    if (m_scene.curLayer >= 0 && (size_t)m_scene.curLayer < m_scene.layers.size()) {
        for (size_t j = 0; j < m_scene.layers[m_scene.curLayer].keys.size(); j++)
            if (m_scene.layers[m_scene.curLayer].keys[j].frame == m_scene.frame) { hasKey = true; break; }
    }
    if (!hasKey) {
        AfxMessageBox(L"当前帧没有关键帧：请先在时间轴上右键→【在此处插入关键帧】或【在此处插入空白关键帧】",
            MB_ICONINFORMATION);
        return;
    }
    EditKeyFrameScript(m_scene.curLayer, m_scene.frame);
}

// 示例 .s3d 完整路径：exe 位于 <工程根>\Win32\Release\，上溯两级定位工程根；
// 优先 sample\<示例名>\<示例名>.s3d（每示例一个文件夹），其次 sample\<示例名>.s3d（根目录兼容）
std::wstring CMainWnd::ExampleFilePath(const std::wstring& name) const {
    wchar_t exe[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    std::wstring root(exe);
    size_t slash = root.find_last_of(L"/\\");
    if (slash != std::wstring::npos) root = root.substr(0, slash + 1);
    for (int i = 0; i < 2; i++) {   // Win32\Release → Win32 → 工程根
        size_t p = root.find_last_of(L"/\\", root.size() - 2);
        if (p == std::wstring::npos) break;
        root = root.substr(0, p + 1);
    }
    std::wstring sub = root + L"sample\\" + name + L"\\" + name + L".s3d";
    if (::GetFileAttributesW(sub.c_str()) != INVALID_FILE_ATTRIBUTES) return sub;
    std::wstring flat = root + L"sample\\" + name + L".s3d";
    if (::GetFileAttributesW(flat.c_str()) != INVALID_FILE_ATTRIBUTES) return flat;
    return sub;   // 均不存在时返回标准子目录路径（文件应有的完整路径，供提示使用）
}

void CMainWnd::LoadExample(int which) {
    Stop();
    PushUndo();
    static const wchar_t* kExNames[] = { L"House", L"Windmill", L"SolarSystem",
                                         L"BounceBall", L"DocClass", L"TankBattle" };
    std::wstring name = (which >= 0 && which < 6) ? kExNames[which] : L"BounceBall";
    m_titleName = name;
    m_docScriptMode = false;              // 示例回到场景编辑
    m_docSSOpen = true;                   // 示例自带文档类，恢复 .ss 标签
    // 优先读磁盘示例文件（用户修改并保存过的版本）；不存在则用内置示例数据
    std::wstring exFile = ExampleFilePath(name);
    bool loaded = false;
    if (::GetFileAttributesW(exFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::wstring err;
        loaded = ProjectIO::LoadProject(m_scene, exFile, err);
        if (loaded) {
            m_scene.projectPath = exFile;   // 绑定示例文件路径：修改后保存即覆盖示例
            // 磁盘示例缺 .ss 文档类时，用内置示例的文档类回填（保留用户场景修改，补全脚本）
            if (m_scene.docClass.empty()) {
                Scene3D builtin;
                Examples::Load(builtin, which);
                m_scene.docClass = builtin.docClass;
            }
        }
        else m_status.SetWindowText((L"示例加载失败：" + err).c_str());
    }
    if (!loaded) {
        Examples::Load(m_scene, which);
        m_scene.projectPath.clear();
    }
    SetDocText(m_scene.docClass.c_str());   // 文档类脚本编辑器载入示例 .ss 内容
    m_docTab.SetCurSel(0);
    m_scene.editRoot = NULL;
    m_docScript.SetText(m_scene.docClass);   // 同步文档类（如 DocClass 示例的 .ss 内容）
    SetScriptEditText(m_scene.script);
    m_scriptWnd.SetPerFrame(m_scene.scriptPerFrame);
    m_scene.SetFrame(0);
    m_scriptWnd.SetOutput(_T(""));
    RefreshAll();
    UpdateTitle();
    UpdateDocTabs();
    Layout();
}

// ------------------------------------------------------------ 文件
// 窗口标题：基础名 + 当前打开的文件名（无文件则用示例名/基础名）
void CMainWnd::UpdateTitle() {
    CString t = _T("Super3D");
    std::wstring name;
    if (!m_scene.projectPath.empty()) {
        size_t pos = m_scene.projectPath.find_last_of(L"/\\");
        name = m_scene.projectPath.substr(pos == std::wstring::npos ? 0 : pos + 1);
    } else if (!m_titleName.empty()) {
        name = m_titleName + L".s3d";
    }
    if (!name.empty()) {
        t += _T(" - ");
        t += name.c_str();
    }
    SetWindowText(t);
}

void CMainWnd::DoSave() {
    Stop();
    // 当前选项卡在 .ss（文档类脚本页）：保存文档类脚本文件（.ss），Flash 风格"保存当前文件"
    if (m_docScriptMode) {
        std::wstring text = GetDocEditText();
        m_scene.docClass = text;   // 同步到文档类（播放/调试用内存版本）
        // 默认文件名 = 工程名.ss（有工程时），始终弹"另存为"对话框提供保存位置
        std::wstring defName = L"文档类脚本.ss";
        if (!m_scene.projectPath.empty()) {
            defName = m_scene.projectPath;
            size_t dot = defName.find_last_of(L'.');
            if (dot != std::wstring::npos) defName = defName.substr(0, dot);
            defName += L".ss";
        }
        CFileDialog dlg(FALSE, _T("ss"), defName.c_str(),
            OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
            _T("SuperScript 文档类 (*.ss)|*.ss|所有文件 (*.*)|*.*||"), this);
        if (dlg.DoModal() != IDOK) return;
        std::wstring ssPath = dlg.GetPathName().GetString();
        if (!ProjectIO::WriteFileUtf8(ssPath, text)) {
            AfxMessageBox(_T("保存文档类脚本失败：无法写入文件。"), MB_ICONERROR);
            return;
        }
        CString t;
        t.Format(_T("已保存：%s"), ssPath.c_str());
        m_status.SetWindowText(t);
        return;
    }
    // 场景模式：保存 .s3d 工程
    if (m_docScriptMode) m_scene.docClass = GetDocEditText();   // 脚本编辑内容同步到文档类
    CFileDialog dlg(FALSE, _T("s3d"), m_scene.projectPath.empty() ? _T("我的作品.s3d") : m_scene.projectPath.c_str(),
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        _T("Super3D 工程 (*.s3d)|*.s3d|所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;
    std::wstring err;
    if (!ProjectIO::SaveProject(m_scene, dlg.GetPathName().GetString(), err)) {
        AfxMessageBox(err.c_str(), MB_ICONERROR);
        return;
    }
    CString t;
    t.Format(_T("已保存：%s"), dlg.GetPathName());
    m_status.SetWindowText(t);
    m_titleName.clear();   // 已保存为真实文件，标题以文件名为准
    UpdateTitle();
    UpdateDocTabs();   // 保存后刷新文档标签文字
}

void CMainWnd::DoOpen() {
    Stop();
    CFileDialog dlg(TRUE, _T("s3d"), NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        _T("Super3D 工程 (*.s3d)|*.s3d|所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;
    PushUndo();
    std::wstring err;
    Scene3D tmp;
    if (!ProjectIO::LoadProject(tmp, dlg.GetPathName().GetString(), err)) {
        AfxMessageBox(err.c_str(), MB_ICONERROR);
        return;
    }
    m_scene.Swap(tmp);
    m_docScriptMode = false;   // 打开工程回到场景编辑（不触发旧文档类保存）
    m_docSSOpen = true;        // 打开工程恢复 .ss 标签
    SetDocText(m_scene.docClass.c_str());   // 脚本编辑器载入新工程文档类
    m_docTab.SetCurSel(0);     // 文档 Tab 回到 .s3d
    m_titleName.clear();       // 打开真实文件后以文件名为准
    m_scene.editRoot = NULL;
    m_selected = NULL;
    m_selName.clear();
    m_scene.frame = 0;
    m_scene.SetFrame(0);
    m_script.SetText(m_scene.script);
    SetScriptEditText(m_scene.script);
    m_scriptWnd.SetPerFrame(m_scene.scriptPerFrame);
    m_viewport.ValidateSpline();
    RefreshAll();
    CString t;
    t.Format(_T("已打开：%s"), dlg.GetPathName());
    m_status.SetWindowText(t);
    UpdateTitle();
    UpdateDocTabs();
    Layout();   // 按场景模式重新布局（含文档标签）
}

// ------------------------------------------------------------ 工具
void CMainWnd::SetTool(int t) {
    m_tool = t;
    UpdateStatus();
}

// 细分线框显示开关：同步渲染器静态标志并刷新视口
void CMainWnd::SetShowWire(bool on) {
    m_showWire = on;
    Renderer::s_showWire = on;
    m_props.Refresh();   // 让属性面板勾选框与状态保持一致
    m_viewport.Repaint();
    UpdateStatus();
}

// ------------------------------------------------------------ 命令
void CMainWnd::OnNew() { DoNew(); }
void CMainWnd::OnOpen() { DoOpen(); }
void CMainWnd::OnSave() { DoSave(); }
void CMainWnd::OnUndo() { DoUndo(); }
void CMainWnd::OnGroup() {
    // Flash 式组合：与右键「转换为 MovieClip」同一逻辑 —— 组有自己的三维坐标系，
    // 原点(0,0,0)在组合体包围盒中心，旋转/缩放绕自身中心整体进行
    if (!m_selected && SelectionCount() == 0) { AddPrimitive(PT_NONE); return; }
    WrapToMovieClip();
}
void CMainWnd::OnUngroup() {
    if (!m_selected || m_selected->type != PT_NONE) {
        m_scriptWnd.SetOutput(L"请先选中一个【组】元件，再点【打散】");
        return;
    }
    PushUndo();
    Object3D* doomed = m_selected;
    m_selected = NULL;
    if (m_scene.Ungroup(doomed)) {
        m_scene.SyncDisplayToLocalAll();
        RefreshAll();
        UpdateStatus();
    }
}
void CMainWnd::OnBox() { AddPrimitive(PT_BOX); }
void CMainWnd::OnSphere() { AddPrimitive(PT_SPHERE); }
void CMainWnd::OnCylinder() { AddPrimitive(PT_CYLINDER); }
void CMainWnd::OnCone() { AddPrimitive(PT_CONE); }
void CMainWnd::OnTorus() { AddPrimitive(PT_TORUS); }
void CMainWnd::OnPlane() { AddPrimitive(PT_PLANE); }
void CMainWnd::OnLeaf() { AddPrimitive(PT_LEAF); }
void CMainWnd::OnText3D() { AddPrimitive(PT_TEXT3D); }
void CMainWnd::OnTube() { AddPrimitive(PT_TUBE); }
void CMainWnd::OnRTube() { AddPrimitive(PT_RTUBE); }
// 曲线工具：切换到“画曲线”，在视口中按住左键拖拽出一条柳条
void CMainWnd::OnToolSpline() { SetTool(TOOL_SPLINE); }
void CMainWnd::OnToolSelect() { SetTool(TOOL_SELECT); }
void CMainWnd::OnToolMove() { SetTool(TOOL_MOVE); }
void CMainWnd::OnToolRotate() { SetTool(TOOL_ROTATE); }
void CMainWnd::OnToolScale() { SetTool(TOOL_SCALE); }
void CMainWnd::OnToolPan() { SetTool(TOOL_PAN); }
void CMainWnd::OnDuplicate() { DoDuplicate(); }
void CMainWnd::OnDelete() { DoDelete(); }
void CMainWnd::OnPlay() { Play(); }
void CMainWnd::OnStop() { Stop(); }
void CMainWnd::OnEx0() { LoadExample(0); }
void CMainWnd::OnEx1() { LoadExample(1); }
void CMainWnd::OnEx2() { LoadExample(2); }
void CMainWnd::OnEx3() { LoadExample(3); }
void CMainWnd::OnEx4() { LoadExample(4); }
void CMainWnd::OnEx5() { LoadExample(5); }

void CMainWnd::OnResetView() {
    m_scene.cam.Reset();
    m_viewport.Repaint();
}

void CMainWnd::OnRun() {
    RunScriptOnce();
}

// ------------------------------------------------------------ 帮助窗口
// 带最大化/还原按钮的可缩放窗口（WS_OVERLAPPEDWINDOW），内容放在只读多行文本框里，
// 随窗口缩放自动换行、内容多时可滚动 —— 替代 AfxMessageBox（内容长会被截断显示不全）。
class CHelpFrame : public CFrameWnd {
public:
    CEdit m_edt;
    HFONT m_hFont;
    const wchar_t* m_text;
    CHelpFrame() : m_hFont(NULL), m_text(L"") {}
    BOOL CreateHelp(HFONT hFont, const wchar_t* text, CWnd* owner) {
        m_hFont = hFont;
        m_text = text;
        if (!Create(NULL, L"Super3D 使用说明", WS_OVERLAPPEDWINDOW,
                    CRect(0, 0, 800, 700), owner, NULL)) return FALSE;
        CenterWindow(owner);
        return TRUE;
    }
    afx_msg int OnCreate(LPCREATESTRUCT) {
        CRect rc; GetClientRect(&rc);
        rc.DeflateRect(6, 6);
        m_edt.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                     ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, rc, this, 1);
        if (m_hFont) m_edt.SendMessage(WM_SETFONT, (WPARAM)m_hFont, FALSE);
        m_edt.SetWindowText(m_text);
        return 0;
    }
    afx_msg void OnSize(UINT, int cx, int cy) {
        if (m_edt.GetSafeHwnd()) m_edt.MoveWindow(6, 6, cx - 12, cy - 12);
    }
    afx_msg void OnSetFocus(CWnd*) { m_edt.SetFocus(); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CHelpFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
END_MESSAGE_MAP()

// 脚本编辑窗口（FrameWnd + 多行编辑 + 保存/取消）——关键帧脚本 / 文档类脚本共用。
// 保存时回调 MainWnd（依据 m_keyScriptTarget 决定写入文档类还是指定关键帧）。
class CFrameScriptWnd : public CFrameWnd {
public:
    CEdit m_edt;
    CButton m_ok, m_cancel;
    CMainWnd* m_owner;
    HFONT m_hFont;
    CFrameScriptWnd() : m_owner(NULL), m_hFont(NULL) {}
    BOOL CreateScript(HFONT hFont, const wchar_t* title, const wchar_t* text, CMainWnd* owner) {
        m_hFont = hFont;
        m_owner = owner;
        if (!Create(NULL, title, WS_OVERLAPPEDWINDOW, CRect(0, 0, 640, 460), owner, NULL)) return FALSE;
        CenterWindow(owner);
        ShowWindow(SW_SHOW);
        m_edt.SetWindowText(text);
        m_edt.SetFocus();
        return TRUE;
    }
    afx_msg int OnCreate(LPCREATESTRUCT) {
        CRect rc; GetClientRect(&rc);
        m_edt.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                     ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
                     CRect(6, 6, rc.Width() - 6, rc.Height() - 42), this, 1);
        if (m_hFont) m_edt.SendMessage(WM_SETFONT, (WPARAM)m_hFont, FALSE);
        m_ok.Create(L"保存", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    CRect(rc.Width() - 164, rc.Height() - 34, rc.Width() - 92, rc.Height() - 10), this, 2);
        m_cancel.Create(L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        CRect(rc.Width() - 86, rc.Height() - 34, rc.Width() - 14, rc.Height() - 10), this, 3);
        if (m_hFont) { m_ok.SendMessage(WM_SETFONT, (WPARAM)m_hFont, FALSE); m_cancel.SendMessage(WM_SETFONT, (WPARAM)m_hFont, FALSE); }
        return 0;
    }
    afx_msg void OnSize(UINT, int cx, int cy) {
        if (!m_edt.GetSafeHwnd()) return;
        m_edt.MoveWindow(6, 6, cx - 12, cy - 44);
        m_ok.MoveWindow(cx - 164, cy - 34, 72, 24);
        m_cancel.MoveWindow(cx - 86, cy - 34, 72, 24);
    }
    afx_msg void OnOK() {
        CString t; m_edt.GetWindowText(t);
        if (m_owner) m_owner->SaveScriptFromEditor((LPCTSTR)t);
        DestroyWindow();
    }
    afx_msg void OnCancel() { DestroyWindow(); }
    afx_msg void OnClose() { DestroyWindow(); }
    // 右键菜单：撤销 / 剪切 / 复制 / 粘贴 / 全选（编辑框原生支持 Ctrl+X/C/V，这里补齐右键与 Ctrl+A）
    afx_msg void OnContextMenu(CWnd*, CPoint pt) {
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 101, L"撤销\tCtrl+Z");
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, 102, L"剪切\tCtrl+X");
        menu.AppendMenu(MF_STRING, 103, L"复制\tCtrl+C");
        menu.AppendMenu(MF_STRING, 104, L"粘贴\tCtrl+V");
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, 105, L"全选\tCtrl+A");
        if (pt.x == -1 && pt.y == -1) {
            CRect rc; m_edt.GetWindowRect(&rc);
            pt = CPoint(rc.left + 40, rc.top + 20);
        }
        menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this, NULL);
    }
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) {
        switch (LOWORD(wParam)) {
            case 101: m_edt.SendMessage(WM_UNDO); return TRUE;
            case 102: m_edt.SendMessage(WM_CUT); return TRUE;
            case 103: m_edt.SendMessage(WM_COPY); return TRUE;
            case 104: m_edt.SendMessage(WM_PASTE); return TRUE;
            case 105: m_edt.SetSel(0, -1); return TRUE;
        }
        return CFrameWnd::OnCommand(wParam, lParam);
    }
    BOOL PreTranslateMessage(MSG* pMsg) {
        // Ctrl+A 全选
        if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'A' &&
            (::GetKeyState(VK_CONTROL) & 0x8000) && m_edt.GetSafeHwnd()) {
            m_edt.SetSel(0, -1);
            return TRUE;
        }
        return CFrameWnd::PreTranslateMessage(pMsg);
    }
    // 注意：CFrameWnd::PostNcDestroy 默认实现即 delete this（new 分配的框架窗口），
    // 这里不再覆盖，避免与默认实现造成双重释放（与 CHelpFrame 的窗口生命周期一致）。
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CFrameScriptWnd, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_CONTEXTMENU()
    ON_BN_CLICKED(2, &CFrameScriptWnd::OnOK)
    ON_BN_CLICKED(3, &CFrameScriptWnd::OnCancel)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

// 文档类脚本（Flash 文档类）：整个工程的初始化脚本，播放开始时构造一次，
// 此后整个运行周期每帧驱动（onEnterFrame / addEventListener 回调）+ 键盘事件全程生效
// 文件菜单"文档类脚本…"：打开 .ss 文档类编辑页（与点 .ss 标签相同；关闭后由此重新打开）
void CMainWnd::OnDocClassScript() {
    OnDocSS();
}

// 编辑指定图层关键帧的脚本（Flash 式帧脚本：进入该帧时执行一次；空白关键帧也可挂）
void CMainWnd::EditKeyFrameScript(int li, int frame) {
    std::wstring text;
    bool hasKey = false;
    if (li >= 0 && (size_t)li < m_scene.layers.size()) {
        for (size_t j = 0; j < m_scene.layers[li].keys.size(); j++)
            if (m_scene.layers[li].keys[j].frame == frame) { text = m_scene.layers[li].keys[j].script; hasKey = true; break; }
    }
    wchar_t b[96];
    swprintf(b, 96, L"%s（第 %d 层 · 第 %d 帧，进入该帧时执行一次）",
        hasKey ? L"关键帧脚本" : L"空白关键帧脚本", li + 1, frame);
    m_keyScriptTarget = L"";
    wchar_t t[32]; swprintf(t, 32, L"%d:%d", li, frame); m_keyScriptTarget = t;
    CFrameScriptWnd* w = new CFrameScriptWnd();
    w->CreateScript((HFONT)m_fontSmall.GetSafeHandle(), b, text.c_str(), this);
}

// 脚本编辑窗口"保存"回调：按 m_keyScriptTarget 写入文档类或指定关键帧
void CMainWnd::SaveScriptFromEditor(const std::wstring& text) {
    if (m_keyScriptTarget.empty()) {
        m_scene.docClass = text;
        // 文档类即时落盘到同名 .ss（工程已保存时）；未保存工程则随下次保存写入
        if (!m_scene.projectPath.empty()) {
            std::wstring ssPath = ProjectIO::DocClassPath(m_scene.projectPath);
            if (text.empty()) ::DeleteFileW(ssPath.c_str());
            else ProjectIO::WriteFileUtf8(ssPath, text);
        }
        UpdateStatus();
        return;
    }
    size_t pos = m_keyScriptTarget.find(L':');
    if (pos == std::wstring::npos) return;
    int li = (int)wcstol(m_keyScriptTarget.substr(0, pos).c_str(), NULL, 10);
    int fr = (int)wcstol(m_keyScriptTarget.substr(pos + 1).c_str(), NULL, 10);
    if (li < 0 || li >= (int)m_scene.layers.size()) return;
    // 只更新已存在的关键帧条目（与右键"编辑此关键帧脚本"一致；不存在则忽略）
    for (size_t j = 0; j < m_scene.layers[li].keys.size(); j++) {
        if (m_scene.layers[li].keys[j].frame == fr) {
            m_scene.layers[li].keys[j].script = text;
            break;
        }
    }
    m_timeline.Invalidate(FALSE);
    UpdateStatus();
}

// Flash 式"编译器错误"消息框：收集关键帧脚本错误后统一弹出（播放/调试自动切换到此框）
void CMainWnd::ShowCompileErrors(const std::vector<std::wstring>& entries) {
    if (entries.empty()) return;
    std::wstring body;
    for (size_t i = 0; i < entries.size(); i++) {
        body += entries[i];
        if (i + 1 < entries.size()) body += L"\r\n";
    }
    wchar_t b[128];
    swprintf(b, 128, L"%d个错误，0个警告", (int)entries.size());
    m_errEdt.SetWindowText(body.c_str());
    m_errStat.SetWindowText(b);
    m_tab.SetCurSel(2);   // 自动切换到"编译器错误"页
    ShowTabPage(2);
}

// 顶部面板 Tab 切换：仅显示当前页，隐藏其余
void CMainWnd::OnTabSelChange(NMHDR*, LRESULT* pResult) {
    *pResult = 0;
    ShowTabPage(m_tab.GetCurSel());
}

void CMainWnd::ShowTabPage(int sel) {
    m_timeline.ShowWindow(sel == 0 ? SW_SHOW : SW_HIDE);
    m_traceEdt.ShowWindow(sel == 1 ? SW_SHOW : SW_HIDE);
    m_traceStat.ShowWindow(sel == 1 ? SW_SHOW : SW_HIDE);
    m_errEdt.ShowWindow(sel == 2 ? SW_SHOW : SW_HIDE);
    m_errStat.ShowWindow(sel == 2 ? SW_SHOW : SW_HIDE);
}

// 文档 Tab 切换（.s3d/.ss，位于时间轴下面、场景导航上面）：与顶部面板同款 Tab 交互
void CMainWnd::OnDocTabSelChange(NMHDR*, LRESULT* pResult) {
    *pResult = 0;
    if (m_docTab.GetCurSel() == 1) OnDocSS();
    else SetDocMode(false);
}

// 文档 Tab 悬停提示：显示 .s3d/.ss 的完整文件路径（内置示例也显示其应有的完整路径）
BOOL CMainWnd::OnDocTabTip(UINT, NMHDR* pNMHDR, LRESULT* pResult) {
    NMTTDISPINFO* pDisp = reinterpret_cast<NMTTDISPINFO*>(pNMHDR);
    int idx = (int)pDisp->hdr.idFrom;   // Tab 标签索引（0=.s3d 1=.ss）
    std::wstring path;
    if (idx == 0) {
        if (!m_scene.projectPath.empty()) path = m_scene.projectPath;
        else if (!m_titleName.empty()) path = ExampleFilePath(m_titleName);
        else path = L"未命名工程（尚未保存）";
    } else {
        if (!m_scene.projectPath.empty()) path = ProjectIO::DocClassPath(m_scene.projectPath);
        else if (!m_titleName.empty()) {
            path = ExampleFilePath(m_titleName);
            size_t dot = path.find_last_of(L'.');
            if (dot != std::wstring::npos) path = path.substr(0, dot) + L".ss";
        } else path = L"文档类脚本（尚未保存）";
    }
    wcsncpy_s(pDisp->szText, path.c_str(), _TRUNCATE);
    *pResult = 0;
    return TRUE;
}

// 文档 Tab"×"关闭：关 .s3d = 关闭工程（新建）；关 .ss = 移除文档类脚本
LRESULT CMainWnd::OnDocTabCloseMsg(WPARAM wParam, LPARAM) {
    int idx = (int)wParam;
    if (idx < 0) return 0;
    if (idx == 0) {        // 关闭 .s3d 工程
        OnNew();
    } else if (idx == 1) { // 关闭 .ss 文档类：仅关闭编辑标签，不删除磁盘 .ss 文件（修改内容保留，可重新打开）
        m_docSSOpen = false;             // .ss 标签关闭（隐藏），文件菜单可重新打开
        SetDocMode(false);               // 回到场景编辑（编辑内容写回内存文档类；磁盘 .ss 保留）
        m_status.SetWindowText(_T("已关闭文档类脚本（.ss）"));
    }
    return 0;
}

// 文档 Tab：切回场景编辑（.s3d）
void CMainWnd::OnDocS3D() { SetDocMode(false); }

// 文档 Tab：切到文档类脚本编辑（.ss）——首次打开自动填模板，随后始终载入最新文档类
void CMainWnd::OnDocSS() {
    m_docSSOpen = true;   // 打开 .ss 标签
    std::wstring cur = m_scene.docClass;
    if (cur.empty()) {
        cur = L"package {\r\n"
              L"\timport super3d.Object3D;\r\n"
              L"\tpublic class main extends Object3D {\r\n"
              L"\t\tpublic function main() {\t\t    \r\n"
              L"\t\t    stage.addEventListener(Event.ENTER_FRAME, onEnterFrame);\r\n"
              L"\t\t}\r\n"
              L"\t\tpublic function onEnterFrame(e:Event):void {\r\n"
              L"\t\t\t\r\n"
              L"\t\t}\r\n"
              L"\t}\r\n"
              L"}\r\n";
        m_scene.docClass = cur;   // 立即写入文档类（保存/导出即生效）
    }
    SetDocText(cur.c_str());
    SetDocMode(true);
}

void CMainWnd::SetDocMode(bool scriptMode) {
    if (m_docScriptMode && !scriptMode) {
        // 切出脚本模式：把编辑内容写回文档类，并同步落盘 .ss（工程已保存时；空文档类则删除 .ss）
        m_scene.docClass = GetDocEditText();
        if (!m_scene.projectPath.empty()) {
            std::wstring ssPath = ProjectIO::DocClassPath(m_scene.projectPath);
            if (m_scene.docClass.empty()) ::DeleteFileW(ssPath.c_str());
            else ProjectIO::WriteFileUtf8(ssPath, m_scene.docClass);
        }
    }
    m_docScriptMode = scriptMode;
    if (m_docTab.GetSafeHwnd()) m_docTab.SetCurSel(scriptMode ? 1 : 0);   // 同步文档 Tab 选中
    Layout();
    UpdateDocTabs();
    if (scriptMode && m_docEdt.GetSafeHwnd()) m_docEdt.SetFocus();   // .ss 页自动聚焦脚本编辑器（可复制粘贴）
}

// 打开/保存后刷新文档 Tab（工程名.s3d | 工程名.ss；无文件时用示例名）
// .ss 标签是否显示由 m_docSSOpen 决定：关闭 .ss 后标签消失，文件菜单"文档类脚本…"重新打开
void CMainWnd::UpdateDocTabs() {
    std::wstring name;
    if (!m_scene.projectPath.empty()) {
        size_t sep = m_scene.projectPath.find_last_of(L"/\\");
        name = (sep == std::wstring::npos) ? m_scene.projectPath : m_scene.projectPath.substr(sep + 1);
        size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos) name = name.substr(0, dot);
    } else if (!m_titleName.empty()) {
        name = m_titleName;
    } else {
        name = L"未命名";
    }
    // 标签文字末尾加留白：让 Tab 控件算出的标签宽度包含右端"×"区，文件名不被省略号截断
    std::wstring s3d = name + L".s3d    ", ss = name + L".ss    ";
    TCITEM ti;
    ti.mask = TCIF_TEXT;
    ti.pszText = (LPWSTR)s3d.c_str();
    m_docTab.SendMessage(TCM_SETITEM, 0, (LPARAM)&ti);
    if (m_docSSOpen) {
        if (m_docTab.GetItemCount() < 2) m_docTab.InsertItem(1, ss.c_str());
        else { ti.pszText = (LPWSTR)ss.c_str(); m_docTab.SendMessage(TCM_SETITEM, 1, (LPARAM)&ti); }
    } else {
        if (m_docTab.GetItemCount() >= 2) m_docTab.DeleteItem(1);   // 关闭 .ss：标签消失（仿 Flash）
    }
    m_docTab.Invalidate(FALSE);
    // 文档 Tab 选中同步：场景模式恒在 .s3d；脚本模式恒在 .ss。
    // .ss 标签关闭时被 DeleteItem 移除，重新打开需在 InsertItem 之后补切回 .ss（否则选择仍停在 .s3d）
    if (!m_docScriptMode) {
        if (m_docTab.GetCurSel() != 0) m_docTab.SetCurSel(0);
    } else {
        if (m_docTab.GetItemCount() >= 2 && m_docTab.GetCurSel() != 1) m_docTab.SetCurSel(1);
    }
}

std::wstring CMainWnd::GetDocEditText() {
    CString t;
    m_docEdt.GetWindowText(t);
    std::wstring s((LPCTSTR)t);
    // RichEdit 行尾是 \r（可能已带 \n），统一为标准 CRLF（记事本/保存/编辑兼容）
    std::wstring out;
    out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\r') {
            out += L"\r\n";
            if (i + 1 < s.size() && s[i + 1] == L'\n') i++;
        } else out += s[i];
    }
    return out;
}

// ---- .ss 编辑器自动缩进 / 代码格式化（AS3.0 式）----
void CMainWnd::AutoIndentReturn() {
    if (!m_docEdt.GetSafeHwnd()) return;
    CHARRANGE sel; sel.cpMin = sel.cpMax = 0;
    m_docEdt.SendMessage(EM_EXGETSEL, 0, (LPARAM)&sel);
    long pos = sel.cpMin;
    // 用 RichEdit 行 API 取光标所在行：索引与 RichEdit 内部一致，
    // 避免 GetWindowText 的 CRLF(2字符) 与 RichEdit 索引(CRLF=1位) 偏移导致行定位错乱
    int line = (int)m_docEdt.SendMessage(EM_LINEFROMCHAR, (WPARAM)pos, 0);
    int lineLen = (int)m_docEdt.SendMessage(EM_LINELENGTH, (WPARAM)pos, 0);
    std::wstring prevLine;
    if (lineLen > 0) {
        std::vector<wchar_t> buf(lineLen + 2, 0);
        buf[0] = (wchar_t)lineLen;   // EM_GETLINE 约定：首字符存放最大长度
        int got = (int)m_docEdt.SendMessage(EM_GETLINE, (WPARAM)line, (LPARAM)&buf[0]);
        if (got > 0) prevLine.assign(buf.begin() + 1, buf.begin() + 1 + got);
    }
    // 上一行前导空白（原样继承 tab/空格）
    size_t ps = prevLine.find_first_not_of(L" \t");
    std::wstring lead = (ps == std::wstring::npos) ? L"" : prevLine.substr(0, ps);
    // 上一行行尾是 { → 加一级缩进
    size_t pe = prevLine.find_last_not_of(L" \t");
    std::wstring pt = (pe == std::wstring::npos) ? L"" : prevLine.substr(0, pe + 1);
    if (!pt.empty() && pt.back() == L'{') lead += L"\t";
    // 光标前文本以 } 开头 → 减一级（对齐右花括号）
    int lineIndex = (int)m_docEdt.SendMessage(EM_LINEINDEX, (WPARAM)line, 0);
    int col = (int)(pos - lineIndex);
    if (col < 0) col = 0;
    if (col > (int)prevLine.size()) col = (int)prevLine.size();
    std::wstring curLine = (col > 0) ? prevLine.substr(0, col) : L"";
    size_t cs = curLine.find_first_not_of(L" \t");
    if (cs != std::wstring::npos && curLine[cs] == L'}' && !lead.empty()) {
        size_t lt = lead.find_last_of(L"\t");
        if (lt != std::wstring::npos) lead.erase(lt);
    }
    m_docEdt.ReplaceSel((L"\r\n" + lead).c_str());
}

// ---- .ss 自动补全（AS3 式：输入 对象名. 弹出属性候选，↑↓/回车/双击选择插入）----
Object3D* CMainWnd::FindSceneObjectByName(const std::wstring& name) {
    std::vector<Object3D*> all;
    m_scene.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++)
        if (all[i] && all[i]->name == name) return all[i];
    return NULL;
}

void CMainWnd::AutoLog(const wchar_t* msg) {   // 临时诊断：自动补全失败原因（定位后移除）
    if (m_errEdt.GetSafeHwnd()) {
        CString t;
        m_errEdt.GetWindowText(t);
        m_errEdt.SetWindowText(t + CString(msg) + L"\r\n");
    }
    if (m_traceEdt.GetSafeHwnd()) {   // 同时写"输出"面板，确保用户可见
        CString t;
        m_traceEdt.GetWindowText(t);
        m_traceEdt.SetWindowText(t + CString(msg) + L"\r\n");
    }
}

void CMainWnd::TryAutoComplete() {
    if (!m_docScriptMode || !m_docEdt.GetSafeHwnd()) { HideAutoComplete(); AutoLog(L"[AC] 非脚本模式或无编辑器"); return; }
    if (m_autoInserted) { HideAutoComplete(); return; }   // 刚完成插入：不重复弹出
    CHARRANGE sel; sel.cpMin = sel.cpMax = 0;
    m_docEdt.SendMessage(EM_EXGETSEL, 0, (LPARAM)&sel);
    long pos = sel.cpMin;
    int line = (int)m_docEdt.SendMessage(EM_LINEFROMCHAR, (WPARAM)pos, 0);
    int lineIndex = (int)m_docEdt.SendMessage(EM_LINEINDEX, (WPARAM)line, 0);
    int col = (int)(pos - lineIndex);
    if (col < 0) col = 0;
    // 用 GetWindowText 全文提取光标所在行——RichEdit 的 EM_GETLINE 在宽字符下
    // 返回内容不可靠（曾取不到行文本导致自动补全误判"行内无点号"）。
    // 行号→全文行起点换算：CRLF 在 GetWindowText 中占 2 字符、RichEdit 索引占 1 位，
    // 但行内偏移 col 与全文行内偏移一致，可直接按行号跳过换行符。
    std::wstring before;
    if (col > 0) {
        CString t;
        m_docEdt.GetWindowText(t);
        std::wstring full((LPCTSTR)t);
        size_t rowStart = 0;
        for (int i = 0; i < line && rowStart < full.size(); i++) {
            size_t p = full.find_first_of(L"\r\n", rowStart);
            if (p == std::wstring::npos) { rowStart = full.size(); break; }
            rowStart = p + 1;
            if (rowStart < full.size() && full[rowStart - 1] == L'\r' && full[rowStart] == L'\n') rowStart++;
        }
        size_t end = rowStart + (size_t)col;
        if (end > full.size()) end = full.size();
        before = full.substr(rowStart, end - rowStart);
    }
    // 找最后一个 '.'，'.' 前必须是合法标识符
    size_t dot = before.find_last_of(L'.');
    if (dot == std::wstring::npos) { HideAutoComplete(); AutoLog(L"[AC] 行内无点号"); return; }
    std::wstring ident;
    size_t k = dot;
    while (k > 0) {
        wchar_t c = before[k - 1];
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
            (c >= L'0' && c <= L'9') || c == L'_') { ident = c + ident; k--; }
        else break;
    }
    if (ident.empty()) { HideAutoComplete(); AutoLog(L"[AC] 点号前无标识符"); return; }
    Object3D* obj = FindSceneObjectByName(ident);
    if (!obj) { HideAutoComplete(); AutoLog((L"[AC] 场景无此对象:" + ident).c_str()); return; }
    // '.' 后已输入前缀：必须全为标识符字符（如 "mc.x = 100" 中 '.' 后含空格则不再补全）
    m_autoPrefix = before.substr(dot + 1);
    for (size_t i = 0; i < m_autoPrefix.size(); i++) {
        wchar_t c = m_autoPrefix[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'_')) { HideAutoComplete(); return; }
    }
    // 候选：SuperScript 引擎支持的 Object3D 属性（读/写一致）
    static const wchar_t* props[] = { L"x", L"y", L"z", L"rx", L"ry", L"rz",
        L"rotation", L"rotationX", L"rotationY", L"rotationZ",
        L"sx", L"sy", L"sz", L"s", L"scaleX", L"scaleY", L"scaleZ",
        L"visible", L"name", L"parent", L"type", L"segments",
        L"red", L"green", L"blue" };
    m_autoItems.clear();
    for (int i = 0; i < (int)(sizeof(props) / sizeof(props[0])); i++) {
        std::wstring p = props[i];
        if (m_autoPrefix.empty() || p.compare(0, m_autoPrefix.size(), m_autoPrefix) == 0)
            m_autoItems.push_back(p);
    }
    if (m_autoItems.empty()) { HideAutoComplete(); AutoLog(L"[AC] 无匹配候选"); return; }
    ShowAutoCompleteList(pos);
    AutoLog(L"[AC] 已弹出候选列表");
}

void CMainWnd::ShowAutoCompleteList(long pos) {
    if (!m_docEdt.GetSafeHwnd() || !m_autoList.GetSafeHwnd()) { AutoLog(L"[AC] 列表控件无效"); return; }   // 列表创建失败时静默禁用自动补全
    POINTL pt; pt.x = -1; pt.y = -1;
    m_docEdt.SendMessage(EM_POSFROMCHAR, (WPARAM)pos, (LPARAM)&pt);
    CPoint sp;
    if (pt.x >= 0 && pt.y >= 0) sp = CPoint(pt.x, pt.y + 18);
    else sp = GetCaretPos();   // 客户区坐标
    m_docEdt.ClientToScreen(&sp);
    int h = (int)m_autoItems.size() * 16 + 4;
    if (h > 240) h = 240;
    m_autoList.SetWindowPos(&CWnd::wndTop, sp.x, sp.y, 250, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    m_autoList.ResetContent();
    for (size_t i = 0; i < m_autoItems.size(); i++)
        m_autoList.AddString(m_autoItems[i].c_str());
    m_autoList.SetCurSel(0);
    m_autoVisible = true;
}

void CMainWnd::HideAutoComplete() {
    if (m_autoVisible) {
        m_autoList.ShowWindow(SW_HIDE);
        m_autoVisible = false;
    }
}

void CMainWnd::InsertAutoItem() {
    int i = m_autoList.GetCurSel();
    if (i < 0) { HideAutoComplete(); return; }
    std::wstring item = m_autoItems[i];
    HideAutoComplete();
    m_docEdt.SetFocus();
    m_autoInserted = true;
    CHARRANGE sel; sel.cpMin = sel.cpMax = 0;
    m_docEdt.SendMessage(EM_EXGETSEL, 0, (LPARAM)&sel);
    long pos = sel.cpMin;
    long start = pos - (long)m_autoPrefix.size();   // 选中 '.' 后已输入前缀
    if (start < 0) start = pos;
    m_docEdt.SendMessage(EM_SETSEL, (WPARAM)start, (LPARAM)pos);
    m_docEdt.ReplaceSel(item.c_str());
    m_autoInserted = false;
}

void CMainWnd::OnAutoDblClk() { InsertAutoItem(); }

// 代码格式化：逐行重排缩进（字符串/注释内的 { } 不计入缩进级别）
static std::wstring FormatSsCode(const std::wstring& src) {
    std::wstring norm;
    norm.reserve(src.size() + 16);
    for (size_t i = 0; i < src.size(); i++) {   // RichEdit 行尾 \r → \n 统一（\r\n 只算一个换行，避免产生空行）
        if (src[i] == L'\r') {
            norm += L'\n';
            if (i + 1 < src.size() && src[i + 1] == L'\n') i++;
        } else norm += src[i];
    }
    std::wstring out;
    int indent = 0;
    bool inBlockC = false;
    size_t i = 0, n = norm.size();
    while (i < n) {
        size_t e = i;
        while (e < n && norm[e] != L'\n') e++;
        std::wstring raw = norm.substr(i, e - i);
        if (e < n) e++;
        std::wstring trimmed = raw;
        size_t t0 = trimmed.find_first_not_of(L" \t");
        if (t0 == std::wstring::npos) { out += L"\r\n"; i = e; continue; }   // 空行保留
        trimmed = trimmed.substr(t0);
        size_t t1 = trimmed.find_last_not_of(L" \t");
        if (t1 != std::wstring::npos) trimmed = trimmed.substr(0, t1 + 1);
        // 统计行内 { }（跳过字符串与注释；块注释状态跨行保持）
        bool startsClose = !trimmed.empty() && trimmed[0] == L'}';
        int open = 0, close = 0;
        bool inStr = false, inLineC = false;
        wchar_t quote = 0;
        for (size_t k = 0; k < trimmed.size(); k++) {
            wchar_t c = trimmed[k];
            if (inBlockC) {
                if (c == L'*' && k + 1 < trimmed.size() && trimmed[k + 1] == L'/') { inBlockC = false; k++; }
                continue;
            }
            if (inLineC) break;
            if (inStr) {
                if (c == L'\\') { k++; continue; }
                if (c == quote) inStr = false;
                continue;
            }
            if (c == L'"' || c == L'\'') { inStr = true; quote = c; continue; }
            if (c == L'/' && k + 1 < trimmed.size() && trimmed[k + 1] == L'/') { inLineC = true; continue; }
            if (c == L'/' && k + 1 < trimmed.size() && trimmed[k + 1] == L'*') { inBlockC = true; k++; continue; }
            if (c == L'{') open++;
            else if (c == L'}') close++;
        }
        // 行首 } 只影响本行输出对齐（闭合花括号减一级），不重复计入缩进状态
        int outIndent = indent;
        if (startsClose && outIndent > 0) outIndent--;
        out += std::wstring(outIndent, L'\t') + trimmed + L"\r\n";
        indent += open - close;
        if (indent < 0) indent = 0;
        i = e;
    }
    return out;
}

void CMainWnd::DoFormatDoc() {
    if (!m_docEdt.GetSafeHwnd()) return;
    CStringW txt;
    m_docEdt.GetWindowText(txt);
    std::wstring src(txt.GetString(), txt.GetLength());
    std::wstring fmt = FormatSsCode(src);
    if (fmt != src) SetDocText(fmt.c_str());   // 格式化后重新着色
    else ApplyDocSyntax();                     // 已规范也强制重扫着色，Ctrl+F 始终有响应
}

// ---- .ss 编辑器 AS3 式语法着色（Flash 默认配色：关键字蓝 / 类型紫 / 注释绿 / 字符串暗红 / 数字蓝）----
static bool SsInList(const wchar_t* tok, const wchar_t* const* list, int count) {
    for (int i = 0; i < count; i++) if (wcscmp(tok, list[i]) == 0) return true;
    return false;
}
static bool SsIsKeyword(const std::wstring& t) {
    static const wchar_t* k[] = { L"package", L"import", L"public", L"private", L"protected",
        L"internal", L"class", L"extends", L"implements", L"function", L"var", L"const",
        L"return", L"if", L"else", L"while", L"for", L"do", L"switch", L"case", L"default",
        L"break", L"continue", L"new", L"this", L"super", L"true", L"false", L"null",
        L"void", L"static", L"final", L"override", L"dynamic", L"native", L"try", L"catch",
        L"finally", L"throw", L"with", L"typeof", L"instanceof", L"delete", L"is", L"in",
        L"as", L"namespace", L"get", L"set", L"using" };
    return SsInList(t.c_str(), k, sizeof(k) / sizeof(k[0]));
}
static bool SsIsType(const std::wstring& t) {
    static const wchar_t* k[] = { L"int", L"uint", L"Number", L"String", L"Boolean",
        L"Array", L"Object", L"MovieClip", L"Sprite", L"TextField", L"Vector", L"Function",
        L"RegExp", L"Date", L"Math", L"Event", L"MouseEvent", L"Point", L"Loader",
        L"URLRequest", L"Stage", L"DisplayObject", L"Shape", L"Graphics", L"Bitmap",
        L"ByteArray", L"Object3D" };
    return SsInList(t.c_str(), k, sizeof(k) / sizeof(k[0]));
}

void CMainWnd::SetDocText(const wchar_t* t) {
    m_bUpdatingScript = true;
    m_docEdt.SendMessage(EM_SETREDRAW, FALSE, 0);   // 关闭重绘：替换文本+着色一次性完成（看不到格式化过程）
    m_docEdt.SetWindowText(t);
    m_bUpdatingScript = false;
    ApplyDocSyntax();
    m_docEdt.SendMessage(EM_SETREDRAW, TRUE, 0);
    m_docEdt.Invalidate(FALSE);
}

void CMainWnd::OnDocEdtChange() {
    if (m_bUpdatingScript) return;   // 程序设置文本（SetDocText）不触发
    m_syntaxDirty = true;
    SetTimer(2, 150, NULL);          // 防抖：停止输入 150ms 后重新着色
    TryAutoComplete();               // 输入 '.' 或前缀变化：尝试弹出/更新/关闭属性候选
}

// .ss 编辑器右键菜单（RichEdit 无默认菜单，仿 Flash 动作面板补齐）
void CMainWnd::OnContextMenu(CWnd* pWnd, CPoint pt) {
    if (pWnd && pWnd->GetSafeHwnd() == m_docEdt.GetSafeHwnd()) {
        if (pt.x == -1 && pt.y == -1) {   // 键盘菜单键
            CRect rc; m_docEdt.GetWindowRect(&rc);
            pt = CPoint(rc.left + 40, rc.top + 20);
        }
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, 2201, L"撤销\tCtrl+Z");
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, 2202, L"剪切\tCtrl+X");
        menu.AppendMenu(MF_STRING, 2203, L"复制\tCtrl+C");
        menu.AppendMenu(MF_STRING, 2204, L"粘贴\tCtrl+V");
        menu.AppendMenu(MF_STRING, 2205, L"全选\tCtrl+A");
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, 2206, L"格式化代码\tCtrl+Shift+F");
        menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
        return;
    }
    CWnd::OnContextMenu(pWnd, pt);
}
void CMainWnd::OnDocMenuUndo() { m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_UNDO); }
void CMainWnd::OnDocMenuCut() { m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_CUT); }
void CMainWnd::OnDocMenuCopy() { m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_COPY); }
void CMainWnd::OnDocMenuPaste() { m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_PASTE); }
void CMainWnd::OnDocMenuSelectAll() { m_docEdt.SetFocus(); m_docEdt.SetSel(0, -1); }
void CMainWnd::OnDocMenuFormat() { m_docEdt.SetFocus(); DoFormatDoc(); }

void CMainWnd::ApplyDocSyntax() {
    if (!m_docEdt.GetSafeHwnd()) return;
    m_docEdt.SendMessage(EM_SETREDRAW, FALSE, 0);   // 抑制重绘：着色逐段进行但一次性呈现
    CStringW txt;
    m_docEdt.GetWindowText(txt);
    int n = txt.GetLength();
    if (n <= 0) {
        m_docEdt.SendMessage(EM_SETREDRAW, TRUE, 0);
        return;
    }
    // 保存当前选择位置（着色后恢复，防止光标跳动）
    CHARRANGE sel; sel.cpMin = sel.cpMax = 0;
    m_docEdt.SendMessage(EM_EXGETSEL, 0, (LPARAM)&sel);
    m_bUpdatingScript = true;
    // 全文先统一黑色
    CHARFORMAT2W def; memset(&def, 0, sizeof(def)); def.cbSize = sizeof(def);
    def.dwMask = CFM_COLOR; def.crTextColor = RGB(0, 0, 0);
    m_docEdt.SetSel(0, -1);
    m_docEdt.SetSelectionCharFormat(def);
    // 逐 token 着色
    const wchar_t* p = txt.GetString();
    int i = 0;
    while (i < n) {
        wchar_t c = p[i];
        // 注释 // 与 /* */
        if (c == L'/' && i + 1 < n && (p[i + 1] == L'/' || p[i + 1] == L'*')) {
            int s = i;
            if (p[i + 1] == L'/') { while (i < n && p[i] != L'\n') i++; }
            else {
                i += 2;
                while (i < n && !(p[i] == L'*' && i + 1 < n && p[i + 1] == L'/')) i++;
                if (i < n) i += 2;
            }
            m_docEdt.SetSel(s, i);
            m_docEdt.SetSelectionCharFormat(def);
            CHARFORMAT2W cf; memset(&cf, 0, sizeof(cf)); cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR; cf.crTextColor = RGB(0, 128, 0);   // 注释绿
            m_docEdt.SetSelectionCharFormat(cf);
            continue;
        }
        // 字符串
        if (c == L'"' || c == L'\'') {
            int s = i; wchar_t q = c; i++;
            while (i < n) {
                if (p[i] == L'\\') { i += 2; continue; }
                if (p[i] == q) { i++; break; }
                i++;
            }
            CHARFORMAT2W cf; memset(&cf, 0, sizeof(cf)); cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR; cf.crTextColor = RGB(128, 0, 0);   // 字符串暗红
            m_docEdt.SetSel(s, i);
            m_docEdt.SetSelectionCharFormat(cf);
            continue;
        }
        // 数字
        if (c >= L'0' && c <= L'9') {
            int s = i;
            while (i < n && (iswalnum(p[i]) || p[i] == L'.')) i++;
            CHARFORMAT2W cf; memset(&cf, 0, sizeof(cf)); cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR; cf.crTextColor = RGB(0, 0, 255);   // 数字蓝
            m_docEdt.SetSel(s, i);
            m_docEdt.SetSelectionCharFormat(cf);
            continue;
        }
        // 标识符（含中文注释性文字：按普通黑处理）
        if (iswalpha(c) || c == L'_') {
            int s = i;
            while (i < n && (iswalnum(p[i]) || p[i] == L'_')) i++;
            std::wstring tok(p + s, i - s);
            if (SsIsKeyword(tok)) {
                CHARFORMAT2W cf; memset(&cf, 0, sizeof(cf)); cf.cbSize = sizeof(cf);
                cf.dwMask = CFM_COLOR; cf.crTextColor = RGB(0, 0, 255);   // 关键字蓝
                m_docEdt.SetSel(s, i);
                m_docEdt.SetSelectionCharFormat(cf);
            } else if (SsIsType(tok)) {
                CHARFORMAT2W cf; memset(&cf, 0, sizeof(cf)); cf.cbSize = sizeof(cf);
                cf.dwMask = CFM_COLOR; cf.crTextColor = RGB(128, 0, 128);   // 类型紫
                m_docEdt.SetSel(s, i);
                m_docEdt.SetSelectionCharFormat(cf);
            }
            continue;
        }
        i++;
    }
    // 恢复选择
    m_docEdt.SendMessage(EM_EXSETSEL, 0, (LPARAM)&sel);
    m_bUpdatingScript = false;
    m_docEdt.SendMessage(EM_SETREDRAW, TRUE, 0);
    m_docEdt.Invalidate(FALSE);
}

// 播放进入关键帧时执行该帧脚本（含空白关键帧；每层每帧一个脚本）。
// 关键帧脚本按 Flash 帧动作语义写入"显示值"（与导出网页一致），
// 执行后把显示值回写本地，使脚本效果在后续帧保持（SetFrame 会跳过脚本关键帧的插值写回）。
void CMainWnd::ExecuteKeyFrameScripts(int frame) {
    std::vector<std::wstring> errs;
    std::wstring path = m_scene.projectPath;
    if (path.empty()) path = L"（未命名工程）";
    for (size_t li = 0; li < m_scene.layers.size(); li++) {
        for (size_t j = 0; j < m_scene.layers[li].keys.size(); j++) {
            LayerKey& k = m_scene.layers[li].keys[j];
            if (k.frame != frame || k.script.empty()) continue;
            ScriptEngine s;
            s.SetText(k.script);
            std::wstring err;
            if (!s.EnsureParsed(err)) {
                std::wstring desc; int ln = ParseErrLine(err, desc);
                wchar_t b[512];
                swprintf(b, 512, L"%ls，第%d行，第1列 1001: 第%lld层·帧%d：%ls",
                    path.c_str(), ln, (long long)li + 1, frame, desc.c_str());
                errs.push_back(b);
                continue;
            }
            s.RunKeyFrame(&m_scene, frame);
            if (s.LastRunError()) {
                std::wstring desc; int ln = ParseErrLine(s.GetLog(), desc);
                wchar_t b[512];
                swprintf(b, 512, L"%ls，第%d行，第1列 1002: 第%lld层·帧%d：%ls",
                    path.c_str(), ln, (long long)li + 1, frame, desc.c_str());
                errs.push_back(b);
                continue;
            }
            if (!s.GetLog().empty()) {
                m_scriptWnd.SetOutput(s.GetLog().c_str());
                m_traceLines.push_back(s.GetLog());
            }
            // 帧动作写的是显示值（dx），必须"显示→本地"持久化，
            // 否则 SyncDisplayToLocalAll（本地→显示）会用旧本地值把脚本效果覆盖回原位
            m_scene.CopyLocalFromDisplayAll();
            m_viewport.Repaint();
        }
    }
    if (!errs.empty()) ShowCompileErrors(errs);
}

void CMainWnd::OnHelp() {
    const wchar_t* msg =
        L"Super3D使用说明\r\n"
        L"----------------------------------------\r\n"
        L"【鼠标操作】\r\n"
        L"  左键：点选元件 / 拖拽（用移动/旋转/缩放工具）\r\n"
        L"  右键拖动：旋转视角\r\n"
        L"  滚轮：拉近拉远\r\n"
        L"  平移画面：按 Shift 再按左键拖动，或点【平移视角】工具后左键拖动\r\n"
        L"\r\n"
        L"【快捷键】\r\n"
        L"  方向键/PageUp/PageDown：微调选中元件位置\r\n"
        L"  Delete：删除    Ctrl+Z：撤销    Ctrl+D：复制\r\n"
        L"  Ctrl+S：保存    Ctrl+O：打开    Ctrl+N：新建\r\n"
        L"\r\n"
        L"【图元件类】\r\n"
        L"  立方体 / 球 / 圆柱 / 圆锥 / 圆环 / 圆管 / 方管 / 平面 / 曲线 / 绿叶 / 3D文本\r\n"
        L"  圆管、方管是空心管：属性面板可改外径/内径（外长/内长）和高\r\n"
        L"  球、圆柱、圆锥的【细分】越大越圆滑（默认 24）\r\n"
        L"\r\n"
        L"【布尔运算（像 Blender 一样组合形状）】\r\n"
        L"  1. 先点选一个元件（它是 A，被打磨的）\r\n"
        L"  2. 按住 Ctrl 再点选另一个元件（它是 B，挖孔的模具）\r\n"
        L"  3. 在场景中点右键 →【布尔差集】A−B 挖孔 /【布尔并集】A∪B 合并 /【布尔交集】A∩B\r\n"
        L"  差集=在 A 上挖出 B 的形状（圆管挖出圆孔、方管挖出方孔）；\r\n"
        L"  孔的圆滑程度跟随 B 的【细分】参数\r\n"
        L"  布尔结果还能再次参与布尔运算（可连续多次挖孔）\r\n"
        L"  提示：只选中 A 一个元件时，软件会自动寻找与它重叠的孔体作为 B\r\n"
        L"\r\n"
        L"【网格编辑（点/线/面，像 Blender 一样）】\r\n"
        L"  1. 选中元件，点属性面板【网格】行的【编辑网格】\r\n"
        L"  2. 模式下拉（或按 1/2/3 键）切换 点/线/面，点的是元件现有的点线面\r\n"
        L"     长方体=8点/12棱/6面；圆柱按细分变成多棱柱的点线面\r\n"
        L"  3. 点击选中点/线/面（按住 Ctrl 或 Shift 点击可多选），拖动即改变形状\r\n"
        L"  4. 按 Del 删除选中的点/线/面（点=相连的面消失，面=整块挖掉）\r\n"
        L"  5. 按 Esc 或再点【退出编辑】结束；【重建网格】可恢复成原始形状\r\n"
        L"  编辑结果随工程保存（Ctrl+Z 可撤销），导出网页/播放器里正常显示\r\n"
        L"\r\n"
        L"【做动画】\r\n"
        L"  1. 选中元件，把时间轴上的红色播放头拖到某一帧\r\n"
        L"  2. 移动/旋转元件到想要的样子（软件会自动记录关键帧）\r\n"
        L"  3. 把播放头拖到另一帧，再改一次\r\n"
        L"  4. 点【播放】，元件就会在两帧之间动起来\r\n"
        L"  时间轴：滚轮横向移动，或拖动底部滚动条；时间轴可无限延长\r\n"
        L"  底部【时间轴】/【脚本】选项卡：脚本面板不再挤占时间轴空间\r\n"
        L"  提示：时间轴上点右键可以删除菱形关键帧\r\n"
        L"  帧脚本（Flash 式帧动作）：在时间轴某关键帧（或空白关键帧）的菱形上\r\n"
        L"    点右键 →【编辑此关键帧脚本…】→ 写入 SuperScript，播放进入该帧时\r\n"
        L"    自动执行一次；时间轴上带橙色小三角的关键帧即挂有脚本\r\n"
        L"  文档类脚本：菜单【文件 → 文档类脚本…】→ Flash 文档类（Document Class），\r\n"
        L"    支持 package/import/class/extends 语法；构造函数播放开始时执行一次，\r\n"
        L"    onEnterFrame / addEventListener 回调每帧执行，onKeyDown/onKeyUp 键盘事件\r\n"
        L"    全程生效——即使时间轴没有任何关键帧脚本，也会影响整个运行周期\r\n"
        L"\r\n"
        L"【元件工作流（像 Flash 一样）】\r\n"
        L"  组合：选中元件（可 Ctrl 多选几个），点【组】或右键「转换为 MovieClip」\r\n"
        L"        → 它们包进一个新组（MovieClip）；组有自己的坐标系，原点在组合体中心，\r\n"
        L"        旋转/缩放组时整体绕自身中心变换（成员世界位置不变）\r\n"
        L"  加入已有组：在场景树里按住元件拖到组上放开即可（位置不变）\r\n"
        L"  编辑：选中组，再点【立方体/球/...】→ 新元件加进组里\r\n"
        L"  实例：选中组点【复制】→ 得到一份副本（可分别控制位置动画）\r\n"
        L"  打散：选中组点【打散】→ 组里的元件散开，位置不变\r\n"
        L"  提示：场景树里可双击改名；脚本用名字控制组里的任何元件\r\n"
        L"\r\n"
        L"【脚本 SuperScript（完整参考已独立成文档）】\r\n"
        L"  脚本语言 SuperScript 与 ActionScript 3.0 语法一致，用英文标识符控制场景元件。\r\n"
        L"  完整语言参考（全局函数 / 元件属性 / 事件 / 键盘 / 数学 / 示例代码）已按\r\n"
        L"  AS3.0 语言参考格式独立成 HTML 帮助文档：\r\n"
        L"    双击打开工程目录下的 SuperScript参考.html 即可浏览。\r\n"
        L"  快速上手：\r\n"
        L"    变量：var speed:Number = 5;          (类型可写可省略)\r\n"
        L"    控制：ball.x += speed;              (用元件英文名控制场景元件)\r\n"
        L"    属性：x y z(位置) rx ry rz(旋转度) sx sy sz(缩放)\r\n"
        L"          visible(可见) red green blue(颜色 0~255) segments(细分)\r\n"
        L"    每帧执行：function onEnterFrame(e:Event):void { ball.ry += 2; }\r\n"
        L"      或 addEventListener(Event.ENTER_FRAME, myFunc);\r\n"
        L"    键盘事件：stage.addEventListener(KeyboardEvent.KEY_DOWN, onKeyDown);\r\n"
        L"      回调参数 e=键码（A=65 D=68 W=87 S=83 ←=37 ↑=38 →=39 ↓=40 空格=32）\r\n"
        L"    查询按键：Key.isDown(Key.LEFT) 返回 true/false（播放时按住才生效）\r\n"
        L"    数学：Math.sin(弧度) cos tan sqrt abs floor ceil round min max pow random\r\n"
        L"        atan2(y,x) Math.PI；内置 sin()/cos() 按角度，Math.sin 按弧度\r\n"
        L"    输出：trace(\"score=\" + score)      (结果显示在下面输出框)\r\n"
        L"    流程：if/else for while break continue return；== === != < <= > >=\r\n"
        L"    注释：// 单行 与 /* 多行 */\r\n"
        L"  内置示例（House/Windmill/SolarSystem/BounceBall/DocClass/TankBattle）的元件名、函数名、变量名\r\n"
        L"  已全部英文化，可直接作为英文脚本的学习模板。\r\n"
        L"\r\n"
        L"【发布预览（调试）】\r\n"
        L"  点【调试】→ 把当前作品一键打包成独立 exe 并自动运行\r\n"
        L"  （效果和发布给别人的作品完全一样，无需保存 .s3d）\r\n"
        L"  .s3d 工程直接拖到播放器 Super3DPlayer.exe 上也能播放\r\n"
        L"\r\n"
        L"【导出给 Three.js（像 Blender 一样）】\r\n"
        L"  点【导出】→ 选保存位置，生成一个 HTML 网页\r\n"
        L"  浏览器打开就能查看、旋转、播放模型（动画也一起导出来）\r\n"
        L"  全部元件类型都能导出：编辑过的网格、布尔结果、圆管/方管、曲线、\r\n"
        L"  绿叶、3D文本会自动烘焙成静态模型，形状与编辑器完全一致\r\n"
        L"  网页已内嵌 Three.js 库，完全离线也能打开\r\n"
        L"  （库文件 three.min.js / OrbitControls.js 需随工程放在一起）";
    // 已打开过帮助窗口则置前（按标题查找，避免悬挂指针）
    CWnd* old = CWnd::FindWindow(NULL, L"Super3D 使用说明");
    if (old) {
        if (old->IsIconic()) old->ShowWindow(SW_RESTORE);
        old->SetForegroundWindow();
        return;
    }
    CHelpFrame* f = new CHelpFrame();   // CFrameWnd 关闭时自动 delete this
    if (!f->CreateHelp((HFONT)GetSmallFont().GetSafeHandle(), msg, this)) {
        delete f;
        return;
    }
    f->ShowWindow(SW_SHOW);
    f->UpdateWindow();
}

void CMainWnd::OnSuperScriptDoc() {
    // 从 exe 所在目录逐级向上找 SuperScript参考.html（exe 可能在 Win32\Release 下）
    wchar_t exe[MAX_PATH];
    if (!::GetModuleFileNameW(NULL, exe, MAX_PATH)) {
        AfxMessageBox(_T("无法定位程序目录。"), MB_ICONWARNING);
        return;
    }
    std::wstring dir = exe;
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir.erase(slash);
    for (int up = 0; up < 6; up++) {
        std::wstring html = dir + L"\\SuperScript参考.html";
        DWORD attr = ::GetFileAttributesW(html.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            HINSTANCE r = ::ShellExecuteW(NULL, L"open", html.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)r <= 32) AfxMessageBox(_T("无法打开 SuperScript 文档。"), MB_ICONWARNING);
            return;
        }
        size_t p = dir.find_last_of(L"\\/");
        if (p == std::wstring::npos) break;
        dir.erase(p);
    }
    AfxMessageBox(_T("未找到 SuperScript参考.html（请与程序放在一起）。"), MB_ICONWARNING);
}

// ------------------------------------------------------------ 一键调试
// 打包数据标记：追加在 exe 末尾，格式 = 标记 + 8位十六进制长度 + UTF-8 文本
// （与播放器的打包格式完全一致，调试出的 exe 可直接独立运行）
static const char kDbgPackMark[] = "SUPER3DPACK1\r\n";

static std::string DbgWToUtf8(const std::wstring& s) {
    if (s.empty()) return std::string();
    int cch = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    std::string out(cch, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], cch, NULL, NULL);
    return out;
}

// 读取文件全部字节；失败返回空
static std::vector<BYTE> DbgReadFile(const std::wstring& path) {
    std::vector<BYTE> buf;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return buf;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0) {
        buf.resize(n);
        if (fread(&buf[0], 1, n, f) != (size_t)n) buf.clear();
    }
    fclose(f);
    return buf;
}

// 严格检查 exe 尾部是否真的是打包数据（标记 + 8位hex + 长度精确吻合），
// 是则剥离并返回 true；否则原样保留（防止误伤 exe 内部代码区的同名字符串）
static bool DbgStripPack(std::vector<BYTE>& buf) {
    const size_t markLen = strlen(kDbgPackMark);
    const size_t headLen = markLen + 8;                       // 标记 + 8位hex
    if (buf.size() <= headLen) return false;
    size_t pos = buf.size() - headLen;
    if (memcmp(&buf[pos], kDbgPackMark, markLen) != 0) return false;
    for (size_t i = 0; i < 8; i++) {
        char c = (char)buf[pos + markLen + i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
        if (!hex) return false;
    }
    char hexBuf[9] = { 0 };
    memcpy(hexBuf, &buf[pos + markLen], 8);
    unsigned long dataLen = strtoul(hexBuf, NULL, 16);
    if (pos + headLen + dataLen != buf.size()) return false;  // 长度必须精确吻合
    buf.resize(pos);
    return true;
}

void CMainWnd::OnDebug() {
    Stop();
    // 每次调试打包：输出页与编译器错误页清空重置
    m_traceLines.clear();
    m_traceEdt.SetWindowText(_T(""));
    m_traceStat.SetWindowText(_T(""));
    m_errEdt.SetWindowText(_T(""));
    m_errStat.SetWindowText(_T(""));
    // 1. 找播放器模板：<root>\Player\<Win32|x64>\Release\Super3DPlayer.exe
    //    编辑器 exe 形如 <root>\<Win32|x64>\<Debug|Release>\Super3D.exe，
    //    逐级上溯两级目录得到 <Win32|x64>（平台）与 <root>（工程根）
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring d(exePath);
    size_t p = d.find_last_of(L"\\/");
    if (p != std::wstring::npos) d = d.substr(0, p);   // ...\<Win32|x64>\<Debug|Release>
    p = d.find_last_of(L"\\/");
    if (p == std::wstring::npos) { AfxMessageBox(_T("无法定位播放器模板。"), MB_ICONWARNING); return; }
    d = d.substr(0, p);                                // ...\<Win32|x64>
    p = d.find_last_of(L"\\/");
    if (p == std::wstring::npos) { AfxMessageBox(_T("无法定位播放器模板。"), MB_ICONWARNING); return; }
    std::wstring platform = d.substr(p + 1);           // Win32 或 x64
    std::wstring root = d.substr(0, p);                // 工程根目录
    std::wstring tpl = root + L"\\Player\\" + platform + L"\\Release\\Super3DPlayer.exe";
    std::vector<BYTE> tmpl = DbgReadFile(tpl);
    if (tmpl.empty()) {
        CString msg;
        msg.Format(_T("找不到播放器程序 Super3DPlayer.exe。\r\n")
                   _T("期望路径：%s\r\n")
                   _T("请先在 VS 里编译 Player 工程（Release），再点【调试】。"),
                   tpl.c_str());
        AfxMessageBox(msg, MB_ICONWARNING);
        return;
    }
    DbgStripPack(tmpl);
    if (tmpl.empty()) { AfxMessageBox(_T("读取播放器失败。"), MB_ICONERROR); return; }
    // 2. 序列化当前场景（不用保存 .s3d，直接打包内存中的内容）
    std::wstring text = ProjectIO::SerializeToString(m_scene);
    // 调试打包传递文档类（外部 .ss 内容，不写回 s3d）：Player 反序列化后按段恢复
    if (!m_scene.docClass.empty()) {
        wchar_t db[64];
        swprintf(db, 64, L"doc_class_len=%d\r\n", (int)m_scene.docClass.size());
        text += db;
        text += m_scene.docClass;
        text += L"\r\n";
    }
    std::string utf8 = DbgWToUtf8(text);
    // 3. 写入调试 exe（放在编辑器 exe 所在目录，文件名带时间戳，避免占用冲突）
    wchar_t outDir[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, outDir, MAX_PATH);
    size_t q = std::wstring(outDir).find_last_of(L"\\/");
    if (q != std::wstring::npos) outDir[q] = 0;            // ...\<Win32|x64>\<Debug|Release>
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t name[64];
    swprintf(name, 64, L"Super3D调试_%04d%02d%02d_%02d%02d%02d.exe",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::wstring out = std::wstring(outDir) + L"\\" + name;
    // 清理同目录下之前调试留下的 exe（正在运行的会被占用，自动跳过）
    WIN32_FIND_DATAW fd;
    std::wstring pattern = std::wstring(outDir) + L"\\Super3D调试_*.exe";
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring fp = std::wstring(outDir) + L"\\" + fd.cFileName;
            if (fp != out) ::DeleteFileW(fp.c_str());
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    FILE* f = _wfopen(out.c_str(), L"wb");
    if (!f) { AfxMessageBox(_T("无法写入临时文件。"), MB_ICONERROR); return; }
    fwrite(&tmpl[0], 1, tmpl.size(), f);
    fwrite(kDbgPackMark, 1, strlen(kDbgPackMark), f);
    char lenHex[16];
    sprintf(lenHex, "%08lX", (unsigned long)utf8.size());
    fwrite(lenHex, 1, 8, f);
    fwrite(utf8.data(), 1, utf8.size(), f);
    fclose(f);
    // 4. 运行它（完全脱离编辑器，自动播放动画 + 逐帧脚本）
    HINSTANCE hr = ShellExecuteW(NULL, L"open", out.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)hr <= 32) {
        AfxMessageBox(_T("启动调试程序失败。"), MB_ICONWARNING);
        return;
    }
    CString s;
    s.Format(_T("调试程序已生成并运行：%s"), out.c_str());
    m_status.SetWindowText(s);
}

// ------------------------------------------------------------ 导出 Three.js
// 把场景（元件层级 + 几何参数 + 颜色 + 关键帧动画）导出成一个 HTML 网页，
// 内嵌 Three.js，浏览器打开即可查看与操作模型，与 Blender 导出给 three.js 类似。
static const wchar_t* kExportTypeName(int t) {
    switch (t) {
        case PT_BOX:     return L"box";
        case PT_SPHERE:  return L"sphere";
        case PT_CYLINDER:return L"cylinder";
        case PT_CONE:    return L"cone";
        case PT_TORUS:   return L"torus";
        case PT_PLANE:   return L"plane";
        case PT_TUBE:    return L"tube";
        case PT_BOOLEAN: return L"mesh";
        default:         return L"group";
    }
}

static void ExportNum(std::wstring& s, double v) {
    wchar_t buf[64];
    swprintf(buf, 64, L"%g", v);
    s += buf;
}

static void ExportJsonStr(std::wstring& s, const std::wstring& in) {
    s += L'"';
    for (size_t i = 0; i < in.size(); i++) {
        wchar_t c = in[i];
        if (c == L'"') s += L"\\\"";
        else if (c == L'\\') s += L"\\\\";
        else if (c == L'\n') s += L"\\n";
        else if (c == L'\r') s += L"\\r";
        else if (c == L'\t') s += L"\\t";
        else s += c;
    }
    s += L'"';
}

// 递归导出元件 JSON
static void ExportObjJson(const Object3D* o, std::wstring& s,
                          const std::map<const Object3D*, std::vector<KeyFrame>>* anims = NULL) {
    s += L"{";
    s += L"\"name\": "; ExportJsonStr(s, o->name);
    // 有编辑网格（Blender 式点/线/面编辑结果）时 three.js 无对应表示：烘焙为静态 mesh 导出。
    // 另外方管/圆管/布尔结果/曲线/绿叶/3D文本在 three.js 没有参数化对应
    // （布尔结果不烘焙会被网页回退成单位立方体），也统一烘焙为静态 mesh，保证导出网页与编辑器显示一致。
    bool bakeMesh = o->HasEditMesh() || o->type == PT_TUBE || o->type == PT_RTUBE ||
                    o->type == PT_BOOLEAN || o->type == PT_SPLINE || o->type == PT_LEAF || o->type == PT_TEXT3D;
    s += L", \"type\": \"";
    if (bakeMesh) s += L"mesh";
    else s += kExportTypeName(o->type);
    s += L"\"";
    if (bakeMesh) {
        std::vector<Vec3> bv, bn;
        o->GenerateMesh(bv, bn);           // 编辑网格的最终网格
        s += L", \"verts\": [";
        for (size_t k = 0; k < bv.size(); k++) {
            if (k) s += L",";
            ExportNum(s, bv[k].x); s += L",";
            ExportNum(s, bv[k].y); s += L",";
            ExportNum(s, bv[k].z);
        }
        s += L"], \"norms\": [";
        for (size_t k = 0; k < bn.size(); k++) {
            if (k) s += L",";
            ExportNum(s, bn[k].x); s += L",";
            ExportNum(s, bn[k].y); s += L",";
            ExportNum(s, bn[k].z);
        }
        s += L"]";
        // 面/线自定义色（网格编辑态）随网格导出：tc=逐三角 24 位色（0=用物体色），
        // elp/elc=彩色边的顶点坐标与顶点色，three.js 端用顶点色与 LineSegments 复现，
        // 保证导出网页里能看到编辑器中单独设置的面/线颜色
        if (o->HasEditMesh()) {
            int ntExp = (int)(o->editTris.size() / 3);
            bool anyTri = false;
            for (size_t t = 0; t < o->editTriColor.size(); t++) {
                if (o->editTriColor[t]) { anyTri = true; break; }
            }
            if (anyTri) {
                s += L", \"tc\": [";
                for (int t = 0; t < ntExp; t++) {
                    if (t) s += L",";
                    unsigned int c = (t < (int)o->editTriColor.size()) ? o->editTriColor[t] : 0u;
                    wchar_t cb[32];
                    swprintf(cb, 32, L"%u", (unsigned int)(c & 0xFFFFFFu));
                    s += cb;
                }
                s += L"]";
            }
            if (!o->editEdgeColorKey.empty()) {
                s += L", \"elp\": [";
                bool first = true;
                for (size_t i = 0; i < o->editEdgeColorKey.size(); i++) {
                    int ea = o->editEdgeColorKey[i].first, eb2 = o->editEdgeColorKey[i].second;
                    if (ea < 0 || eb2 < 0 || ea >= (int)o->editVerts.size() || eb2 >= (int)o->editVerts.size()) continue;
                    if (!first) s += L",";
                    first = false;
                    ExportNum(s, o->editVerts[ea].x); s += L",";
                    ExportNum(s, o->editVerts[ea].y); s += L",";
                    ExportNum(s, o->editVerts[ea].z); s += L",";
                    ExportNum(s, o->editVerts[eb2].x); s += L",";
                    ExportNum(s, o->editVerts[eb2].y); s += L",";
                    ExportNum(s, o->editVerts[eb2].z);
                }
                s += L"], \"elc\": [";
                first = true;
                for (size_t i = 0; i < o->editEdgeColorKey.size(); i++) {
                    int ea = o->editEdgeColorKey[i].first, eb2 = o->editEdgeColorKey[i].second;
                    if (ea < 0 || eb2 < 0 || ea >= (int)o->editVerts.size() || eb2 >= (int)o->editVerts.size()) continue;
                    double cr, cg, cb3;
                    Object3D::UnpackEditColor(o->editEdgeColorVal[i], cr, cg, cb3);
                    for (int k2 = 0; k2 < 2; k2++) {
                        if (!first) s += L",";
                        first = false;
                        ExportNum(s, cr); s += L",";
                        ExportNum(s, cg); s += L",";
                        ExportNum(s, cb3);
                    }
                }
                s += L"]";
            }
        }
    } else if (Object3D::IsPrimitive(o->type)) {
        s += L", \"p\": [";
        ExportNum(s, o->params[0]); s += L",";
        ExportNum(s, o->params[1]); s += L",";
        ExportNum(s, o->params[2]); s += L",";
        ExportNum(s, o->params[3]);
        s += L"], \"seg\": "; ExportNum(s, o->segments);
        if (o->type == PT_BOOLEAN && o->_meshValid && !o->_meshVerts.empty()) {
            s += L", \"verts\": [";
            for (size_t k = 0; k < o->_meshVerts.size(); k++) {
                if (k) s += L",";
                ExportNum(s, o->_meshVerts[k].x); s += L",";
                ExportNum(s, o->_meshVerts[k].y); s += L",";
                ExportNum(s, o->_meshVerts[k].z);
            }
            s += L"], \"norms\": [";
            for (size_t k = 0; k < o->_meshNorms.size(); k++) {
                if (k) s += L",";
                ExportNum(s, o->_meshNorms[k].x); s += L",";
                ExportNum(s, o->_meshNorms[k].y); s += L",";
                ExportNum(s, o->_meshNorms[k].z);
            }
            s += L"]";
        }
    }
    s += L", \"pos\": [";
    ExportNum(s, o->x); s += L","; ExportNum(s, o->y); s += L","; ExportNum(s, o->z); s += L"]";
    s += L", \"rot\": [";
    ExportNum(s, o->rx); s += L","; ExportNum(s, o->ry); s += L","; ExportNum(s, o->rz); s += L"]";
    s += L", \"scl\": [";
    ExportNum(s, o->sx); s += L","; ExportNum(s, o->sy); s += L","; ExportNum(s, o->sz); s += L"]";
    s += L", \"color\": [";
    ExportNum(s, o->r); s += L","; ExportNum(s, o->g); s += L","; ExportNum(s, o->b);
    s += L"], \"vis\": "; s += (o->visible ? L"1" : L"0");
    // 动画关键帧：优先用图层关键帧折算出的每对象时间线（整层快照语义），
    // 否则回退到旧的元件级 keys（现已不再写入，保留兼容）
    const std::vector<KeyFrame>* kp = NULL;
    if (anims) {
        std::map<const Object3D*, std::vector<KeyFrame>>::const_iterator it = anims->find(o);
        if (it != anims->end()) kp = &it->second;
    }
    if (!kp || kp->empty()) kp = o->keys.empty() ? NULL : &o->keys;
    if (kp) {
        s += L", \"keys\": [";
        for (size_t i = 0; i < kp->size(); i++) {
            const KeyFrame& k = (*kp)[i];
            if (i) s += L",";
            s += L"{\"f\": "; ExportNum(s, k.frame);
            s += L", \"pos\": [";
            ExportNum(s, k.px); s += L","; ExportNum(s, k.py); s += L","; ExportNum(s, k.pz); s += L"]";
            s += L", \"rot\": [";
            ExportNum(s, k.rx); s += L","; ExportNum(s, k.ry); s += L","; ExportNum(s, k.rz); s += L"]";
            s += L", \"scl\": [";
            ExportNum(s, k.sx); s += L","; ExportNum(s, k.sy); s += L","; ExportNum(s, k.sz); s += L"]}";
        }
        s += L"]";
    }
    if (!o->children.empty()) {
        s += L", \"children\": [";
        for (size_t i = 0; i < o->children.size(); i++) {
            if (i) s += L",";
            ExportObjJson(o->children[i], s, anims);
        }
        s += L"]";
    }
    s += L"}";
}

// 以 UTF-8 读取文本文件并转成宽字符串
static bool LoadUtf8FileAsW(const std::wstring& path, std::wstring& out) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return false; }
    std::string bytes((size_t)sz, '\0');
    fread(&bytes[0], 1, (size_t)sz, f);
    fclose(f);
    int n = ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), NULL, 0);
    if (n <= 0) return false;
    out.resize(n);
    ::MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), &out[0], n);
    return true;
}

// 导出库文件所在目录：先取编辑器 exe 上溯两级（Win32\Debug → 工程根）
static std::wstring ExportLibRootDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = ::GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (n == 0) return L"";
    std::wstring dir(buf, n);
    size_t p = dir.find_last_of(L"\\/");
    if (p != std::wstring::npos) dir = dir.substr(0, p);
    for (int i = 0; i < 2; i++) {              // 上溯两级到工程根
        size_t q = dir.find_last_of(L"\\/");
        if (q == std::wstring::npos || q == 0) break;
        dir = dir.substr(0, q);
    }
    return dir;
}

// 加载内嵌 Three.js 库（three.min.js / OrbitControls.js），首次成功后缓存
static bool LoadExportLibs(std::wstring& threeLib, std::wstring& orbitLib, std::wstring& err) {
    static std::wstring s_three, s_orbit;
    static bool s_done = false;
    if (s_done) { threeLib = s_three; orbitLib = s_orbit; return !s_three.empty(); }
    s_done = true;

    std::wstring cand[2];
    cand[0] = ExportLibRootDir();              // 工程根
    wchar_t buf[MAX_PATH];
    DWORD n = ::GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (n > 0) {
        std::wstring d(buf, n);
        size_t p = d.find_last_of(L"\\/");
        if (p != std::wstring::npos) cand[1] = d.substr(0, p);   // exe 同目录
    }
    for (int i = 0; i < 2; i++) {
        if (cand[i].empty() || (i > 0 && cand[i] == cand[0])) continue;
        if (LoadUtf8FileAsW(cand[i] + L"\\three.min.js", s_three) &&
            LoadUtf8FileAsW(cand[i] + L"\\OrbitControls.js", s_orbit)) {
            threeLib = s_three; orbitLib = s_orbit;
            return true;
        }
        s_three.clear(); s_orbit.clear();
    }
    err = L"缺少导出库文件：three.min.js 与 OrbitControls.js\r\n\r\n请把这两个文件放到：\r\n" +
          cand[0] + L"\r\n\r\n（可向老师/开发者索取，或从 Three.js 官网下载 0.128.0 版）";
    return false;
}

// 防止内嵌库代码中出现 "</script" 提前闭合标签（JS 字符串中 \/ 与 / 等价）
static void EscapeScriptClose(std::wstring& s) {
    size_t p = 0;
    while ((p = s.find(L"</script", p)) != std::wstring::npos) {
        s.replace(p, 9, L"<\\/script");
        p += 9;
    }
}

// 生成完整 HTML（头部模板含占位符 @THREE_LIB@ / @ORBIT_LIB@ / @DATA@）
static const wchar_t* kExportHtmlHead =
LR"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Super3D 场景 - Three.js</title>
<style>
html,body{margin:0;height:100%;overflow:hidden;background:#101018;font-family:Microsoft YaHei,Arial,sans-serif}
#bar{position:absolute;top:12px;left:12px;z-index:10;color:#eee;background:rgba(10,14,26,.72);
padding:8px 14px;border-radius:8px;font-size:14px;user-select:none}
#bar button{margin-left:10px;padding:4px 14px;cursor:pointer;border:0;border-radius:6px;
background:#3a7bd5;color:#fff;font-size:14px}
#bar button:hover{background:#4f8fe6}
#info{margin-left:12px;color:#9fd0ff}
#tip{position:absolute;bottom:14px;left:50%;transform:translateX(-50%);z-index:10;color:#aab;
font-size:13px;background:rgba(0,0,0,.4);padding:6px 14px;border-radius:6px}
#hud{position:absolute;bottom:48px;left:50%;transform:translateX(-50%);z-index:10;color:#fff;
font-size:16px;background:rgba(0,0,0,.55);padding:8px 18px;border-radius:8px;display:none;white-space:nowrap}
#log{position:absolute;left:12px;bottom:12px;z-index:10;max-width:480px;max-height:200px;overflow:auto;
color:#eee;background:rgba(0,0,0,.6);padding:8px 12px;border-radius:8px;font-size:13px;display:none;white-space:pre-wrap}
#logbtn{display:none}
#rst{display:none}
</style>
<script>@THREE_LIB@</script>
<script>@ORBIT_LIB@</script>
</head><body>
<div id="bar">Super3D 场景<button onclick="toggle()">播放/暂停</button><button id="logbtn" onclick="toggleLog()">日志</button><button id="rst" onclick="location.reload()">重新开始</button><span id="info"></span></div>
<div id="hud"></div>
<div id="log"></div>
<div id="tip">鼠标左键拖动旋转 · 滚轮缩放 · 右键平移</div>
<script>
var DATA = @DATA@;
)HTML";

static const wchar_t* kExportHtmlTail =
LR"HTML(
var scene = new THREE.Scene();
scene.background = new THREE.Color(0x101018);
var camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 2000);
var renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setPixelRatio(window.devicePixelRatio);
document.body.appendChild(renderer.domElement);
scene.add(new THREE.AmbientLight(0xffffff, 0.55));
var sun = new THREE.DirectionalLight(0xffffff, 0.85);
sun.position.set(40, 60, 30);
scene.add(sun);

var grid = new THREE.GridHelper(60, 30, 0x5577aa, 0x335577);
grid.position.y = -0.02;
scene.add(grid);

var anims = [], byName = {};
function build(o, parent) {
    var obj;
    if (o.type === 'group') {
        obj = new THREE.Group();
    } else {
        var seg = Math.min(64, Math.max(4, o.seg));   // 与编辑器细分一致（4~64）
        var geo;
        switch (o.type) {
            case 'box':      geo = new THREE.BoxGeometry(o.p[0], o.p[1], o.p[2]); break;
            case 'sphere':   geo = new THREE.SphereGeometry(o.p[0], seg, Math.max(3, Math.round(seg / 2))); break;
            case 'cylinder': geo = new THREE.CylinderGeometry(o.p[0], o.p[0], o.p[1], seg); break;
            case 'cone':     geo = new THREE.ConeGeometry(o.p[0], o.p[1], seg); break;
            case 'torus':    geo = new THREE.TorusGeometry(o.p[0], o.p[1], Math.max(4, Math.round(seg / 2)), seg); geo.rotateX(-Math.PI / 2); break;
            case 'plane':    geo = new THREE.PlaneGeometry(o.p[0], o.p[0]); geo.rotateX(-Math.PI / 2); break;
            case 'mesh':
                if (o.verts && o.verts.length) {
                    geo = new THREE.BufferGeometry();
                    geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(o.verts), 3));
                    geo.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(o.norms), 3));
                    if (o.tc && o.tc.length) {   // 网格编辑逐面自定义色 → 顶点色（0=物体色）
                        var ntri = Math.min(o.tc.length, Math.floor(o.verts.length / 9));
                        var cols = new Float32Array(o.verts.length);
                        for (var t = 0; t < ntri; t++) {
                            var cc = o.tc[t], cr, cg, cb;
                            if (cc === 0) { cr = o.color[0]; cg = o.color[1]; cb = o.color[2]; }
                            else { cr = ((cc >> 16) & 255) / 255; cg = ((cc >> 8) & 255) / 255; cb = (cc & 255) / 255; }
                            for (var v3 = t * 9; v3 < t * 9 + 9; v3 += 3) { cols[v3] = cr; cols[v3 + 1] = cg; cols[v3 + 2] = cb; }
                        }
                        geo.setAttribute('color', new THREE.BufferAttribute(cols, 3));
                    }
                } else { geo = new THREE.BoxGeometry(1, 1, 1); }
                break;
            default:         geo = new THREE.BoxGeometry(1, 1, 1);
        }
        var mat = new THREE.MeshPhongMaterial({
            color: new THREE.Color(o.color[0], o.color[1], o.color[2]),
            side: THREE.DoubleSide
        });
        if (geo.getAttribute && geo.getAttribute('color')) mat.vertexColors = true;
        obj = new THREE.Mesh(geo, mat);
        if (o.elp && o.elp.length) {   // 网格编辑逐线自定义色 → LineSegments（顶点色）
            var eg = new THREE.BufferGeometry();
            eg.setAttribute('position', new THREE.BufferAttribute(new Float32Array(o.elp), 3));
            eg.setAttribute('color', new THREE.BufferAttribute(new Float32Array(o.elc), 3));
            obj.add(new THREE.LineSegments(eg, new THREE.LineBasicMaterial({ vertexColors: true })));
        }
    }
    obj.position.set(o.pos[0], o.pos[1], o.pos[2]);
    obj.rotation.set(o.rot[0] * Math.PI / 180, o.rot[1] * Math.PI / 180, o.rot[2] * Math.PI / 180);
    obj.scale.set(o.scl[0], o.scl[1], o.scl[2]);
    obj.visible = (o.vis === 1);
    obj.userData = { name: o.name || '', type: o.type || '' };
    if (o.name) byName[o.name] = obj;
    if (o.keys && o.keys.length) anims.push({ obj: obj, keys: o.keys });
    parent.add(obj);
    if (o.children) o.children.forEach(function (c) { build(c, obj); });
}
DATA.roots.forEach(function (r) { build(r, scene); });

// 脚本引擎增强版见 kExportScript1/2/3（支持变量/if/for/函数/Key.isDown/onEnterFrame/HUD）
)HTML"
// 注：含中文的宽字符 raw string 在 MSVC 下实际约 8KB 就会触发 C2026（内部展开后撞 65535 上限），
// 故拆成两段相邻字面量拼接（同 kExportHtmlHead/kExportHtmlTail 的拆分原因）。
;

static const wchar_t* kExportScript1a =
LR"HTML(
// ================= 增强脚本引擎：第 1 段（词法与解析器）=================
// 语义与桌面 Script.cpp 一致：var/if/for/while/函数/数组/Key.isDown/onEnterFrame。
var KDEG = Math.PI / 180;
var __keys = {}, __globals = {}, __fns = {}, __frameFns = [], __curClass = null;
var __hasFrameHandler = false, __topDone = false, __prog = null;
var __breakF = false, __contF = false, __retF = false, __retV = null;
var __scope = [], __logTxt = '';
function __keyConst(n) {
    if (n === 'Key.LEFT') return 37; if (n === 'Key.UP') return 38;
    if (n === 'Key.RIGHT') return 39; if (n === 'Key.DOWN') return 40;
    if (n === 'Key.SPACE') return 32; if (n === 'Key.A') return 65;
    if (n === 'Key.D') return 68; if (n === 'Key.W') return 87; if (n === 'Key.S') return 83;
    if (n === 'Key.ENTER') return 13; if (n === 'Key.ESC') return 27;
    if (n === 'Key.TAB') return 9; if (n === 'Key.SHIFT') return 16;
    if (n === 'Key.CTRL') return 17; if (n === 'Key.ALT') return 18;
    if (n === 'Key.B') return 66; if (n === 'Key.C') return 67; if (n === 'Key.E') return 69;
    if (n === 'Key.F') return 70; if (n === 'Key.G') return 71; if (n === 'Key.H') return 72;
    if (n === 'Key.I') return 73; if (n === 'Key.J') return 74; if (n === 'Key.K') return 75;
    if (n === 'Key.L') return 76; if (n === 'Key.M') return 77; if (n === 'Key.N') return 78;
    if (n === 'Key.O') return 79; if (n === 'Key.P') return 80; if (n === 'Key.Q') return 81;
    if (n === 'Key.R') return 82; if (n === 'Key.T') return 84; if (n === 'Key.U') return 85;
    if (n === 'Key.V') return 86; if (n === 'Key.X') return 88; if (n === 'Key.Y') return 89;
    if (n === 'Key.Z') return 90;
    if (n === 'Key.NUM0') return 48; if (n === 'Key.NUM1') return 49; if (n === 'Key.NUM2') return 50;
    if (n === 'Key.NUM3') return 51; if (n === 'Key.NUM4') return 52; if (n === 'Key.NUM5') return 53;
    if (n === 'Key.NUM6') return 54; if (n === 'Key.NUM7') return 55; if (n === 'Key.NUM8') return 56;
    if (n === 'Key.NUM9') return 57;
    if (n === 'Key.F1') return 112; if (n === 'Key.F2') return 113; if (n === 'Key.F3') return 114;
    if (n === 'Key.F4') return 115; if (n === 'Key.F5') return 116; if (n === 'Key.F6') return 117;
    if (n === 'Key.F7') return 118; if (n === 'Key.F8') return 119; if (n === 'Key.F9') return 120;
    if (n === 'Key.F10') return 121; if (n === 'Key.F11') return 122; if (n === 'Key.F12') return 123;
    return null;
}
var __tok = [], __ti = 0;
function __lex(src) {
    __tok = [];
    var i = 0, n = src.length;
    var kw = { 'var': 1, 'if': 1, 'else': 1, 'for': 1, 'while': 1, 'break': 1, 'continue': 1,
        'function': 1, 'return': 1, 'new': 1, 'null': 1, 'true': 1, 'false': 1,
        'do': 1, 'switch': 1, 'case': 1, 'default': 1, 'typeof': 1, 'this': 1,
        'try': 1, 'catch': 1, 'finally': 1, 'throw': 1, 'in': 1, 'delete': 1,
        'is': 1, 'as': 1, 'instanceof': 1, 'arguments': 1,
        'package': 1, 'class': 1, 'extends': 1, 'implements': 1,
        'public': 1, 'private': 1, 'protected': 1, 'static': 1, 'internal': 1,
        'final': 1, 'override': 1, 'dynamic': 1, 'const': 1, 'interface': 1,
        'namespace': 1, 'super': 1, 'import': 1,
        '定义': 1, '如果': 1, '否则': 1, '循环': 1, '当': 1, '跳出': 1, '继续': 1, '真': 1, '假': 1, '函数': 1,
        '做': 1, '开关': 1, '情况': 1, '默认': 1, '类型': 1, '这': 1,
        '尝试': 1, '捕捉': 1, '最后': 1, '抛出': 1, '在': 1, '删除': 1,
        '是': 1, '作为': 1, '实例': 1, '参数': 1 };
    while (i < n) {
        var c = src.charAt(i);
        if (c === ' ' || c === '\t' || c === '\r' || c === '\n') { i++; continue; }
        if (c === '/' && src.charAt(i + 1) === '/') { while (i < n && src.charAt(i) !== '\n') i++; continue; }
        if (c === '/' && src.charAt(i + 1) === '*') { i += 2; while (i < n && !(src.charAt(i) === '*' && src.charAt(i + 1) === '/')) i++; i += 2; continue; }
        if ((c >= '0' && c <= '9') || (c === '.' && src.charAt(i + 1) >= '0' && src.charAt(i + 1) <= '9')) {
            var j = i;
            while (j < n && ((src.charAt(j) >= '0' && src.charAt(j) <= '9') || src.charAt(j) === '.')) j++;
            __tok.push({ t: 'num', v: parseFloat(src.slice(i, j)) });
            i = j; continue;
        }
        if (c === '"' || c === "'") {
            var q = c, j2 = i + 1, s2 = '';
            while (j2 < n && src.charAt(j2) !== q) {
                if (src.charAt(j2) === '\\' && j2 + 1 < n) { s2 += src.charAt(j2 + 1); j2 += 2; }
                else { s2 += src.charAt(j2); j2++; }
            }
            __tok.push({ t: 'str', v: s2 });
            i = j2 + 1; continue;
        }
        if (/[\u4e00-\u9fffA-Za-z_$]/.test(c)) {
            var j3 = i;
            while (j3 < n && /[\u4e00-\u9fffA-Za-z0-9_$]/.test(src.charAt(j3))) j3++;
            var w = src.slice(i, j3);
            __tok.push(kw[w] ? { t: 'kw', v: w } : { t: 'id', v: w });
            i = j3; continue;
        }
        var two = src.slice(i, i + 2);
        var three = src.slice(i, i + 3);
        if (three === '...') {
            __tok.push({ t: 'op', v: '...' });
            i += 3; continue;
        }
        if (three === '===' || three === '!==') {
            __tok.push({ t: 'op', v: three });
            i += 3; continue;
        }
        if (two === '++' || two === '--' || two === '+=' || two === '-=' || two === '*=' || two === '/=' ||
            two === '==' || two === '!=' || two === '<=' || two === '>=' || two === '&&' || two === '||') {
            __tok.push({ t: 'op', v: two });
            i += 2; continue;
        }
        __tok.push({ t: 'op', v: c });
        i++;
    }
    __tok.push({ t: 'eof', v: '' });
    __ti = 0;
}
function __expectOp(v) {
    var tt = __tok[__ti];
    if (!(tt.v === v && (tt.t === 'op' || tt.t === 'kw'))) throw new Error('missing: ' + v);
    __ti++;
}

)HTML";

static const wchar_t* kExportScript1a2 =
LR"HTML(
function __parseExpr() { return __parseAssign(); }
function __parseAssign() {
    var l = __parseTernary();
    if (__tok[__ti].t === 'op') {
        var op = __tok[__ti].v;
        if (op === '=' || op === '+=' || op === '-=' || op === '*=' || op === '/=') {
            __ti++;
            var r = __parseAssign();
            return { k: 'assign', a: l, op: op, v: r };
        }
    }
    return l;
}
function __parseTernary() {
    var c = __parseOr();
    if (__tok[__ti].t === 'op' && __tok[__ti].v === '?') {
        __ti++;
        var a = __parseOr();
        __expectOp(':');
        var b = __parseTernary();
        return { k: 'ternary', c: c, a: a, b: b };
    }
    return c;
}
function __parseOr() {
    var l = __parseAnd();
    while (__tok[__ti].t === 'op' && __tok[__ti].v === '||') { __ti++; var r = __parseAnd(); l = { k: 'op2', op: '||', a: l, b: r }; }
    return l;
}
function __parseAnd() {
    var l = __parseEq();
    while (__tok[__ti].t === 'op' && __tok[__ti].v === '&&') { __ti++; var r = __parseEq(); l = { k: 'op2', op: '&&', a: l, b: r }; }
    return l;
}
function __parseEq() {
    var l = __parseCmp();
    while ((__tok[__ti].t === 'op') && (__tok[__ti].v === '==' || __tok[__ti].v === '!=' || __tok[__ti].v === '===' || __tok[__ti].v === '!==')) { var op = __tok[__ti].v; __ti++; var r = __parseCmp(); l = { k: 'op2', op: op, a: l, b: r }; }
    return l;
}
function __parseCmp() {
    var l = __parseAdd();
    while (__tok[__ti].t === 'op' && (__tok[__ti].v === '<' || __tok[__ti].v === '<=' || __tok[__ti].v === '>' || __tok[__ti].v === '>=')) { var op = __tok[__ti].v; __ti++; var r = __parseAdd(); l = { k: 'op2', op: op, a: l, b: r }; }
    while (__tok[__ti].t === 'kw' && (__tok[__ti].v === 'is' || __tok[__ti].v === 'as' || __tok[__ti].v === 'instanceof' || __tok[__ti].v === '是' || __tok[__ti].v === '作为' || __tok[__ti].v === '实例')) { var op2 = __tok[__ti].v; __ti++; var r2 = __parseAdd(); l = { k: 'op2', op: op2, a: l, b: r2 }; }
    return l;
}
function __parseAdd() {
    var l = __parseMul();
    while (__tok[__ti].t === 'op' && (__tok[__ti].v === '+' || __tok[__ti].v === '-')) { var op = __tok[__ti].v; __ti++; var r = __parseMul(); l = { k: 'op2', op: op, a: l, b: r }; }
    return l;
}
function __parseMul() {
    var l = __parseUnary();
    while (__tok[__ti].t === 'op' && (__tok[__ti].v === '*' || __tok[__ti].v === '/' || __tok[__ti].v === '%')) { var op = __tok[__ti].v; __ti++; var r = __parseUnary(); l = { k: 'op2', op: op, a: l, b: r }; }
    return l;
}
function __parseUnary() {
    var t = __tok[__ti];
    if (t.t === 'op' && t.v === '-') { __ti++; return { k: 'neg', a: __parseUnary() }; }
    if (t.t === 'op' && t.v === '!') { __ti++; return { k: 'not', a: __parseUnary() }; }
    if (t.t === 'op' && t.v === '++') { __ti++; return { k: 'inc', a: __parseUnary(), pre: true, d: 1 }; }
    if (t.t === 'op' && t.v === '--') { __ti++; return { k: 'inc', a: __parseUnary(), pre: true, d: -1 }; }
    if (t.t === 'kw' && (t.v === 'typeof' || t.v === '类型')) { __ti++; return { k: 'typeof', a: __parseUnary() }; }
    if (t.t === 'kw' && (t.v === 'delete' || t.v === '删除')) { __ti++; return { k: 'delete', a: __parseUnary() }; }
    return __parsePostfix();
}
)HTML";

static const wchar_t* kExportScript1b =
LR"HTML(
function __parsePostfix() {
    var e = __parsePrimary();
    for (;;) {
        var t = __tok[__ti];
        if (t.t === 'op' && t.v === '.') { __ti++; var id = __tok[__ti]; if (id.t !== 'id' && id.t !== 'kw') throw new Error('bad prop'); __ti++; e = { k: 'prop', o: e, p: id.v }; }
        else if (t.t === 'op' && t.v === '[') { __ti++; var idx = __parseExpr(); __expectOp(']'); e = { k: 'index', o: e, i: idx }; }
        else if (t.t === 'op' && t.v === '(') {
            __ti++;
            var args = [];
            if (__tok[__ti].v !== ')') { args.push(__parseExpr()); while (__tok[__ti].v === ',') { __ti++; args.push(__parseExpr()); } }
            __expectOp(')');
            e = { k: 'call', f: e, args: args };
        }
        else if (t.t === 'op' && (t.v === '++' || t.v === '--')) { __ti++; e = { k: 'inc', a: e, pre: false, d: t.v === '++' ? 1 : -1 }; }
        else break;
    }
    return e;
}
function __parsePrimary() {
    var t = __tok[__ti];
    if (t.t === 'num') { __ti++; return { k: 'num', v: t.v }; }
    if (t.t === 'str') { __ti++; return { k: 'str', v: t.v }; }
    if (t.t === 'id') {
        __ti++;
        if (t.v === 'true' || t.v === '真') return { k: 'bool', v: true };
        if (t.v === 'false' || t.v === '假') return { k: 'bool', v: false };
        if (t.v === 'null') return { k: 'null' };
        return { k: 'id', n: t.v };
    }
    if (t.t === 'kw') {
        __ti++;
        if (t.v === 'true' || t.v === '真') return { k: 'bool', v: true };
        if (t.v === 'false' || t.v === '假') return { k: 'bool', v: false };
        if (t.v === 'null') return { k: 'null' };
        if (t.v === 'this' || t.v === '这') return { k: 'this' };
        throw new Error('bad kw: ' + t.v);
    }
    if (t.t === 'op' && t.v === '(') { __ti++; var e = __parseExpr(); __expectOp(')'); return e; }
    if (t.t === 'op' && t.v === '[') {
        __ti++;
        var items = [];
        if (__tok[__ti].v !== ']') { items.push(__parseExpr()); while (__tok[__ti].v === ',') { __ti++; items.push(__parseExpr()); } }
        __expectOp(']');
        return { k: 'array', items: items };
    }
    throw new Error('bad token: ' + t.v);
}

)HTML";

static const wchar_t* kExportScript1b2 =
LR"HTML(
function __parseVar() {
    var list = [];
    for (;;) {
        var id = __tok[__ti];
        if (id.t !== 'id') throw new Error('bad var');
        __ti++;
        if (__tok[__ti].v === ':') { __ti++; if (__tok[__ti].t === 'id' || __tok[__ti].t === 'kw') __ti++; }
        var v = null;
        if (__tok[__ti].v === '=') { __ti++; v = __parseAssign(); }
        list.push({ k: 'var', n: id.v, v: v });
        if (__tok[__ti].v === ',') { __ti++; continue; }
        break;
    }
    if (__tok[__ti].v === ';') __ti++;
    return { k: 'vars', list: list };
}
function __parseIf() {
    if (__tok[__ti].t === 'op' && __tok[__ti].v === '(') __ti++;
    var c = __parseExpr();
    if (__tok[__ti].v === ')') __ti++;
    var t = __parseStmt();
    var e = null;
    if (__tok[__ti].t === 'kw' && __tok[__ti].v === 'else') { __ti++; e = __parseStmt(); }
    return { k: 'if', c: c, t: t, e: e };
}
function __parseWhile() {
    if (__tok[__ti].t === 'op' && __tok[__ti].v === '(') __ti++;
    var c = __parseExpr();
    if (__tok[__ti].v === ')') __ti++;
    var b = __parseStmt();
    return { k: 'while', c: c, b: b };
}
function __parseFor() {
    var saveFor = __ti;
    var fp = false;
    if (__tok[__ti].t === 'op' && __tok[__ti].v === '(') { __ti++; fp = true; }
    var isForIn = false, itVar = '';
    var save2 = __ti;
    if (__tok[__ti].t === 'kw' && (__tok[__ti].v === 'var' || __tok[__ti].v === '定义')) {
        __ti++;
        if (__tok[__ti].t === 'id') { itVar = __tok[__ti].v; __ti++; if (__tok[__ti].t === 'kw' && __tok[__ti].v === 'in') { __ti++; isForIn = true; } }
    } else if (__tok[__ti].t === 'id') {
        itVar = __tok[__ti].v; __ti++; if (__tok[__ti].t === 'kw' && __tok[__ti].v === 'in') { __ti++; isForIn = true; }
    }
    if (isForIn) {
        var obj2 = __parseExpr();
        if (fp && __tok[__ti].t === 'op' && __tok[__ti].v === ')') __ti++;
        var body2 = __parseStmt();
        return { k: 'forin', v: itVar, o: obj2, b: body2 };
    }
    __ti = save2;
    if (fp && __tok[__ti].v !== ';') {}   // 占位保持对齐
    var i = null, c = null, s = null;
    if (__tok[__ti].v !== ';') {
        if (__tok[__ti].t === 'kw' && (__tok[__ti].v === 'var' || __tok[__ti].v === '定义')) { __ti++; i = __parseVar(); }
        else i = __parseExpr();
    }
    if (__tok[__ti].v === ';') __ti++;
    if (__tok[__ti].v !== ';') c = __parseExpr();
    if (__tok[__ti].v === ';') __ti++;
    if (__tok[__ti].v !== ')') s = __parseExpr();
    if (__tok[__ti].v === ')') __ti++;
    var b = __parseStmt();
    return { k: 'for', i: i, c: c, s: s, b: b };
}

)HTML";

static const wchar_t* kExportScript1b3 =
LR"HTML(
function __parseFunc() {
    var id = __tok[__ti];
    if (id.t !== 'id') throw new Error('bad func name');
    __ti++;
    __expectOp('(');
    var params = [];
    if (__tok[__ti].v !== ')') {
        if (__tok[__ti].v === '...') __ti++;              // AS3 剩余参数 ...args：当普通参数
        params.push(__tok[__ti].v); __ti++;
        if (__tok[__ti].v === ':') { __ti++; __ti++; }
        while (__tok[__ti].v === ',') {
            __ti++;
            if (__tok[__ti].v === '...') __ti++;
            params.push(__tok[__ti].v); __ti++;
            if (__tok[__ti].v === ':') { __ti++; __ti++; }
        }
    }
    __expectOp(')');
    if (__tok[__ti].v === ':') { __ti++; __ti++; }
    var body = __parseStmt();
    var fnName = id.v;
    __fns[fnName] = { params: params, body: body };
    if (fnName === 'onEnterFrame' || fnName === 'onEnterframe') __hasFrameHandler = true;
    // 类构造函数（与类同名）：返回 ctor 标记，__run 顶层执行一次
    if (__curClass && fnName === __curClass) return { k: 'func', n: fnName, ctor: true };
    return { k: 'func', n: fnName };
}
function __parseStmt() {
    var t = __tok[__ti];
    if (t.t === 'kw') {
        if (t.v === 'var' || t.v === '定义' || t.v === 'const' || t.v === '常量') { __ti++; return __parseVar(); }
        if (t.v === 'import' || t.v === '导入') { __ti++; while (__tok[__ti].v !== ';' && __tok[__ti].v !== 'eof') __ti++; if (__tok[__ti].v === ';') __ti++; return { k: 'block', list: [] }; }
        if (t.v === 'super' || t.v === '父类') { __ti++; while (__tok[__ti].v !== ';' && __tok[__ti].v !== 'eof' && __tok[__ti].v !== '}') __ti++; if (__tok[__ti].v === ';') __ti++; return { k: 'block', list: [] }; }
        // AS3 访问/类修饰符：跳过后解析真正的语句
        if (t.v === 'public' || t.v === 'private' || t.v === 'protected' || t.v === 'static' ||
            t.v === 'internal' || t.v === 'final' || t.v === 'override' || t.v === 'dynamic' ||
            t.v === '公有' || t.v === '私有' || t.v === '保护' || t.v === '静态' || t.v === '重写') { __ti++; return __parseStmt(); }
        if (t.v === 'package' || t.v === '包') {
            __ti++;
            while ((__tok[__ti].t === 'id' || __tok[__ti].t === 'kw') || __tok[__ti].v === '.') __ti++;
            __expectOp('{');
            var plist = [];
            while (__tok[__ti].v !== '}') plist.push(__parseStmt());
            __ti++;
            return { k: 'block', list: plist };
        }
        if (t.v === 'class' || t.v === '类') {
            __ti++;
            var cid = __tok[__ti];
            if (cid.t !== 'id') throw new Error('bad class name');
            __ti++;
            if (__tok[__ti].t === 'kw' && __tok[__ti].v === 'extends') {
                __ti++;
                if (__tok[__ti].t === 'id' || __tok[__ti].t === 'kw') __ti++;
                while (__tok[__ti].v === '.') { __ti++; if (__tok[__ti].t === 'id' || __tok[__ti].t === 'kw') __ti++; }
            }
            if (__tok[__ti].t === 'kw' && __tok[__ti].v === 'implements') {
                __ti++;
                do { if (__tok[__ti].t === 'id' || __tok[__ti].t === 'kw') __ti++; if (__tok[__ti].v === ',') __ti++; } while (__tok[__ti].v !== '{');
            }
            __expectOp('{');
            var saveCls = __curClass;
            __curClass = cid.v;                       // 构造函数识别（与类同名）
            var clist = [];
            while (__tok[__ti].v !== '}') clist.push(__parseStmt());
            __curClass = saveCls;
            __ti++;
            return { k: 'block', list: clist };
        }
        if (t.v === 'if' || t.v === '如果') { __ti++; return __parseIf(); }
        if (t.v === 'for' || t.v === '循环') { __ti++; return __parseFor(); }
        if (t.v === 'while' || t.v === '当') { __ti++; return __parseWhile(); }
        if (t.v === 'do' || t.v === '做') { __ti++; return __parseDo(); }
        if (t.v === 'switch' || t.v === '开关') { __ti++; return __parseSwitch(); }
        if (t.v === 'try' || t.v === '尝试') { __ti++; return __parseTry(); }
        if (t.v === 'throw' || t.v === '抛出') { __ti++; return __parseThrow(); }
        if (t.v === 'break' || t.v === '跳出') { __ti++; if (__tok[__ti].v === ';') __ti++; return { k: 'break' }; }
        if (t.v === 'continue' || t.v === '继续') { __ti++; if (__tok[__ti].v === ';') __ti++; return { k: 'continue' }; }
        if (t.v === 'function' || t.v === '函数') { __ti++; return __parseFunc(); }
        if (t.v === 'return') { __ti++; if (__tok[__ti].v === ';') { __ti++; return { k: 'return', v: null }; } var e = __parseExpr(); if (__tok[__ti].v === ';') __ti++; return { k: 'return', v: e }; }
        if (t.v === 'new') { __ti++; return __parseNew(); }
    }
    if (t.t === 'op' && t.v === '{') {
        __ti++;
        var list = [];
        while (__tok[__ti].v !== '}') list.push(__parseStmt());
        __ti++;
        return { k: 'block', list: list };
    }
    var e2 = __parseExpr();
    if (__tok[__ti].v === ';') __ti++;
    return { k: 'expr', e: e2 };
}
function __parseDo() {
    var b = __parseStmt();
    if (__tok[__ti].t === 'kw' && (__tok[__ti].v === 'while' || __tok[__ti].v === '当')) __ti++;
    if (__tok[__ti].t === 'op' && __tok[__ti].v === '(') __ti++;
    var c = __parseExpr();
    if (__tok[__ti].t === 'op' && __tok[__ti].v === ')') __ti++;
    if (__tok[__ti].t === 'op' && __tok[__ti].v === ';') __ti++;
    return { k: 'dowhile', b: b, c: c };
}
function __parseSwitch() {
    if (__tok[__ti].t === 'op' && __tok[__ti].v === '(') __ti++;
    var e = __parseExpr();
    if (__tok[__ti].t === 'op' && __tok[__ti].v === ')') __ti++;
    if (__tok[__ti].t === 'op' && __tok[__ti].v === '{') __ti++;
    var cases = [], def = null;
    while (__tok[__ti].v !== '}') {
        var t2 = __tok[__ti];
        if (t2.t === 'kw' && (t2.v === 'case' || t2.v === '情况')) {
            __ti++;
            var cv = __parseExpr();
            __expectOp(':');
            var bl = [];
            while (!(__tok[__ti].t === 'kw' && (__tok[__ti].v === 'case' || __tok[__ti].v === 'default' || __tok[__ti].v === '情况' || __tok[__ti].v === '默认')) && __tok[__ti].v !== '}') bl.push(__parseStmt());
            cases.push({ v: cv, b: bl });
        } else if (t2.t === 'kw' && (t2.v === 'default' || t2.v === '默认')) {
            __ti++;
            __expectOp(':');
            var bl2 = [];
            while (__tok[__ti].v !== '}') bl2.push(__parseStmt());
            def = bl2;
        } else throw new Error('bad switch: ' + t2.v);
    }
    __ti++;
    return { k: 'switch', e: e, cases: cases, d: def };
}
function __parseTry() {
    var tb = __parseStmt();
    var cv = null, cb = null, fb = null;
    if (__tok[__ti].t === 'kw' && (__tok[__ti].v === 'catch' || __tok[__ti].v === '捕捉')) {
        __ti++;
        if (__tok[__ti].t === 'op' && __tok[__ti].v === '(') __ti++;
        var id2 = __tok[__ti]; __ti++;
        if (__tok[__ti].t === 'op' && __tok[__ti].v === ':') { __ti++; __ti++; }
        if (__tok[__ti].t === 'op' && __tok[__ti].v === ')') __ti++;
        cv = id2.v;
        cb = __parseStmt();
    }
    if (__tok[__ti].t === 'kw' && (__tok[__ti].v === 'finally' || __tok[__ti].v === '最后')) {
        __ti++;
        fb = __parseStmt();
    }
    return { k: 'try', b: tb, v: cv, c: cb, f: fb };
}
function __parseThrow() {
    var e3 = __parseExpr();
    if (__tok[__ti].t === 'op' && __tok[__ti].v === ';') __ti++;
    return { k: 'throw', e: e3 };
}
function __parseNew() {
    var ty = __tok[__ti];
    __ti++;
    __expectOp('(');
    var args = [];
    if (__tok[__ti].v !== ')') { args.push(__parseExpr()); while (__tok[__ti].v === ',') { __ti++; args.push(__parseExpr()); } }
    __expectOp(')');
    return { k: 'new', t: ty.v, args: args };
}
function __parseProg() {
    var list = [];
    while (__tok[__ti].t !== 'eof') list.push(__parseStmt());
    return { k: 'block', list: list };
}
function __compile() {
    __lex(DATA.script || '');
    __prog = __parseProg();
    if (DATA.docClass) { __lex(DATA.docClass); __docProg = __parseProg(); }
    var ks = DATA.keyScripts || [];
    for (var i = 0; i < ks.length; i++) {
        __lex(ks[i].s);
        __keyProgs[ks[i].f] = __parseProg();
    }
}
)HTML";

static const wchar_t* kExportScript2a =
LR"HTML(
// ================= 增强脚本引擎：第 2 段（求值器与内置函数）=================
function __truth(v) { return !!v; }
function __num(v) { return typeof v === 'number' ? v : (Number(v) || 0); }
function __str(v) {
    if (typeof v === 'string') return v;
    if (typeof v === 'number') return String(v);
    if (typeof v === 'boolean') return v ? 'true' : 'false';
    if (v === null || v === undefined) return '';
    if (Array.isArray(v)) return '[' + v.map(__str).join(',') + ']';
    return String(v);
}
function __prop(o, p) {
    if (o === Math) {
        if (p === 'PI') return Math.PI; if (p === 'E') return Math.E;
        if (p === 'LN2') return Math.LN2; if (p === 'LN10') return Math.LN10;
        if (p === 'LOG2E') return Math.LOG2E; if (p === 'LOG10E') return Math.LOG10E;
        if (p === 'SQRT2') return Math.SQRT2; if (p === 'SQRT1_2') return Math.SQRT1_2;
        var f = o[p]; return typeof f === 'function' ? f : 0;
    }
    if (o && o.__isKey) { var kc = __keyConst('Key.' + p); return kc === null ? 0 : kc; }
    if (o && o.isObject3D) {
        if (p === 'x') return o.position.x; if (p === 'y') return o.position.y; if (p === 'z') return o.position.z;
        if (p === 'rx') return Math.round(o.rotation.x / KDEG * 1e6) / 1e6; if (p === 'ry') return Math.round(o.rotation.y / KDEG * 1e6) / 1e6; if (p === 'rz') return Math.round(o.rotation.z / KDEG * 1e6) / 1e6;
        if (p === 'rotationX' || p === 'rotationY' || p === 'rotationZ' || p === 'rotation') {
            var ax = (p === 'rotationY') ? 'y' : (p === 'rotationZ') ? 'z' : 'x';
            return Math.round(o.rotation[ax] / KDEG * 1e6) / 1e6;
        }
        if (p === 'parent') return o.parent;
        if (p === 'sx') return o.scale.x; if (p === 'sy') return o.scale.y; if (p === 'sz') return o.scale.z;
        if (p === 's') return o.scale.x;
        if (p === 'visible') return o.visible ? 1 : 0;
        if (p === 'name') return (o.userData && o.userData.name) || '';
        if (p === 'type') return (o.userData && o.userData.type) || '';
        if (p === 'segments' || p === '细分') return (o.userData && o.userData.seg) || 16;
        if (p === 'red') return (o.material && o.material.color) ? Math.round(o.material.color.r * 255) : 0;
        if (p === 'green') return (o.material && o.material.color) ? Math.round(o.material.color.g * 255) : 0;
        if (p === 'blue') return (o.material && o.material.color) ? Math.round(o.material.color.b * 255) : 0;
        return 0;
    }
    if (typeof o === 'string' && p === 'length') return o.length;
    if (Array.isArray(o) && p === 'length') return o.length;
    if (o !== null && o !== undefined && typeof o === 'object' && (p in o)) return o[p];
    return 0;
}
function __setprop(o, p, v) {
    if (!o || !o.isObject3D) return;
    if (p === 'x') o.position.x = v; else if (p === 'y') o.position.y = v; else if (p === 'z') o.position.z = v;
    else if (p === 'rx') o.rotation.x = v * KDEG; else if (p === 'ry') o.rotation.y = v * KDEG; else if (p === 'rz') o.rotation.z = v * KDEG;
    else if (p === 'rotationX') o.rotation.x = v * KDEG;
    else if (p === 'rotationY') o.rotation.y = v * KDEG;
    else if (p === 'rotationZ') o.rotation.z = v * KDEG;
    else if (p === 'rotation') o.rotation.z = v * KDEG;
    else if (p === 'sx') o.scale.x = v; else if (p === 'sy') o.scale.y = v; else if (p === 'sz') o.scale.z = v;
    else if (p === 'scaleX') o.scale.x = v; else if (p === 'scaleY') o.scale.y = v; else if (p === 'scaleZ') o.scale.z = v;
    else if (p === 's') { o.scale.set(v, v, v); }
    else if (p === 'visible') o.visible = (v !== 0 && v !== false && v !== 'false');
    else if (p === 'red' || p === 'green' || p === 'blue') {
        if (o.material && o.material.color) {
            var c = Math.max(0, Math.min(1, __num(v) / 255));
            if (p === 'red') o.material.color.r = c;
            else if (p === 'green') o.material.color.g = c;
            else o.material.color.b = c;
        }
    }
}
function __findVar(name) {
    for (var i = __scope.length - 1; i >= 0; i--) if (name in __scope[i]) return __scope[i][name];
    if (name in __globals) return __globals[name];
    return undefined;
}
function __setVar(name, v) {
    for (var i = __scope.length - 1; i >= 0; i--) if (name in __scope[i]) { __scope[i][name] = v; return; }
    __globals[name] = v;
}
function __calcAssign(op, old, r) {
    switch (op) {
        case '=': return r;
        case '+=': return __num(old) + __num(r);
        case '-=': return __num(old) - __num(r);
        case '*=': return __num(old) * __num(r);
        case '/=': return __num(old) / __num(r);
    }
    return r;
}
function __bin(op, a, b) {
    switch (op) {
        case '+': if (typeof a === 'string' || typeof b === 'string') return __str(a) + __str(b); return __num(a) + __num(b);
        case '-': return __num(a) - __num(b);
        case '*': return __num(a) * __num(b);
        case '/': return __num(a) / __num(b);
        case '%': return __num(a) % __num(b);
        case '==': return a == b;
        case '!=': return a != b;
        case '===': return a === b;
        case '!==': return a !== b;
        case '<': return __num(a) < __num(b);
        case '<=': return __num(a) <= __num(b);
        case '>': return __num(a) > __num(b);
        case '>=': return __num(a) >= __num(b);
        case '&&': return __truth(a) && __truth(b);
        case '||': return __truth(a) || __truth(b);
    }
    return 0;
}

)HTML";

static const wchar_t* kExportScript2a2 =
LR"HTML(
function __eval(n) {
    switch (n.k) {
        case 'num': return n.v;
        case 'str': return n.v;
        case 'bool': return n.v;
        case 'null': return null;
        case 'array': { var a = []; for (var i = 0; i < n.items.length; i++) a.push(__eval(n.items[i])); return a; }
        case 'ternary': return __truth(__eval(n.c)) ? __eval(n.a) : __eval(n.b);
        case 'typeof': {
            var tv = __eval(n.a);
            if (tv === null || tv === undefined) return 'object';
            return typeof tv;
        }
        case 'this': return __thisObj;
        case 'delete': {
            var d = n.a;
            if (d && d.k === 'index') {
                var ob3 = __eval(d.o), iv3 = __eval(d.i);
                if (Array.isArray(ob3)) { var i5 = Math.floor(__num(iv3)); if (i5 >= 0 && i5 < ob3.length) { ob3.splice(i5, 1); return true; } return false; }
                if (typeof ob3 === 'string') { var i6 = Math.floor(__num(iv3)); if (i6 >= 0 && i6 < ob3.length) return true; return false; }
            }
            return false;
        }
        case 'id': {
            var v = __findVar(n.n);
            if (v !== undefined) return v;
            var kc = __keyConst(n.n);
            if (kc !== null) return kc;
            if (n.n === 'Math') return Math;
            if (n.n === 'Key') return { __isKey: true };
            if (n.n === 'stage') return __stageObj;
            if (n.n === 'KeyboardEvent') return { KEY_DOWN: 'keyDown', KEY_UP: 'keyUp' };
            if (n.n === 'Event') return { ENTER_FRAME: 'enterFrame' };
            if (n.n === 'Math.PI') return Math.PI;
            if (n.n === 'Math.E') return Math.E;
            if (n.n === 'Event.ENTER_FRAME') return 'enterFrame';
            if (n.n === 'stage.keyCode' || n.n === 'keyCode') return __stageObj.keyCode;
            if (n.n === '帧' || n.n === 'frame') return __globals['帧'] || 0;
            if (n.n === 'fps') return DATA.fps;
            if (__fns[n.n]) return n.n;
            if (byName[n.n]) return byName[n.n];
            return 0;
        }
        case 'prop': { var o = __eval(n.o); return __prop(o, n.p); }
        case 'index': {
            var o2 = __eval(n.o), idx = __eval(n.i);
            if (Array.isArray(o2)) { var ii = Math.floor(__num(idx)); return (ii >= 0 && ii < o2.length) ? o2[ii] : 0; }
            if (typeof o2 === 'string') { var ii2 = Math.floor(__num(idx)); return (ii2 >= 0 && ii2 < o2.length) ? o2.charAt(ii2) : ''; }
            return 0;
        }
        case 'assign': {
            var r = __eval(n.v);
            var a = n.a;
            if (a.k === 'prop') {
                var oo = __eval(a.o);
                var old = __prop(oo, a.p);
                __setprop(oo, a.p, __calcAssign(n.op, old, r));
            } else if (a.k === 'index') {
                var ob = __eval(a.o), iv = __eval(a.i);
                if (Array.isArray(ob)) { var ii3 = Math.floor(__num(iv)); if (ii3 >= 0 && ii3 < ob.length) ob[ii3] = r; }
            } else if (a.k === 'id') {
                __setVar(a.n, __calcAssign(n.op, __findVar(a.n), r));
            }
            return r;
        }
        case 'op2': return __bin(n.op, __eval(n.a), __eval(n.b));
        case 'neg': return -__num(__eval(n.a));
        case 'not': return !__truth(__eval(n.a));
        case 'inc': {
            var tgt = n.a;
            var old = __eval(tgt);
            var nv = __num(old) + n.d;
            if (tgt.k === 'prop') __setprop(__eval(tgt.o), tgt.p, nv);
            else if (tgt.k === 'index') {
                var ob2 = __eval(tgt.o), iv2 = __eval(tgt.i);
                if (Array.isArray(ob2)) { var i4 = Math.floor(__num(iv2)); if (i4 >= 0 && i4 < ob2.length) ob2[i4] = nv; }
            } else if (tgt.k === 'id') __setVar(tgt.n, nv);
            return n.pre ? nv : old;
        }
        case 'call': return __call(n);
        case 'new': return null;
        default: return 0;
    }
}

)HTML";

static const wchar_t* kExportScript2b =
LR"HTML(
function __builtin(fn, args) {
    var a = args[0], b = args[1];
    if (fn.indexOf('Math.') === 0) {
        var m = fn.slice(5);
        if (m === 'sin') return Math.sin(__num(a));
        if (m === 'cos') return Math.cos(__num(a));
        if (m === 'tan') return Math.tan(__num(a));
        if (m === 'asin') return Math.asin(__num(a));
        if (m === 'acos') return Math.acos(__num(a));
        if (m === 'atan') return Math.atan(__num(a));
        if (m === 'atan2') return Math.atan2(__num(a), __num(b));
        if (m === 'sqrt') { var x = __num(a); return x < 0 ? 0 : Math.sqrt(x); }
        if (m === 'pow') return Math.pow(__num(a), __num(b));
        if (m === 'abs') return Math.abs(__num(a));
        if (m === 'floor') return Math.floor(__num(a));
        if (m === 'ceil') return Math.ceil(__num(a));
        if (m === 'round') return Math.floor(__num(a) + 0.5);
        if (m === 'exp') return Math.exp(__num(a));
        if (m === 'log') return Math.log(__num(a));
        if (m === 'min') { var best = 0; for (var i = 0; i < args.length; i++) { if (i === 0 || __num(args[i]) < best) best = __num(args[i]); } return best; }
        if (m === 'max') { var best2 = 0; for (var i2 = 0; i2 < args.length; i2++) { if (i2 === 0 || __num(args[i2]) > best2) best2 = __num(args[i2]); } return best2; }
        if (m === 'random') return Math.random();
        return 0;
    }
    if (fn === 'Key.isDown') { var c = Math.floor(__num(a)); return !!__keys[c]; }
    if (fn === 'trace' || fn === 'print' || fn === '输出') {
        __logTxt += args.map(__str).join(' ') + '\n';
        var logEl2 = document.getElementById('log');
        if (logEl2) logEl2.textContent = __logTxt;
        return null;
    }
    if (fn === 'Number') return __num(a);
    if (fn === 'int' || fn === 'uint') return Math.floor(__num(a));
    if (fn === 'String') return __str(a);
    if (fn === 'Boolean') return __truth(a);
    if (fn === 'addEventListener' || fn === 'stage.addEventListener') {
        if (args.length >= 2) {
            var et = __str(a), nm = __str(b);
            if (et === 'enterFrame') {
                if (__frameFns.indexOf(nm) < 0) __frameFns.push(nm);
                __hasFrameHandler = true;
            } else if (et === 'keyDown' || et === 'keyUp') {
                if (!__stageFns[et]) __stageFns[et] = [];
                if (__stageFns[et].indexOf(nm) < 0) __stageFns[et].push(nm);
                __hasFrameHandler = true;
            }
        }
        return null;
    }
    if (fn === 'sin' || fn === '正弦') return Math.sin(__num(a) * KDEG);
    if (fn === 'cos' || fn === '余弦') return Math.cos(__num(a) * KDEG);
    if (fn === 'tan' || fn === '正切') return Math.tan(__num(a) * KDEG);
    if (fn === 'sqrt' || fn === '开方') { var y = __num(a); return y < 0 ? 0 : Math.sqrt(y); }
    if (fn === 'abs' || fn === '绝对值') return Math.abs(__num(a));
    if (fn === 'min' || fn === '最小') return Math.min(__num(a), __num(b));
    if (fn === 'max' || fn === '最大') return Math.max(__num(a), __num(b));
    if (fn === 'random' || fn === '随机') return Math.random();
    if (fn === 'play' || fn === '播放') { __setPlay(true); return null; }
    if (fn === 'stop' || fn === '停止') { __setPlay(false); return null; }
    if (fn === 'gotoFrame' || fn === '跳到帧') { __setFrame(__num(a)); return null; }
    if (fn === 'isNaN') return isNaN(a);
    if (fn === 'isFinite') return isFinite(__num(a));
    if (fn === 'parseInt') return parseInt(__str(a), 10);
    if (fn === 'parseFloat') return parseFloat(__str(a));
    if (fn === 'Number.parseInt') return parseInt(__str(a), 10);
    if (fn === 'String.fromCharCode') return String.fromCharCode.apply(null, args.map(__num));
    if (fn === 'getTimer') return Date.now() - __t0;
    if (fn === 'setInterval' || fn === 'setTimeout') {
        var fnN = __str(a), msN = __num(b);
        if (__fns[fnN]) {
            var h = (fn === 'setInterval')
                ? setInterval(function () { try { __callUser(fnN, []); } catch (e4) {} }, msN)
                : setTimeout(function () { try { __callUser(fnN, []); } catch (e4) {} }, msN);
            return h;
        }
        return 0;
    }
    if (fn === 'clearInterval' || fn === 'clearTimeout') { clearInterval(__num(a)); clearTimeout(__num(a)); return null; }
    return 0;
}
function __call(n) {
    var f = n.f;
    var args = n.args.map(__eval);
    if (f.k === 'id') {
        var name = f.n;
        var dot = name.lastIndexOf('.');
        if (dot > 0) {
            var ns = name.slice(0, dot), fn2 = name.slice(dot + 1);
            if (ns === 'Math' || ns === 'Key') return __builtin(ns + '.' + fn2, args);
        }
        if (__fns[name]) return __callUser(name, args);
        return __builtin(name, args);
    }
    if (f.k === 'prop') {
        var o = __eval(f.o);
        if (o === Math) { return __builtin('Math.' + f.p, args); }
        if (o === String) { return __builtin('String.' + f.p, args); }
        if (o === Number) { return __builtin('Number.' + f.p, args); }
        if (o === Boolean) { return __builtin('Boolean.' + f.p, args); }
        if (o && o.__isKey) return __builtin('Key.' + f.p, args);
        if (o && o.__isStage) return __builtin('stage.' + f.p, args);
        if (Array.isArray(o)) {
            if (f.p === 'push') { o.push(args[0]); return o.length; }
            if (f.p === 'pop') return o.pop();
            if (f.p === 'shift') return o.shift();
            if (f.p === 'unshift') { o.unshift(args[0]); return o.length; }
            if (f.p === 'splice') { var sp = [Math.floor(__num(args[0])), Math.floor(__num(args[1]))]; return o.splice.apply(o, sp.concat(args.slice(2))); }
            if (f.p === 'concat') { return o.concat.apply(o, args); }
            if (f.p === 'join') return o.join(args[0] !== undefined ? __str(args[0]) : ',');
            if (f.p === 'reverse') { o.reverse(); return o; }
            if (f.p === 'sort') { o.sort(); return o; }
            if (f.p === 'indexOf') return o.indexOf(args[0]);
            if (f.p === 'lastIndexOf') return o.lastIndexOf(args[0]);
            if (f.p === 'slice') return o.slice(Math.floor(__num(args[0])), args[1] !== undefined ? Math.floor(__num(args[1])) : undefined);
            if (f.p === 'toString') return o.join(',');
            return 0;
        }
        if (typeof o === 'number') {
            if (f.p === 'toFixed') { var dp = Math.max(0, Math.floor(__num(args[0]))); return o.toFixed(dp); }
            if (f.p === 'toPrecision') { var pr = Math.max(1, Math.floor(__num(args[0]))); return o.toPrecision(pr); }
            if (f.p === 'toString') return String(o);
            return 0;
        }
        if (typeof o === 'string') {
            if (f.p === 'charAt') { var ci = Math.floor(__num(args[0])); return (ci >= 0 && ci < o.length) ? o.charAt(ci) : ''; }
            if (f.p === 'charCodeAt') { var ci2 = Math.floor(__num(args[0])); return (ci2 >= 0 && ci2 < o.length) ? o.charCodeAt(ci2) : NaN; }
            if (f.p === 'indexOf') return o.indexOf(__str(args[0]));
            if (f.p === 'lastIndexOf') return o.lastIndexOf(__str(args[0]));
            if (f.p === 'slice') return o.slice(Math.floor(__num(args[0])), args[1] !== undefined ? Math.floor(__num(args[1])) : undefined);
            if (f.p === 'substring') return o.substring(Math.floor(__num(args[0])), args[1] !== undefined ? Math.floor(__num(args[1])) : o.length);
            if (f.p === 'substr') return o.substr(Math.floor(__num(args[0])), args[1] !== undefined ? Math.floor(__num(args[1])) : undefined);
            if (f.p === 'split') return o.split(__str(args[0]));
            if (f.p === 'replace') return o.replace(__str(args[0]), __str(args[1]));
            if (f.p === 'toUpperCase') return o.toUpperCase();
            if (f.p === 'toLowerCase') return o.toLowerCase();
            if (f.p === 'trim') return o.trim();
            if (f.p === 'concat') return o.concat(args.map(__str));
            if (f.p === 'toString') return o;
            return 0;
        }
        return 0;
    }
    return 0;
}

)HTML";

static const wchar_t* kExportScript2b2 =
LR"HTML(
function __run(n) {
    if (!n || __retF) return;
    switch (n.k) {
        case 'block':
            for (var i = 0; i < n.list.length; i++) {
                __run(n.list[i]);
                if (__breakF || __contF || __retF) break;
            }
            break;
        case 'vars':
            for (var i2 = 0; i2 < n.list.length; i2++) {
                var d = n.list[i2];
                if (__findVar(d.n) === undefined) __setVar(d.n, 0);
                if (d.v) __setVar(d.n, __eval(d.v));
            }
            break;
        case 'expr': __eval(n.e); break;
        case 'if':
            if (__truth(__eval(n.c))) __run(n.t);
            else if (n.e) __run(n.e);
            break;
        case 'while':
            while (__truth(__eval(n.c))) {
                __run(n.b);
                if (__breakF) { __breakF = false; break; }
                if (__contF) { __contF = false; continue; }
                if (__retF) break;
            }
            break;
        case 'dowhile':
            do {
                __run(n.b);
                if (__breakF) { __breakF = false; break; }
                if (__contF) { __contF = false; }
                if (__retF) break;
            } while (__truth(__eval(n.c)));
            break;
        case 'switch': {
            var sv = __eval(n.e);
            var matched = null;
            for (var i7 = 0; i7 < n.cases.length; i7++) {
                if (__eval(n.cases[i7].v) == sv) { matched = n.cases[i7].b; break; }
            }
            if (!matched) matched = n.d;
            if (matched) {
                for (var i8 = 0; i8 < matched.length; i8++) {
                    __run(matched[i8]);
                    if (__breakF) { __breakF = false; break; }
                    if (__contF || __retF) break;
                }
            }
            break;
        }
        case 'try': {
            var sb = __breakF, sc = __contF, sr = __retF, srv = __retV;
            try {
                __run(n.b);
            } catch (ex) {
                if (n.v) { __setVar(n.v, ex); }
                __breakF = sb; __contF = sc; __retF = sr; __retV = srv;
                if (n.c) __run(n.c);
            } finally {
                if (n.f) {
                    var sb2 = __breakF, sc2 = __contF, sr2 = __retF, srv2 = __retV;
                    __breakF = sb; __contF = sc; __retF = sr; __retV = srv;
                    __run(n.f);
                    __breakF = sb2; __contF = sc2; __retF = sr2; __retV = srv2;
                }
            }
            break;
        }
        case 'throw': throw __eval(n.e);
        case 'forin': {
            var ov = __eval(n.o);
            if (Array.isArray(ov)) {
                for (var i9 = 0; i9 < ov.length; i9++) {
                    __setVar(n.v, i9);
                    __run(n.b);
                    if (__breakF) { __breakF = false; break; }
                    if (__contF) { __contF = false; continue; }
                    if (__retF) break;
                }
            } else if (typeof ov === 'string') {
                for (var i10 = 0; i10 < ov.length; i10++) {
                    __setVar(n.v, i10);
                    __run(n.b);
                    if (__breakF) { __breakF = false; break; }
                    if (__contF) { __contF = false; continue; }
                    if (__retF) break;
                }
            } else if (typeof ov === 'object' && ov !== null && !ov.isObject3D && !Array.isArray(ov)) {
                var ks = Object.keys(ov);
                for (var i11 = 0; i11 < ks.length; i11++) {
                    __setVar(n.v, ks[i11]);
                    __run(n.b);
                    if (__breakF) { __breakF = false; break; }
                    if (__contF) { __contF = false; continue; }
                    if (__retF) break;
                }
            } else if (typeof ov === 'number') {
                var lim = Math.floor(ov);
                for (var i12 = 0; i12 < lim; i12++) {
                    __setVar(n.v, i12);
                    __run(n.b);
                    if (__breakF) { __breakF = false; break; }
                    if (__contF) { __contF = false; continue; }
                    if (__retF) break;
                }
            }
            break;
        }
        case 'for':
            if (n.i) __run(n.i);
            while (!n.c || __truth(__eval(n.c))) {
                __run(n.b);
                if (__breakF) { __breakF = false; break; }
                if (__contF) { __contF = false; }
                if (__retF) break;
                if (n.s) __eval(n.s);
            }
            break;
        case 'break': __breakF = true; break;
        case 'continue': __contF = true; break;
        case 'return': __retF = true; __retV = n.v ? __eval(n.v) : null; break;
        case 'func':
            // 类构造函数（与类同名）：顶层执行一次（Flash 文档类构造）
            if (n.ctor && n.n && __fns[n.n]) __run(__fns[n.n].body);
            break;
        default: break;
    }
}
function __callUser(name, args) {
    var f = __fns[name];
    if (!f) return 0;
    var sc = {};
    for (var i = 0; i < f.params.length; i++) sc[f.params[i]] = args[i] !== undefined ? args[i] : 0;
    __scope.push(sc);
    var sb = __breakF, sc2 = __contF, sr = __retF, srv = __retV;
    __breakF = __contF = __retF = false;
    __retV = null;
    __run(f.body);
    var res = __retF ? __retV : 0;
    __retF = sr; __retV = srv; __breakF = sb; __contF = sc2;
    __scope.pop();
    return (res === null || res === undefined) ? 0 : res;
}
)HTML";

static const wchar_t* kExportScript3 =
LR"HTML(
// ================= 增强脚本引擎：第 3 段（帧驱动 / 键盘 / stage 事件 / UI / 自检）=================
// AS3 舞台：stage.addEventListener("keyDown"/"keyUp", 函数名)；回调参数 e = 键码；stage.keyCode 只读
var __stageObj = { __isStage: true, keyCode: 0 };
var __thisObj = null;                 // this（当前脚本上下文，保留为 null）
var __t0 = Date.now();                // getTimer() 计时起点
var __stageFns = {};
var __keyProgs = {}, __ksDone = {}, __docProg = null;   // 关键帧脚本表 / 已执行帧 / 文档类脚本
function __stageFire(type, code) {
    __stageObj.keyCode = code;
    var fns = __stageFns[type];
    if (!fns) return;
    for (var i = 0; i < fns.length; i++) {
        if (__fns[fns[i]]) {
            try { __callUser(fns[i], [code]); } catch (e2) {}
        }
    }
}
function __setPlay(p) { playing = p; if (!p) lastT = 0; }
function __setFrame(fr) { frame = fr; lastT = 0; }
function __scriptRun(f) {
    __globals['帧'] = f;
    if (__hasFrameHandler) {
        // 帧驱动不依赖主脚本（文档类单独注册 onEnterFrame/addEventListener 时无主脚本也每帧执行）
        if (__prog) { if (!__topDone) { __topDone = true; __run(__prog); } }
        var fns = __frameFns.slice();
        if (__fns['onEnterFrame'] && fns.indexOf('onEnterFrame') < 0) fns.push('onEnterFrame');
        for (var i = 0; i < fns.length; i++) {
            if (__fns[fns[i]]) __callUser(fns[i], [null]);
        }
    } else if (__prog) {
        __run(__prog);
    }
    var ht = __globals['hudText'];
    var hudEl = document.getElementById('hud');
    if (hudEl) {
        if (ht !== undefined && ht !== '' && __str(ht).length > 0) {
            hudEl.style.display = 'block';
            hudEl.textContent = __str(ht);
        } else hudEl.style.display = 'none';
    }
}
function toggleLog() {
    var el = document.getElementById('log');
    if (el) el.style.display = el.style.display === 'none' ? 'block' : 'none';
}
// 键盘：状态表（Key.isDown）+ stage 事件（边沿：按下/抬起瞬间各触发一次）
window.addEventListener('keydown', function (e) {
    var was = !!__keys[e.keyCode];
    __keys[e.keyCode] = true;
    if (!was && !e.repeat) __stageFire('keyDown', e.keyCode);
});
window.addEventListener('keyup', function (e) {
    __keys[e.keyCode] = false;
    __stageFire('keyUp', e.keyCode);
});
window.addEventListener('blur', function () { __keys = {}; });
var compileErr = null;
try { __compile(); } catch (e) { compileErr = e.message; }
// 文档类脚本：页面加载后、播放前执行一次（Flash 文档类初始化）
if (!compileErr && __docProg) {
    try { __run(__docProg); } catch (e5) { compileErr = '文档类错误: ' + e5.message; }
}
// 只有游戏类脚本（定义了 onEnterFrame/函数）才显示 日志/重新开始，原示例外观不变
if (__hasFrameHandler) {
    var lb = document.getElementById('logbtn'), rb = document.getElementById('rst');
    if (lb) lb.style.display = 'inline-block';
    if (rb) rb.style.display = 'inline-block';
}
if (location.search.indexOf('selftest=1') >= 0) {
    var out = 'funcs=' + (__hasFrameHandler ? 'yes' : 'no') + ' progErr=' + (compileErr ? compileErr : 'none');
    if (!compileErr) {
        try {
            for (var sf = 1; sf <= 100; sf++) __scriptRun(sf);
            // 键盘事件链自检：验证注册表 + 模拟按下 W(87)/A(65) 移动转向 + 松开复位
            out += ' kd=' + (__stageFns['keyDown'] ? __stageFns['keyDown'].join(',') : 'NONE');
            out += ' ku=' + (__stageFns['keyUp'] ? __stageFns['keyUp'].join(',') : 'NONE');
            __stageFire('keyDown', 87);
            __stageFire('keyDown', 65);
            for (var sf2 = 1; sf2 <= 30; sf2++) __scriptRun(sf2);
            var mv = (__globals['mv'] !== undefined) ? __globals['mv'] : -1;
            var tr = (__globals['tr'] !== undefined) ? __globals['tr'] : -1;
            var pc = byName['playerTank'];
            var pz = pc ? Math.round(pc.position.z * 100) / 100 : 999;
            var pr = pc ? Math.round(pc.rotation.y * 180 / Math.PI * 100) / 100 : 999;
            out += ' mv=' + mv + ' tr=' + tr + ' pz=' + pz + ' pr=' + pr;
            __stageFire('keyUp', 87);
            __stageFire('keyUp', 65);
            for (var sf3 = 1; sf3 <= 20; sf3++) __scriptRun(sf3);
            var mv2 = (__globals['mv'] !== undefined) ? __globals['mv'] : -1;
            var tr2 = (__globals['tr'] !== undefined) ? __globals['tr'] : -1;
            out += ' mv2=' + mv2 + ' tr2=' + tr2;
            out += ' objs=' + Object.keys(byName).length;
        } catch (e2) { out += ' runErr=' + e2.message; }
    }
    var sd = document.createElement('div');
    sd.id = 'selftest';
    sd.textContent = out;
    document.body.appendChild(sd);
}
)HTML";

static const wchar_t* kExportHtmlTailB =
LR"HTML(
// 相机视角与编辑器一致
var cm = DATA.cam, pr = Math.PI / 180;
camera.position.set(
    cm.tx + cm.dist * Math.cos(cm.pitch * pr) * Math.sin(cm.yaw * pr),
    cm.ty + cm.dist * Math.sin(cm.pitch * pr),
    cm.tz + cm.dist * Math.cos(cm.pitch * pr) * Math.cos(cm.yaw * pr));
camera.lookAt(new THREE.Vector3(cm.tx, cm.ty, cm.tz));
var controls = new THREE.OrbitControls(camera, renderer.domElement);
controls.target.set(cm.tx, cm.ty, cm.tz);
controls.enableDamping = true;
controls.update();

// 关键帧线性插值（与 Super3D 相同）
function frameValues(keys, f) {
    if (!keys || !keys.length) return null;
    var a0 = keys[0], a1 = keys[keys.length - 1];
    if (f <= a0.f) return a0;
    if (f >= a1.f) return a1;
    for (var i = 0; i < keys.length - 1; i++) {
        var a = keys[i], b = keys[i + 1];
        if (f >= a.f && f <= b.f) {
            var t = (b.f === a.f) ? 0 : (f - a.f) / (b.f - a.f);
            return {
                pos: [a.pos[0] + (b.pos[0] - a.pos[0]) * t, a.pos[1] + (b.pos[1] - a.pos[1]) * t, a.pos[2] + (b.pos[2] - a.pos[2]) * t],
                rot: [a.rot[0] + (b.rot[0] - a.rot[0]) * t, a.rot[1] + (b.rot[1] - a.rot[1]) * t, a.rot[2] + (b.rot[2] - a.rot[2]) * t],
                scl: [a.scl[0] + (b.scl[0] - a.scl[0]) * t, a.scl[1] + (b.scl[1] - a.scl[1]) * t, a.scl[2] + (b.scl[2] - a.scl[2]) * t]
            };
        }
    }
    return a1;
}
function applyFrame(f) {
    for (var i = 0; i < anims.length; i++) {
        var v = frameValues(anims[i].keys, f);
        if (!v) continue;
        var o = anims[i].obj;
        o.position.set(v.pos[0], v.pos[1], v.pos[2]);
        o.rotation.set(v.rot[0] * Math.PI / 180, v.rot[1] * Math.PI / 180, v.rot[2] * Math.PI / 180);
        o.scale.set(v.scl[0], v.scl[1], v.scl[2]);
    }
}
var frame = 0, playing = true, lastT = 0, infoEl = document.getElementById('info');
// 关键帧脚本：进入关键帧时执行一次（Flash 式帧动作；同帧多层已由 DATA 聚合）
function __runKeyScripts(fr) {
    if (__ksDone[fr] || !__keyProgs || !__keyProgs[fr]) return;
    __ksDone[fr] = true;
    try { __run(__keyProgs[fr]); } catch (e) { playing = false; infoEl.textContent = '关键帧脚本错误: ' + e.message; }
}
function tick(t) {
    requestAnimationFrame(tick);
    if (playing) {
        if (lastT === 0) lastT = t;
        frame += (t - lastT) / 1000 * DATA.fps;
        lastT = t;
        if (frame >= DATA.totalFrames) {
            frame = DATA.loop ? frame % DATA.totalFrames : DATA.totalFrames - 1;
            __ksDone = {};   // 循环回绕：关键帧脚本从头重新执行
        }
        applyFrame(frame);
        __runKeyScripts(Math.floor(frame + 0.5));
        infoEl.textContent = '帧 ' + Math.floor(frame + 0.5) + '/' + DATA.totalFrames;
    }
    // 帧驱动：主脚本（scriptPerFrame）或文档类注册的帧回调（onEnterFrame）任一存在即执行；
    // stop() 只停时间轴（playing=false），类代码/帧代码照常每帧执行（Flash 语义）
    if (DATA.scriptPerFrame || __hasFrameHandler) {
        if (compileErr) { playing = false; infoEl.textContent = '脚本错误: ' + compileErr; }
        else { try { __scriptRun(frame); } catch (e) { playing = false; infoEl.textContent = '脚本错误: ' + e.message; } }
    }
    controls.update();
    renderer.render(scene, camera);
}
requestAnimationFrame(tick);
function toggle() { playing = !playing; lastT = 0; }
window.addEventListener('resize', function () {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
});
</script></body></html>
)HTML";

static std::wstring EscJson(const std::wstring& s) {
    std::wstring r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        wchar_t c = s[i];
        if (c == L'"') r += L"\\\"";
        else if (c == L'\\') r += L"\\\\";
        else if (c == L'\n') r += L"\\n";
        else if (c == L'\r') r += L"\\r";
        else if (c == L'\t') r += L"\\t";
        else r += c;
    }
    return r;
}

static std::wstring BuildThreeJsHtml(const Scene3D& sc) {
    std::wstring data;
    data += L"{\"fps\": "; ExportNum(data, sc.fps);
    data += L", \"totalFrames\": "; ExportNum(data, sc.totalFrames);
    data += L", \"loop\": "; data += (sc.loopPlay ? L"true" : L"false");
    data += L", \"scriptPerFrame\": "; data += (sc.scriptPerFrame ? L"true" : L"false");
    data += L", \"script\": \""; data += EscJson(sc.script); data += L"\"";
    // 文档类脚本（播放开始前执行一次）
    data += L", \"docClass\": \""; data += EscJson(sc.docClass); data += L"\"";
    // 关键帧脚本：按帧聚合（同帧多层脚本以换行合并，进入该帧时执行一次）
    std::map<int, std::wstring> ksMap;
    for (size_t li = 0; li < sc.layers.size(); li++) {
        const std::vector<LayerKey>& lks = sc.layers[li].keys;
        for (size_t ki = 0; ki < lks.size(); ki++) {
            const LayerKey& lk = lks[ki];
            if (lk.script.empty()) continue;
            std::map<int, std::wstring>::iterator it = ksMap.find(lk.frame);
            if (it == ksMap.end()) ksMap[lk.frame] = lk.script;
            else { it->second += L"\n"; it->second += lk.script; }
        }
    }
    data += L", \"keyScripts\": [";
    bool kfFirst = true;
    for (std::map<int, std::wstring>::iterator it = ksMap.begin(); it != ksMap.end(); ++it) {
        if (!kfFirst) data += L", ";
        kfFirst = false;
        data += L"{\"f\": "; ExportNum(data, it->first);
        data += L", \"s\": \""; data += EscJson(it->second); data += L"\"}";
    }
    data += L"]";
    data += L", \"cam\": {\"yaw\": "; ExportNum(data, sc.cam.yaw);
    data += L", \"pitch\": "; ExportNum(data, sc.cam.pitch);
    data += L", \"dist\": "; ExportNum(data, sc.cam.dist);
    data += L", \"tx\": "; ExportNum(data, sc.cam.tx);
    data += L", \"ty\": "; ExportNum(data, sc.cam.ty);
    data += L", \"tz\": "; ExportNum(data, sc.cam.tz);
    data += L"}";
    // 图层关键帧 → 每对象时间线（与编辑器 SetFrame 的整层快照语义一致）：
    // 同一帧出现在多层时保留后面的层（与图层行序自上而下覆盖一致），按帧升序去重
    std::map<const Object3D*, std::vector<KeyFrame>> animMap;
    for (size_t li = 0; li < sc.layers.size(); li++) {
        const std::vector<LayerKey>& lks = sc.layers[li].keys;
        for (size_t ki = 0; ki < lks.size(); ki++) {
            const LayerKey& lk = lks[ki];
            if (!lk.script.empty()) continue;   // 脚本关键帧：由关键帧脚本控制，不参与插值动画（Flash 帧动作语义）
            size_t n = lk.objs.size() < lk.snaps.size() ? lk.objs.size() : lk.snaps.size();
            for (size_t i = 0; i < n; i++) {
                if (!lk.objs[i]) continue;
                KeyFrame kf = lk.snaps[i];
                kf.frame = lk.frame;
                animMap[lk.objs[i]].push_back(kf);
            }
        }
    }
    for (std::map<const Object3D*, std::vector<KeyFrame>>::iterator it = animMap.begin();
         it != animMap.end(); ++it) {
        std::vector<KeyFrame>& v = it->second;
        std::sort(v.begin(), v.end(),
                  [](const KeyFrame& a, const KeyFrame& b) { return a.frame < b.frame; });
        std::vector<KeyFrame> out;
        for (size_t i = 0; i < v.size(); i++) {
            if (!out.empty() && out.back().frame == v[i].frame) out.back() = v[i];
            else out.push_back(v[i]);
        }
        v.swap(out);
    }

    data += L", \"roots\": [";
    for (size_t i = 0; i < sc.roots.size(); i++) {
        if (i) data += L",";
        ExportObjJson(sc.roots[i], data, &animMap);
    }
    data += L"]}";

    std::wstring threeLib, orbitLib, libErr;
    if (!LoadExportLibs(threeLib, orbitLib, libErr)) {
        ::MessageBoxW(NULL, libErr.c_str(), L"Super3D 导出", MB_ICONWARNING);
        return L"";
    }
    EscapeScriptClose(threeLib);
    EscapeScriptClose(orbitLib);
    std::wstring html(kExportHtmlHead);
    size_t pos = html.find(L"@THREE_LIB@");
    if (pos != std::wstring::npos) html.replace(pos, 11, threeLib);
    pos = html.find(L"@ORBIT_LIB@");
    if (pos != std::wstring::npos) html.replace(pos, 11, orbitLib);
    pos = html.find(L"@DATA@");
    if (pos != std::wstring::npos) html.replace(pos, 6, data);
    html += kExportHtmlTail;
    html += kExportScript1a;
    html += kExportScript1a2;
    html += kExportScript1b;
    html += kExportScript1b2;
    html += kExportScript1b3;
    html += kExportScript2a;
    html += kExportScript2a2;
    html += kExportScript2b;
    html += kExportScript2b2;
    html += kExportScript3;
    html += kExportHtmlTailB;
    return html;
}

void CMainWnd::OnExport() {
    Stop();
    CFileDialog dlg(FALSE, _T("html"), _T("Super3D场景_threejs.html"),
        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
        _T("Three.js 网页 (*.html)|*.html|所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;
    std::wstring outPath = (LPCTSTR)dlg.GetPathName();
    if (!ExportToFile(outPath)) {
        AfxMessageBox(_T("导出失败：缺少导出库文件，或无法写入文件。"), MB_ICONERROR);
        return;
    }
    CString msg;
    msg.Format(_T("已导出 Three.js 网页：\r\n%s\r\n\r\n双击用浏览器打开即可查看、旋转和播放模型。\r\n网页已内嵌 Three.js 库，完全离线可用。"), outPath.c_str());
    AfxMessageBox(msg, MB_ICONINFORMATION);
    ShellExecuteW(NULL, L"open", outPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

bool CMainWnd::ExportToFile(const std::wstring& path) {
    std::wstring html = BuildThreeJsHtml(m_scene);
    if (html.empty()) return false;   // 缺导出库文件（已弹具体提示）
    std::string utf8 = DbgWToUtf8(html);
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) return false;
    fwrite(utf8.data(), 1, utf8.size(), f);
    fclose(f);
    return true;
}
