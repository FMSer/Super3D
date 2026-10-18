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
// McpServer.cpp : Super3D 内置 MCP 服务器实现（Winsock TCP + Streamable HTTP + JSON-RPC 2.0）
#include "stdafx.h"
#include "McpServer.h"
#include "MainWnd.h"
#pragma comment(lib, "ws2_32.lib")

// ------------------------------------------------------------ JSON 轻量辅助
std::string McpJsonEscape(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 0x20) { char b[16]; sprintf(b, "\\u%04x", c); r += b; }
        else r += (char)c;
    }
    return r;
}

std::string McpJsonFind(const std::string& json, const std::string& key) {
    std::string k = "\"" + key + "\"";
    size_t p = json.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    while (p < json.size() && isspace((unsigned char)json[p])) p++;
    if (p >= json.size() || json[p] != ':') return "";
    p++;
    while (p < json.size() && isspace((unsigned char)json[p])) p++;
    if (p >= json.size()) return "";
    char c = json[p];
    if (c == '"') {   // 字符串
        std::string v;
        p++;
        bool esc = false;
        while (p < json.size()) {
            char ch = json[p++];
            if (esc) {
                if (ch == 'n') v += '\n';
                else if (ch == 'r') v += '\r';
                else if (ch == 't') v += '\t';
                else if (ch == '"') v += '"';
                else if (ch == '\\') v += '\\';
                else if (ch == 'u') { v += '?'; if (p + 3 < json.size()) p += 4; }  // 简化：unicode 转义不展开
                else v += ch;
                esc = false;
            } else if (ch == '\\') esc = true;
            else if (ch == '"') break;
            else v += ch;
        }
        return v;
    }
    if (c == 't' || c == 'f' || c == 'n' || c == '-' || isdigit((unsigned char)c)) {
        size_t e = p;
        while (e < json.size()) {
            char d = json[e];
            if (isalnum((unsigned char)d) || d == '.' || d == '-' || d == '+' || d == 'e' || d == 'E') e++;
            else break;
        }
        return json.substr(p, e - p);
    }
    return "";
}

double McpJsonNum(const std::string& json, const std::string& key, double def) {
    std::string v = McpJsonFind(json, key);
    if (v.empty() || v == "null") return def;
    return atof(v.c_str());
}

bool McpJsonBool(const std::string& json, const std::string& key, bool def) {
    std::string v = McpJsonFind(json, key);
    if (v == "true") return true;
    if (v == "false") return false;
    if (v.empty()) return def;
    return atoi(v.c_str()) != 0;
}

std::wstring McpToW(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &w[0], n);
    return w;
}

std::string McpToA(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

// ------------------------------------------------------------ 服务器
McpServer::McpServer() : m_running(false), m_listen(INVALID_SOCKET), m_port(8765), m_owner(NULL) {}
McpServer::~McpServer() { Stop(); }

bool McpServer::Start(unsigned short port, CMainWnd* owner) {
    if (m_running) return false;
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) return false;
    m_port = port;
    m_owner = owner;
    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) { WSACleanup(); return false; }
    SOCKADDR_IN addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // 仅本机，安全
    BOOL reuse = TRUE;
    setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    if (bind(m_listen, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(m_listen); m_listen = INVALID_SOCKET; WSACleanup(); return false;
    }
    if (listen(m_listen, 8) == SOCKET_ERROR) {
        closesocket(m_listen); m_listen = INVALID_SOCKET; WSACleanup(); return false;
    }
    m_running = true;
    m_thread = std::thread(&McpServer::ThreadFunc, this);
    return true;
}

void McpServer::Stop() {
    if (!m_running) return;
    m_running = false;
    if (m_listen != INVALID_SOCKET) {
        closesocket(m_listen);
        m_listen = INVALID_SOCKET;
    }
    if (m_thread.joinable()) m_thread.join();
    WSACleanup();
}

void McpServer::ThreadFunc() {
    while (m_running) {
        SOCKET c = accept(m_listen, NULL, NULL);
        if (c == INVALID_SOCKET) break;   // Stop 时 closesocket 唤醒
        HandleClient(c);
    }
}

void McpServer::HandleClient(SOCKET s) {
    std::string req;
    char buf[4096];
    bool headerDone = false;
    int bodyLen = -1;
    std::string body;
    int total = 0;
    while (m_running) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.append(buf, n);
        total += n;
        if (!headerDone) {
            size_t hEnd = req.find("\r\n\r\n");
            if (hEnd != std::string::npos) {
                headerDone = true;
                std::string header = req.substr(0, hEnd);
                // 客户端（.NET HttpClient / PowerShell Invoke-WebRequest）默认发 Expect: 100-continue，
                // 不回 100 Continue 客户端就死锁等我们发 body——这里立即回 100 继续读 body
                if (header.find("100-continue") != std::string::npos) {
                    const char* cont = "HTTP/1.1 100 Continue\r\n\r\n";
                    send(s, cont, (int)strlen(cont), 0);
                }
                size_t cl = header.find("Content-Length:");
                if (cl == std::string::npos) cl = header.find("content-length:");
                if (cl != std::string::npos) {
                    size_t e = header.find("\r\n", cl);
                    std::string v = header.substr(cl + 15, (e == std::string::npos) ? std::string::npos : e - cl - 15);
                    while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(v.begin());
                    bodyLen = atoi(v.c_str());
                }
            }
        }
        if (headerDone) {
            // header 已解析，从 req 里切出当前 body（后续 recv 会追加数据，body 要同步更新）
            size_t hEnd2 = req.find("\r\n\r\n");
            if (hEnd2 != std::string::npos) body = req.substr(hEnd2 + 4);
        }
        if (headerDone && bodyLen >= 0 && (int)body.size() >= bodyLen) break;
        if (total > 64 * 1024) break;
    }
    std::string respBody;
    if (headerDone && bodyLen >= 0) {
        if ((int)body.size() > bodyLen) body.resize(bodyLen);
        respBody = HandleRequest(body);
    } else {
        respBody = "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"parse error\"}}";
    }
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                       std::to_string((int)respBody.size()) + "\r\nConnection: close\r\n\r\n" + respBody;
    send(s, resp.c_str(), (int)resp.size(), 0);
    closesocket(s);
}

// 提取从 start 开始（body[start]=='{'）的完整 JSON 对象子串：跳过字符串（含转义），避免代码文本中的 { } 干扰深度计数
static std::string McpJsonObj(const std::string& body, size_t start) {
    int depth = 0;
    bool inStr = false, esc = false;
    for (size_t i = start; i < body.size(); i++) {
        char c = body[i];
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') { inStr = true; continue; }
        if (c == '{') depth++;
        else if (c == '}') { depth--; if (depth == 0) return body.substr(start, i - start + 1); }
    }
    return "";
}

std::string McpServer::HandleRequest(const std::string& body) {
    std::string id = McpJsonFind(body, "id");
    std::string method = McpJsonFind(body, "method");
    // params（对象）子串
    std::string params;
    size_t pp = body.find("\"params\"");
    if (pp != std::string::npos) {
        size_t colon = body.find(':', pp + 8);
        if (colon != std::string::npos) {
            size_t start = body.find_first_not_of(" \t\r\n", colon + 1);
            if (start != std::string::npos && body[start] == '{') {
                params = McpJsonObj(body, start);
            }
        }
    }
    std::string toolName, argsJson;
    if (method == "tools/call") {
        toolName = McpJsonFind(params, "name");
        size_t ap = params.find("\"arguments\"");
        if (ap != std::string::npos) {
            size_t colon = params.find(':', ap + 11);
            if (colon != std::string::npos) {
                size_t start = params.find_first_not_of(" \t\r\n", colon + 1);
                if (start != std::string::npos && params[start] == '{') {
                    argsJson = McpJsonObj(params, start);
                }
            }
        }
    }
    return Dispatch(id, method, toolName, argsJson);
}

std::string McpServer::Dispatch(const std::string& id, const std::string& method,
                                const std::string& toolName, const std::string& argsJson) {
    std::string rpcId = id.empty() ? "null" : id;
    std::string body;
    if (method == "initialize") {
        body = "{\"protocolVersion\":\"2025-03-26\",\"capabilities\":{\"tools\":{\"listChanged\":false}},\"serverInfo\":{\"name\":\"Super3D\",\"version\":\"1.0\"}}";
    } else if (method == "notifications/initialized" || method == "notifications/cancelled") {
        return "";   // 通知：无响应
    } else if (method == "ping") {
        body = "{}";
    } else if (method == "tools/list") {
        body = m_owner ? m_owner->McpToolsList() : "{\"error\":\"no window\"}";
    } else if (method == "tools/call") {
        McpCmd cmd;
        cmd.id = id;
        cmd.method = method;
        cmd.toolName = toolName;
        cmd.argsJson = argsJson;
        cmd.doneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!cmd.doneEvent) {
            return "{\"jsonrpc\":\"2.0\",\"id\":" + rpcId +
                   ",\"result\":{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"event fail\"}]}}";
        }
        if (m_owner && m_owner->GetSafeHwnd())
            ::PostMessage(m_owner->GetSafeHwnd(), WM_APP_MCP_CMD, (WPARAM)&cmd, 0);
        WaitForSingleObject((HANDLE)cmd.doneEvent, 15000);
        CloseHandle((HANDLE)cmd.doneEvent);
        if (cmd.resultJson.empty())
            cmd.resultJson = "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"timeout or window closed\"}]}";
        body = cmd.resultJson;
    } else {
        body = "{\"error\":{\"code\":-32601,\"message\":\"method not found\"}}";
    }
    if (body.empty()) return "";
    if (body.find("\"jsonrpc\"") == std::string::npos)
        return "{\"jsonrpc\":\"2.0\",\"id\":" + rpcId + ",\"result\":" + body + "}";
    return body;
}
