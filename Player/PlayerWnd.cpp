// PlayerWnd.cpp : 播放器主窗口实现
#include "stdafx.h"
#include "PlayerWnd.h"
#include "Renderer.h"
#include <vector>

BEGIN_MESSAGE_MAP(CPlayerWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_COMMAND(ID_PLY_OPEN, &CPlayerWnd::OnOpen)
    ON_COMMAND(ID_PLY_PLAY, &CPlayerWnd::OnPlay)
    ON_COMMAND(ID_PLY_REPLAY, &CPlayerWnd::OnReplay)
    ON_COMMAND(ID_PLY_RESETVIEW, &CPlayerWnd::OnResetView)
    ON_COMMAND(ID_PLY_PACK, &CPlayerWnd::OnPack)
    ON_COMMAND(ID_PLY_EXIT, &CPlayerWnd::OnExit)
END_MESSAGE_MAP()

// 打包数据标记：追加在 exe 末尾，格式 = 标记 + 8位十六进制长度 + UTF-8 文本
static const char kPackMark[] = "SUPER3DPACK1\r\n";

// Flash 风格"编译器错误"消息框（播放器调试通道：关键帧脚本错误自动弹出）
class CCompileErrorWnd : public CFrameWnd {
public:
    CEdit m_edt;
    CStatic m_stat;
    CButton m_ok;
    CCompileErrorWnd() {}
    BOOL CreateError(const wchar_t* title, const wchar_t* body, const wchar_t* summary, CWnd* owner) {
        if (!Create(NULL, title, WS_OVERLAPPEDWINDOW, CRect(0, 0, 760, 320), owner, NULL)) return FALSE;
        CenterWindow(owner);
        ShowWindow(SW_SHOW);
        m_edt.SetWindowText(body);
        m_stat.SetWindowText(summary);
        m_ok.SetFocus();
        return TRUE;
    }
    afx_msg int OnCreate(LPCREATESTRUCT) {
        CRect rc; GetClientRect(&rc);
        HFONT f = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        m_edt.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                     CRect(6, 6, rc.Width() - 6, rc.Height() - 44), this, 1);
        m_edt.SendMessage(WM_SETFONT, (WPARAM)f, FALSE);
        m_stat.Create(L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                      CRect(8, rc.Height() - 34, 360, rc.Height() - 10), this, 2);
        m_stat.SendMessage(WM_SETFONT, (WPARAM)f, FALSE);
        m_ok.Create(L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    CRect(rc.Width() - 96, rc.Height() - 34, rc.Width() - 16, rc.Height() - 10), this, 3);
        m_ok.SendMessage(WM_SETFONT, (WPARAM)f, FALSE);
        return 0;
    }
    afx_msg void OnSize(UINT, int cx, int cy) {
        if (!m_edt.GetSafeHwnd()) return;
        m_edt.MoveWindow(6, 6, cx - 12, cy - 46);
        m_stat.MoveWindow(8, cy - 34, 360, 24);
        m_ok.MoveWindow(cx - 96, cy - 34, 80, 24);
    }
    afx_msg void OnOK() { DestroyWindow(); }
    afx_msg void OnClose() { DestroyWindow(); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CCompileErrorWnd, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_BN_CLICKED(3, &CCompileErrorWnd::OnOK)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

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

CPlayerWnd::CPlayerWnd() : m_playing(false), m_tlStopped(false), m_selfContained(false), m_lastKeyScriptFrame(-1) {}

CPlayerWnd::~CPlayerWnd() {}

BOOL CPlayerWnd::CreateMain() {
    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1), NULL);
    CRect rc(0, 0, 1000, 680);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (rc.Width() > sw - 60) rc.right = sw - 60;
    if (rc.Height() > sh - 100) rc.bottom = sh - 100;
    // 顶层窗口统一使用 CreateEx（与编辑软件一致）
    BOOL ok = CreateEx(0, cls, _T("Super3D 播放器"),
        WS_OVERLAPPEDWINDOW, rc, NULL, 0);
    if (ok) CenterWindow();
    return ok;
}

int CPlayerWnd::OnCreate(LPCREATESTRUCT lpcs) {
    if (CWnd::OnCreate(lpcs) == -1) return -1;

    m_fontBtn.CreatePointFont(85, _T("微软雅黑"));
    m_fontSmall.CreatePointFont(80, _T("微软雅黑"));

    // 按钮（宽度按文字实测，避免截断）
    struct { CButton* btn; UINT id; LPCWSTR label; } btns[] = {
        { &m_btnOpen, ID_PLY_OPEN, L"打开..." },
        { &m_btnPlay, ID_PLY_PLAY, L"播放" },
        { &m_btnReplay, ID_PLY_REPLAY, L"重播" },
        { &m_btnResetView, ID_PLY_RESETVIEW, L"重置视角" },
        { &m_btnPack, ID_PLY_PACK, L"打包" },
        { &m_btnExit, ID_PLY_EXIT, L"退出" },
    };
    CClientDC dc(this);
    CFont* old = dc.SelectObject(&m_fontBtn);
    for (int i = 0; i < 6; i++) {
        CSize sz = dc.GetTextExtent(btns[i].label);
        int w = sz.cx + 18; if (w < 52) w = 52;
        btns[i].btn->Create(btns[i].label, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(0, 0, w, 24), this, btns[i].id);
        btns[i].btn->SetFont(&m_fontBtn);
    }
    dc.SelectObject(old);

    m_view.SetOwner(this);
    m_view.CreateView(this, CRect(0, 0, 10, 10));

    m_status.Create(_T("请打开一个 .s3d 工程（也可以直接把 .s3d 文件拖到本播放器上）"),
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_ENDELLIPSIS,
        CRect(0, 0, 10, 10), this, 0);
    m_status.SetFont(&m_fontSmall);

    m_script.SetPlaybackHook(&CPlayerWnd::PlaybackHook, this);
    m_scene.SetFrame(0);
    return 0;
}

void CPlayerWnd::OnDestroy() {
    if (m_playing) KillTimer(1);
    CWnd::OnDestroy();
}

void CPlayerWnd::Layout() {
    CRect rc;
    GetClientRect(&rc);
    int W = rc.Width(), H = rc.Height();
    int y = 4, bh = 24, gap = 4;
    int x = gap;
    // 打包作品模式：隐藏 打开/打包 两个按钮
    CButton* arr[] = {
        &m_btnOpen, &m_btnPlay, &m_btnReplay,
        &m_btnResetView, &m_btnPack, &m_btnExit
    };
    for (int i = 0; i < 6; i++) {
        if (m_selfContained && (arr[i] == &m_btnOpen || arr[i] == &m_btnPack)) continue;
        CRect r;
        arr[i]->GetWindowRect(&r);
        int w = r.Width();
        arr[i]->MoveWindow(x, y, w, bh);
        x += w + gap;
    }
    int barH = 22;
    m_status.MoveWindow(0, H - barH, W, barH);
    m_view.MoveWindow(0, y + bh + 4, W, H - (y + bh + 4) - barH);
}

void CPlayerWnd::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    if (m_view.GetSafeHwnd()) Layout();
}

void CPlayerWnd::SetBtnTexts() {
    m_btnPlay.SetWindowText(m_playing ? _T("暂停") : _T("播放"));
}

void CPlayerWnd::UpdateStatus() {
    CString s;
    if (m_fileName.empty()) return;
    s.Format(_T("文件：%s    帧 %d / %d    fps %d    %s"),
        m_fileName.c_str(), m_scene.frame, m_scene.totalFrames, m_scene.fps,
        m_playing ? _T("播放中") : _T("已停止"));
    m_status.SetWindowText(s);
}

void CPlayerWnd::ViewChanged() {
    UpdateStatus();
}

void CPlayerWnd::Play() {
    if (m_playing) return;
    m_lastKeyScriptFrame = -1;   // 重新播放：关键帧脚本从头开始执行
    m_traceLines.clear();        // 重新播放：trace 输出从头收集
    // 文档类脚本：播放开始时执行一次（Flash 文档类初始化）
    if (!m_scene.docClass.empty()) {
        m_docScript.SetText(m_scene.docClass);
        std::wstring err;
        if (!m_docScript.EnsureParsed(err)) {
            // 文档类语法错误 → Flash 式编译器错误框（自动弹出）
            std::wstring desc; int ln = ParseErrLine(err, desc);
            wchar_t b[512];
            swprintf(b, 512, L"%ls，第%d行，第1列 1001: %ls", L"调试工程", ln, desc.c_str());
            CCompileErrorWnd* w = new CCompileErrorWnd();
            w->CreateError(L"编译器错误", b, L"1个错误，0个警告", this);
        } else {
            m_docScript.RunOnce(&m_scene);
            // 收集文档类 main() 的 trace 输出（调试时在"输出"窗口显示）
            if (!m_docScript.GetLog().empty()) m_traceLines.push_back(m_docScript.GetLog());
            m_scene.SyncDisplayToLocalAll();
            m_view.Repaint();
        }
    }
    m_tlStopped = false;   // 重新播放：时间轴恢复推进（脚本 stop() 状态清除）
    m_playing = true;
    // 播放开始：先执行当前帧的关键帧脚本（起始帧=0，帧号与时间轴/脚本 frame 完全一致）
    ExecuteKeyFrameScripts(m_scene.frame);
    SetTimer(1, 16, NULL);   // 16ms 定时器：键盘采样更跟手，帧推进由时间累积控制
    SetBtnTexts();
    UpdateStatus();
}

void CPlayerWnd::Stop() {
    if (!m_playing) return;
    m_playing = false;
    KillTimer(1);
    m_script.ClearKeys();
    m_docScript.ClearKeys();
    ShowTraceOutput();       // 停止播放：统一显示本次播放收集的 trace 输出
    SetBtnTexts();
    UpdateStatus();
}

// Flash 式"脚本输出"窗口：播放/调试期间收集的 trace 输出统一显示（与编译器错误同款窗口）
void CPlayerWnd::ShowTraceOutput() {
    if (m_traceLines.empty()) return;
    std::wstring body;
    for (size_t i = 0; i < m_traceLines.size(); i++) body += m_traceLines[i];  // 每行自带 \r\n
    wchar_t b[128];
    swprintf(b, 128, L"输出 %d 条", (int)m_traceLines.size());
    CCompileErrorWnd* w = new CCompileErrorWnd();
    w->CreateError(L"脚本输出", body.c_str(), b, this);
}

// 播放进入关键帧时执行该帧脚本（含空白关键帧；每层每帧一个脚本）。
// 关键帧脚本按 Flash 帧动作语义写入"显示值"（与导出网页一致），
// 执行后"显示→本地"持久化，使脚本效果在后续帧保持（SetFrame 跳过脚本关键帧的插值写回）。
void CPlayerWnd::ExecuteKeyFrameScripts(int frame) {
    std::vector<std::wstring> errs;
    std::wstring path = L"调试工程";
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
            if (!s.GetLog().empty()) m_traceLines.push_back(s.GetLog());   // 关键帧脚本 trace
            // 帧动作写的是显示值（dx），必须"显示→本地"持久化，
            // 否则 SyncDisplayToLocalAll（本地→显示）会用旧本地值把脚本效果覆盖回原位
            m_scene.CopyLocalFromDisplayAll();
            m_view.Repaint();
        }
    }
    if (!errs.empty()) {
        std::wstring body;
        for (size_t i = 0; i < errs.size(); i++) {
            body += errs[i];
            if (i + 1 < errs.size()) body += L"\r\n";
        }
        wchar_t b[128];
        swprintf(b, 128, L"%d个错误，0个警告", (int)errs.size());
        CCompileErrorWnd* w = new CCompileErrorWnd();
        w->CreateError(L"编译器错误", body.c_str(), b, this);
    }
}

void CPlayerWnd::JumpFrame(int f) {
    if (f < 0) f = 0;
    if (f >= m_scene.totalFrames) f = m_scene.totalFrames - 1;
    m_scene.SetFrame(f);
    m_view.Repaint();
    UpdateStatus();
}

void CPlayerWnd::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent != 1) { CWnd::OnTimer(nIDEvent); return; }
    if (!m_playing) return;
    // 键盘状态高频采样（60Hz 定时器下按键更跟手，与 HTML 导出一致）
    // 文档类常驻整个运行周期：无论主脚本开关，都采样并分发事件；主脚本按需采样
    {
        // 每帧直接读物理键盘状态喂给脚本引擎（Key.isDown 查询）。
        // 不依赖消息队列/焦点：WM_KEYDOWN 由 PreTranslateMessage 兜底，这里保证漏网消息也能读到。
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
            // 播放结束点 = 最后一图层关键帧（与编辑器一致；无关键帧时沿用总帧数）
            int endFrame = m_scene.LastLayerKeyFrame();
            if (endFrame < 1) endFrame = m_scene.totalFrames - 1;
            if (nf > endFrame) {
                if (m_scene.loopPlay) nf = 0;
                else { nf = endFrame; Stop(); break; }
            }
            // 先关键帧插值，再执行该帧的关键帧脚本（Flash 帧动作），再跑逐帧脚本（脚本写显示值），最后统一渲染
            m_scene.SetFrame(nf);
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
            // 持久化文档类写入的显示值：否则 scriptPerFrame=0 / 无关键帧时，
            // 下一帧 SetFrame 的 SyncDisplayToLocal 会把增量覆盖回初始（"只动一下"）
            m_scene.CopyLocalFromDisplayAll();
            if (m_docScript.LastRunError()) {
                // 文档类运行错误 → Flash 式编译器错误框（调试时自动弹出）
                std::wstring desc; int ln = ParseErrLine(m_docScript.GetLog(), desc);
                wchar_t b[512];
                swprintf(b, 512, L"%ls，第%d行，第1列 1002: %ls", L"调试工程", ln, desc.c_str());
                CCompileErrorWnd* w = new CCompileErrorWnd();
                w->CreateError(L"编译器错误", b, L"1个错误，0个警告", this);
                Stop();
                break;
            }
            if (!m_docScript.GetLog().empty()) m_traceLines.push_back(m_docScript.GetLog());   // 文档类 trace
        }
        if (m_scene.scriptPerFrame) {
            m_script.RunFrame(&m_scene, cur);
            if (m_script.LastRunError()) {
                CString t;
                t.Format(_T("脚本错误：%s（已暂停）"), m_script.GetLog().c_str());
                m_status.SetWindowText(t);
                Stop();
                break;
            }
        }
        advanced = true;
        if (m_tlStopped) break;   // stop()：时间轴停在当前帧，仅执行脚本不再推进
    }
    if (advanced) m_view.Repaint();
    UpdateStatus();
}

void CPlayerWnd::OpenFile(const std::wstring& path) {
    Stop();
    std::wstring err;
    if (!ProjectIO::LoadProject(m_scene, path, err)) {
        CString t;
        t.Format(_T("打开失败：%s"), err.c_str());
        m_status.SetWindowText(t);
        AfxMessageBox(t, MB_ICONWARNING);
        return;
    }
    m_script.SetText(m_scene.script);
    m_fileName = path;
    CString title;
    title.Format(_T("Super3D 播放器 - %s"), path.c_str());
    SetWindowText(title);
    m_scene.SetFrame(0);
    Layout();
    m_view.Repaint();
    UpdateStatus();
    Play(); // 打开后自动播放
}

void CPlayerWnd::PlaybackHook(int cmd, double arg, void* user) {
    CPlayerWnd* w = (CPlayerWnd*)user;
    if (!w) return;
    if (cmd == 1) { // play()：恢复时间轴推进（未在播放时启动播放）
        w->m_tlStopped = false;
        if (!w->m_playing) w->Play();
    } else if (cmd == 2) w->m_tlStopped = true;   // stop()：时间轴停在当前帧，脚本照常执行
    else if (cmd == 3) w->JumpFrame((int)arg);
}

// 键盘状态喂给脚本引擎：Key.isDown(Key.W) 等查询在 RunFrame 中生效。
// 不吞消息（交给 CWnd 继续处理），只记录按下/抬起状态。
// 文档类常驻整个运行周期：无论主脚本开关，都接收键盘状态与事件
BOOL CPlayerWnd::PreTranslateMessage(MSG* pMsg) {
    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN) {
        m_script.SetKeyDown((int)pMsg->wParam);
        m_docScript.SetKeyDown((int)pMsg->wParam);
    } else if (pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP) {
        m_script.SetKeyUp((int)pMsg->wParam);
        m_docScript.SetKeyUp((int)pMsg->wParam);
    } else if (pMsg->message == WM_ACTIVATE && pMsg->wParam == WA_INACTIVE) {
        m_script.ClearKeys();   // 窗口失焦：清掉残留按键，防止键位卡死
        m_docScript.ClearKeys();
    }
    return CWnd::PreTranslateMessage(pMsg);
}

void CPlayerWnd::OnOpen() {
    CFileDialog dlg(TRUE, _T("s3d"), NULL,
        OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        _T("Super3D 工程 (*.s3d)|*.s3d|所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;
    OpenFile((LPCTSTR)dlg.GetPathName());
}

void CPlayerWnd::OnPlay() {
    if (m_fileName.empty()) { OnOpen(); return; }
    if (m_playing) Stop();
    else Play();
}

void CPlayerWnd::OnReplay() {
    if (m_fileName.empty()) { OnOpen(); return; }
    Stop();
    JumpFrame(0);
    Play();
}

void CPlayerWnd::OnResetView() {
    m_scene.cam.Reset();
    m_view.Repaint();
}

void CPlayerWnd::OnExit() {
    PostMessage(WM_CLOSE);
}

// ------------------------------------------------------------ 一键打包
std::vector<BYTE> CPlayerWnd::ReadSelfExe() {
    std::vector<BYTE> buf;
    wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, path, MAX_PATH);
    FILE* f = _wfopen(path, L"rb");
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

std::string CPlayerWnd::WToUtf8(const std::wstring& s) {
    if (s.empty()) return std::string();
    int cch = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    std::string out(cch, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], cch, NULL, NULL);
    return out;
}

std::wstring CPlayerWnd::Utf8ToW(const std::string& s) {
    if (s.empty()) return std::wstring();
    int cch = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring out(cch, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], cch);
    return out;
}

// 从 exe 缓冲里提取打包数据：格式 = 标记 + 8位十六进制长度 + UTF-8 文本
bool CPlayerWnd::ExtractPackData(std::vector<BYTE>& buf, std::wstring& s3dText) {
    if (buf.empty()) return false;
    std::string str(buf.begin(), buf.end());
    size_t pos = str.rfind(kPackMark);
    if (pos == std::string::npos) return false;
    size_t lenPos = pos + strlen(kPackMark);
    if (lenPos + 8 > str.size()) return false;
    long dataLen = strtol(str.substr(lenPos, 8).c_str(), NULL, 16);
    if (dataLen < 0 || lenPos + 8 + (size_t)dataLen > str.size()) return false;
    s3dText = Utf8ToW(str.substr(lenPos + 8, (size_t)dataLen));
    return !s3dText.empty();
}

// 打包作品模式：从自身 exe 末尾读出 .s3d 数据并直接播放
bool CPlayerWnd::LoadSelfContained(std::wstring& err) {
    std::vector<BYTE> self = ReadSelfExe();
    std::wstring text;
    if (!ExtractPackData(self, text)) return false;
    if (!ProjectIO::DeserializeFromString(m_scene, text, err)) return false;
    // 调试打包附带的文档类（doc_class_len 段在 # end 之后，外部 .ss 内容随包传递）
    {
        size_t dm = text.rfind(L"doc_class_len=");
        if (dm != std::wstring::npos) {
            size_t nl = text.find(L'\n', dm);
            if (nl != std::wstring::npos) {
                nl += 1;
                size_t eq = text.find(L'=', dm);
                if (eq != std::wstring::npos) {
                    int len = (int)wcstol(text.substr(eq + 1, nl - eq - 1).c_str(), NULL, 10);
                    if (len >= 0 && (int)(text.size() - nl) >= len)
                        m_scene.docClass = text.substr(nl, (size_t)len);
                }
            }
        }
    }
    m_script.SetText(m_scene.script);
    m_selfContained = true;
    m_btnOpen.ShowWindow(SW_HIDE);
    m_btnPack.ShowWindow(SW_HIDE);
    m_fileName = L"打包作品";
    SetWindowText(_T("Super3D 作品"));
    m_scene.SetFrame(0);
    Layout();
    m_view.Repaint();
    UpdateStatus();
    Play();
    return true;
}

// 严格检查 exe 尾部是否真的是打包数据（标记 + 8位hex + 长度精确吻合），
// 是则剥离并返回 true；否则原样保留（防止误伤 exe 内部代码区的同名字符串）
static bool StripTailPackStrict(std::vector<BYTE>& buf) {
    const size_t markLen = strlen(kPackMark);
    const size_t headLen = markLen + 8;                       // 标记 + 8位hex
    if (buf.size() <= headLen) return false;
    size_t pos = buf.size() - headLen;
    if (memcmp(&buf[pos], kPackMark, markLen) != 0) return false;
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

void CPlayerWnd::OnPack() {
    if (m_fileName.empty() || m_selfContained) {
        AfxMessageBox(_T("请先打开一个 .s3d 工程，再点【打包】。"), MB_ICONINFORMATION);
        return;
    }
    // 模板 = 播放器自身 exe（去掉尾部可能存在的旧打包数据）
    std::vector<BYTE> tpl = ReadSelfExe();
    StripTailPackStrict(tpl);
    if (tpl.empty()) { AfxMessageBox(_T("读取播放器程序失败，无法打包。"), MB_ICONERROR); return; }
    // 序列化当前场景 → UTF-8 字节
    std::wstring text = ProjectIO::SerializeToString(m_scene);
    std::string utf8 = WToUtf8(text);
    // 默认文件名：作品名.exe
    CString defName;
    {
        size_t pos = m_fileName.find_last_of(L"\\/");
        std::wstring base = (pos == std::wstring::npos) ? m_fileName : m_fileName.substr(pos + 1);
        size_t dot = base.find_last_of(L'.');
        if (dot != std::wstring::npos) base = base.substr(0, dot);
        defName = (base + L".exe").c_str();
    }
    CFileDialog dlg(FALSE, _T("exe"), defName,
        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
        _T("Super3D 作品 (*.exe)|*.exe|所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;
    std::wstring outPath = (LPCTSTR)dlg.GetPathName();
    // 写入：模板 + 标记 + 长度 + 数据
    FILE* f = _wfopen(outPath.c_str(), L"wb");
    if (!f) { AfxMessageBox(_T("无法写入文件。"), MB_ICONERROR); return; }
    fwrite(&tpl[0], 1, tpl.size(), f);
    fwrite(kPackMark, 1, strlen(kPackMark), f);
    char lenHex[16];
    sprintf(lenHex, "%08lX", (unsigned long)utf8.size());
    fwrite(lenHex, 1, 8, f);
    fwrite(utf8.data(), 1, utf8.size(), f);
    fclose(f);
    CString msg;
    msg.Format(_T("打包成功！\r\n%s\r\n\r\n把这份 .exe 发给别人，双击就能播放你的作品。"), outPath.c_str());
    AfxMessageBox(msg, MB_ICONINFORMATION);
}
