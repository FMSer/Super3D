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
// MainWnd.h : 主窗口（工具栏 + 场景树 + 属性 + 视口 + 时间轴 + 脚本）
#pragma once
#include "stdafx.h"
#include "Scene3D.h"
#include "Script.h"
#include "ViewportWnd.h"
#include "SceneTreeCtrl.h"
#include "PropertiesPanel.h"
#include "TimelineWnd.h"
#include "ScriptWnd.h"
#include "McpServer.h"
#include "EditCtxMenu.h"
#include <atlimage.h>
#include <deque>
#include <map>

// 文档 Tab 控件（owner-draw）：文字左侧显示，标签右端固定画"×"关闭按钮（仿 Flash/Animate 文档标签）
// 在 WM_LBUTTONDOWN 判断点击右端 18px 区域 → 向父窗口发 WM_USER+55（wParam=项索引）关闭，不切换选中
class CDocTabCtrl : public CTabCtrl {
public:
    CDocTabCtrl() {}
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    DECLARE_MESSAGE_MAP()
};

// 撤销快照：文本部分不含布尔网格缓存（上万顶点序列化成文本太慢），
// 网格按对象名以二进制 vector 侧挂，恢复时按名挂回
struct UndoSnap {
    std::wstring text;
    std::map<std::wstring, std::pair<std::vector<Vec3>, std::vector<Vec3> > > boolMesh;
    // 视图/编辑上下文（撤销/重做时一并恢复，避免退回外层、丢失多选）
    std::vector<std::wstring> editPath;  // 编辑层级：根→当前容器 的名称链（空=主场景根层）
    std::vector<std::wstring> selNames;  // 多选对象名（首个为主选中；空=无选中）
};

class CMainWnd : public CWnd {
public:
    CMainWnd();
    virtual ~CMainWnd();

    enum Tool { TOOL_SELECT = 0, TOOL_MOVE, TOOL_ROTATE, TOOL_SCALE, TOOL_PAN, TOOL_SPLINE };

    BOOL CreateMain();
    bool ExportToFile(const std::wstring& path);   // 导出 Three.js HTML，成功返回 true

    // 供子控件调用的接口
    Scene3D& Scene() { return m_scene; }
    CFont& GetSmallFont() { return m_fontSmall; }
    Object3D* GetSelected() { return m_selected; }
    int GetTool() const { return m_tool; }
    void SelectObject(Object3D* o);
    void EnterGroup(Object3D* g);   // 双击组：进入该元件内部编辑子元件
    void ExitGroup();               // 双击空白：退回上一层编辑
    void SetEditLevel(Object3D* g); // 直接跳转到指定组编辑（g==NULL 回到根层）
    std::vector<Object3D*> EditPath() const;  // 从根到当前编辑层的组链（不含根）
    void SetCurrentFrame(int f);

    // ---- Ctrl 多选 + 转换为元件 ----
    void ClearSelection();                 // 清空多选
    void AddToSelection(Object3D* o, bool makePrimary);
    void RemoveFromSelection(Object3D* o);
    void SetSelectionSet(const std::vector<Object3D*>& objs, Object3D* primary);  // 批量替换整个选中集（框选），只刷新一次 UI
    bool InSelection(Object3D* o) const;
    int SelectionCount() const;
    void RefreshSelectionUI();             // 多选状态变化后的界面刷新
    void SelectAllObjects();               // Ctrl+A：全选当前编辑层级的直接子级
    void GetSelectionSet(std::vector<Object3D*>& out) const;   // 输出当前多选集合（含主选中）
    void WrapToMovieClip();                // 把选中元件转换为一个 MovieClip（组）
    void ShowObjectMenu(CPoint screenPt);  // 视口右键菜单
    void PushUndo();
    void CaptureSnap(UndoSnap& s);       // 快照：场景文本 + 布尔网格 + 编辑层级 + 多选状态
    void OnViewTransformChanged(bool light = false);   // light=true：拖动中只更新场景，不刷新文本避免闪烁
    void OnPropChanged();
    void NotifyPropertyEdit();   // 属性面板编辑中：只更新对象+关键帧+视口，不刷新面板文本
    void NotifyTimelineChanged();   // 时间轴图标切换后轻量刷新（不插关键帧）
    void OnTimelineKeyMoved();
    void OnTreeRenamed();
    void DeleteLayerKeyAt(int layerIdx, int frame);
    void InsertLayerKeyAtFrame(int layerIdx, int frame);   // 时间轴右键：对指定图层在指定帧插入图层关键帧，无需选中
    void InsertEmptyLayerKeyAtFrame(int layerIdx, int frame);
    void AddObjectToKeyAt(int layerIdx, int frame);
    void RemoveObjectFromKeyAt(int layerIdx, int frame);
    void MoveIntoObject(Object3D* o, Object3D* g);   // 场景树拖拽：元件移入组
    void MoveLayer(Object3D* o, int dir);            // 时间轴层级：-1上移 1下移 2置顶 3置底
    void SetCurrentLayerIdx(int idx);      // 时间轴点击图层行：设为当前图层
    void MoveObjectToLayer(Object3D* o, int layerIdx);   // 把选中根元件移到指定图层
    void MoveLayerByIndex(int layerIdx, int dir);        // 图层顺序：-1上移 1下移 2置顶 3置底
    void AddNewLayer();                    // 新建图层
    void DeleteEmptyLayer(int idx);        // 删除空图层
    void DoUndo();
    void DoRedo();
    void RestoreSnap(const UndoSnap& snap);   // 撤销/重做共用：把快照反序列化回场景并刷新界面
    void DoDelete();
    void DoDuplicate();
    void ClearClipboard();      // 清空内部剪贴板（释放多选克隆）
    void DoSave();
    void DoSaveAs();
    void DoOpen();
    void UpdateTitle();   // 窗口标题：基础名 + 当前打开的文件名
    void DoNew();
    void Play();
    void Stop();
    void ShowScriptWindow();           // 窗口菜单【脚本窗口】：打开当前帧（当前图层）的关键帧/空白关键帧脚本
    void SaveScriptFromEditor(const std::wstring& text);        // 脚本编辑窗口"保存"回调（public：编辑窗口调用）
    // 文档类 / 关键帧脚本（Flash 式帧脚本；时间轴窗口调用需 public）
    void OnDocClassScript();                                    // 编辑文档类脚本
    void EditKeyFrameScript(int li, int frame);                 // 编辑指定图层关键帧的脚本
    void ExecuteKeyFrameScripts(int frame);                     // 播放进入关键帧时执行该帧脚本
    void ShowCompileErrors(const std::vector<std::wstring>& entries); // Flash 式编译器错误页（播放/调试自动切换）
    void AppendTraceLive(bool force = false);                     // 播放期间把 trace 实时追加到"输出"页（节流防闪烁）
    void ShowTabPage(int sel);                                    // 按选中 Tab 显示/隐藏子页
    void SetDocMode(bool scriptMode);                             // 切换 .s3d/.ss 文档 Tab（编辑对象）
    void UpdateDocTabs();                                         // 打开/保存后刷新文档 Tab 文字
    std::wstring GetDocEditText();                                // 取文档类脚本编辑器文本
    void SetDocText(const wchar_t* t);                            // 设置文档类脚本编辑器文本（含语法着色）
    void ApplyDocSyntax(bool fullInvalidate = false);              // .ss 编辑器 AS3 式关键字着色（全文重扫）；true=全量失效（加载/切tab），false=行级失效（输入防抖）
    bool SetTokenColor(long s, long i, COLORREF color);           // 对选区设色（先查当前格式，同则跳过：只重绘变化 token，避免全文闪烁）；返回是否实际变化
    void AutoIndentReturn();                                      // .ss 编辑器回车自动缩进（继承上一行缩进）
    void DoFormatDoc();                                           // .ss 编辑器代码格式化（Ctrl+Shift+F，Flash 式 Auto Format）
    afx_msg void OnDocEdtChange();                                // .ss 编辑器文本变化（防抖后重新着色）
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint pt);            // .ss 编辑器右键菜单（撤销/复制/粘贴/全选/格式化）
    void OnDocMenuUndo(); void OnDocMenuCut(); void OnDocMenuCopy();
    void OnDocMenuPaste(); void OnDocMenuSelectAll(); void OnDocMenuFormat();
    afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult); // 顶部面板 Tab 切换
    afx_msg void OnDocTabSelChange(NMHDR* pNMHDR, LRESULT* pResult); // 文档 Tab（.s3d/.ss）切换
    afx_msg BOOL OnDocTabTip(UINT id, NMHDR* pNMHDR, LRESULT* pResult); // 文档 Tab 悬停提示完整文件路径
    afx_msg LRESULT OnDocTabCloseMsg(WPARAM wParam, LPARAM lParam);  // 文档 Tab"×"关闭（自定义消息）
    afx_msg void OnDocS3D();                                      // 文档 Tab：切回场景编辑（.s3d）
    afx_msg void OnDocSS();                                       // 文档 Tab：切到文档类脚本编辑（.ss）
    // ---- .ss 自动补全（AS3 式：输入 对象名. 弹出属性/方法候选）----
    void TryAutoComplete();                                       // 输入 '.' 或前缀变化时尝试弹出候选
    void ShowAutoCompleteList(long pos);                          // 在光标处弹出候选列表
    void HideAutoComplete();                                      // 关闭候选列表
    void AutoLog(const wchar_t* msg);                             // 临时诊断：自动补全失败原因（定位后移除）
    void InsertAutoItem();                                        // 把选中候选插入编辑器（替换已输入前缀）
    Object3D* FindSceneObjectByName(const std::wstring& name);    // 按名称查找场景元件
    afx_msg void OnAutoDblClk();                                  // 双击候选列表项插入

    // 独立脚本窗口回调
    void ScriptTextChanged(const std::wstring& text);
    void PerFrameChanged(bool on);

    // ---- 细分显示开关（选中图元的网格线框，默认关闭）----
    bool GetShowWire() const { return m_showWire; }
    void SetShowWire(bool on);
    void SetTool(int t);      // 供 ViewportWnd 画曲线完成后切回选择工具
    void RefreshAll();        // 供 ViewportWnd 画线过短删除后刷新
    void OnCut();             // 剪切：暂存剪贴板并从场景移除
    void OnPaste();           // 粘贴：把剪贴板克隆生成副本加入场景

    // ---- Blender 式网格编辑（点/线/面）----
    void EnterMeshEdit(Object3D* o);   // 进入网格编辑态（固化当前网格）
    void ExitMeshEdit();               // 退出编辑态
    void RebuildEditMesh(Object3D* o); // 丢弃编辑网格，回退到程序化参数网格
    void DeleteEditSelection();        // 删除编辑态选中的点/线/面
    void ExtrudeEditSelection();       // 面模式挤出选中面（Blender 式，随后沿法线拖动）
    void ExtrudeEditSelectionDepth(double d); // 按给定深度值精确挤出（不进入拖动，可负）
    int  GetMeshEditMode() { return m_viewport.m_emMode; }
    void SetMeshEditMode(int m);
    // 网格编辑态信息（属性面板对选中面/线独立着色用）
    Object3D* GetEditMeshObj() { return m_scene.editMeshObj; }          // 当前网格编辑元件（NULL=非编辑态）
    void GetEditSelFaces(std::vector<int>& out) { out = m_viewport.m_emSelF; }              // 面模式选中集
    void GetEditSelEdges(std::vector<std::pair<int, int> >& out) { out = m_viewport.m_emSelE; }   // 线模式选中集

    // ---- MCP（Model Context Protocol）服务器：AI 直接驱动建模 ----
    std::string McpToolsList();                 // tools/list：工具清单 JSON
    std::string McpExecute(const std::string& tool, const std::string& args);   // tools/call：工具路由（UI 线程）
    afx_msg LRESULT OnMcpCmd(WPARAM wParam, LPARAM lParam);                     // WM_APP_MCP_CMD：HTTP 线程→UI 线程

protected:
    Scene3D m_scene;
    ScriptEngine m_script;
    ScriptEngine m_docScript;         // 文档类脚本（播放开始时执行一次）
    int m_lastKeyScriptFrame;         // 已执行关键帧脚本的帧号（-1=未执行，防重复进入）
    // trace 实时显示：各脚本每帧输出的待显示文本 / 上次刷新时刻 / 是否已切到"输出"页
    std::wstring m_pendingTrace;
    DWORD m_lastTraceFlush;
    bool m_traceSwitched;
    std::vector<std::wstring> m_sampleFiles; // 示例菜单动态扫描 sample\*.s3d 的结果（与菜单动态 ID 对应）
    CTabCtrl m_tab;                   // 顶部面板 Tab：时间轴 | 输出 | 编译器错误 | 操作记录（仿 Flash）
    CEditCtxMenu m_traceEdt, m_errEdt;   // 输出页 / 编译器错误页内容（右键复制/剪切/清除）
    CStatic m_traceStat, m_errStat;   // 输出页 / 错误页底部统计
    std::wstring m_keyScriptTarget;   // 正在编辑的关键帧标识 "li:frame"（空=文档类）
    Object3D* m_selected;
    std::vector<Object3D*> m_clipboard;   // 内部剪贴板：复制/剪切暂存的元件克隆集合（支持多选，粘贴才生成副本）
    std::wstring m_selName;
    std::vector<Object3D*> m_selSet;   // Ctrl 多选集合（m_selected 为其“主选中”）
    std::deque<UndoSnap> m_undo;   // deque：淘汰最老快照 O(1)，避免 vector 整体搬移大网格
    std::deque<UndoSnap> m_redo;   // 重做栈：撤销时把当前状态压入；新编辑操作（PushUndo）清空
    int m_tool;
    bool m_playing;
    bool m_tlStopped;              // 脚本 stop()：时间轴停在当前帧，事件驱动代码照常每帧执行（Flash 语义）
    bool m_syntaxDirty;            // .ss 编辑器文本已变化，待防抖重新着色
    std::wstring m_lastSyntaxText; // 上次着色的全文：增量着色对比基准（文本未变跳过；变则只重着色变化行块）
    std::wstring m_lastColorNorm;  // 上次着色时“归一化（去\r）”文本：颜色缓存逐字符对齐基准
    std::vector<unsigned char> m_docColorId; // 上次着色时每个归一化字符的颜色 id（0黑 1关键字 2类型 3注释 4字符串 5数字）
    bool m_bUpdatingScript;
    bool m_showWire;               // 细分线框显示开关

    // 控件
    CButton m_btn[64];   // 工具栏按钮数组（留余量，新增按钮自动容纳）
    CSceneTreeCtrl m_tree;
    CPropertiesPanel m_props;
    CViewportWnd m_viewport;
    CDocTabCtrl m_docTab;                 // 文档 Tab：工程.s3d | 工程.ss（可点"×"关闭，仿 Animate 文档标签）
    CRichEditCtrl m_docEdt;       // 文档类脚本编辑页（.ss 激活时占视口区域；AS3 式关键字着色）
    CListBox m_autoList;                          // .ss 自动补全候选弹出列表（WS_POPUP）
    std::vector<std::wstring> m_autoItems;        // 当前候选列表
    std::wstring m_autoPrefix;                    // '.' 后已输入的前缀（插入时替换用）
    bool m_autoVisible;                           // 候选列表是否可见
    bool m_autoInserted;                          // 刚完成一次补全插入（抑制 EN_CHANGE 再弹）
    CFont m_fontMono;                     // 脚本编辑器等宽字体
    bool m_docScriptMode;                 // true=正在编辑文档类脚本（.ss 文档）
    bool m_docSSOpen;                     // .ss 标签是否打开（关闭后隐藏，文件菜单可重新打开）
    std::wstring m_titleName;             // 无文件路径时的显示名（示例等），空=未命名
    CTimelineWnd m_timeline;
    CScriptWnd m_scriptWnd;           // 独立浮动的“脚本”窗口
    McpServer m_mcp;                  // MCP 服务器（AI 驱动建模，localhost:8765）
    CStatic m_status;
    CString m_statusText;             // 状态栏当前文本（节流去闪烁用）
    DWORD m_statusTick;               // 状态栏上次刷新时刻
    std::wstring m_ssPath;            // .ss 文档类脚本最近保存路径（Ctrl+S 直接覆盖用；空=尚未保存）
    CFont m_fontBtn, m_fontSmall, m_navBoldFont;   // m_navBoldFont：导航栏当前层加粗字
    CToolTipCtrl m_tooltip;       // 左侧图标工具栏的悬停提示
    CStatic m_toolbarTitle;       // 左侧工具栏标题
    CMenu m_menu;                 // Flash 风格顶部菜单
    CImage m_iconSheet;           // 工具栏图标表（4行×5列）
    bool m_iconLoaded;
    HICON m_hIconSmall, m_hIconLarge;  // 主窗口图标（运行时 GDI 绘制的立方体，标题栏/任务栏）

    // 视口外编辑导航栏：场景与文本分开。导航栏/底部提示用 GDI 画在主窗口（视口外），
    // 不叠加在 GL 场景上——既清晰又不随场景双缓冲闪烁。
    CRect m_navRect;                              // 导航栏整条区域
    CRect m_navReturn, m_navRoot;                 // 返回箭头 / “场景1”选项卡
    std::vector<std::pair<CRect, Object3D*>> m_navItems;  // 各层级组选项卡
    bool m_navInside;                             // 当前是否处于组内编辑
    std::vector<CRect> m_sideDivs;                // 左侧工具栏分组分隔线（布局时记录，背景重画时绘制）

    void Layout();
    void RefreshNavBar();                         // 编辑层级变化后重算导航项并刷新（GDI）
    void UpdateStatus();
    void AddPrimitive(int type);
    void LoadExample(int which);
    std::wstring ExampleFilePath(const std::wstring& name) const;   // 示例 .s3d 完整路径（扁平优先，兼容子目录）
    void LoadSampleFile(const std::wstring& s3dPath);               // 直接打开 sample 目录下的示例工程文件
    std::wstring SampleDir() const;                                 // 工程根 sample\ 目录
    void ScanSampleFiles();                                         // 枚举 sample\*.s3d 填入 m_sampleFiles（供示例菜单）
    void RunScriptOnce();
    void SetScriptEditText(const std::wstring& text);
    void BuildMenu();                       // Flash 风格顶部菜单
    void DrawToolIcon(CDC* dc, CRect rc, UINT id, bool pressed);  // 左侧图标工具栏绘制
    static void PlaybackHook(int cmd, double arg, void* user);

    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);          // 画视口外导航栏（GDI）
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);  // 静态控件同色背景，消除状态栏重绘闪烁
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);   // 命中导航栏：返回/切换编辑层级
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);  // 播放期间喂入键盘状态供脚本查询
    afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);

    afx_msg void OnNew();       afx_msg void OnOpen();      afx_msg void OnSave();
    afx_msg void OnSaveAs();    afx_msg void OnUndo();      afx_msg void OnRedo();
    afx_msg void OnGroup();     afx_msg void OnUngroup();   afx_msg void OnBox();       afx_msg void OnSphere();
    afx_msg void OnCylinder();  afx_msg void OnCone();      afx_msg void OnTorus();
    afx_msg void OnPlane();     afx_msg void OnLeaf();      afx_msg void OnText3D();
    afx_msg void OnTube();
    afx_msg void OnRTube();
    afx_msg void OnRoundBox();
    afx_msg void OnCapsule();
    afx_msg void OnHemisphere();
    afx_msg void OnLathe();
    afx_msg void OnExtrude();
    afx_msg void OnBooleanHole();
    afx_msg void OnBooleanUnion();
    afx_msg void OnBooleanInter();
    void OnBooleanOp(int op);
    afx_msg void OnToolSelect(); afx_msg void OnToolMove();  afx_msg void OnToolRotate();
    afx_msg void OnToolScale();  afx_msg void OnToolPan();  afx_msg void OnToolSpline();
    afx_msg void OnDuplicate(); afx_msg void OnDelete();
    afx_msg void OnAddKey();    afx_msg void OnDelKey();
    afx_msg void OnPlay();      afx_msg void OnStop();
    afx_msg void OnEx0();       afx_msg void OnEx1();       afx_msg void OnEx2();
    afx_msg void OnEx3();       afx_msg void OnEx4();       afx_msg void OnEx5();
    afx_msg void OnEx6();      afx_msg void OnEx7();
    afx_msg void OnSampleMenuItem(UINT nID);   // 示例菜单动态项（ON_COMMAND_RANGE 9400 起）
    afx_msg void OnResetView(); afx_msg void OnRun();       afx_msg void OnDebug();
    afx_msg void OnLightSettings();  // 视图菜单：灯光设置
    afx_msg LRESULT OnLightRefresh(WPARAM, LPARAM);  // 灯光滑块实时刷新
    afx_msg void OnExport();
    afx_msg void OnHelp();
    afx_msg void OnSuperScriptDoc();   // 打开 SuperScript参考.html
    afx_msg void OnAbout();            // 版权与许可证（GPL-3.0 声明）
    afx_msg void OnWrapToMovieClip();
    afx_msg void OnAlignMenu(UINT nID);   // 世界坐标对齐（子菜单 9 项：轴 X/Y/Z × 起始边/中心/终止边）
    virtual BOOL PreTranslateMessage(MSG* pMsg);   // 转发消息给工具栏 tooltip
    DECLARE_MESSAGE_MAP()
};
