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
// MainWnd.cpp : 主窗口实现
#include "stdafx.h"
#include "MainWnd.h"
#include "Examples.h"
#include "ProjectIO.h"
#include "Renderer.h"
#include <map>
#include <algorithm>
#include <cstdlib>   // strtod（MCP points 参数解析）
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

#define WM_LIGHT_REFRESH (WM_USER + 200)

enum {
    IDC_BTN_NEW = 2000, IDC_BTN_OPEN, IDC_BTN_SAVE, IDC_BTN_SAVE_AS, IDC_BTN_UNDO,
    IDC_BTN_GROUP, IDC_BTN_UNGROUP, IDC_BTN_BOX, IDC_BTN_SPHERE,
    IDC_BTN_CYLINDER, IDC_BTN_CONE, IDC_BTN_TORUS, IDC_BTN_PLANE,
    IDC_BTN_TOOL_SELECT, IDC_BTN_TOOL_MOVE, IDC_BTN_TOOL_ROTATE, IDC_BTN_TOOL_SCALE,
    IDC_BTN_TOOL_PAN, IDC_BTN_TOOL_SPLINE,
    IDC_BTN_LEAF,
    IDC_BTN_TEXT3D,
    IDC_BTN_TUBE,
    IDC_BTN_RTUBE,
    IDC_BTN_ROUNDBOX, IDC_BTN_CAPSULE, IDC_BTN_HEMISPHERE,
    IDC_BTN_LATHE, IDC_BTN_EXTRUDE,
    IDC_BTN_DUPLICATE, IDC_BTN_CUT, IDC_BTN_DELETE,
    IDC_BTN_ADDKEY, IDC_BTN_DELKEY,
    IDC_BTN_PLAY, IDC_BTN_STOP,
    IDC_BTN_EX0, IDC_BTN_EX1, IDC_BTN_EX2, IDC_BTN_EX3, IDC_BTN_EX4, IDC_BTN_EX5, IDC_BTN_EX6,
    IDC_BTN_EX7,
    IDC_BTN_RESETVIEW, IDC_BTN_RUN, IDC_BTN_DEBUG, IDC_BTN_EXPORT, IDC_BTN_HELP,
    IDC_MENU_LIGHT,     // 视图菜单：灯光设置
    IDC_BTN_SUPERDOC,   // 帮助菜单：SuperScript 文档（打开 SuperScript参考.html）
    IDC_BTN_ABOUT,      // 帮助菜单：版权与许可证（GPL-3.0 声明）
    IDC_BTN_DOC_CLASS,  // 文件菜单：文档类脚本（整个工程的初始化脚本）
    IDC_OPEN_SCRIPT = 2100,
    IDC_CMD_TOMC,         // 右键：转换为 MovieClip
    IDC_CMD_PASTE,        // 右键：粘贴（剪贴板）
    IDC_CMD_BOOLEAN,      // 右键：布尔差集（A−B 挖孔）
    IDC_CMD_BOOLUNION,    // 右键：布尔并集（A∪B）
    IDC_CMD_BOOLINTER,    // 右键：布尔交集（A∩B）
    // 元件对齐：世界坐标对齐（子菜单 9 项，单击即对齐）
    // axis = (nID - IDC_ALG_X_MIN)/3（0=X 1=Y 2=Z）；mode = (nID - IDC_ALG_X_MIN)%3（0=起始边 1=中心 2=终止边）
    IDC_ALG_X_MIN, IDC_ALG_X_CEN, IDC_ALG_X_MAX,   // X 轴：起始边/中心/终止边
    IDC_ALG_Y_MIN, IDC_ALG_Y_CEN, IDC_ALG_Y_MAX,   // Y 轴：起始边/中心/终止边
    IDC_ALG_Z_MIN, IDC_ALG_Z_CEN, IDC_ALG_Z_MAX,   // Z 轴：起始边/中心/终止边
    IDC_ALG_X_DIST, IDC_ALG_Y_DIST, IDC_ALG_Z_DIST,   // X/Y/Z 轴平均分布（等空隙重排）
    IDC_BTN_REDO
};

struct ToolBtn { UINT id; const wchar_t* label; int row; };
static const ToolBtn kToolBtns[] = {
    { IDC_BTN_NEW, L"新建", 0 },   { IDC_BTN_OPEN, L"打开", 0 },  { IDC_BTN_SAVE, L"保存", 0 },
    { IDC_BTN_UNDO, L"撤销", 0 },  { IDC_BTN_REDO, L"重做", 0 },
    { IDC_BTN_TOOL_SELECT, L"选择", 0 }, { IDC_BTN_TOOL_MOVE, L"移动", 0 },
    { IDC_BTN_TOOL_ROTATE, L"旋转", 0 }, { IDC_BTN_TOOL_SCALE, L"缩放", 0 },
    { IDC_BTN_TOOL_PAN, L"平移视角", 0 },
    { IDC_BTN_TOOL_SPLINE, L"曲线", 0 },
    { IDC_BTN_DUPLICATE, L"复制", 0 },   { IDC_BTN_CUT, L"剪切", 0 }, { IDC_CMD_PASTE, L"粘贴", 0 }, { IDC_BTN_DELETE, L"删除", 0 },
    { IDC_BTN_ADDKEY, L"插关键帧", 0 },  { IDC_BTN_DELKEY, L"删关键帧", 0 },
    { IDC_BTN_PLAY, L"播放", 0 },        { IDC_BTN_STOP, L"停止", 0 },
    { IDC_BTN_RESETVIEW, L"恢复默认视角", 0 },
    { IDC_BTN_GROUP, L"组", 1 },         { IDC_BTN_UNGROUP, L"打散", 1 },
    { IDC_BTN_BOX, L"立方体", 1 },
    { IDC_BTN_SPHERE, L"球", 1 },        { IDC_BTN_CYLINDER, L"圆柱", 1 },
    { IDC_BTN_CONE, L"圆锥", 1 },        { IDC_BTN_TORUS, L"圆环", 1 },
    { IDC_BTN_PLANE, L"平面", 1 },
    { IDC_BTN_LEAF, L"绿叶", 1 },
    { IDC_BTN_TEXT3D, L"文本", 1 },
    { IDC_BTN_TUBE, L"方管", 1 },
    { IDC_BTN_RTUBE, L"圆管", 1 },
    { IDC_BTN_ROUNDBOX, L"圆角盒", 1 },
    { IDC_BTN_CAPSULE, L"胶囊", 1 },
    { IDC_BTN_HEMISPHERE, L"半球", 1 },
    { IDC_BTN_LATHE, L"车削", 1 },
    { IDC_BTN_EXTRUDE, L"拉伸", 1 },
    { IDC_BTN_EX0, L"House", 1 },       { IDC_BTN_EX1, L"Windmill", 1 },
    { IDC_BTN_EX2, L"SolarSystem", 1 }, { IDC_BTN_EX3, L"BounceBall", 1 },
    { IDC_BTN_EX4, L"DocClass", 1 },    { IDC_BTN_EX5, L"TankBattle", 1 },
    { IDC_BTN_EX6, L"Willow", 1 },  { IDC_BTN_EX7, L"Rocket", 1 },
    { IDC_BTN_RUN, L"运行脚本", 1 },  { IDC_BTN_DEBUG, L"调试", 1 },
    { IDC_BTN_EXPORT, L"导出", 1 },
    { IDC_BTN_HELP, L"帮助", 1 }
};
static const int kBtnCount = sizeof(kToolBtns) / sizeof(kToolBtns[0]);

// 示例菜单动态项 ID 基址：扫描 sample\*.s3d 后按序分配（避开 9000 区面板/编辑框控件 ID）
static const UINT kSampleMenuBase = 9400;
static const UINT kSampleMenuMax = kSampleMenuBase + 63;   // 最多 64 个示例

// 左侧图标工具栏（Flash 常用工具，竖排；其余功能进菜单）
// 按功能分三组：图元（基本体/曲面装饰）、工具（选择/变换/视角）、控制（播放/停止），
// 组与组之间在布局时留出加大的间隔并绘制分隔线（见 OnSize / OnEraseBkgnd）
// 顺序：图元组（立方体…文本，圆管紧随圆环、方管在后）在前，其次是选择/移动/旋转/缩放/平移，
//       最后是播放/停止
static const UINT kSideBarIds[] = {
    // 图元组（基本体在前，圆管/方管相邻，曲面装饰在后，圆角盒/胶囊/半球为扩展基本体）
    IDC_BTN_BOX, IDC_BTN_SPHERE, IDC_BTN_CYLINDER, IDC_BTN_CONE, IDC_BTN_TORUS,
    IDC_BTN_RTUBE, IDC_BTN_TUBE, IDC_BTN_PLANE, IDC_BTN_TOOL_SPLINE, IDC_BTN_LEAF, IDC_BTN_TEXT3D,
    IDC_BTN_ROUNDBOX, IDC_BTN_CAPSULE, IDC_BTN_HEMISPHERE,
    IDC_BTN_LATHE, IDC_BTN_EXTRUDE,
    // 工具组（选择/移动/旋转/缩放/平移）
    IDC_BTN_TOOL_SELECT, IDC_BTN_TOOL_MOVE, IDC_BTN_TOOL_ROTATE, IDC_BTN_TOOL_SCALE,
    IDC_BTN_TOOL_PAN,
    // 控制组
    IDC_BTN_PLAY, IDC_BTN_STOP
};
// 与 kSideBarIds 一一对应：true = 该项是某一功能组的第一项（前面画加大的分组间隔+分隔线）
static const bool kSideGroupStart[] = {
    true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    true, false, false, false, false,
    true, false
};
// 与 kSideBarIds 一一对应的悬停提示文本
static const wchar_t* kSideTips[] = {
    L"添加立方体", L"添加球体", L"添加圆柱", L"添加圆锥", L"添加圆环",
    L"添加圆管", L"添加方管", L"添加平面", L"绘制曲线", L"添加绿叶", L"添加文本",
    L"添加圆角长方体", L"添加胶囊（圆柱段+两半球）", L"添加半球/球冠",
    L"添加车削回转体（母线绕轴：喷管/瓶碗/螺栓）", L"添加拉伸体（截面沿轴拉伸：齿轮/型材/导轨）",
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
    ON_MESSAGE(WM_APP_MCP_CMD, &CMainWnd::OnMcpCmd)   // MCP 命令（HTTP 线程投递）
    ON_NOTIFY(TCN_SELCHANGE, 9000, &CMainWnd::OnTabSelChange)
    ON_NOTIFY(TCN_SELCHANGE, 9010, &CMainWnd::OnDocTabSelChange)
    ON_NOTIFY_EX(TTN_GETDISPINFO, 0, &CMainWnd::OnDocTabTip)
    ON_MESSAGE(WM_USER + 55, &CMainWnd::OnDocTabCloseMsg)
    ON_COMMAND(IDC_BTN_NEW, &CMainWnd::OnNew)
    ON_COMMAND(IDC_BTN_OPEN, &CMainWnd::OnOpen)
    ON_COMMAND(IDC_BTN_SAVE, &CMainWnd::OnSave)
    ON_COMMAND(IDC_BTN_SAVE_AS, &CMainWnd::OnSaveAs)
    ON_COMMAND(IDC_BTN_UNDO, &CMainWnd::OnUndo)
    ON_COMMAND(IDC_BTN_REDO, &CMainWnd::OnRedo)
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
    ON_COMMAND(IDC_BTN_ROUNDBOX, &CMainWnd::OnRoundBox)
    ON_COMMAND(IDC_BTN_CAPSULE, &CMainWnd::OnCapsule)
    ON_COMMAND(IDC_BTN_HEMISPHERE, &CMainWnd::OnHemisphere)
    ON_COMMAND(IDC_BTN_LATHE, &CMainWnd::OnLathe)
    ON_COMMAND(IDC_BTN_EXTRUDE, &CMainWnd::OnExtrude)
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
    ON_COMMAND(IDC_BTN_EX6, &CMainWnd::OnEx6)
    ON_COMMAND(IDC_BTN_EX7, &CMainWnd::OnEx7)
    ON_COMMAND_RANGE(kSampleMenuBase, kSampleMenuMax, &CMainWnd::OnSampleMenuItem)
    ON_COMMAND(IDC_BTN_RESETVIEW, &CMainWnd::OnResetView)
    ON_COMMAND(IDC_MENU_LIGHT, &CMainWnd::OnLightSettings)
    ON_MESSAGE(WM_LIGHT_REFRESH, &CMainWnd::OnLightRefresh)
    ON_COMMAND(IDC_BTN_RUN, &CMainWnd::OnRun)
    ON_COMMAND(IDC_BTN_DEBUG, &CMainWnd::OnDebug)
    ON_COMMAND(IDC_BTN_EXPORT, &CMainWnd::OnExport)
    ON_COMMAND(IDC_BTN_HELP, &CMainWnd::OnHelp)
    ON_COMMAND(IDC_BTN_SUPERDOC, &CMainWnd::OnSuperScriptDoc)
    ON_COMMAND(IDC_BTN_ABOUT, &CMainWnd::OnAbout)
    ON_COMMAND(IDC_BTN_DOC_CLASS, &CMainWnd::OnDocClassScript)
    ON_COMMAND(IDC_OPEN_SCRIPT, &CMainWnd::ShowScriptWindow)
    ON_COMMAND(IDC_CMD_TOMC, &CMainWnd::OnWrapToMovieClip)
    ON_COMMAND(IDC_CMD_PASTE, &CMainWnd::OnPaste)
    ON_COMMAND(IDC_CMD_BOOLEAN, &CMainWnd::OnBooleanHole)
    ON_COMMAND(IDC_CMD_BOOLUNION, &CMainWnd::OnBooleanUnion)
    ON_COMMAND(IDC_CMD_BOOLINTER, &CMainWnd::OnBooleanInter)
    ON_COMMAND_RANGE(IDC_ALG_X_MIN, IDC_ALG_Z_DIST, &CMainWnd::OnAlignMenu)
    ON_WM_DRAWITEM()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_WM_LBUTTONUP()
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
END_MESSAGE_MAP()

CMainWnd::CMainWnd() : m_selected(NULL), m_tool(TOOL_SELECT), m_playing(false),
    m_tlStopped(false), m_syntaxDirty(false), m_bUpdatingScript(false), m_showWire(false), m_navInside(false),
    m_hIconSmall(NULL), m_hIconLarge(NULL), m_lastKeyScriptFrame(-1),
    m_docScriptMode(false), m_docSSOpen(true) {}

CMainWnd::~CMainWnd() {
    ClearClipboard();
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
                          FIXED_PITCH | FF_MODERN, _T("Consolas"));   // 紧凑等宽字体（Courier New 偏宽）
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
    m_status.ModifyStyleEx(0, WS_EX_COMPOSITED);   // 合成重绘（双缓冲）：文字变化时不闪烁

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
    // MCP 服务器：AI 可通过 localhost:8765 直接驱动本编辑器建模（Streamable HTTP + JSON-RPC）
    if (!m_mcp.Start(8765, this)) {
        TRACE(_T("[Super3D] MCP 服务器启动失败（端口 8765 可能被占用）\n"));
    }
    return 0;
}

void CMainWnd::OnDestroy() {
    m_mcp.Stop();
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

// 静态文本控件（状态栏等）返回与主窗口背景同色画刷 + 透明文字背景：
// SetWindowText 重绘时不再以系统默认白底擦除，消除状态栏逐帧刷新闪烁
HBRUSH CMainWnd::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH hbr = (HBRUSH)CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
    if (nCtlColor == CTLCOLOR_STATIC) {
        pDC->SetBkMode(TRANSPARENT);
        static CBrush s_bg(RGB(240, 241, 245));   // 与 OnEraseBkgnd 主背景一致
        return (HBRUSH)s_bg.GetSafeHandle();
    }
    return hbr;
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
    // .ss 文档类编辑页：Ctrl+A/C/V/X/Z/Y 显式作用于脚本编辑器。
    // CEdit 原生不支持 Ctrl+A（会响蜂鸣），需自处理；其余显式发送命令保证焦点不在编辑框时也生效
    if (m_docScriptMode && pMsg->message == WM_KEYDOWN &&
        (GetKeyState(VK_CONTROL) & 0x8000)) {
        switch (pMsg->wParam) {
            case 'A': m_docEdt.SetFocus(); m_docEdt.SetSel(0, -1); return TRUE;
            case 'C': m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_COPY); return TRUE;
            case 'V': m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_PASTE); return TRUE;
            case 'X': m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_CUT); return TRUE;
            case 'Z': {   // 着色（SetSelectionCharFormat）会写入撤销栈，Ctrl+Z 可能先撤销
                         // "颜色修改"而非用户输入，表现为"撤销没反应"。循环撤销直到文本
                         // 真正变化（跳过低级格式撤销），上限 20 次防极端死循环
                m_docEdt.SetFocus();
                CStringW before;
                m_docEdt.GetWindowText(before);
                for (int k = 0; k < 20; k++) {
                    m_docEdt.SendMessage(WM_UNDO);
                    CStringW after;
                    m_docEdt.GetWindowText(after);
                    if (after != before) break;
                }
                return TRUE;
            }
            case 'Y': {   // 重做同理：跳过"格式重做"，只回退到文本变化
                m_docEdt.SetFocus();
                CStringW before;
                m_docEdt.GetWindowText(before);
                for (int k = 0; k < 20; k++) {
                    m_docEdt.SendMessage(EM_REDO);
                    CStringW after;
                    m_docEdt.GetWindowText(after);
                    if (after != before) break;
                }
                return TRUE;
            }
        }
    }
    // 全局快捷键：Ctrl+F 格式化 .ss / Ctrl+S 保存 / Ctrl+O 打开 / Ctrl+N 新建 / Ctrl+Z 撤销 / Ctrl+Y 重做
    //（帮助文档已声明；.ss 页保存当前文件；焦点在文本框/编辑框时 Ctrl+Z/Y 保留系统文本撤销）
    if (pMsg->message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) &&
        !(GetKeyState(VK_MENU) & 0x8000)) {
        switch (pMsg->wParam) {
            case 'F': DoFormatDoc(); return TRUE;
            case 'S': DoSave(); return TRUE;
            case 'O': DoOpen(); return TRUE;
            case 'N': DoNew(); return TRUE;
            case 'Z': {
                CWnd* f = GetFocus();
                if (f && f->IsKindOf(RUNTIME_CLASS(CEdit))) break;   // 编辑框内：系统文本撤销
                DoUndo(); return TRUE;
            }
            case 'Y': {
                CWnd* f = GetFocus();
                if (f && f->IsKindOf(RUNTIME_CLASS(CEdit))) break;
                DoRedo(); return TRUE;
            }
        }
    }
    // Ctrl+X：统一“剪切”。
    //  · 焦点在可编辑文本框（属性面板数值框、关键帧脚本、.ss 编辑器等）→ 剪切选中文字；
    //  · 焦点在只读输出框（CEditCtxMenu：输出/编译错误）→ 不拦截，由控件自身 OnKeyDown 剪切；
    //  · 焦点在视口/场景树/时间轴等非文本处，且有选中元件 → 剪切元件（存入内部剪贴板，可粘贴）。
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'X' &&
        (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
        CWnd* f = GetFocus();
        if (f && f->IsKindOf(RUNTIME_CLASS(CRichEditCtrl))) {
            f->SendMessage(WM_CUT);
            return TRUE;
        }
        if (f && f->IsKindOf(RUNTIME_CLASS(CEdit))) {
            if (!(f->GetStyle() & ES_READONLY)) {
                f->SendMessage(WM_CUT);
                return TRUE;
            }
            // 只读输出框：交给控件自身（CEditCtxMenu::OnKeyDown）处理
        } else if (!m_scene.editMeshObj && (m_selected || SelectionCount() > 0)) {
            OnCut();
            return TRUE;
        }
    }
    // Ctrl+C：统一“复制”。
    //  · 焦点在可编辑文本框（属性面板数值框、脚本、.ss 等）→ 复制选中文字；
    //  · 焦点在只读输出框（输出/编译错误）→ 不拦截，由控件自身处理；
    //  · 焦点在视口/场景树/时间轴等非文本处，且有选中元件 → 复制元件到内部剪贴板（可多选）。
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'C' &&
        (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
        CWnd* f = GetFocus();
        if (f && f->IsKindOf(RUNTIME_CLASS(CRichEditCtrl))) {
            f->SendMessage(WM_COPY);
            return TRUE;
        }
        if (f && f->IsKindOf(RUNTIME_CLASS(CEdit))) {
            if (!(f->GetStyle() & ES_READONLY)) {
                f->SendMessage(WM_COPY);
                return TRUE;
            }
            // 只读输出框：交给控件自身（CEditCtxMenu::OnKeyDown）处理
        } else if (!m_scene.editMeshObj && (m_selected || SelectionCount() > 0)) {
            DoDuplicate();
            return TRUE;
        }
    }
    // Ctrl+V：统一“粘贴”。
    //  · 焦点在可编辑文本框 → 粘贴系统剪贴板文字；
    //  · 焦点在只读输出框 → 不拦截，由控件自身处理；
    //  · 焦点在视口/场景树/时间轴等非文本处，且内部剪贴板非空 → 粘贴元件副本（可多选）。
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'V' &&
        (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
        CWnd* f = GetFocus();
        if (f && f->IsKindOf(RUNTIME_CLASS(CRichEditCtrl))) {
            f->SendMessage(WM_PASTE);
            return TRUE;
        }
        if (f && f->IsKindOf(RUNTIME_CLASS(CEdit))) {
            if (!(f->GetStyle() & ES_READONLY)) {
                f->SendMessage(WM_PASTE);
                return TRUE;
            }
            // 只读输出框：交给控件自身处理
        } else if (!m_scene.editMeshObj && !m_clipboard.empty()) {
            OnPaste();
            return TRUE;
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
                            {IDC_BTN_SAVE, L"保存"}, {IDC_BTN_SAVE_AS, L"另存为"}, {IDC_BTN_EXPORT, L"导出"} }, true);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("文件(&F)")); f.Detach();
    }
    {   // 编辑
        CMenu f; popup(f, { {IDC_BTN_UNDO, L"撤销\tCtrl+Z"}, {IDC_BTN_REDO, L"重做\tCtrl+Y"},
                            {IDC_BTN_DUPLICATE, L"复制\tCtrl+C"},
                            {IDC_BTN_CUT, L"剪切\tCtrl+X"},
                            {IDC_CMD_PASTE, L"粘贴\tCtrl+V"},
                            {IDC_BTN_DELETE, L"删除\tDel"},
                            {IDC_BTN_GROUP, L"组合"},
                            {IDC_BTN_UNGROUP, L"打散"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("编辑(&E)")); f.Detach();
    }
    {   // 视图
        CMenu f; popup(f, { {IDC_BTN_RESETVIEW, L"恢复默认视角"}, {IDC_BTN_TOOL_PAN, L"平移视角"} }, false);
        f.AppendMenu(MF_SEPARATOR);
        f.AppendMenu(MF_STRING, IDC_MENU_LIGHT, L"灯光设置...");
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("视图(&V)")); f.Detach();
    }
    {   // 插入
        CMenu f; popup(f, { {IDC_BTN_BOX, L"立方体"}, {IDC_BTN_SPHERE, L"球"},
                            {IDC_BTN_CYLINDER, L"圆柱"}, {IDC_BTN_CONE, L"圆锥"},
                            {IDC_BTN_TORUS, L"圆环"}, {IDC_BTN_PLANE, L"平面"},
                            {IDC_BTN_LEAF, L"绿叶"}, {IDC_BTN_TOOL_SPLINE, L"曲线"},
                            {IDC_BTN_TEXT3D, L"文本"}, {IDC_BTN_TUBE, L"方管"},
                            {IDC_BTN_RTUBE, L"圆管"}, {IDC_BTN_LATHE, L"车削"},
                            {IDC_BTN_EXTRUDE, L"拉伸"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("插入(&I)")); f.Detach();
    }
    {   // 对齐：按 XYZ 轴归类的子菜单，每个轴下含 3 种对齐 + 平均分布
        CMenu f;
        CMenu ax, ay, az;
        popup(ax, { {IDC_ALG_X_MIN, L"起始边(左)对齐"}, {IDC_ALG_X_CEN, L"中心对齐"},
                    {IDC_ALG_X_MAX, L"终止边(右)对齐"} }, false);
        ax.AppendMenu(MF_SEPARATOR);
        ax.AppendMenu(MF_STRING, IDC_ALG_X_DIST, L"平均分布");
        popup(ay, { {IDC_ALG_Y_MIN, L"起始边(顶)对齐"}, {IDC_ALG_Y_CEN, L"中心对齐"},
                    {IDC_ALG_Y_MAX, L"终止边(底)对齐"} }, false);
        ay.AppendMenu(MF_SEPARATOR);
        ay.AppendMenu(MF_STRING, IDC_ALG_Y_DIST, L"平均分布");
        popup(az, { {IDC_ALG_Z_MIN, L"起始边(前)对齐"}, {IDC_ALG_Z_CEN, L"中心对齐"},
                    {IDC_ALG_Z_MAX, L"终止边(后)对齐"} }, false);
        az.AppendMenu(MF_SEPARATOR);
        az.AppendMenu(MF_STRING, IDC_ALG_Z_DIST, L"平均分布");
        f.CreatePopupMenu();
        f.AppendMenu(MF_POPUP, (UINT_PTR)ax.m_hMenu, _T("X 轴")); ax.Detach();
        f.AppendMenu(MF_POPUP, (UINT_PTR)ay.m_hMenu, _T("Y 轴")); ay.Detach();
        f.AppendMenu(MF_POPUP, (UINT_PTR)az.m_hMenu, _T("Z 轴")); az.Detach();
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("对齐(&Q)")); f.Detach();
    }
    {   // 控制
        CMenu f; popup(f, { {IDC_BTN_PLAY, L"播放"}, {IDC_BTN_STOP, L"停止"},
                            {IDC_BTN_DEBUG, L"调试"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("控制(&O)")); f.Detach();
    }
    {   // 脚本：文档类 + 关键帧脚本 + 运行脚本
        CMenu f; popup(f, { {IDC_BTN_DOC_CLASS, L"文档类"}, {IDC_OPEN_SCRIPT, L"关键帧脚本"},
                            {IDC_BTN_RUN, L"运行脚本"} }, false);
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("脚本(&J)")); f.Detach();
    }
    {   // 示例：动态扫描 sample\*.s3d（内置示例已全部文件化；以后新增示例只需把 .s3d/.ss 放入 sample 目录）
        ScanSampleFiles();
        CMenu f; f.CreatePopupMenu();
        if (m_sampleFiles.empty()) {
            f.AppendMenu(MF_STRING | MF_GRAYED, 0, L"（sample 目录暂无示例）");
        } else {
            for (size_t i = 0; i < m_sampleFiles.size(); i++) {
                std::wstring fn = m_sampleFiles[i];
                size_t sp = fn.find_last_of(L"/\\");
                if (sp != std::wstring::npos) fn = fn.substr(sp + 1);
                size_t dot = fn.find_last_of(L'.');
                if (dot != std::wstring::npos) fn = fn.substr(0, dot);   // 显示名去掉 .s3d
                f.AppendMenu(MF_STRING, kSampleMenuBase + (UINT)i, fn.c_str());
            }
        }
        top.AppendMenu(MF_POPUP, (UINT_PTR)f.m_hMenu, _T("示例(&S)")); f.Detach();
    }
    {   // 帮助：使用说明 + SuperScript 文档
        CMenu f; popup(f, { {IDC_BTN_HELP, L"使用说明"}, {IDC_BTN_SUPERDOC, L"SuperScript文档"}, {IDC_BTN_ABOUT, L"版权与许可证"} }, false);
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
    auto fillRoundRect = [&](int l, int t, int r, int b, int ew, int eh, COLORREF fill) {
        CBrush br(fill); CBrush* ob = dc->SelectObject(&br);
        dc->RoundRect(l, t, r, b, ew, eh);
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
    case IDC_BTN_ROUNDBOX: {      // 圆角长方体：圆角方块 + 顶面
        POINT top[] = { {cx - 13, cy - 7}, {cx - 2, cy - 12}, {cx + 13, cy - 7}, {cx + 3, cy - 2} };
        fillPoly(top, 4, f3);
        fillRoundRect(cx - 13, cy - 3, cx + 12, cy + 11, 8, 8, f2);
        break;
    }
    case IDC_BTN_CAPSULE: {       // 胶囊：竖直药丸（两端半圆）+ 中间拼缝
        fillRoundRect(cx - 8, cy - 13, cx + 8, cy + 13, 16, 16, f2);
        dc->MoveTo(cx - 8, cy); dc->LineTo(cx + 8, cy);
        break;
    }
    case IDC_BTN_HEMISPHERE: {    // 半球：圆顶 + 平底（背景色盖住下半圆）
        fillEllipse(cx - 14, cy - 13, cx + 14, cy + 13, f2);
        fillRect(cx - 15, cy, cx + 15, cy + 14, bgFill);
        dc->MoveTo(cx - 14, cy); dc->LineTo(cx + 14, cy);
        break;
    }
    case IDC_BTN_LATHE: {         // 车削：旋转轴 + 半边母线轮廓（瓶形）
        POINT prof[] = { {cx - 10, cy + 11}, {cx + 4, cy + 11}, {cx + 7, cy + 5}, {cx + 6, cy},
                         {cx - 1, cy - 5}, {cx - 2, cy - 10}, {cx + 1, cy - 12}, {cx - 10, cy - 12} };
        fillPoly(prof, 8, f2);
        dc->Polygon(prof, 8);
        dc->MoveTo(cx - 10, cy - 14); dc->LineTo(cx - 10, cy + 14);   // 旋转轴
        break;
    }
    case IDC_BTN_EXTRUDE: {       // 挤出：前六角截面 + 厚度错位面（齿轮坯）
        POINT h0[6], h1[6];
        for (int i = 0; i < 6; i++) {
            double a = 3.14159265358979323846 * 2.0 * i / 6.0 - 3.14159265358979323846 / 2.0;
            h0[i].x = cx - 2 + (LONG)(std::cos(a) * 10); h0[i].y = cy + 2 + (LONG)(std::sin(a) * 10);
            h1[i].x = h0[i].x - 4; h1[i].y = h0[i].y - 5;
        }
        fillPoly(h1, 6, f1);                          // 厚度面（先画，深色）
        for (int i = 0; i < 6; i++) { dc->MoveTo(h0[i]); dc->LineTo(h1[i]); }
        fillPoly(h0, 6, f2);                          // 前截面
        dc->Polygon(h0, 6);
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
    case IDC_CMD_PASTE: {       // 粘贴：剪贴板
        dc->Rectangle(cx - 9, cy - 7, cx + 9, cy + 11);      // 板
        dc->Rectangle(cx - 4, cy - 11, cx + 4, cy - 6);      // 顶部夹子
        dc->MoveTo(cx - 5, cy - 1); dc->LineTo(cx + 5, cy - 1);
        dc->MoveTo(cx - 5, cy + 3); dc->LineTo(cx + 5, cy + 3);
        dc->MoveTo(cx - 5, cy + 7); dc->LineTo(cx + 2, cy + 7);
        break;
    }
    case IDC_BTN_UNDO: {          // 撤销：左弯箭头
        dc->MoveTo(cx + 12, cy - 6); dc->LineTo(cx + 12, cy + 2);
        dc->Arc(cx - 6, cy - 9, cx + 12, cy + 9, cx + 12, cy + 2, cx - 6, cy + 2);
        dc->MoveTo(cx - 8, cy + 2); dc->LineTo(cx - 1, cy - 5);
        dc->MoveTo(cx - 8, cy + 2); dc->LineTo(cx - 1, cy + 8);
        break;
    }
    case IDC_BTN_REDO: {          // 重做：右弯箭头（与撤销镜像）
        dc->MoveTo(cx - 12, cy - 6); dc->LineTo(cx - 12, cy + 2);
        dc->Arc(cx - 12, cy - 9, cx + 6, cy + 9, cx - 12, cy + 2, cx + 6, cy + 2);
        dc->MoveTo(cx + 8, cy + 2); dc->LineTo(cx + 1, cy - 5);
        dc->MoveTo(cx + 8, cy + 2); dc->LineTo(cx + 1, cy + 8);
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
                m_pendingTrace += m_docScript.GetLog();   // 当帧 trace 并入待显示
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
                m_pendingTrace += m_script.GetLog();   // 当帧 trace 并入待显示
            }
        }
        AppendTraceLive();      // 边运行边把新增 trace 追加到"输出"页（节流防闪烁）
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
    // 重新播放：trace 显示从头收集（实时追加"输出"页）
    m_pendingTrace.clear();
    m_lastTraceFlush = 0;
    m_traceSwitched = false;
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
            // 收集文档类 main() 的 trace 输出（实时追加"输出"页）
            if (!m_docScript.GetLog().empty()) m_pendingTrace += m_docScript.GetLog();
            AppendTraceLive(true);
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
    AppendTraceLive(true);   // 停止播放：把节流期间剩余待显示的 trace 一次刷出
    UpdateStatus();
}

// Flash 式"输出"页：播放期间把各脚本每帧 trace 输出的待显示文本实时追加到顶部"输出"页。
// 各脚本日志每帧（RunFrame/ResetFrameTemp）清空、GetLog 只返回当帧输出，故在每帧执行处把
// GetLog 直接并入 m_pendingTrace；此处只做"待显示 → 控件"的追加，按 120ms 节流防逐帧闪烁。
void CMainWnd::AppendTraceLive(bool force) {
    if (m_pendingTrace.empty()) return;
    // 节流：非强制刷新且距上次不足 120ms 时暂缓，待下帧合并追加（不丢内容）
    DWORD now = ::GetTickCount();
    if (!force && m_lastTraceFlush && now - m_lastTraceFlush < 120) return;
    m_lastTraceFlush = now;
    // 追加到"输出"页并滚动到末尾
    CString cur; m_traceEdt.GetWindowText(cur);
    m_traceEdt.SetWindowText(cur + CString(m_pendingTrace.c_str()));
    m_pendingTrace.clear();
    wchar_t b[64];
    swprintf(b, 64, L"输出 %d 行", m_traceEdt.GetLineCount());
    m_traceStat.SetWindowText(b);
    m_traceEdt.LineScroll(m_traceEdt.GetLineCount());
    // 首次有输出时自动切到"输出"页（本次无编译错误时）；之后保持当前页不打扰
    if (!m_traceSwitched) {
        m_traceSwitched = true;
        CString errTxt; m_errEdt.GetWindowText(errTxt);
        if (errTxt.IsEmpty()) { m_tab.SetCurSel(1); ShowTabPage(1); }
    }
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

// 批量替换整个选中集（框选用）：一次完成标记 + 一次 UI 刷新，
// 避免逐条 AddToSelection 反复重建场景树导致闪烁
void CMainWnd::SetSelectionSet(const std::vector<Object3D*>& objs, Object3D* primary) {
    std::vector<Object3D*> all;
    m_scene.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) all[i]->selected = false;
    m_selSet.clear();
    m_selSet = objs;
    for (size_t i = 0; i < m_selSet.size(); i++)
        if (m_selSet[i]) m_selSet[i]->selected = true;
    m_selected = primary;
    m_selName = primary ? primary->name : L"";
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
    // 关键帧语义：先把成员从各层关键帧记录中移除（此时它们仍是根），再移入组，
    // 最后把新组加入当前帧关键帧——否则该层当前关键帧仍记录"球/立方体"而新组未入帧，
    // 渲染按根判定可见性 → 组不在关键帧内容中 → 整个组连同成员一起消失。
    for (size_t i = 0; i < items.size(); i++)
        m_scene.RemoveObjectFromAllLayerKeys(items[i]);
    // 成员移入组并保持世界位置（组仅平移、无旋转缩放，位置/关键帧换算完全精确）
    for (size_t i = 0; i < items.size(); i++)
        m_scene.MoveInto(items[i], g);
    m_scene.AddNewObjectToCurrentKey(g);
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
    menu.AppendMenu(MF_STRING | selEn, IDC_BTN_DUPLICATE, L"复制\tCtrl+C");
    menu.AppendMenu(MF_STRING | selEn, IDC_BTN_CUT, L"剪切\tCtrl+X");
    menu.AppendMenu(MF_STRING | (m_clipboard.empty() ? MF_GRAYED : 0), IDC_CMD_PASTE, L"粘贴\tCtrl+V");
    menu.AppendMenu(MF_STRING | selEn, IDC_BTN_DELETE, L"删除\tDel");
    UINT ungroupEn = (m_selected && m_selected->type == PT_NONE) ? 0 : MF_GRAYED;
    menu.AppendMenu(MF_STRING | ungroupEn, IDC_BTN_UNGROUP, L"打散");
    // 布尔运算：任意图元/布尔结果(A) 与 孔体(B)，单选/多选均可（自动找重叠配对元件）
    bool canBool = false;
    for (size_t i = 0; i < m_selSet.size(); i++) {
        int t = m_selSet[i]->type;
        if (Object3D::IsSolidForCSG(t)) { canBool = true; break; }
    }
    UINT boolEn = canBool ? 0 : MF_GRAYED;
    menu.AppendMenu(MF_STRING | boolEn, IDC_CMD_BOOLEAN, L"布尔差集(A−B挖孔)");
    menu.AppendMenu(MF_STRING | boolEn, IDC_CMD_BOOLUNION, L"布尔并集(A∪B合并)");
    menu.AppendMenu(MF_STRING | boolEn, IDC_CMD_BOOLINTER, L"布尔交集(A∩B共体)");
    menu.AppendMenu(MF_STRING | selEn, IDC_CMD_TOMC, L"转换为 MovieClip");
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, screenPt.x, screenPt.y, this);
}

void CMainWnd::OnWrapToMovieClip() {
     WrapToMovieClip(); }

// 把对象的世界坐标位置写入本地平移（考虑父级显示矩阵），仅改平移保持旋转/缩放；
// 与对齐用到的包围盒/对象位置统一以显示矩阵为基准，保证动画或嵌套父级下对齐仍精确。
static void SetWorldPos(Object3D* o, const Vec3& pw) {
    Mat4 wp = o->parent ? o->parent->DisplayWorldMatrix() : Mat4();
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
        return Object3D::IsSolidForCSG(t);
    };
    auto isTarget = [](int t) { return Object3D::IsSolidForCSG(t); };   // 任何闭合实体均可为主体（含布尔结果）
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

// 世界坐标对齐 / 平均分布：以第一个选中的元件为基准（保持不动）。
// 对齐：把其余元件的世界包围盒在所选轴（X/Y/Z）上按所选方式（起始边/中心/终止边）对齐到基准；
// 平均分布：沿所选轴保持首尾元件不动，中间元件等空隙重排。都只动所选轴，其余两轴不变，不堆叠。
// nID 在 IDC_ALG_X_MIN..IDC_ALG_Z_DIST 之间：k<9 为对齐（axis=k/3，mode=k%3），k>=9 为平均分布（axis=k-9）。
void CMainWnd::OnAlignMenu(UINT nID) {
    int k = (int)(nID - IDC_ALG_X_MIN);
    if (k < 0 || k > 11) return;
    // 收集待对齐对象：优先 Ctrl 多选集合；不足则补入当前编辑层级中已标记选中的元件
    std::vector<Object3D*> sel = m_selSet;
    if (sel.size() < 2) {
        std::vector<Object3D*> kids;
        m_scene.EditChildren(kids);
        for (Object3D* k : kids)
            if (k->selected && !InSelection(k)) sel.push_back(k);
    }
    if (sel.size() < 2) {
        MessageBox(_T("对齐/分布需要至少两个元件。\r\n请先用 Ctrl+单击 多选两个及以上的元件，再执行。"),
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
        MessageBox(_T("无法计算所选元件的包围盒，操作取消。"), _T("Super3D"), MB_ICONINFORMATION);
        return;
    }
    PushUndo();
    if (k >= 9) {
        // 平均分布：沿所选轴，保持首尾元件不动，中间元件等空隙重排；只动该轴，其余两轴不变。
        int axis = k - 9;   // 0=X 1=Y 2=Z
        auto centerOf = [axis](const Item& it) -> double {
            return axis == 0 ? (it.mn.x + it.mx.x) * 0.5 : (axis == 1 ? (it.mn.y + it.mx.y) * 0.5 : (it.mn.z + it.mx.z) * 0.5);
        };
        auto sizeOf = [axis](const Item& it) -> double {
            return axis == 0 ? (it.mx.x - it.mn.x) : (axis == 1 ? (it.mx.y - it.mn.y) : (it.mx.z - it.mn.z));
        };
        std::vector<Item*> order;
        for (Item& it : items) order.push_back(&it);
        std::sort(order.begin(), order.end(), [&](const Item* a, const Item* b) { return centerOf(*a) < centerOf(*b); });
        if (order.size() < 3) { m_scene.SyncDisplayToLocalAll(); RefreshAll(); return; }
        double firstC = centerOf(*order.front()), lastC = centerOf(*order.back());
        double midSize = 0;   // 首尾之外各元件尺寸和
        for (size_t i = 1; i + 1 < order.size(); i++) midSize += sizeOf(*order[i]);
        double freeGap = (lastC - firstC) - (sizeOf(*order.front()) + sizeOf(*order.back())) * 0.5 - midSize;
        double gap = freeGap / (double)(order.size() - 1);
        if (gap < 0) gap = 0;
        double edge = firstC + sizeOf(*order.front()) * 0.5;   // 首元件远侧边缘
        for (size_t i = 1; i + 1 < order.size(); i++) {
            double sz = sizeOf(*order[i]);
            edge += gap + sz * 0.5;                            // 前元件远边缘 → 本元件中心
            double target = edge;
            Vec3 pw = order[i]->o->DisplayWorldMatrix().TransformPoint(Vec3(0, 0, 0));
            double cur = centerOf(*order[i]);
            if (axis == 0) pw.x += target - cur;
            else if (axis == 1) pw.y += target - cur;
            else               pw.z += target - cur;
            SetWorldPos(order[i]->o, pw);
            edge += sz * 0.5;                                  // 本元件中心 → 本元件远边缘
        }
        m_scene.SyncDisplayToLocalAll();
        RefreshAll();
        return;
    }
    // 对齐：axis = k/3（0=X 1=Y 2=Z），mode = k%3（0=起始边 1=中心 2=终止边）
    int axis = k / 3, mode = k % 3;
    auto axisVal = [axis](const Item& it, int w) -> double {
        switch (axis) {
            case 0: return w == 0 ? it.mn.x : (w == 1 ? (it.mn.x + it.mx.x) * 0.5 : it.mx.x);
            case 1: return w == 0 ? it.mn.y : (w == 1 ? (it.mn.y + it.mx.y) * 0.5 : it.mx.y);
            default:return w == 0 ? it.mn.z : (w == 1 ? (it.mn.z + it.mx.z) * 0.5 : it.mx.z);
        }
    };
    // 以第一个选中的元件为基准，其余元件在所选轴上对齐到它（基准保持不动）
    const Item& ref = items[0];
    double target = axisVal(ref, mode);
    for (size_t i = 1; i < items.size(); i++) {
        Item& it = items[i];
        double d = target - axisVal(it, mode);
        if (d == 0.0) continue;
        Vec3 pw = it.o->DisplayWorldMatrix().TransformPoint(Vec3(0, 0, 0));   // 对象世界位置（与包围盒同基准）
        if (axis == 0) pw.x += d;
        else if (axis == 1) pw.y += d;
        else               pw.z += d;
        SetWorldPos(it.o, pw);
    }
    m_scene.SyncDisplayToLocalAll();
    RefreshAll();
}

void CMainWnd::RefreshAll() {
    // 导航栏缓存的对象指针先失效：场景对象可能刚被重建/释放（撤销、新建、打开、载入示例），
    // m_tree.Rebuild 的 DeleteAllItems 会同步触发主窗口 WM_ERASEBKGND（OnEraseBkgnd 画导航栏），
    // 若此时 m_navItems 还持有已释放对象的指针，访问 ->name 会读到 0xDDDDDDDD 崩溃（TankBattle 进组删元件撤销复现）。
    // RefreshNavBar() 随后会按新场景重建 m_navItems，这里清空不会影响正常显示。
    m_navItems.clear();
    m_selected = m_scene.FindByName(m_selName);
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
    // 状态栏刷新节流：文本未变化一律不刷新（拖动元件时文本不变 → 不再每 200ms 重绘闪烁）；
    // 只有文本真正变化才 SetWindowText，变化时 100ms 下限防高频（播放中帧号推进）。
    DWORD now = ::GetTickCount();
    if (s == m_statusText) return;
    if (now - m_statusTick < 100) return;
    m_statusText = s;
    m_statusTick = now;
    m_status.SetWindowText(s);
}

// 操作记录：把一条操作写入"操作记录"页（最新在底部，滚动跟随），并更新状态栏"最近操作"
// ------------------------------------------------------------ 撤销/重做
// 快照收集：文本不含布尔网格体；网格缓存按对象名二进制侧挂（memcpy 级，远比序列化成文本快）
static void CollectUndoSnap(UndoSnap& s, Scene3D& scene) {
    s.text = ProjectIO::SerializeToString(scene, true);
    std::vector<Object3D*> all;
    scene.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) {
        Object3D* o = all[i];
        if (o->type == PT_BOOLEAN && o->_meshValid && !o->_meshVerts.empty())
            s.boolMesh[o->name] = std::make_pair(o->_meshVerts, o->_meshNorms);
    }
}

// 快照：场景内容 + 当前编辑层级（根→容器名称链）+ 多选集合（主选中首位）。
// 撤销/重做据此回到组件内部并恢复多选，而不是一律退回外层、清空选中。
void CMainWnd::CaptureSnap(UndoSnap& s) {
    s.boolMesh.clear();
    s.text.clear();
    s.editPath.clear();
    s.selNames.clear();
    CollectUndoSnap(s, m_scene);
    // 编辑层级：从 editRoot 向上到根，再反转为“根→容器”
    for (Object3D* p = m_scene.editRoot; p; p = p->parent) s.editPath.push_back(p->name);
    std::reverse(s.editPath.begin(), s.editPath.end());
    // 多选：主选中首位（保证属性面板作用对象），其余 Ctrl 多选集合依次补上，去重
    if (m_selected) s.selNames.push_back(m_selected->name);
    for (size_t i = 0; i < m_selSet.size(); i++) {
        Object3D* o = m_selSet[i];
        if (!o) continue;
        bool dup = false;
        for (size_t j = 0; j < s.selNames.size(); j++)
            if (s.selNames[j] == o->name) { dup = true; break; }
        if (!dup) s.selNames.push_back(o->name);
    }
}

void CMainWnd::PushUndo() {
    UndoSnap s;
    CaptureSnap(s);
    // 相邻快照去重：视口点选/未移动拖拽等操作会在 OnLButtonDown 时入栈却未产生任何变化，
    // 若不去重，删除元件后按 Ctrl+Z 第一次会撤销这条"空操作"（界面无变化），第二次才真正恢复。
    // 与栈顶快照完全相同时视为空操作不入栈；场景确有变化的操作仍正常入栈。
    if (!m_undo.empty() && m_undo.back().text == s.text)
        return;
    m_undo.push_back(s);
    if (m_undo.size() > 30) m_undo.pop_front();
    m_redo.clear();   // 新编辑操作使旧的重做分支失效（与主流编辑器一致）
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
    // 撤销前把当前状态存入重做栈，Ctrl+Y 可恢复
    UndoSnap cur;
    CaptureSnap(cur);
    m_redo.push_back(cur);
    if (m_redo.size() > 30) m_redo.pop_front();
    RestoreSnap(snap);
}

void CMainWnd::DoRedo() {
    if (m_redo.empty()) return;
    Stop();
    UndoSnap snap = m_redo.back();
    m_redo.pop_back();
    // 重做前把当前状态存回撤销栈（不调用 PushUndo，避免清空重做栈）
    UndoSnap cur;
    CaptureSnap(cur);
    m_undo.push_back(cur);
    if (m_undo.size() > 30) m_undo.pop_front();
    RestoreSnap(snap);
}

// 撤销/重做共用：把快照反序列化回场景并刷新界面。
// 撤销只恢复场景编辑内容（对象/关键帧/脚本），不改变相机视角：
// 相机属于视图状态，视角操作（右键旋转/滚轮/平移）不入撤销栈，
// 反序列化 scene.Clear() 会 cam.Reset()、文本快照又恢复旧相机 → 这里覆盖回当前相机。
void CMainWnd::RestoreSnap(const UndoSnap& snap) {
    Camera camKeep = m_scene.cam;
    int frameKeep = m_scene.frame;   // 播放头同属视图状态，撤销/重做不跳帧
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
        std::map<std::wstring, std::pair<std::vector<Vec3>, std::vector<Vec3> > >::const_iterator it = snap.boolMesh.find(o->name);
        if (it != snap.boolMesh.end()) {
            o->_meshVerts = it->second.first;
            o->_meshNorms = it->second.second;
            o->_meshValid = !o->_meshVerts.empty();
            o->_meshRev++;
        }
    }
    // 撤销快照重建了全部对象，编辑态指针一律失效；网格编辑态不恢复（回到普通选择态）。
    // 编辑层级（editRoot）则按快照记录的名称链恢复——组件内部做的操作撤销后应仍在组件内部，
    // 而不是一律退回外层。
    m_scene.editMeshObj = NULL;
    Object3D* newRoot = NULL;
    if (!snap.editPath.empty()) {
        for (size_t pi = 0; pi < snap.editPath.size(); pi++) {
            Object3D* nxt = FindSceneObjectByName(snap.editPath[pi]);
            if (!nxt) { newRoot = NULL; break; }   // 容器已被删除等：退回根层
            newRoot = nxt;
        }
        // 仅允许编辑容器（PT_NONE 组）；目标若不是组则回退根层
        if (newRoot && newRoot->type != PT_NONE) newRoot = NULL;
    }
    m_scene.editRoot = newRoot;
    m_scene.frame = frameKeep;
    m_scene.SetFrame(frameKeep);
    // 反序列化 scene.Clear() 已删除所有旧对象，旧的选中指针已悬空；先清掉所有选中标记，
    // 再按快照记录的对象名恢复多选（首个为主选中），实现“撤销后回到对齐前的多选中状态”。
    {
        std::vector<Object3D*> all;
        m_scene.CollectAll(all);
        for (size_t i = 0; i < all.size(); i++) if (all[i]) all[i]->selected = false;
    }
    m_selected = NULL;
    m_selName.clear();
    m_selSet.clear();
    for (size_t i = 0; i < snap.selNames.size(); i++) {
        Object3D* o = FindSceneObjectByName(snap.selNames[i]);
        if (!o) continue;
        o->selected = true;
        m_selSet.push_back(o);
        if (i == 0) { m_selected = o; m_selName = o->name; }
    }
    if (!m_selected && !m_selSet.empty()) { m_selected = m_selSet.front(); m_selName = m_selected->name; }
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

void CMainWnd::ClearClipboard() {
    for (size_t i = 0; i < m_clipboard.size(); i++)
        if (m_clipboard[i]) delete m_clipboard[i];
    m_clipboard.clear();
}

// 从选中集挑出“顶层”元件：父级也在选中集时，子级随父级一起深拷贝，不单独处理（避免重复）
static void CollectClipTops(const std::vector<Object3D*>& items, std::vector<Object3D*>& tops) {
    tops.clear();
    for (size_t i = 0; i < items.size(); i++) {
        Object3D* o = items[i];
        if (!o) continue;
        bool parentInSet = false;
        for (size_t j = 0; j < items.size(); j++)
            if (items[j] && items[j] == o->parent) { parentInSet = true; break; }
        if (!parentInSet) tops.push_back(o);
    }
}

void CMainWnd::DoDuplicate() {   // 复制：把选中元件（可多选）暂存到剪贴板（不生成副本，Ctrl+V/右键粘贴才出副本）
    if (m_scene.editMeshObj) return;   // 网格编辑态不做整物体复制
    std::vector<Object3D*> items, tops;
    GetSelectionSet(items);
    if (items.empty()) return;
    CollectClipTops(items, tops);
    if (tops.empty()) return;
    ClearClipboard();
    for (size_t i = 0; i < tops.size(); i++)
        m_clipboard.push_back(tops[i]->CloneDeep(tops[i]->name, false));
    UpdateStatus();
}

void CMainWnd::OnCut() {
              // 剪切：暂存剪贴板并从场景移除（支持多选）
    if (m_scene.editMeshObj) return;
    std::vector<Object3D*> items, tops;
    GetSelectionSet(items);
    if (items.empty()) return;
    CollectClipTops(items, tops);
    if (tops.empty()) return;
    ClearClipboard();
    for (size_t i = 0; i < tops.size(); i++)
        m_clipboard.push_back(tops[i]->CloneDeep(tops[i]->name, false));
    Stop();
    PushUndo();
    for (size_t i = 0; i < tops.size(); i++) {
        m_scene.RemoveObjectFromAllLayerKeys(tops[i]);
        m_scene.DeleteObject(tops[i]);
    }
    ClearSelection();
    m_selName.clear();
    RefreshAll();
    UpdateStatus();
}

void CMainWnd::OnPaste() {
            // 粘贴：把剪贴板克隆生成副本加入当前编辑层级（支持多选，一次粘贴全部）
    if (m_clipboard.empty() || m_scene.editMeshObj) return;
    Stop();
    PushUndo();
    std::vector<Object3D*> newObjs;
    for (size_t i = 0; i < m_clipboard.size(); i++) {
        Object3D* src = m_clipboard[i];
        if (!src) continue;
        Object3D* c = src->CloneDeep(src->name, true);
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
        newObjs.push_back(c);
    }
    if (newObjs.size() == 1)
        SelectObject(newObjs[0]);
    else if (newObjs.size() > 1)
        SetSelectionSet(newObjs, newObjs.back());   // 多选粘贴：全部选中，便于整组移动
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

// 面模式挤出选中面（Blender 式 E：生成顶盖与侧壁，随后沿法线拖出深度，单击确认）
void CMainWnd::ExtrudeEditSelection() {
    if (m_scene.editMeshObj) m_viewport.ExtrudeEditSelection();
}

// 按给定深度值精确挤出（属性面板输入框；d 可负=向内），一步到位不进入拖动
void CMainWnd::ExtrudeEditSelectionDepth(double d) {
    if (m_scene.editMeshObj) m_viewport.ExtrudeEditSelection(d);
}

// 切换编辑模式（0点 1线 2面）：切换后清空选择（不同元素类型不通用）
void CMainWnd::SetMeshEditMode(int m) {
    if (m < 0 || m > 2) m = 0;
    m_viewport.m_emMode = m;
    m_viewport.ClearEditSelection();
    m_viewport.Repaint();
    m_props.Refresh();   // 刷新“挤出”按钮显隐（仅面模式显示）
}

void CMainWnd::DoNew() {
    Stop();
    PushUndo();
    m_scene.Clear();
    ClearClipboard();                 // 新工程：清空内部剪贴板，避免粘贴出旧场景的元件
    m_ssPath.clear();                   // 新工程：.ss 无保存路径，Ctrl+S 首次走另存为
    m_selected = NULL;
    m_selName.clear();
    m_selSet.clear();
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
    // 主选中 + 多选集合全部更新当前帧图层关键帧快照：
    // 多选拖动/微调时若只更新主选中快照，其余选中对象的快照仍是变换前位置，
    // 保存→重开时 SetFrame 会用旧快照把其余对象复位（位置回退，与保存前不一致）。
    std::vector<Object3D*> items;
    if (m_selected) items.push_back(m_selected);
    for (size_t i = 0; i < m_selSet.size(); i++)
        if (m_selSet[i] && m_selSet[i] != m_selected) items.push_back(m_selSet[i]);
    for (size_t i = 0; i < items.size(); i++) {
        Object3D* o = items[i];
        m_scene.UpdateLayerKeySnap(o, m_scene.frame);   // 更新当前帧图层关键帧快照
        o->SyncDisplayToLocal();
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
    std::wstring root = SampleDir();   // 兼容便携版/开发目录两种布局
    std::wstring sub = root + name + L"\\" + name + L".s3d";   // 每个示例一个独立文件夹 sample\<名>\<名>.s3d
    if (::GetFileAttributesW(sub.c_str()) != INVALID_FILE_ATTRIBUTES) return sub;
    std::wstring flat = root + name + L".s3d";
    if (::GetFileAttributesW(flat.c_str()) != INVALID_FILE_ATTRIBUTES) return flat;   // 兼容旧扁平形式
    return sub;   // 均不存在时返回标准子目录路径（文件应有的完整路径，供提示使用）
}

// 工程根 sample 目录。兼容两种布局：
//  - 便携版：exe 与 sample 文件夹同级（用户解压后直接运行）
//  - 开发目录：exe 在 Win32/Release 下，需向上推两层到工程根
std::wstring CMainWnd::SampleDir() const {
    wchar_t exe[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    std::wstring root(exe);
    size_t slash = root.find_last_of(L"/\\");
    if (slash != std::wstring::npos) root = root.substr(0, slash + 1);
    // 1) 便携版：exe 同级目录下直接有 sample 文件夹
    std::wstring portable = root + L"sample\\";
    if (GetFileAttributesW(portable.c_str()) != INVALID_FILE_ATTRIBUTES)
        return portable;
    // 2) 开发目录：从 exe 路径 Win32/Release 向上推两层到工程根
    for (int i = 0; i < 2; i++) {
        size_t p = root.find_last_of(L"/\\", root.size() - 2);
        if (p == std::wstring::npos) break;
        root = root.substr(0, p + 1);
    }
    return root + L"sample\\";
}

// 枚举 sample\ 下每个示例子目录（sample\<名>\<名>.s3d）填入 m_sampleFiles——示例菜单动态列出
void CMainWnd::ScanSampleFiles() {
    m_sampleFiles.clear();
    std::wstring dir = SampleDir();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                std::wstring sub = fd.cFileName;
                if (sub == L"." || sub == L"..") continue;
                std::wstring s3d = dir + sub + L"\\" + sub + L".s3d";
                if (GetFileAttributesW(s3d.c_str()) != INVALID_FILE_ATTRIBUTES)
                    m_sampleFiles.push_back(s3d);
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(m_sampleFiles.begin(), m_sampleFiles.end());
}

// 示例菜单动态项：按 ID 查表打开对应 sample 文件
void CMainWnd::OnSampleMenuItem(UINT nID) {
    
    UINT i = nID - kSampleMenuBase;
    if (i < m_sampleFiles.size()) LoadSampleFile(m_sampleFiles[i]);
}

// 直接打开 sample 目录下的示例工程文件（.s3d + 同名 .ss 文档类）
void CMainWnd::LoadSampleFile(const std::wstring& s3dPath) {
    Stop();
    PushUndo();
    std::wstring err;
    if (!ProjectIO::LoadProject(m_scene, s3dPath, err)) {
        AfxMessageBox((L"示例加载失败：" + err).c_str(), MB_ICONWARNING);
        return;
    }
    m_scene.projectPath = s3dPath;   // 绑定示例文件路径：修改后保存即覆盖示例
    m_ssPath = ProjectIO::DocClassPath(s3dPath);   // .ss 随示例绑定：Ctrl+S 直接覆盖示例脚本
    std::wstring name = s3dPath;
    size_t p = name.find_last_of(L"/\\");
    if (p != std::wstring::npos) name = name.substr(p + 1);
    size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name = name.substr(0, dot);
    m_titleName = name;
    m_docScriptMode = false;          // 示例回到场景编辑
    m_docSSOpen = true;               // 示例自带文档类，恢复 .ss 标签
    SetDocText(m_scene.docClass.c_str());
    m_docTab.SetCurSel(0);
    m_scene.editRoot = NULL;
    m_selected = NULL;
    m_selName.clear();
    m_selSet.clear();   // 场景已整体重建：旧选中集的指针全部失效，必须清空
    m_docScript.SetText(m_scene.docClass);
    SetScriptEditText(m_scene.script);
    m_scriptWnd.SetPerFrame(m_scene.scriptPerFrame);
    m_scene.SetFrame(0);
    m_scene.SyncDisplayToLocalAll();   // 无关键帧时 SetFrame 不补间，display 变换停在默认值，必须同步
    m_scriptWnd.SetOutput(_T(""));
    RefreshAll();
    UpdateTitle();
    UpdateDocTabs();
    Layout();
}

void CMainWnd::LoadExample(int which) {
    Stop();
    PushUndo();
    static const wchar_t* kExNames[] = { L"House", L"Windmill", L"SolarSystem",
                                         L"BounceBall", L"DocClass", L"TankBattle", L"Willow", L"Rocket" };
    std::wstring name = (which >= 0 && which < 8) ? kExNames[which] : L"BounceBall";
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
            m_ssPath = ProjectIO::DocClassPath(exFile);   // .ss 随示例绑定：Ctrl+S 直接覆盖示例脚本
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
        m_ssPath.clear();
    }
    SetDocText(m_scene.docClass.c_str());   // 文档类脚本编辑器载入示例 .ss 内容
    m_docTab.SetCurSel(0);
    m_scene.editRoot = NULL;
    m_selected = NULL;
    m_selName.clear();
    m_selSet.clear();   // 场景已整体重建：旧选中集的指针全部失效，必须清空
    m_docScript.SetText(m_scene.docClass);   // 同步文档类（如 DocClass 示例的 .ss 内容）
    SetScriptEditText(m_scene.script);
    m_scriptWnd.SetPerFrame(m_scene.scriptPerFrame);
    m_scene.SetFrame(0);
    m_scene.SyncDisplayToLocalAll();   // 无关键帧时 SetFrame 不补间，display 变换停在默认值，必须同步
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
    // 当前选项卡在 .ss（文档类脚本页）：直接覆盖保存到 m_ssPath；尚未保存过则走另存为选位置
    if (m_docScriptMode) {
        if (m_ssPath.empty()) { DoSaveAs(); return; }
        std::wstring text = GetDocEditText();
        m_scene.docClass = text;   // 同步到文档类（播放/调试用内存版本）
        if (!ProjectIO::WriteFileUtf8(m_ssPath, text)) {
            AfxMessageBox(_T("保存文档类脚本失败：无法写入文件。"), MB_ICONERROR);
            return;
        }
        CString t;
        t.Format(_T("已保存：%s"), m_ssPath.c_str());
        m_status.SetWindowText(t);
        return;
    }
    // 场景模式：直接覆盖保存到 projectPath；尚未保存过则走另存为选位置
    if (m_scene.projectPath.empty()) { DoSaveAs(); return; }
    std::wstring err;
    if (!ProjectIO::SaveProject(m_scene, m_scene.projectPath, err)) {
        AfxMessageBox(err.c_str(), MB_ICONERROR);
        return;
    }
    CString t;
    t.Format(_T("已保存：%s"), m_scene.projectPath.c_str());
    m_status.SetWindowText(t);
    m_titleName.clear();   // 已保存为真实文件，标题以文件名为准
    UpdateTitle();
    UpdateDocTabs();   // 保存后刷新文档标签文字
}

void CMainWnd::DoSaveAs() {
    Stop();
    // .ss 页：弹"另存为"选择位置，保存后记录路径（后续 Ctrl+S/保存直接覆盖）
    if (m_docScriptMode) {
        std::wstring text = GetDocEditText();
        m_scene.docClass = text;
        std::wstring defName = L"文档类脚本.ss";
        if (!m_ssPath.empty()) defName = m_ssPath;
        else if (!m_scene.projectPath.empty()) {
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
        m_ssPath = ssPath;   // 记录：后续 Ctrl+S 直接覆盖
        CString t;
        t.Format(_T("已保存：%s"), ssPath.c_str());
        m_status.SetWindowText(t);
        return;
    }
    // 场景模式：弹"另存为"选择位置，保存后绑定 projectPath（后续 Ctrl+S 直接覆盖）
    CFileDialog dlg(FALSE, _T("s3d"), m_scene.projectPath.empty() ? _T("我的作品.s3d") : m_scene.projectPath.c_str(),
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        _T("Super3D 工程 (*.s3d)|*.s3d|所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;
    std::wstring err;
    if (!ProjectIO::SaveProject(m_scene, dlg.GetPathName().GetString(), err)) {
        AfxMessageBox(err.c_str(), MB_ICONERROR);
        return;
    }
    m_scene.projectPath = dlg.GetPathName().GetString();   // 绑定：后续 Ctrl+S 直接覆盖
    CString t;
    t.Format(_T("已保存：%s"), dlg.GetPathName());
    m_status.SetWindowText(t);
    m_titleName.clear();
    UpdateTitle();
    UpdateDocTabs();
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
    ClearClipboard();           // 打开新工程：清空内部剪贴板
    m_ssPath.clear();           // 打开新工程：.ss 无保存路径，Ctrl+S 首次走另存为
    m_docScriptMode = false;   // 打开工程回到场景编辑（不触发旧文档类保存）
    m_docSSOpen = true;        // 打开工程恢复 .ss 标签
    SetDocText(m_scene.docClass.c_str());   // 脚本编辑器载入新工程文档类
    m_docTab.SetCurSel(0);     // 文档 Tab 回到 .s3d
    m_titleName.clear();       // 打开真实文件后以文件名为准
    m_scene.editRoot = NULL;
    m_selected = NULL;
    m_selName.clear();
    m_selSet.clear();
    m_scene.frame = 0;
    m_scene.SetFrame(0);
    m_scene.SyncDisplayToLocalAll();   // 无关键帧时 SetFrame 不补间，display 变换停在默认值，必须同步
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
void CMainWnd::OnNew() {
     DoNew(); }
void CMainWnd::OnOpen() {
     DoOpen(); }
void CMainWnd::OnSave() {
     DoSave(); }
void CMainWnd::OnSaveAs() {
     DoSaveAs(); }
void CMainWnd::OnUndo() {
     DoUndo(); }
void CMainWnd::OnRedo() {
     DoRedo(); }
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
void CMainWnd::OnBox() {
     AddPrimitive(PT_BOX); }
void CMainWnd::OnSphere() {
     AddPrimitive(PT_SPHERE); }
void CMainWnd::OnCylinder() {
     AddPrimitive(PT_CYLINDER); }
void CMainWnd::OnCone() {
     AddPrimitive(PT_CONE); }
void CMainWnd::OnTorus() {
     AddPrimitive(PT_TORUS); }
void CMainWnd::OnPlane() {
     AddPrimitive(PT_PLANE); }
void CMainWnd::OnLeaf() {
     AddPrimitive(PT_LEAF); }
void CMainWnd::OnText3D() {
     AddPrimitive(PT_TEXT3D); }
void CMainWnd::OnTube() {
     AddPrimitive(PT_TUBE); }
void CMainWnd::OnRTube() {
     AddPrimitive(PT_RTUBE); }
void CMainWnd::OnRoundBox() {
     AddPrimitive(PT_ROUNDBOX); }
void CMainWnd::OnCapsule() {
     AddPrimitive(PT_CAPSULE); }
void CMainWnd::OnHemisphere() {
     AddPrimitive(PT_HEMISPHERE); }
void CMainWnd::OnLathe() {
     AddPrimitive(PT_LATHE); }
void CMainWnd::OnExtrude() {
     AddPrimitive(PT_EXTRUDE); }
// 曲线工具：切换到“画曲线”，在视口中按住左键拖拽出一条柳条
void CMainWnd::OnToolSpline() {
    
    // 曲线按钮与其他元件一致：直接在场景中添加一条默认 3 控制点曲线，
    // 控制点数量可在属性面板“控制点”参数中修改（沿原曲线路径重采样，保持形状）
    AddPrimitive(PT_SPLINE);
}
void CMainWnd::OnToolSelect() {
     SetTool(TOOL_SELECT); }
void CMainWnd::OnToolMove() {
     SetTool(TOOL_MOVE); }
void CMainWnd::OnToolRotate() {
     SetTool(TOOL_ROTATE); }
void CMainWnd::OnToolScale() {
     SetTool(TOOL_SCALE); }
void CMainWnd::OnToolPan() {
     SetTool(TOOL_PAN); }
void CMainWnd::OnDuplicate() {
     DoDuplicate(); }
void CMainWnd::OnDelete() {
     DoDelete(); }
void CMainWnd::OnPlay() {
     Play(); }
void CMainWnd::OnStop() {
     Stop(); }
void CMainWnd::OnEx0() {
     LoadExample(0); }
void CMainWnd::OnEx1() {
     LoadExample(1); }
void CMainWnd::OnEx2() {
     LoadExample(2); }
void CMainWnd::OnEx3() {
     LoadExample(3); }
void CMainWnd::OnEx4() {
     LoadExample(4); }
void CMainWnd::OnEx5() {
     LoadExample(5); }
void CMainWnd::OnEx6() {
     LoadExample(6); }
void CMainWnd::OnEx7() {
     LoadExample(7); }

void CMainWnd::OnResetView() {

    m_scene.cam.Reset();
    m_viewport.Repaint();
}

// 灯光设置窗口：5个滑块实时调节
class CLightFrame : public CFrameWnd {
public:
    CSliderCtrl m_sldAmb, m_sldDif, m_sldX, m_sldY, m_sldZ;
    CStatic m_lblAmb, m_lblDif, m_lblX, m_lblY, m_lblZ;
    CStatic m_lblClose;

    BOOL Create(CWnd* pParent) {
        int margin = 12, lblW = 70, gap = 8, rowH = 36, rows = 5, top = 28;
        int cliW = margin + lblW + gap + 220 + margin;
        int cliH = top + rowH * rows + margin;
        RECT r = {0, 0, cliW, cliH};
        AdjustWindowRect(&r, WS_POPUP|WS_BORDER|WS_CAPTION|WS_SYSMENU, FALSE);
        int winW = r.right - r.left, winH = r.bottom - r.top;
        return CFrameWnd::Create(NULL, L"灯光设置",
            WS_POPUP|WS_BORDER|WS_CAPTION|WS_SYSMENU,
            CRect(100, 100, 100+winW, 100+winH), pParent);
    }
    afx_msg int OnCreate(LPCREATESTRUCT lp) {
        if (CFrameWnd::OnCreate(lp) == -1) return -1;
        CFont* f = CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT));
        CRect rc; GetClientRect(&rc);
        int W = rc.Width(), rowH = 36, startY = 12, lblW = 70, gap = 8;
        int sldX = lblW + gap, sldW = W - sldX - 15;

        m_lblAmb.Create(L"环境光", WS_CHILD|WS_VISIBLE, CRect(12, startY+6, 12+lblW, startY+24), this);
        m_sldAmb.Create(WS_CHILD|WS_VISIBLE|TBS_HORZ, CRect(sldX, startY, sldX+sldW, startY+rowH), this, 1001);
        m_sldAmb.SetRange(0, 100); m_sldAmb.SetPos((int)(Renderer::s_lightAmbient*100));

        m_lblDif.Create(L"方向光", WS_CHILD|WS_VISIBLE, CRect(12, startY+rowH+6, 12+lblW, startY+rowH+24), this);
        m_sldDif.Create(WS_CHILD|WS_VISIBLE|TBS_HORZ, CRect(sldX, startY+rowH, sldX+sldW, startY+rowH+rowH), this, 1002);
        m_sldDif.SetRange(0, 150); m_sldDif.SetPos((int)(Renderer::s_lightDiffuse*100));

        m_lblX.Create(L"方向 X", WS_CHILD|WS_VISIBLE, CRect(12, startY+rowH*2+6, 12+lblW, startY+rowH*2+24), this);
        m_sldX.Create(WS_CHILD|WS_VISIBLE|TBS_HORZ, CRect(sldX, startY+rowH*2, sldX+sldW, startY+rowH*2+rowH), this, 1003);
        m_sldX.SetRange(-100, 100); m_sldX.SetPos((int)(Renderer::s_lightDirX*100));

        m_lblY.Create(L"方向 Y", WS_CHILD|WS_VISIBLE, CRect(12, startY+rowH*3+6, 12+lblW, startY+rowH*3+24), this);
        m_sldY.Create(WS_CHILD|WS_VISIBLE|TBS_HORZ, CRect(sldX, startY+rowH*3, sldX+sldW, startY+rowH*3+rowH), this, 1004);
        m_sldY.SetRange(-100, 100); m_sldY.SetPos((int)(Renderer::s_lightDirY*100));

        m_lblZ.Create(L"方向 Z", WS_CHILD|WS_VISIBLE, CRect(12, startY+rowH*4+6, 12+lblW, startY+rowH*4+24), this);
        m_sldZ.Create(WS_CHILD|WS_VISIBLE|TBS_HORZ, CRect(sldX, startY+rowH*4, sldX+sldW, startY+rowH*4+rowH), this, 1005);
        m_sldZ.SetRange(-100, 100); m_sldZ.SetPos((int)(Renderer::s_lightDirZ*100));

        m_lblAmb.SetFont(f); m_lblDif.SetFont(f); m_lblX.SetFont(f);
        m_lblY.SetFont(f); m_lblZ.SetFont(f);
        m_sldAmb.SetFont(f); m_sldDif.SetFont(f); m_sldX.SetFont(f);
        m_sldY.SetFont(f); m_sldZ.SetFont(f);
        return 0;
    }
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar) {
        int id = pBar->GetDlgCtrlID();
        if (id == 1001) Renderer::s_lightAmbient = m_sldAmb.GetPos()/100.0f;
        if (id == 1002) Renderer::s_lightDiffuse = m_sldDif.GetPos()/100.0f;
        if (id == 1003) Renderer::s_lightDirX = m_sldX.GetPos()/100.0f;
        if (id == 1004) Renderer::s_lightDirY = m_sldY.GetPos()/100.0f;
        if (id == 1005) Renderer::s_lightDirZ = m_sldZ.GetPos()/100.0f;
        ((CMainWnd*)AfxGetMainWnd())->SendMessage(WM_LIGHT_REFRESH);
        ((CMainWnd*)AfxGetMainWnd())->UpdateWindow();
        CFrameWnd::OnHScroll(nSBCode, nPos, pBar);
    }
    afx_msg BOOL OnEraseBkgnd(CDC* pDC) {
        CRect r; GetClientRect(&r);
        pDC->FillRect(&r, CBrush::FromHandle(GetSysColorBrush(COLOR_BTNFACE)));
        return TRUE;
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CLightFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_HSCROLL()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

void CMainWnd::OnLightSettings() {
    static CLightFrame* pFrame = NULL;
    if (pFrame && ::IsWindow(pFrame->m_hWnd)) { pFrame->SetForegroundWindow(); return; }
    pFrame = new CLightFrame();
    if (!pFrame->Create(this)) {
        AfxMessageBox(L"灯光窗口创建失败");
        delete pFrame; pFrame = NULL;
        return;
    }
    pFrame->ShowWindow(SW_SHOW);
    pFrame->UpdateWindow();
}

LRESULT CMainWnd::OnLightRefresh(WPARAM, LPARAM) {
    m_viewport.Repaint();
    return 0;
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
    BOOL CreateHelp(HFONT hFont, const wchar_t* title, const wchar_t* text, CWnd* owner) {
        m_hFont = hFont;
        m_text = text;
        if (!Create(NULL, title, WS_OVERLAPPEDWINDOW,
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
              L"\timport super3d.display.Object3D;\r\n"
              L"\timport super3d.events.Event;\r\n"
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
    // 用 RichEdit 行 API 取光标所在行：索引与 RichEdit 内部一致
    int line = (int)m_docEdt.SendMessage(EM_LINEFROMCHAR, (WPARAM)pos, 0);
    int lineIndex = (int)m_docEdt.SendMessage(EM_LINEINDEX, (WPARAM)line, 0);
    // 行定位修正：光标停在行尾（CR 位置）时可能归属到下一行，回退一行
    if (pos < lineIndex) {
        line--;
        lineIndex = (int)m_docEdt.SendMessage(EM_LINEINDEX, (WPARAM)line, 0);
    }
    // 行内容一律用 GetWindowText 全文本按行取（EM_GETLINE 的容量/写入位置语义不可靠，
    // 实测会丢失行首字符；WM_GETTEXT 返回完整行文本，行号与内部行号一致）
    CStringW txt;
    m_docEdt.GetWindowText(txt);
    std::wstring full(txt.GetString(), txt.GetLength());
    std::wstring norm;
    norm.reserve(full.size());
    for (size_t i = 0; i < full.size(); i++) {
        if (full[i] == L'\r') {
            if (i + 1 < full.size() && full[i + 1] == L'\n') i++;
            norm += L'\n';
        } else norm += full[i];
    }
    std::vector<std::wstring> lines;
    size_t bp = 0;
    while (bp <= norm.size()) {
        size_t e = norm.find(L'\n', bp);
        if (e == std::wstring::npos) { lines.push_back(norm.substr(bp)); break; }
        lines.push_back(norm.substr(bp, e - bp));
        bp = e + 1;
    }
    std::wstring prevLine = (line >= 0 && line < (int)lines.size()) ? lines[line] : L"";
    // 当前行前导空白（原样继承空格/tab——同一函数内上下行左对齐）
    size_t ps = prevLine.find_first_not_of(L" \t");
    std::wstring lead = (ps == std::wstring::npos) ? L"" : prevLine.substr(0, ps);
    // 一级缩进宽度：从上方最近非空行的缩进差推断（当前缩进 - 上层缩进），推断失败默认 4 空格
    int step = 4;
    for (int up = line - 1; up >= 0; up--) {
        if (up >= (int)lines.size()) continue;
        std::wstring uline = lines[up];
        size_t ups = uline.find_first_not_of(L" \t");
        if (ups == std::wstring::npos) continue;   // 空行继续向上找
        std::wstring ulead = uline.substr(0, ups);
        int diff = (int)lead.size() - (int)ulead.size();
        if (diff > 0) { step = diff; }   // 同层或更浅都停止；有差即用差作为一级宽度
        break;
    }
    std::wstring stepSpaces(step, L' ');
    // 规范化前导空白：残留 tab 按一级宽度展开为空格（早期版本可能写入过 tab），统一为纯空格风格
    std::wstring normLead;
    for (size_t k = 0; k < lead.size(); k++)
        normLead += (lead[k] == L'\t') ? stepSpaces : std::wstring(1, lead[k]);
    lead = normLead;
    // 当前行行尾是 { → 新行加一级缩进（用推断宽度空格，跟随文件缩进风格）
    size_t pe = prevLine.find_last_not_of(L" \t");
    std::wstring pt = (pe == std::wstring::npos) ? L"" : prevLine.substr(0, pe + 1);
    if (!pt.empty() && pt.back() == L'{') lead += stepSpaces;
    // 光标前文本以 } 开头 → 新行减一级（对齐上层花括号）
    int col = (int)(pos - lineIndex);
    if (col < 0) col = 0;
    if (col > (int)prevLine.size()) col = (int)prevLine.size();
    std::wstring curLine = (col > 0) ? prevLine.substr(0, col) : L"";
    size_t cs = curLine.find_first_not_of(L" \t");
    if (cs != std::wstring::npos && curLine[cs] == L'}' && !lead.empty()) {
        // 优先去掉推断宽度的一级空格；否则回退最后一个 tab；否则回退单字符
        if (lead.size() >= stepSpaces.size() && lead.substr(lead.size() - stepSpaces.size()) == stepSpaces) {
            lead.erase(lead.size() - stepSpaces.size());
        } else {
            size_t lt = lead.find_last_of(L"\t");
            if (lt != std::wstring::npos) lead.erase(lt);
            else lead.erase(lead.size() - 1);
        }
    }
    // 显式设置零宽选区再替换：防止行尾选区被 RichEdit 扩展（误选 CR 或相邻字符）导致光标/内容偏移
    CHARRANGE ins; ins.cpMin = ins.cpMax = pos;
    m_docEdt.SendMessage(EM_EXSETSEL, 0, (LPARAM)&ins);
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

// ------------------------------------------------------------ MCP 工具路由（AI 直接驱动建模）
// 类型名 ↔ PrimType
static int McpTypeFromName(const std::string& s) {
    if (s == "group" || s == "0") return PT_NONE;
    if (s == "box" || s == "1") return PT_BOX;
    if (s == "sphere" || s == "2") return PT_SPHERE;
    if (s == "cylinder" || s == "3") return PT_CYLINDER;
    if (s == "cone" || s == "4") return PT_CONE;
    if (s == "torus" || s == "5") return PT_TORUS;
    if (s == "plane" || s == "6") return PT_PLANE;
    if (s == "spline" || s == "7") return PT_SPLINE;
    if (s == "leaf" || s == "8") return PT_LEAF;
    if (s == "text3d" || s == "9") return PT_TEXT3D;
    if (s == "tube" || s == "10") return PT_TUBE;
    if (s == "rtube" || s == "11") return PT_RTUBE;
    if (s == "boolean" || s == "12") return PT_BOOLEAN;
    if (s == "capsule" || s == "13") return PT_CAPSULE;
    if (s == "hemisphere" || s == "14") return PT_HEMISPHERE;
    if (s == "roundbox" || s == "15") return PT_ROUNDBOX;
    if (s == "lathe" || s == "16") return PT_LATHE;
    if (s == "extrude" || s == "17") return PT_EXTRUDE;
    return -1;
}
// 解析 "x,y,z;x,y,z;..." 为点列（MCP add_primitive/set_points 的 points 参数）
static int McpParsePoints(const std::string& s, std::vector<Vec3>& out) {
    out.clear();
    const char* p = s.c_str();
    while (*p) {
        char* end = NULL;
        double a = std::strtod(p, &end);
        if (end == p) { p++; continue; }
        p = end;
        double b = 0, c = 0;
        if (*p == ',') { p++; b = std::strtod(p, &end); if (end != p) p = end; }
        if (*p == ',') { p++; c = std::strtod(p, &end); if (end != p) p = end; }
        out.push_back(Vec3(a, b, c));
        while (*p && *p != ';') { if (*p == ',') break; p++; }
        if (*p == ';') p++;
    }
    return (int)out.size();
}

static const char* McpTypeToName(int t) {
    switch (t) {
        case PT_NONE: return "group";
        case PT_BOX: return "box";
        case PT_SPHERE: return "sphere";
        case PT_CYLINDER: return "cylinder";
        case PT_CONE: return "cone";
        case PT_TORUS: return "torus";
        case PT_PLANE: return "plane";
        case PT_SPLINE: return "spline";
        case PT_LEAF: return "leaf";
        case PT_TEXT3D: return "text3d";
        case PT_TUBE: return "tube";
        case PT_RTUBE: return "rtube";
        case PT_BOOLEAN: return "boolean";
        case PT_CAPSULE: return "capsule";
        case PT_HEMISPHERE: return "hemisphere";
        case PT_ROUNDBOX: return "roundbox";
        case PT_LATHE: return "lathe";
        case PT_EXTRUDE: return "extrude";
    }
    return "?";
}

std::string CMainWnd::McpToolsList() {
    // 每个工具的 inputSchema 精简描述（AI 客户端据此构造调用参数）
    auto T = [](const char* name, const char* desc, const char* props) {
        std::string r = "{\"name\":\""; r += name; r += "\",\"description\":\""; r += desc;
        r += "\",\"inputSchema\":{\"type\":\"object\",\"properties\":{";
        r += props;
        r += "},\"required\":[]}}";
        return r;
    };
    std::string list;
    list += T("list_objects", "列出当前场景全部元件（名称/类型/变换/父级）", "");
    list += ",\n";
    list += T("get_object", "查询单个元件的全部属性", "\"name\":{\"type\":\"string\"}");
    list += ",\n";
    list += T("add_primitive", "添加图元：box/sphere/cylinder/cone/torus/plane/leaf/tube/rtube/roundbox/capsule/hemisphere/text3d/spline/lathe/extrude；lathe/extrude/spline 可用 points 传轮廓点",
        "\"type\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"parent\":{\"type\":\"string\"},"
        "\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"},\"z\":{\"type\":\"number\"},"
        "\"rx\":{\"type\":\"number\"},\"ry\":{\"type\":\"number\"},\"rz\":{\"type\":\"number\"},"
        "\"sx\":{\"type\":\"number\"},\"sy\":{\"type\":\"number\"},\"sz\":{\"type\":\"number\"},"
        "\"r\":{\"type\":\"number\"},\"g\":{\"type\":\"number\"},\"b\":{\"type\":\"number\"},\"alpha\":{\"type\":\"number\"},"
        "\"p1\":{\"type\":\"number\"},\"p2\":{\"type\":\"number\"},\"p3\":{\"type\":\"number\"},"
        "\"segments\":{\"type\":\"number\"},\"text\":{\"type\":\"string\"},"
        "\"points\":{\"type\":\"string\"}");
    list += ",\n";
    list += T("set_points", "设置轮廓/曲线控制点（spline 曲线路径点；lathe 母线 x=半径,y=高度；extrude 截面 x,z 闭合折线）。points 格式：x,y,z;x,y,z;...（lathe 只用 x,y；extrude 只用 x,z）",
        "\"name\":{\"type\":\"string\"},\"points\":{\"type\":\"string\"}");
    list += ",\n";
    list += T("add_group", "添加组（MovieClip 容器），后续元件可指定 parent 放入其中",
        "\"name\":{\"type\":\"string\"},\"parent\":{\"type\":\"string\"},\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"},\"z\":{\"type\":\"number\"},"
        "\"rx\":{\"type\":\"number\"},\"ry\":{\"type\":\"number\"},\"rz\":{\"type\":\"number\"},\"sx\":{\"type\":\"number\"},\"sy\":{\"type\":\"number\"},\"sz\":{\"type\":\"number\"}");
    list += ",\n";
    list += T("set_transform", "设置元件位置/旋转/缩放（只改传入的字段）",
        "\"name\":{\"type\":\"string\"},\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"},\"z\":{\"type\":\"number\"},"
        "\"rx\":{\"type\":\"number\"},\"ry\":{\"type\":\"number\"},\"rz\":{\"type\":\"number\"},\"sx\":{\"type\":\"number\"},\"sy\":{\"type\":\"number\"},\"sz\":{\"type\":\"number\"}");
    list += ",\n";
    list += T("set_color", "设置元件颜色（0..1）", "\"name\":{\"type\":\"string\"},\"r\":{\"type\":\"number\"},\"g\":{\"type\":\"number\"},\"b\":{\"type\":\"number\"}");
    list += ",\n";
    list += T("set_visible", "设置元件可见性", "\"name\":{\"type\":\"string\"},\"visible\":{\"type\":\"boolean\"}");
    list += ",\n";
    list += T("set_params", "设置图元参数（box=长宽高 p1p2p3；cylinder=底径p1、高p2、顶径p3/topRadius、封盖p4/cap；cone=半径p1、高p2；torus=半径p1、管粗p2；text3d=字高p1、厚度p2）",
        "\"name\":{\"type\":\"string\"},\"p1\":{\"type\":\"number\"},\"p2\":{\"type\":\"number\"},\"p3\":{\"type\":\"number\"},\"p4\":{\"type\":\"number\"},\"topRadius\":{\"type\":\"number\"},\"cap\":{\"type\":\"number\"},\"segments\":{\"type\":\"number\"}");
    list += ",\n";
    list += T("set_property", "通用属性设置：x/y/z/rx/ry/rz/sx/sy/sz/r/g/b/visible/locked/segments/p1/p2/p3/text",
        "\"name\":{\"type\":\"string\"},\"property\":{\"type\":\"string\"},\"value\":{\"type\":\"number\"}");
    list += ",\n";
    list += T("rename", "重命名元件", "\"name\":{\"type\":\"string\"},\"new_name\":{\"type\":\"string\"}");
    list += ",\n";
    list += T("delete_object", "删除元件（含子元件）", "\"name\":{\"type\":\"string\"}");
    list += ",\n";
    list += T("boolean", "通用布尔运算：a=保留主体元件名，b=参与运算元件名，op=0差集(a-b)/1并集(a∪b)/2交集(a∩b)。任意闭合图元（盒/球/柱/锥/截锥/圆环/方管/圆管/胶囊/半球/圆角盒/曲线/文本/车削/拉伸及布尔结果）均可参与；运算后 b 被删除，结果并入 a",
        "\"a\":{\"type\":\"string\"},\"b\":{\"type\":\"string\"},\"op\":{\"type\":\"number\"}");
    list += ",\n";
    list += T("select", "选中元件（name 为空=取消选中；add=true 追加多选，等价 Ctrl+点选）", "\"name\":{\"type\":\"string\"},\"add\":{\"type\":\"boolean\"}");
    list += ",\n";
    list += T("copy", "复制当前选中元件到剪贴板（支持多选，等价 Ctrl+C）", "");
    list += ",\n";
    list += T("cut", "剪切当前选中元件到剪贴板并从场景移除（支持多选，等价 Ctrl+X）", "");
    list += ",\n";
    list += T("paste", "粘贴剪贴板元件到当前编辑层级（等价 Ctrl+V）", "");
    list += ",\n";
    list += T("enter_group", "进入组内编辑（双击组等价；name 空=回到根层）", "\"name\":{\"type\":\"string\"}");
    list += ",\n";
    list += T("exit_group", "退出组编辑，回到上一层", "");
    list += ",\n";
    list += T("set_frame", "跳到指定帧", "\"frame\":{\"type\":\"number\"}");
    list += ",\n";
    list += T("play", "播放时间轴/脚本动画", "");
    list += ",\n";
    list += T("stop", "停止播放", "");
    list += ",\n";
    list += T("load_sample", "加载内置示例（Rocket/TankBattle/Willow/House 等）", "\"name\":{\"type\":\"string\"}");
    list += ",\n";
    list += T("new_scene", "新建空白场景", "");
    list += ",\n";
    list += T("save", "保存当前工程（菜单保存，覆盖当前文件路径）", "");
    list += ",\n";
    list += T("export_html", "把当前场景导出为 Three.js HTML（指定绝对路径，可在浏览器旋转查看）",
        "\"path\":{\"type\":\"string\"}");
    list += ",\n";
    list += T("add_keyframe", "在指定帧为元件插入关键帧（记录当前位置/旋转/缩放）", "\"name\":{\"type\":\"string\"},\"frame\":{\"type\":\"number\"}");
    list += ",\n";
    list += T("execute_script", "设置并立即执行脚本（kind=doc 文档类[默认]/scene 主场景脚本；播放开始时也会执行）",
        "\"text\":{\"type\":\"string\"},\"kind\":{\"type\":\"string\"}");
    return "{\"tools\":[" + list + "]}";
}

// 元件的 JSON 描述（get_object / list_objects 共用）
static std::string McpObjectJson(Object3D* o) {
    std::string r = "{\"name\":\"" + McpJsonEscape(McpToA(o->name)) + "\",";
    r += "\"type\":\"" + std::string(McpTypeToName(o->type)) + "\",";
    r += "\"x\":" + std::to_string(o->x) + ",\"y\":" + std::to_string(o->y) + ",\"z\":" + std::to_string(o->z) + ",";
    r += "\"rx\":" + std::to_string(o->rx) + ",\"ry\":" + std::to_string(o->ry) + ",\"rz\":" + std::to_string(o->rz) + ",";
    r += "\"sx\":" + std::to_string(o->sx) + ",\"sy\":" + std::to_string(o->sy) + ",\"sz\":" + std::to_string(o->sz) + ",";
    r += "\"r\":" + std::to_string(o->r) + ",\"g\":" + std::to_string(o->g) + ",\"b\":" + std::to_string(o->b) + ",\"alpha\":" + std::to_string(o->alpha) + ",";
    r += "\"visible\":" + std::string(o->visible ? "true" : "false") + ",";
    r += "\"locked\":" + std::string(o->locked ? "true" : "false") + ",";
    r += "\"text\":\"" + McpJsonEscape(McpToA(o->text)) + "\",";
    r += "\"parent\":\"" + McpJsonEscape(McpToA(o->parent ? o->parent->name : L"")) + "\",";
    r += "\"children\":[";
    for (size_t i = 0; i < o->children.size(); i++) {
        if (i) r += ",";
        r += "\"" + McpJsonEscape(McpToA(o->children[i]->name)) + "\"";
    }
    r += "]}";
    return r;
}

std::string CMainWnd::McpExecute(const std::string& tool, const std::string& args) {
    try {
        if (tool == "list_objects") {
            std::vector<Object3D*> all;
            m_scene.CollectAll(all);
            std::string r = "[";
            for (size_t i = 0; i < all.size(); i++) {
                if (i) r += ",";
                r += McpObjectJson(all[i]);
            }
            return r + "]";
        }
        if (tool == "get_object") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found: " + McpJsonEscape(McpJsonFind(args, "name")) + "\"}";
            return McpObjectJson(o);
        }
        if (tool == "add_primitive" || tool == "add_group") {
            std::string ts = McpJsonFind(args, "type");
            int t = (tool == "add_group") ? PT_NONE : McpTypeFromName(ts);
            if (t < 0) return "{\"error\":\"unknown type: " + McpJsonEscape(ts) + "\"}";
            Stop();
            PushUndo();
            std::wstring nm = McpToW(McpJsonFind(args, "name"));
            std::wstring pn = McpToW(McpJsonFind(args, "parent"));
            Object3D* parent = pn.empty() ? m_scene.editRoot : FindSceneObjectByName(pn);
            if (nm.empty()) nm = m_scene.AutoName(t);
            Object3D* o = m_scene.AddObject(t, nm, parent);
            if (!o) return "{\"error\":\"add failed\"}";
            o->x = McpJsonNum(args, "x", 0); o->y = McpJsonNum(args, "y", 0); o->z = McpJsonNum(args, "z", 0);
            o->rx = McpJsonNum(args, "rx", 0); o->ry = McpJsonNum(args, "ry", 0); o->rz = McpJsonNum(args, "rz", 0);
            o->sx = McpJsonNum(args, "sx", 1); o->sy = McpJsonNum(args, "sy", 1); o->sz = McpJsonNum(args, "sz", 1);
            o->r = McpJsonNum(args, "r", 0.75); o->g = McpJsonNum(args, "g", 0.75); o->b = McpJsonNum(args, "b", 0.75);
            if (args.find("\"p1\"") != std::string::npos) { o->params[0] = McpJsonNum(args, "p1", 1); }
            if (args.find("\"p2\"") != std::string::npos) { o->params[1] = McpJsonNum(args, "p2", 1); }
            if (args.find("\"p3\"") != std::string::npos) { o->params[2] = McpJsonNum(args, "p3", 1); }
            if (args.find("\"p4\"") != std::string::npos) { o->params[3] = McpJsonNum(args, "p4", 0); }
            if (args.find("\"segments\"") != std::string::npos) o->segments = (int)McpJsonNum(args, "segments", 24);
            if (args.find("\"text\"") != std::string::npos) o->text = McpToW(McpJsonFind(args, "text"));
            if (args.find("\"points\"") != std::string::npos) {   // 轮廓/曲线点列（spline/lathe/extrude）
                std::vector<Vec3> pts;
                if (McpParsePoints(McpJsonFind(args, "points"), pts) >= 2) {
                    o->ctrlPts = pts;
                    o->InvalidateMesh();
                }
            }
            if (o->parent == NULL) m_scene.AddNewObjectToCurrentKey(o);
            m_scene.SyncDisplayToLocalAll();
            SelectObject(o);
            RefreshAll();
            return "{\"ok\":true,\"name\":\"" + McpJsonEscape(McpToA(o->name)) + "\"}";
        }
        if (tool == "set_points") {
            // 设置轮廓/曲线控制点：spline 曲线路径；lathe 母线(x=半径,y=高)；extrude 截面(x,z)
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            if (o->type != PT_SPLINE && o->type != PT_LATHE && o->type != PT_EXTRUDE)
                return "{\"error\":\"only spline/lathe/extrude accept points\"}";
            std::vector<Vec3> pts;
            if (McpParsePoints(McpJsonFind(args, "points"), pts) < 2)
                return "{\"error\":\"points format: x,y,z;x,y,z;... (>=2 points)\"}";
            Stop();
            PushUndo();
            o->ctrlPts = pts;
            o->InvalidateMesh();
            m_scene.SyncDisplayToLocalAll();
            SelectObject(o);
            RefreshAll();
            return "{\"ok\":true,\"count\":" + std::to_string(pts.size()) + "}";
        }
        if (tool == "set_transform") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            if (args.find("\"x\"") != std::string::npos) o->x = McpJsonNum(args, "x", o->x);
            if (args.find("\"y\"") != std::string::npos) o->y = McpJsonNum(args, "y", o->y);
            if (args.find("\"z\"") != std::string::npos) o->z = McpJsonNum(args, "z", o->z);
            if (args.find("\"rx\"") != std::string::npos) o->rx = McpJsonNum(args, "rx", o->rx);
            if (args.find("\"ry\"") != std::string::npos) o->ry = McpJsonNum(args, "ry", o->ry);
            if (args.find("\"rz\"") != std::string::npos) o->rz = McpJsonNum(args, "rz", o->rz);
            if (args.find("\"sx\"") != std::string::npos) o->sx = McpJsonNum(args, "sx", o->sx);
            if (args.find("\"sy\"") != std::string::npos) o->sy = McpJsonNum(args, "sy", o->sy);
            if (args.find("\"sz\"") != std::string::npos) o->sz = McpJsonNum(args, "sz", o->sz);
            m_scene.SyncDisplayToLocalAll();
            SelectObject(o);
            RefreshAll();
            return "{\"ok\":true}";
        }
        if (tool == "set_color") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            o->r = McpJsonNum(args, "r", o->r); o->g = McpJsonNum(args, "g", o->g); o->b = McpJsonNum(args, "b", o->b);
            if (args.find("\"alpha\"") != std::string::npos) o->alpha = McpJsonNum(args, "alpha", o->alpha);
            SelectObject(o);
            RefreshAll();
            return "{\"ok\":true}";
        }
        if (tool == "set_visible") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            o->visible = McpJsonBool(args, "visible", o->visible);
            m_scene.SyncDisplayToLocalAll();
            RefreshAll();
            return "{\"ok\":true}";
        }
        if (tool == "set_params") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            if (args.find("\"p1\"") != std::string::npos) o->params[0] = McpJsonNum(args, "p1", o->params[0]);
            if (args.find("\"p2\"") != std::string::npos) o->params[1] = McpJsonNum(args, "p2", o->params[1]);
            if (args.find("\"p3\"") != std::string::npos) o->params[2] = McpJsonNum(args, "p3", o->params[2]);
            if (args.find("\"p4\"") != std::string::npos) o->params[3] = McpJsonNum(args, "p4", o->params[3]);   // 圆柱封盖模式 0/1/2
            // 语义别名：cylinder 顶半径 / 封盖模式
            if (args.find("\"topRadius\"") != std::string::npos) o->params[2] = McpJsonNum(args, "topRadius", o->params[2]);
            if (args.find("\"cap\"") != std::string::npos) o->params[3] = McpJsonNum(args, "cap", o->params[3]);
            if (args.find("\"segments\"") != std::string::npos) o->segments = (int)McpJsonNum(args, "segments", o->segments);
            m_scene.SyncDisplayToLocalAll();
            SelectObject(o);
            RefreshAll();
            return "{\"ok\":true}";
        }
        if (tool == "set_property") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            std::string prop = McpJsonFind(args, "property");
            double v = McpJsonNum(args, "value", 0);
            bool vb = McpJsonBool(args, "value", false);
            if (prop == "x") o->x = v; else if (prop == "y") o->y = v; else if (prop == "z") o->z = v;
            else if (prop == "rx") o->rx = v; else if (prop == "ry") o->ry = v; else if (prop == "rz") o->rz = v;
            else if (prop == "sx") o->sx = v; else if (prop == "sy") o->sy = v; else if (prop == "sz") o->sz = v;
            else if (prop == "r") o->r = v; else if (prop == "g") o->g = v; else if (prop == "b") o->b = v;
            else if (prop == "alpha") { double a = v; if (a < 0) a = 0; if (a > 1) a = 1; o->alpha = a; }
            else if (prop == "visible") o->visible = vb;
            else if (prop == "locked") o->locked = vb;
            else if (prop == "segments") o->segments = (int)v;
            else if (prop == "p1") o->params[0] = v;
            else if (prop == "p2") o->params[1] = v;
            else if (prop == "p3") o->params[2] = v;
            else if (prop == "p4") o->params[3] = v;   // 圆柱封盖模式 0/1/2
            else if (prop == "text") o->text = McpToW(McpJsonFind(args, "value"));
            else return "{\"error\":\"unknown property: " + McpJsonEscape(prop) + "\"}";
            m_scene.SyncDisplayToLocalAll();
            SelectObject(o);
            RefreshAll();
            return "{\"ok\":true}";
        }
        if (tool == "rename") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            o->name = McpToW(McpJsonFind(args, "new_name"));
            RefreshAll();
            return "{\"ok\":true}";
        }
        if (tool == "delete_object") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            Stop();
            PushUndo();
            m_scene.RemoveObjectFromAllLayerKeys(o);
            m_scene.DeleteObject(o);
            ClearSelection();
            RefreshAll();
            return "{\"ok\":true}";
        }
        if (tool == "boolean") {
            std::string an = McpJsonFind(args, "a");
            std::string bn = McpJsonFind(args, "b");
            int op = (int)McpJsonNum(args, "op", 0);
            if (op < 0 || op > 2) op = 0;
            Object3D* A = FindSceneObjectByName(McpToW(an));
            Object3D* B = FindSceneObjectByName(McpToW(bn));
            if (!A) return "{\"error\":\"a not found: " + McpJsonEscape(an) + "\"}";
            if (!B || A == B) return "{\"error\":\"b not found: " + McpJsonEscape(bn) + "\"}";
            // 先纯计算：成功再改场景；失败无副作用，不弹模态框（MCP 无人值守）
            A->RebuildBooleanMesh(A, B, op);
            if (A->_meshVerts.size() < 3)
                return "{\"error\":\"布尔结果为空：两物体可能不相交，或图元不是闭合实体（平面/曲线不支持）\"}";
            Stop();
            PushUndo();
            A->type = PT_BOOLEAN;
            RemoveFromSelection(B);
            if (m_selected == B) m_selected = A;
            m_scene.RemoveObjectFromAllLayerKeys(B);
            m_scene.DeleteObject(B);
            SelectObject(A);
            RefreshAll();
            return "{\"ok\":true,\"op\":" + std::to_string(op) + ",\"result\":\"" +
                   McpJsonEscape(McpToA(A->name)) + "\",\"tris\":" + std::to_string(A->_meshVerts.size() / 3) + "}";
        }
        if (tool == "select") {
            std::string nm = McpJsonFind(args, "name");
            Object3D* o = nm.empty() ? NULL : FindSceneObjectByName(McpToW(nm));
            if (McpJsonBool(args, "add", false) && o) AddToSelection(o, true);   // add=true 追加多选（Ctrl+点选）
            else SelectObject(o);
            return "{\"ok\":true,\"selCount\":" + std::to_string(m_selSet.size()) + "}";
        }
        if (tool == "copy") {           // 复制当前选中（可多选）到内部剪贴板
            DoDuplicate();
            return "{\"ok\":true,\"clipboard\":" + std::to_string(m_clipboard.size()) + "}";
        }
        if (tool == "cut") {            // 剪切当前选中（可多选）到内部剪贴板并从场景移除
            int before = (int)m_clipboard.size();
            OnCut();
            (void)before;
            return "{\"ok\":true,\"clipboard\":" + std::to_string(m_clipboard.size()) + "}";
        }
        if (tool == "paste") {          // 粘贴内部剪贴板（与 Ctrl+V 同一函数）
            if (m_clipboard.empty()) return "{\"error\":\"clipboard empty\"}";
            int n = (int)m_clipboard.size();
            OnPaste();
            return "{\"ok\":true,\"pasted\":" + std::to_string(n) + "}";
        }
        if (tool == "enter_group") {
            std::string nm = McpJsonFind(args, "name");
            if (nm.empty()) { SetEditLevel(NULL); }
            else {
                Object3D* o = FindSceneObjectByName(McpToW(nm));
                if (!o) return "{\"error\":\"not found\"}";
                EnterGroup(o);
            }
            return "{\"ok\":true}";
        }
        if (tool == "exit_group") { ExitGroup(); return "{\"ok\":true}"; }
        if (tool == "set_camera") {
            if (args.find("\"yaw\"") != std::string::npos) m_scene.cam.yaw = McpJsonNum(args, "yaw", m_scene.cam.yaw);
            if (args.find("\"pitch\"") != std::string::npos) m_scene.cam.pitch = McpJsonNum(args, "pitch", m_scene.cam.pitch);
            if (args.find("\"dist\"") != std::string::npos) m_scene.cam.dist = McpJsonNum(args, "dist", m_scene.cam.dist);
            if (args.find("\"tx\"") != std::string::npos) m_scene.cam.tx = McpJsonNum(args, "tx", m_scene.cam.tx);
            if (args.find("\"ty\"") != std::string::npos) m_scene.cam.ty = McpJsonNum(args, "ty", m_scene.cam.ty);
            if (args.find("\"tz\"") != std::string::npos) m_scene.cam.tz = McpJsonNum(args, "tz", m_scene.cam.tz);
            RefreshAll();
            m_viewport.UpdateWindow();   // 强制同步重绘，确保视角变化立即可见
            char cbuf[192];
            sprintf_s(cbuf, "{\"ok\":true,\"yaw\":%.2f,\"pitch\":%.2f,\"dist\":%.2f,\"tx\":%.2f,\"ty\":%.2f,\"tz\":%.2f}",
                m_scene.cam.yaw, m_scene.cam.pitch, m_scene.cam.dist,
                m_scene.cam.tx, m_scene.cam.ty, m_scene.cam.tz);
            return std::string(cbuf);
        }
        if (tool == "set_frame") { SetCurrentFrame((int)McpJsonNum(args, "frame", 0)); return "{\"ok\":true}"; }
        if (tool == "play") { Play(); return "{\"ok\":true}"; }
        if (tool == "stop") { Stop(); return "{\"ok\":true}"; }
        if (tool == "load_sample") {
            std::string nm = McpJsonFind(args, "name");
            if (nm.empty()) return "{\"error\":\"no name\"}";
            std::wstring w = McpToW(nm);
            LoadSampleFile(SampleDir() + w + L"\\" + w + L".s3d");
            return "{\"ok\":true}";
        }
        if (tool == "new_scene") { DoNew(); return "{\"ok\":true}"; }
        if (tool == "save") { DoSave(); return "{\"ok\":true}"; }
        if (tool == "export_html") {
            std::string path = McpJsonFind(args, "path");
            if (path.empty()) return "{\"error\":\"缺少 path（输出 HTML 绝对路径）\"}";
            bool ok = ExportToFile(McpToW(path));
            return ok ? "{\"ok\":true,\"path\":\"" + McpJsonEscape(path) + "\"}"
                      : "{\"error\":\"导出失败：无法写入文件\"}";
        }
        if (tool == "add_keyframe") {
            Object3D* o = FindSceneObjectByName(McpToW(McpJsonFind(args, "name")));
            if (!o) return "{\"error\":\"not found\"}";
            // 关键帧必须写入图层（InsertLayerKey）：播放时 Scene3D::SetFrame 按图层关键帧
            // 插值驱动运动；仅写对象 keys（InsertKey）不会驱动播放（火箭不动 bug 根因）。
            Object3D* r = o;
            while (r->parent) r = r->parent;
            int li = m_scene.RootLayerOf(r);
            if (li < 0 || li >= (int)m_scene.layers.size()) return "{\"error\":\"no layer\"}";
            int fr = (int)McpJsonNum(args, "frame", 0);
            // MCP 可直接传目标位置（x/y/z）：InsertLayerKey 记录的是“当前显示位置”，
            // 若只插帧不先移动，所有关键帧位置相同，播放时对象纹丝不动（“发射动画没有”根因）。
            if (args.find("\"x\"") != std::string::npos) r->x = McpJsonNum(args, "x", r->x);
            if (args.find("\"y\"") != std::string::npos) r->y = McpJsonNum(args, "y", r->y);
            if (args.find("\"z\"") != std::string::npos) r->z = McpJsonNum(args, "z", r->z);
            m_scene.InsertLayerKey(li, fr);
            m_scene.SetFrame(fr);
            m_viewport.Repaint();
            m_timeline.Invalidate(FALSE);
            return "{\"ok\":true}";
        }
        if (tool == "execute_script") {
            // 代码能力：设置文档类/主场景脚本并立即执行一次（播放开始时 RunScriptOnce 会再执行）
            std::string txt = McpJsonFind(args, "text");
            if (txt.empty()) return "{\"error\":\"empty script\"}";
            std::string kind = McpJsonFind(args, "kind");
            if (kind.empty()) kind = "doc";
            std::wstring ws = McpToW(txt);
            if (kind == "scene") {
                m_scene.script = ws;
                m_script.SetText(m_scene.script);
                std::wstring err;
                if (!m_script.EnsureParsed(err))
                    return "{\"error\":\"" + McpJsonEscape(McpToA(err)) + "\"}";
                m_script.RunOnce(&m_scene);
                m_scriptWnd.SetOutput(m_script.GetLog().c_str());
                m_scene.SyncDisplayToLocalAll();
                m_viewport.Repaint();
                return "{\"ok\":true,\"log\":\"" + McpJsonEscape(McpToA(m_script.GetLog())) + "\"}";
            }
            m_scene.docClass = ws;
            m_docScript.SetText(m_scene.docClass);
            std::wstring err;
            if (!m_docScript.EnsureParsed(err))
                return "{\"error\":\"" + McpJsonEscape(McpToA(err)) + "\"}";
            m_docScript.RunOnce(&m_scene);
            m_scriptWnd.SetOutput(m_docScript.GetLog().c_str());
            m_scene.SyncDisplayToLocalAll();
            SetDocText(m_scene.docClass.c_str());
            SetDocMode(true);
            m_viewport.Repaint();
            return "{\"ok\":true,\"log\":\"" + McpJsonEscape(McpToA(m_docScript.GetLog())) + "\"}";
        }
        return "{\"error\":\"unknown tool: " + McpJsonEscape(tool) + "\"}";
    } catch (...) {
        return "{\"error\":\"exception\"}";
    }
}

LRESULT CMainWnd::OnMcpCmd(WPARAM wParam, LPARAM lParam) {
    McpCmd* cmd = (McpCmd*)wParam;
    if (cmd) {
        std::string body;
        if (cmd->method == "tools/call") {
            std::string r = McpExecute(cmd->toolName, cmd->argsJson);
            // 统一包装为 MCP 结果（若工具直接返回了 isError 结构则原样透传）
            if (r.find("\"error\"") != std::string::npos && r.find("\"content\"") == std::string::npos) {
                std::string txt = r;
                size_t p = txt.find("\"error\":\"");
                if (p != std::string::npos) {
                    txt = txt.substr(p + 10);
                    size_t e = txt.find("\"");
                    if (e != std::string::npos) txt = txt.substr(0, e);
                }
                body = "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"" + McpJsonEscape(txt) + "\"}]}";
            } else {
                body = "{\"content\":[{\"type\":\"text\",\"text\":\"" + McpJsonEscape(r) + "\"}]}";
            }
        } else {
            body = cmd->resultJson;
        }
        cmd->resultJson = body;
        SetEvent((HANDLE)cmd->doneEvent);
    }
    return 0;
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
    if (!m_docScriptMode || !m_docEdt.GetSafeHwnd()) { HideAutoComplete(); return; }
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
    // 找最后一个 '.'，'.' 前回溯完整标识符路径（支持包路径：super3d.events. 提示包内类）
    size_t dot = before.find_last_of(L'.');
    if (dot == std::wstring::npos) { HideAutoComplete(); return; }
    std::wstring path;
    {
        size_t k = dot;
        while (k > 0) {
            std::wstring seg;
            while (k > 0) {
                wchar_t c = before[k - 1];
                if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
                    (c >= L'0' && c <= L'9') || c == L'_') { seg = c + seg; k--; }
                else break;
            }
            if (seg.empty()) break;
            path = (path.empty() ? seg : seg + L"." + path);
            if (k > 0 && before[k - 1] == L'.') { k--; continue; }   // 继续回溯上一段（包路径）
            break;
        }
    }
    std::wstring ident = path;
    if (ident.empty()) { HideAutoComplete(); return; }
    if (path != L"super3d" && path != L"super3d.events" && path != L"super3d.display" && path != L"super3d.input" &&
        ident != L"Math" && ident != L"Event" && ident != L"KeyboardEvent" && ident != L"Key" && ident != L"stage") {
        if (!FindSceneObjectByName(ident)) { HideAutoComplete(); return; }   // 非场景对象、非内置类、非包路径：无候选
    }
    // '.' 后已输入前缀：必须全为标识符字符（如 "mc.x = 100" 中 '.' 后含空格则不再补全）
    m_autoPrefix = before.substr(dot + 1);
    for (size_t i = 0; i < m_autoPrefix.size(); i++) {
        wchar_t c = m_autoPrefix[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'_')) { HideAutoComplete(); return; }
    }
    // 候选：静态类成员（Math/Event/KeyboardEvent/Key/stage）或场景对象 Object3D 属性
    static const wchar_t* kMathMembers[] = { L"sin", L"cos", L"tan", L"asin", L"acos", L"atan", L"atan2",
        L"sqrt", L"pow", L"abs", L"floor", L"ceil", L"round", L"exp", L"log", L"min", L"max", L"random", L"PI" };
    static const wchar_t* kEventMembers[] = { L"ENTER_FRAME" };
    static const wchar_t* kKeyboardEventMembers[] = { L"KEY_DOWN", L"KEY_UP" };
    static const wchar_t* kKeyMembers[] = { L"isDown" };
    static const wchar_t* kStageMembers[] = { L"addEventListener", L"keyCode" };
    static const wchar_t* kEventPackage[] = { L"Event", L"KeyboardEvent" };   // super3d.events. 包内类
    static const wchar_t* kDisplayPackage[] = { L"Object3D" };                // super3d.display. 包内类
    static const wchar_t* kInputPackage[] = { L"Key" };                        // super3d.input. 包内类
    static const wchar_t* kSuper3dRoot[] = { L"display", L"events", L"input" };  // super3d. 子包
    static const wchar_t* props[] = { L"x", L"y", L"z", L"rx", L"ry", L"rz",
        L"rotation", L"rotationX", L"rotationY", L"rotationZ",
        L"sx", L"sy", L"sz", L"s", L"scaleX", L"scaleY", L"scaleZ",
        L"visible", L"name", L"parent", L"type", L"segments",
        L"topRadius", L"cap",   // 圆柱：顶半径（截锥）/ 封盖模式 0实心 1开口 2碗状
        L"red", L"green", L"blue", L"alpha" };
    const wchar_t* const* src = NULL; int srcN = 0;
    if (path == L"super3d") { src = kSuper3dRoot; srcN = (int)(sizeof(kSuper3dRoot) / sizeof(kSuper3dRoot[0])); }
    else if (path == L"super3d.events") { src = kEventPackage; srcN = (int)(sizeof(kEventPackage) / sizeof(kEventPackage[0])); }
    else if (path == L"super3d.display") { src = kDisplayPackage; srcN = (int)(sizeof(kDisplayPackage) / sizeof(kDisplayPackage[0])); }
    else if (path == L"super3d.input") { src = kInputPackage; srcN = (int)(sizeof(kInputPackage) / sizeof(kInputPackage[0])); }
    else if (ident == L"Math") { src = kMathMembers; srcN = (int)(sizeof(kMathMembers) / sizeof(kMathMembers[0])); }
    else if (ident == L"Event") { src = kEventMembers; srcN = (int)(sizeof(kEventMembers) / sizeof(kEventMembers[0])); }
    else if (ident == L"KeyboardEvent") { src = kKeyboardEventMembers; srcN = (int)(sizeof(kKeyboardEventMembers) / sizeof(kKeyboardEventMembers[0])); }
    else if (ident == L"Key") { src = kKeyMembers; srcN = (int)(sizeof(kKeyMembers) / sizeof(kKeyMembers[0])); }
    else if (ident == L"stage") { src = kStageMembers; srcN = (int)(sizeof(kStageMembers) / sizeof(kStageMembers[0])); }
    else { src = props; srcN = (int)(sizeof(props) / sizeof(props[0])); }
    m_autoItems.clear();
    for (int i = 0; i < srcN; i++) {
        std::wstring p = src[i];
        if (m_autoPrefix.empty() || p.compare(0, m_autoPrefix.size(), m_autoPrefix) == 0)
            m_autoItems.push_back(p);
    }
    if (m_autoItems.empty()) { HideAutoComplete(); return; }
    ShowAutoCompleteList(pos);
}

void CMainWnd::ShowAutoCompleteList(long pos) {
    if (!m_docEdt.GetSafeHwnd() || !m_autoList.GetSafeHwnd()) { return; }   // 列表创建失败时静默禁用自动补全
    // 候选框贴住当前光标位置。不再用 EM_POSFROMCHAR：输入过程中 msftedit 布局未就绪，
    // 查询字符坐标会内部越界崩溃（堆栈 msftedit!5dc375d7 ← ShowAutoCompleteList）。
    CPoint sp = GetCaretPos();   // 客户区坐标（插入点位置）
    if (sp.x < 0 || sp.y < 0) sp = CPoint(0, 0);
    sp.y += 18;
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
        out += std::wstring(outIndent * 4, L' ') + trimmed + L"\r\n";   // 每层 4 空格（用户规范：与 C++/AS3 一致）
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
    else ApplyDocSyntax(true);                // 已规范也强制重扫着色，Ctrl+F 始终有响应（格式化后全量失效）
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

#ifndef EM_GETSCROLLPOS
#define EM_GETSCROLLPOS (WM_USER + 221)
#define EM_SETSCROLLPOS (WM_USER + 222)
#endif

void CMainWnd::SetDocText(const wchar_t* t) {
    m_bUpdatingScript = true;
    // 文档类脚本加载后自动规范化缩进（每层 4 空格），无需用户手动 Ctrl+F
    std::wstring src(t ? t : L"");
    std::wstring fmt = FormatSsCode(src);
    m_lastSyntaxText.clear();   // 换文档强制全量着色，避免沿用旧文档格式导致单词错色/双色
    m_lastColorNorm.clear();    // 同步清空颜色缓存，防止旧文档的逐字符颜色误判
    m_docColorId.clear();
    m_docEdt.SetWindowText((fmt != src) ? fmt.c_str() : t);
    m_bUpdatingScript = false;
    ApplyDocSyntax(true);   // 设色 + 全量失效（切 tab/加载时布局未就绪，行级失效查坐标会崩）
}

void CMainWnd::OnDocEdtChange() {
    if (m_bUpdatingScript) return;   // 程序设置文本（SetDocText）不触发
    m_syntaxDirty = true;
    SetTimer(2, 250, NULL);          // 防抖：停止输入 250ms 后全文重扫着色（同色跳过：只重绘变化的 token）
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
void CMainWnd::OnDocMenuUndo() {
    m_docEdt.SetFocus();
    CStringW before;
    m_docEdt.GetWindowText(before);
    for (int k = 0; k < 20; k++) {   // 跳过"格式撤销"，直到文本真正变化（着色不干扰撤销）
        m_docEdt.SendMessage(WM_UNDO);
        CStringW after;
        m_docEdt.GetWindowText(after);
        if (after != before) break;
    }
}
void CMainWnd::OnDocMenuCut() { m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_CUT); }
void CMainWnd::OnDocMenuCopy() { m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_COPY); }
void CMainWnd::OnDocMenuPaste() { m_docEdt.SetFocus(); m_docEdt.SendMessage(WM_PASTE); }
void CMainWnd::OnDocMenuSelectAll() { m_docEdt.SetFocus(); m_docEdt.SetSel(0, -1); }
void CMainWnd::OnDocMenuFormat() { m_docEdt.SetFocus(); DoFormatDoc(); }

bool CMainWnd::SetTokenColor(long s, long i, COLORREF color) {
    if (i <= s) return false;
    m_docEdt.SetSel(s, i);
    // 直接以 CHARFORMAT2W 发 EM_SETCHARFORMAT：
    // ① 不用 MFC SetSelectionCharFormat（它内部强制 cbSize=sizeof(CHARFORMAT)=60，
    //    msftedit.dll 对 60 字节格式可能不应用颜色→显示全黑）；
    // ② 不先 GET 同色跳过（GET/掩码干扰曾导致 SET 失效=全黑），每次带掩码直接 SET。
    CHARFORMAT2W cf; memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_COLOR | CFM_FACE;
    cf.dwEffects = 0;
    cf.crTextColor = color;
    cf.wWeight = FW_NORMAL;
    wcscpy_s(cf.szFaceName, L"Consolas");
    m_docEdt.SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    return true;
}

// fullInvalidate=true：文档加载/切换（SetDocText/Ctrl+F）路径。
// 此时 RichEdit 刚 SetWindowText，内部布局可能尚未就绪，
// 行级失效用 EM_POSFROMCHAR 查行坐标会让 msftedit 内部越界崩溃
// （堆栈 msftedit!51c075d7 ← ApplyDocSyntax 行 4060 ← SetDocText ← OnDocSS）。
// 全量失效 InvalidateRect(NULL) 只标脏区、不查布局坐标，重绘在消息循环里完成（届时布局已就绪）→ 安全。
// fullInvalidate=false：输入防抖路径（编辑器已激活、布局就绪）。
//
// 关键防滚动：旧实现逐 token SetSel+EM_SETCHARFORMAT，而 SetSel 会把视图滚到选区，
// 长代码每 250ms 从首行扫到末行 → 视图反复滚动/闪烁。现在：
//   1) 先扫描得到“新文本逐字符颜色 id”；
//   2) 着色期间 WM_SETREDRAW 关闭重绘（中间 SetSel 不再滚动/重绘）；
//   3) 仅对颜色确实变化的 token 设色（与上次缓存逐字符对比，未变跳过）；
//   4) 恢复光标与滚动位置，开重绘，只对可见区做一次失效重绘。
void CMainWnd::ApplyDocSyntax(bool fullInvalidate) {
    if (!m_docEdt.GetSafeHwnd()) return;
    CStringW txt;
    m_docEdt.GetWindowText(txt);
    std::wstring cur(txt.GetString(), txt.GetLength());
    if (cur.empty()) {
        m_lastSyntaxText.clear();
        m_lastColorNorm.clear();
        m_docColorId.clear();
        return;
    }
    // 文本未变化：跳过（光标/选区变化不触发重扫）
    if (cur == m_lastSyntaxText) return;
    // 索引对齐（单词双色根因）：GetWindowText 的 \r\n 占 2 字符，RichEdit 段落标记只占 1 位。
    // 去掉所有 \r（\r\n→\n）后，字符串索引与 RichEdit 字符索引一致。
    std::wstring cur2;
    cur2.reserve(cur.size());
    for (size_t k = 0; k < cur.size(); k++) if (cur[k] != L'\r') cur2.push_back(cur[k]);
    int n = (int)cur2.size();
    const wchar_t* p = cur2.c_str();

    // 颜色：0黑 1关键字蓝 2类型紫 3注释绿 4字符串暗红 5数字蓝
    const COLORREF C_BLACK = RGB(0, 0, 0), C_KW = RGB(0, 0, 255), C_TYPE = RGB(128, 0, 128),
                   C_COM = RGB(0, 128, 0), C_STR = RGB(128, 0, 0), C_NUM = RGB(0, 0, 255);
    std::vector<unsigned char> newId(n, 0);
    struct SynTok { long s, i; COLORREF col; unsigned char id; };
    std::vector<SynTok> toks;
    toks.reserve(n / 4 + 8);
    auto markToken = [&](int s, int i, COLORREF col, unsigned char id) {
        if (i <= s) return;
        toks.push_back(SynTok{ (long)s, (long)i, col, id });
        for (int k = s; k < i; k++) newId[k] = id;
    };

    // 全文扫描（不改任何格式，仅得到颜色表）
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
            markToken(s, i, C_COM, 3);
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
            markToken(s, i, C_STR, 4);
            continue;
        }
        // 数字
        if (c >= L'0' && c <= L'9') {
            int s = i;
            while (i < n && (iswalnum(p[i]) || p[i] == L'.')) i++;
            markToken(s, i, C_NUM, 5);
            continue;
        }
        // 标识符
        if (iswalpha(c) || c == L'_') {
            int s = i;
            while (i < n && (iswalnum(p[i]) || p[i] == L'_')) i++;
            std::wstring tok(p + s, i - s);
            COLORREF col = C_BLACK; unsigned char id = 0;
            if (SsIsKeyword(tok)) { col = C_KW; id = 1; }
            else if (SsIsType(tok)) { col = C_TYPE; id = 2; }
            markToken(s, i, col, id);
            continue;
        }
        i++;
    }

    // 缓存按同位置对齐的前提：归一化长度一致。长度一致但边界移动（如插入空格拆词）时，
    // 原来有色、现在变为黑色（非 token 字符）的区间要显式重设为黑，避免残留颜色。
    bool aligned = (m_docColorId.size() == (size_t)n && m_lastColorNorm.size() == (size_t)n);
    if (aligned) {
        int k = 0;
        while (k < n) {
            if (newId[k] == 0 && m_docColorId[k] != 0) {
                int s = k;
                while (k < n && newId[k] == 0 && m_docColorId[k] != 0) k++;
                toks.push_back(SynTok{ (long)s, (long)k, C_BLACK, 0 });
            } else k++;
        }
    }

    // 保存光标与滚动位置；着色期间关闭重绘，避免逐 token SetSel 把视图反复滚动/闪烁
    CHARRANGE sel; sel.cpMin = sel.cpMax = 0;
    m_docEdt.SendMessage(EM_EXGETSEL, 0, (LPARAM)&sel);
    POINT sp0; sp0.x = sp0.y = 0;
    m_docEdt.SendMessage(EM_GETSCROLLPOS, 0, (LPARAM)&sp0);

    m_docEdt.SetRedraw(FALSE);
    m_bUpdatingScript = true;
    for (size_t t = 0; t < toks.size(); t++) {
        long s = toks[t].s, e = toks[t].i;
        bool skip = false;
        // 全量路径（加载/格式化刚 SetWindowText）格式不确定，一律重设，不做缓存跳过
        if (!fullInvalidate && aligned) {
            skip = true;
            for (long k = s; k < e; k++) {
                if (m_docColorId[k] != toks[t].id || m_lastColorNorm[k] != cur2[k]) { skip = false; break; }
            }
        }
        if (!skip) SetTokenColor(s, e, toks[t].col);
    }
    m_docEdt.SendMessage(EM_EXSETSEL, 0, (LPARAM)&sel);   // 先恢复光标（redraw-off 期间不重绘）
    m_bUpdatingScript = false;
    m_docEdt.SetRedraw(TRUE);

    // 记录本次着色结果作为下次增量基准
    m_lastSyntaxText = cur;
    m_lastColorNorm = cur2;
    m_docColorId.swap(newId);

    // 失效重绘：全量路径不查任何布局坐标（SetDocText 后布局可能未就绪，查坐标=msftedit 崩溃）
    if (fullInvalidate) {
        m_docEdt.InvalidateRect(NULL, TRUE);
        return;
    }
    // 输入防抖路径：最后恢复滚动位置（SetSel 扫到文末可能移动过滚动条），
    // 再对可见区做一次失效重绘（redraw-off 已消除中途的滚动/闪烁）。
    m_docEdt.SendMessage(EM_SETSCROLLPOS, 0, (LPARAM)&sp0);
    m_docEdt.InvalidateRect(NULL, FALSE);
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
                m_pendingTrace += s.GetLog();   // 关键帧脚本 trace：实时追加"输出"页
            }
            // 帧动作写的是显示值（dx），必须"显示→本地"持久化，
            // 否则 SyncDisplayToLocalAll（本地→显示）会用旧本地值把脚本效果覆盖回原位
            m_scene.CopyLocalFromDisplayAll();
            m_viewport.Repaint();
        }
    }
    AppendTraceLive(true);   // 关键帧脚本 trace 实时刷到"输出"页
    if (!errs.empty()) ShowCompileErrors(errs);
}

void CMainWnd::OnHelp() {
    // 帮助菜单【使用说明】：用默认浏览器打开 Super3D使用说明.html
    // （从 exe 所在目录逐级向上找 html：exe 可能在 Win32\Release 下）
    wchar_t exe[MAX_PATH];
    if (!::GetModuleFileNameW(NULL, exe, MAX_PATH)) {
        AfxMessageBox(_T("无法定位程序目录。"), MB_ICONWARNING);
        return;
    }
    std::wstring dir = exe;
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir.erase(slash);
    for (int up = 0; up < 6; up++) {
        std::wstring html = dir + L"\\Super3D使用说明.html";
        DWORD attr = ::GetFileAttributesW(html.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            HINSTANCE r = ::ShellExecuteW(NULL, L"open", html.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)r <= 32) AfxMessageBox(_T("无法打开使用说明文档。"), MB_ICONWARNING);
            return;
        }
        size_t p = dir.find_last_of(L"\\/");
        if (p == std::wstring::npos) break;
        dir.erase(p);
    }
    AfxMessageBox(_T("未找到 Super3D使用说明.html（请与程序放在一起）。"), MB_ICONWARNING);
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

void CMainWnd::OnAbout() {
    // 帮助菜单【版权与许可证】：版权声明 + GPL-3.0 无担保声明（滚动文本窗口）
    const wchar_t* msg =
        L"Super3D — 3D Scene Designer for Kids\r\n"
        L"Copyright (C) 2026 FMSer.CN\r\n"
        L"Contact: 3781320752@qq.com\r\n"
        L"----------------------------------------\r\n"
        L"\r\n"
        L"This program is free software: you can redistribute it and/or modify\r\n"
        L"it under the terms of the GNU General Public License as published by\r\n"
        L"the Free Software Foundation, either version 3 of the License, or\r\n"
        L"(at your option) any later version.\r\n"
        L"\r\n"
        L"This program is distributed in the hope that it will be useful,\r\n"
        L"but WITHOUT ANY WARRANTY; without even the implied warranty of\r\n"
        L"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\r\n"
        L"GNU General Public License for more details.\r\n"
        L"\r\n"
        L"You should have received a copy of the GNU General Public License\r\n"
        L"along with this program.  If not, see <https://www.gnu.org/licenses/>.\r\n"
        L"\r\n"
        L"----------------------------------------\r\n"
        L"中文说明\r\n"
        L"本软件按 GNU GPL v3.0（或更高版本）协议发布：\r\n"
        L"  可自由使用、修改、分发本软件及其源码；\r\n"
        L"  分发时须保留本版权声明与 GPL-3.0 条款，并向接收方提供源码；\r\n"
        L"  本软件按“现状”提供，不附带任何担保（ABSOLUTELY NO WARRANTY）。\r\n"
        L"完整许可文本见工程根目录 LICENSE 文件。\r\n";
    CHelpFrame* f = new CHelpFrame();   // CFrameWnd 关闭时自动 delete this
    if (!f->CreateHelp((HFONT)GetSmallFont().GetSafeHandle(), L"Super3D 版权与许可证", msg, this)) {
        delete f;
        return;
    }
    f->ShowWindow(SW_SHOW);
    f->UpdateWindow();
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

// 当前工程的显示基名（与 .s3d 文件名一致）：真实路径文件名去扩展名 / 示例名 / 未命名
static std::wstring ProjectBaseName(const std::wstring& path, const std::wstring& title) {
    std::wstring base;
    if (!path.empty()) {
        size_t q = path.find_last_of(L"/\\");
        base = path.substr(q == std::wstring::npos ? 0 : q + 1);
    } else if (!title.empty()) {
        base = title;
    } else {
        base = L"未命名";
    }
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base = base.substr(0, dot);
    return base;
}

void CMainWnd::OnDebug() {
    
    Stop();
    // 每次调试打包：输出页与编译器错误页清空重置
    m_pendingTrace.clear();
    m_lastTraceFlush = 0;
    m_traceSwitched = false;
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
    // 便携版：Player 与主程序同级目录
    if (tmpl.empty()) {
        wchar_t exePath2[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, exePath2, MAX_PATH);
        std::wstring sameDir(exePath2);
        size_t sp = sameDir.find_last_of(L"\\/");
        if (sp != std::wstring::npos) sameDir = sameDir.substr(0, sp + 1) + L"Super3DPlayer.exe";
        std::vector<BYTE> tmp2 = DbgReadFile(sameDir);
        if (!tmp2.empty()) { tpl = sameDir; tmpl = tmp2; }
    }
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
    // 3. 写入调试 exe（放在编辑器 exe 所在目录；文件名与 .s3d 工程名一致，固定名覆盖写入）
    wchar_t outDir[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, outDir, MAX_PATH);
    size_t q = std::wstring(outDir).find_last_of(L"\\/");
    if (q != std::wstring::npos) outDir[q] = 0;            // ...\<Win32|x64>\<Debug|Release>
    std::wstring base = ProjectBaseName(m_scene.projectPath, m_titleName);
    std::wstring out = std::wstring(outDir) + L"\\" + base + L".exe";
    // 清理同目录下历史调试残留 exe（旧时间戳名；正在运行的会被占用，自动跳过）
    WIN32_FIND_DATAW fd;
    std::wstring pattern = std::wstring(outDir) + L"\\Super3D调试_*.exe";
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring fp = std::wstring(outDir) + L"\\" + fd.cFileName;
            ::DeleteFileW(fp.c_str());
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    FILE* f = _wfopen(out.c_str(), L"wb");
    if (!f) { AfxMessageBox(_T("无法写入临时文件（同名调试程序可能正在运行，请先关闭它）。"), MB_ICONERROR); return; }
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
                    o->type == PT_BOOLEAN || o->type == PT_SPLINE || o->type == PT_LEAF || o->type == PT_TEXT3D ||
                    o->type == PT_CAPSULE || o->type == PT_HEMISPHERE || o->type == PT_ROUNDBOX ||
                    o->type == PT_LATHE || o->type == PT_EXTRUDE;
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
    ExportNum(s, o->r); s += L","; ExportNum(s, o->g); s += L","; ExportNum(s, o->b); s += L",";
    ExportNum(s, o->alpha);
    s += L"], \"vis\": "; s += (o->visible ? L"1" : L"0");
    {   // 所属图层（根元件的 layer；子元件跟随根）：导出端按帧可见性用
        const Object3D* rr = o;
        while (rr->parent) rr = rr->parent;
        s += L", \"ly\": "; ExportNum(s, rr->layer);
    }
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
scene.background = new THREE.Color(0xe0e6f0);
var camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 2000);
var renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setPixelRatio(window.devicePixelRatio);
document.body.appendChild(renderer.domElement);
scene.add(new THREE.AmbientLight(0xffffff, @AMB@));
var sun = new THREE.DirectionalLight(0xffffff, @DIF@);
sun.position.set(@LX@, @LY@, @LZ@);
scene.add(sun);

var grid = new THREE.GridHelper(60, 30, 0x99aabb, 0xbcc4d0);
grid.position.y = -0.02;
scene.add(grid);

var anims = [], byName = {}, objects = [];
function build(o, parent, rootIdx) {
    var obj;
    if (o.type === 'group') {
        obj = new THREE.Group();
    } else {
        var seg = Math.min(64, Math.max(4, o.seg));   // 与编辑器细分一致（4~64）
        var geo;
        switch (o.type) {
            case 'box':      geo = new THREE.BoxGeometry(o.p[0], o.p[1], o.p[2]); break;
            case 'sphere':   geo = new THREE.SphereGeometry(o.p[0], seg, Math.max(3, Math.round(seg / 2))); break;
            case 'cylinder': geo = new THREE.CylinderGeometry((o.p[2] > 0 ? o.p[2] : o.p[0]), o.p[0], o.p[1], seg, 1, o.p[3] === 1 || o.p[3] === 2); break;   // 顶半径+封盖(1开口/2碗→两端开口)
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
            ambient: new THREE.Color(0xffffff),
            side: THREE.DoubleSide
        });
        if (o.color.length >= 4 && o.color[3] < 0.999) {   // 透明材质：导出 4 值 color 的第 4 值为 alpha
            mat.transparent = true;
            mat.opacity = o.color[3];
        }
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
    var myIdx = objects.length;
    objects.push(obj);
    obj.userData = {
        name: o.name || '', type: o.type || '',
        idx: myIdx,
        rootIdx: (rootIdx === undefined) ? myIdx : rootIdx,
        baseVis: (o.vis === 1),
        localVis: (o.vis === 1),
        layer: (o.ly === undefined) ? 0 : o.ly
    };
    if (o.name) byName[o.name] = obj;
    if (o.keys && o.keys.length) anims.push({ obj: obj, keys: o.keys });
    parent.add(obj);
    if (o.children) o.children.forEach(function (c) { build(c, obj, obj.userData.rootIdx); });
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
        if (p === 'visible') return (o.userData && 'localVis' in o.userData) ? (o.userData.localVis ? 1 : 0) : (o.visible ? 1 : 0);
        if (p === 'name') return (o.userData && o.userData.name) || '';
        if (p === 'type') return (o.userData && o.userData.type) || '';
        if (p === 'segments') return (o.userData && o.userData.seg) || 16;
        if (p === 'red') return (o.material && o.material.color) ? Math.round(o.material.color.r * 255) : 0;
        if (p === 'green') return (o.material && o.material.color) ? Math.round(o.material.color.g * 255) : 0;
        if (p === 'blue') return (o.material && o.material.color) ? Math.round(o.material.color.b * 255) : 0;
        if (p === 'alpha') return (o.material) ? o.material.opacity : 1;
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
    else if (p === 'visible') { o.visible = (v !== 0 && v !== false && v !== 'false'); if (o.userData) o.userData.localVis = o.visible; }
    else if (p === 'red' || p === 'green' || p === 'blue') {
        if (o.material && o.material.color) {
            var c = Math.max(0, Math.min(1, __num(v) / 255));
            if (p === 'red') o.material.color.r = c;
            else if (p === 'green') o.material.color.g = c;
            else o.material.color.b = c;
        }
    }
    else if (p === 'alpha') {
        if (o.material) {
            var a = Math.max(0, Math.min(1, __num(v)));
            o.material.transparent = (a < 0.999);
            o.material.opacity = a;
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
    if (fn === 'trace' || fn === 'print') {
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
    if (fn === 'sin') return Math.sin(__num(a) * KDEG);
    if (fn === 'cos') return Math.cos(__num(a) * KDEG);
    if (fn === 'tan') return Math.tan(__num(a) * KDEG);
    if (fn === 'sqrt') { var y = __num(a); return y < 0 ? 0 : Math.sqrt(y); }
    if (fn === 'abs') return Math.abs(__num(a));
    if (fn === 'min') return Math.min(__num(a), __num(b));
    if (fn === 'max') return Math.max(__num(a), __num(b));
    if (fn === 'random') return Math.random();
    if (fn === 'play') { __setPlay(true); return null; }
    if (fn === 'stop') { __setPlay(false); return null; }
    if (fn === 'gotoFrame') { __setFrame(__num(a)); return null; }
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
// 图层关键帧集合 → 按帧可见性（与编辑器 LayerContainsAt 语义一致）：
// 无关键帧=编辑态全显示；否则只看当前帧区间最近关键帧的内容是否含该元件（的根）。
function __layerVisible(o, f) {
    var li = o.userData.layer;
    var L = DATA.layers;
    if (li === undefined || !L || !L[li] || !L[li].k || !L[li].k.length) return true;   // 编辑态
    var ks = L[li].k, k0 = null;
    for (var i = 0; i < ks.length; i++) { if (ks[i].f <= f) k0 = ks[i]; else break; }
    if (!k0) return false;                    // 首关键帧之前：空白
    if (!k0.o || !k0.o.length) return false;  // 空关键帧：无元件
    var ridx = o.userData.rootIdx;
    for (var j = 0; j < k0.o.length; j++) if (k0.o[j] === ridx) return true;
    return false;                             // 该元件不属于当前关键帧内容
}
function applyFrame(f) {
    for (var i = 0; i < objects.length; i++) {
        var ob = objects[i];
        // 编辑器语义：本地可见性（脚本可改，localVis）与图层关键帧集合判定（每帧重算）分离
        ob.visible = ob.userData.localVis && __layerVisible(ob, f);
    }
    // 脚本驱动（文档类 onEnterFrame / 逐帧脚本）时，静态单关键帧（无帧间动画）不重复写回：
    // 否则每帧会被拉回关键帧基准，mc.rx++ / dx+=speed 等自增"一直不变化"。
    // 多关键帧的真实动画仍按关键帧插值写回。
    var scriptDriven = DATA.scriptPerFrame || __hasFrameHandler;
    for (var i = 0; i < anims.length; i++) {
        if (scriptDriven && anims[i].keys.length === 1) continue;
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
applyFrame(0);   // 初始画面与第0帧一致
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
    // 图层关键帧集合 → 导出端按帧可见性（与编辑器 LayerContainsAt 语义一致）：
    // 深度优先收集全部对象（顺序必须与 ExportObjJson / 导出端 build() 一致：先自身后子）
    std::vector<const Object3D*> flat;
    std::map<const Object3D*, int> idxMap;
    {
        // 深度优先、先根后子的栈式遍历；入栈逆序保证 pop 顺序与 ExportObjJson / build() 一致
        std::vector<const Object3D*> stk;
        for (size_t i = sc.roots.size(); i > 0; i--) stk.push_back(sc.roots[i - 1]);
        while (!stk.empty()) {
            const Object3D* r = stk.back(); stk.pop_back();
            if (idxMap.find(r) != idxMap.end()) continue;
            idxMap[r] = (int)flat.size(); flat.push_back(r);
            for (size_t j = r->children.size(); j > 0; j--) stk.push_back(r->children[j - 1]);
        }
    }
    data += L", \"layers\": [";
    for (size_t li = 0; li < sc.layers.size(); li++) {
        if (li) data += L",";
        const std::vector<LayerKey>& lks = sc.layers[li].keys;
        data += L"{\"k\": [";
        for (size_t ki = 0; ki < lks.size(); ki++) {
            if (ki) data += L",";
            data += L"{\"f\": "; ExportNum(data, lks[ki].frame);
            data += L", \"o\": [";
            bool oFirst = true;
            for (size_t j = 0; j < lks[ki].objs.size(); j++) {
                if (!lks[ki].objs[j]) continue;
                std::map<const Object3D*, int>::const_iterator it = idxMap.find(lks[ki].objs[j]);
                if (it == idxMap.end()) continue;
                if (!oFirst) data += L",";
                oFirst = false;
                ExportNum(data, it->second);
            }
            data += L"]}";
        }
        data += L"]}";
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
    // 灯光参数：用当前编辑环境的灯光设置
    wchar_t tmp[64];
    swprintf(tmp, 64, L"%.2f", Renderer::s_lightAmbient);
    pos = html.find(L"@AMB@"); if (pos != std::wstring::npos) html.replace(pos, 5, tmp);
    swprintf(tmp, 64, L"%.2f", Renderer::s_lightDiffuse);
    pos = html.find(L"@DIF@"); if (pos != std::wstring::npos) html.replace(pos, 5, tmp);
    swprintf(tmp, 64, L"%.1f", Renderer::s_lightDirX*50);
    pos = html.find(L"@LX@"); if (pos != std::wstring::npos) html.replace(pos, 4, tmp);
    swprintf(tmp, 64, L"%.1f", Renderer::s_lightDirY*50);
    pos = html.find(L"@LY@"); if (pos != std::wstring::npos) html.replace(pos, 4, tmp);
    swprintf(tmp, 64, L"%.1f", Renderer::s_lightDirZ*50);
    pos = html.find(L"@LZ@"); if (pos != std::wstring::npos) html.replace(pos, 4, tmp);
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
    std::wstring base = ProjectBaseName(m_scene.projectPath, m_titleName);
    CString defHtml = (base + L".html").c_str();
    CFileDialog dlg(FALSE, _T("html"), defHtml,
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
