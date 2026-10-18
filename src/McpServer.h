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
// McpServer.h : Super3D 内置 MCP（Model Context Protocol）服务器
// 传输：Streamable HTTP（localhost:8765），协议：JSON-RPC 2.0
// 方法：initialize / notifications/initialized / ping / tools/list / tools/call
// 命令经 PostMessage 投递到主窗口 UI 线程执行（建模操作必须主线程），结果事件同步回 HTTP 线程
#pragma once
#include <string>
#include <thread>
#include <atomic>

class CMainWnd;

// 一条待执行的 MCP 命令（HTTP 线程栈上构造 → PostMessage 到 UI 线程 → SetEvent 通知）
struct McpCmd {
    std::string id;          // JSON-RPC id（原样回显）
    std::string method;      // initialize / tools/list / tools/call ...
    std::string toolName;    // tools/call 的工具名
    std::string argsJson;    // tools/call 的 arguments JSON（扁平对象）
    std::string resultJson;  // UI 线程填写的 tools/call 响应体（{"content":[...]}）
    void* doneEvent;         // HANDLE 完成事件
    McpCmd() : doneEvent(NULL) {}
};

// ---- JSON 轻量辅助（McpServer.cpp 实现，MainWnd 工具路由共用）----
std::string  McpJsonFind(const std::string& json, const std::string& key);   // 提取 "key":值（字符串/数字/布尔/null）
double       McpJsonNum(const std::string& json, const std::string& key, double def);
bool         McpJsonBool(const std::string& json, const std::string& key, bool def);
std::string  McpJsonEscape(const std::string& s);
std::wstring McpToW(const std::string& utf8);
std::string  McpToA(const std::wstring& w);

// 自定义消息：HTTP 线程 → 主窗口 UI 线程执行 MCP 命令（wParam = McpCmd*）
#define WM_APP_MCP_CMD (WM_APP + 100)

class McpServer {
public:
    McpServer();
    ~McpServer();
    bool Start(unsigned short port, CMainWnd* owner);
    void Stop();
    bool IsRunning() const { return m_running; }
    unsigned short Port() const { return m_port; }
private:
    void ThreadFunc();
    void HandleClient(SOCKET s);
    std::string HandleRequest(const std::string& body);
    std::string Dispatch(const std::string& id, const std::string& method,
                         const std::string& toolName, const std::string& argsJson);
    std::thread m_thread;
    std::atomic<bool> m_running;
    SOCKET m_listen;
    unsigned short m_port;
    CMainWnd* m_owner;
};
