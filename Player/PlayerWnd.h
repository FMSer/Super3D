// PlayerWnd.h : 播放器主窗口（极简：打开/播放/重播/重置视角/打包/退出 + 状态栏）
#pragma once
#include "stdafx.h"
#include "Scene3D.h"
#include "Script.h"
#include "ProjectIO.h"
#include "PlayerViewWnd.h"

enum { ID_PLY_OPEN = 2000, ID_PLY_PLAY, ID_PLY_REPLAY, ID_PLY_RESETVIEW, ID_PLY_PACK, ID_PLY_EXIT };

class CPlayerWnd : public CWnd {
public:
    CPlayerWnd();
    virtual ~CPlayerWnd();

    BOOL CreateMain();
    Scene3D& Scene() { return m_scene; }
    CFont& GetSmallFont() { return m_fontSmall; }

    // 全局拦截键盘消息喂给脚本引擎（Key.isDown 查询用），任何子窗口有焦点都生效
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    void OpenFile(const std::wstring& path);   // 打开 .s3d 并自动播放
    bool LoadSelfContained(std::wstring& err); // 检测自身是否含打包数据，有则直接播放
    void ViewChanged();                        // 视角变化后刷新状态栏
    void ShowTraceOutput();                    // 停止播放时统一显示 trace 输出（脚本输出窗口）

protected:
    Scene3D m_scene;
    ScriptEngine m_script;
    ScriptEngine m_docScript;   // 文档类脚本（Flash Document Class：播放开始时执行一次）
    int m_lastKeyScriptFrame;   // 关键帧脚本去重：已执行的帧号
    std::vector<std::wstring> m_traceLines;   // 播放/调试期间收集的 trace 输出（停止时弹"脚本输出"窗口）
    CFont m_fontBtn, m_fontSmall;
    CButton m_btnOpen, m_btnPlay, m_btnReplay, m_btnResetView, m_btnPack, m_btnExit;
    CPlayerViewWnd m_view;
    CStatic m_status;
    std::wstring m_fileName;
    bool m_playing;
    bool m_tlStopped;              // 脚本 stop()：时间轴停在当前帧，事件驱动代码照常每帧执行（Flash 语义）
    bool m_selfContained;   // 打包作品模式：隐藏打开/打包按钮

    void Layout();
    void UpdateStatus();
    void SetBtnTexts();
    void Play();
    void Stop();
    void JumpFrame(int f);
    void ExecuteKeyFrameScripts(int frame);   // 进入关键帧时执行该帧脚本（Flash 帧动作）
    static void PlaybackHook(int cmd, double arg, void* user);
    static std::vector<BYTE> ReadSelfExe();
    static std::string WToUtf8(const std::wstring& s);
    static std::wstring Utf8ToW(const std::string& s);
    static bool ExtractPackData(std::vector<BYTE>& buf, std::wstring& s3dText);

    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnOpen();
    afx_msg void OnPlay();
    afx_msg void OnReplay();
    afx_msg void OnResetView();
    afx_msg void OnPack();
    afx_msg void OnExit();
    DECLARE_MESSAGE_MAP()
};
