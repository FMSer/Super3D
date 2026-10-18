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
// ProjectIO.cpp : 工程文件实现
#include "ProjectIO.h"
#include "Renderer.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <map>

namespace ProjectIO {

static void AppendNum(std::wstring& s, double v) {
    wchar_t b[64];
    swprintf(b, 64, L"%.6f", v);
    s += b;
}

// 宽字符串 → 十六进制编码（避免文字含空格/换行破坏空白分隔的 obj 行解析）
static std::wstring WsHex(const std::wstring& s) {
    wchar_t b[8];
    std::wstring r;
    for (size_t i = 0; i < s.size(); i++) {
        swprintf(b, 8, L"%04x", (unsigned int)(unsigned short)s[i]);
        r += b;
    }
    return r;
}
static std::wstring WsUnhex(const std::wstring& s) {
    std::wstring r;
    for (size_t i = 0; i + 4 <= s.size(); i += 4) {
        unsigned long v = wcstoul(s.substr(i, 4).c_str(), NULL, 16);
        r += (wchar_t)(unsigned short)v;
    }
    return r;
}

static void WriteObject(std::wstring& out, const Object3D* o, bool skipBoolCache) {
    out += L"obj name=";
    out += o->name;
    wchar_t b[32];
    swprintf(b, 32, L" type=%d", o->type);
    out += b;
    out += L" parent=";
    out += o->parent ? o->parent->name : std::wstring();
    out += o->visible ? L" visible=1" : L" visible=0";
    out += o->locked ? L" locked=1" : L" locked=0";
    {
        wchar_t lb[32]; swprintf(lb, 32, L" layer=%d", o->layer); out += lb;
    }
    out += L" color=";
    AppendNum(out, o->r); out += L",";
    AppendNum(out, o->g); out += L",";
    AppendNum(out, o->b); out += L",";
    AppendNum(out, o->alpha);
    out += L" params=";
    AppendNum(out, o->params[0]); out += L",";
    AppendNum(out, o->params[1]); out += L",";
    AppendNum(out, o->params[2]); out += L",";
    AppendNum(out, o->params[3]);
    swprintf(b, 32, L" seg=%d", o->segments);
    out += b;
    // 编辑网格（Blender 式点/线/面编辑结果）：局部坐标顶点表 + 三角索引表
    if (o->HasEditMesh()) {
        out += L" emv=";
        for (size_t i = 0; i < o->editVerts.size(); i++) {
            if (i) out += L";";
            AppendNum(out, o->editVerts[i].x); out += L",";
            AppendNum(out, o->editVerts[i].y); out += L",";
            AppendNum(out, o->editVerts[i].z);
        }
        out += L" emf=";
        for (size_t k = 0; k < o->editTris.size(); k++) {
            if (k) out += (k % 3) ? L"," : L";";
            swprintf(b, 32, L"%d", o->editTris[k]);
            out += b;
        }
        // 面自定义色：只存非继承项（0=继承物体色），格式 tri序号:8位十六进制打包色;...
        {
            bool any = false;
            for (size_t t = 0; t < o->editTriColor.size() && !any; t++)
                if (o->editTriColor[t]) any = true;
            if (any) {
                out += L" etc=";
                bool first = true;
                for (size_t t = 0; t < o->editTriColor.size(); t++) {
                    if (!o->editTriColor[t]) continue;
                    if (!first) out += L";";
                    swprintf(b, 32, L"%d:", (int)t);
                    out += b;
                    swprintf(b, 32, L"%08x", o->editTriColor[t]);
                    out += b;
                    first = false;
                }
            }
        }
        // 线自定义色：格式 a,b:8位十六进制打包色;...（a<b，与 editEdgeColorKey 对应）
        if (!o->editEdgeColorKey.empty()) {
            out += L" eec=";
            for (size_t i = 0; i < o->editEdgeColorKey.size(); i++) {
                if (i) out += L";";
                swprintf(b, 32, L"%d,%d:%08x", o->editEdgeColorKey[i].first,
                         o->editEdgeColorKey[i].second, o->editEdgeColorVal[i]);
                out += b;
            }
        }
    }
    if ((o->type == PT_SPLINE || o->type == PT_LATHE || o->type == PT_EXTRUDE) && !o->ctrlPts.empty()) {
        out += L" pts=";
        for (size_t i = 0; i < o->ctrlPts.size(); i++) {
            if (i) out += L";";
            AppendNum(out, o->ctrlPts[i].x); out += L",";
            AppendNum(out, o->ctrlPts[i].y); out += L",";
            AppendNum(out, o->ctrlPts[i].z);
        }
    }
    if (o->type == PT_TEXT3D) {
        out += L" text="; out += WsHex(o->text);
        out += L" font="; out += WsHex(o->font);
    }
    if (o->type == PT_BOOLEAN && o->_meshValid && !o->_meshVerts.empty() && !skipBoolCache) {
        out += L" mesh=";
        for (size_t k = 0; k < o->_meshVerts.size(); k++) {
            if (k) out += L";";
            AppendNum(out, o->_meshVerts[k].x); out += L",";
            AppendNum(out, o->_meshVerts[k].y); out += L",";
            AppendNum(out, o->_meshVerts[k].z);
        }
        out += L" norm=";
        for (size_t k = 0; k < o->_meshNorms.size(); k++) {
            if (k) out += L";";
            AppendNum(out, o->_meshNorms[k].x); out += L",";
            AppendNum(out, o->_meshNorms[k].y); out += L",";
            AppendNum(out, o->_meshNorms[k].z);
        }
    }
    out += L" tf=";
    AppendNum(out, o->x); out += L",";
    AppendNum(out, o->y); out += L",";
    AppendNum(out, o->z); out += L",";
    AppendNum(out, o->rx); out += L",";
    AppendNum(out, o->ry); out += L",";
    AppendNum(out, o->rz); out += L",";
    AppendNum(out, o->sx); out += L",";
    AppendNum(out, o->sy); out += L",";
    AppendNum(out, o->sz);
    out += L"\r\n";
}

static void WriteTree(std::wstring& out, const Object3D* o, bool skipBoolCache) {
    WriteObject(out, o, skipBoolCache);
    for (size_t i = 0; i < o->children.size(); i++)
        WriteTree(out, o->children[i], skipBoolCache);
}

std::wstring SerializeToString(const Scene3D& scene, bool skipBoolCache) {
    std::wstring out;
    out += L"# Super3D Project v1\r\n";
    wchar_t b[256];
    swprintf(b, 256, L"camera yaw=%.6f pitch=%.6f dist=%.6f tx=%.6f ty=%.6f tz=%.6f\r\n",
             scene.cam.yaw, scene.cam.pitch, scene.cam.dist,
             scene.cam.tx, scene.cam.ty, scene.cam.tz);
    out += b;
    swprintf(b, 256, L"light amb=%.6f dif=%.6f dx=%.6f dy=%.6f dz=%.6f\r\n",
             Renderer::s_lightAmbient, Renderer::s_lightDiffuse,
             Renderer::s_lightDirX, Renderer::s_lightDirY, Renderer::s_lightDirZ);
    out += b;
    swprintf(b, 256, L"fps=%d\r\nframes=%d\r\nloop=%d\r\nscript_per_frame=%d\r\n",
             scene.fps, scene.totalFrames, scene.loopPlay ? 1 : 0, scene.scriptPerFrame ? 1 : 0);
    out += b;
    swprintf(b, 256, L"layers=%d\r\n", (int)scene.layers.size());
    out += b;
    for (size_t i = 0; i < scene.layers.size(); i++) {
        swprintf(b, 256, L"layer_def name=%s visible=%d locked=%d\r\n",
                 scene.layers[i].name.c_str(),
                 scene.layers[i].visible ? 1 : 0,
                 scene.layers[i].locked ? 1 : 0);
        out += b;
    }
    swprintf(b, 256, L"cur_layer=%d\r\n", scene.curLayer);
    out += b;
    for (size_t i = 0; i < scene.roots.size(); i++)
        WriteTree(out, scene.roots[i], skipBoolCache);
    // 对象序号映射（与反序列化 created 向量顺序一致：深度优先前序）：
    // layer_key 行用 id 精确引用对象，避免同名对象（如 willowLeaf×47）按名查找错指第一份。
    std::map<const Object3D*, int> idMap;
    {
        int idc = 0;
        for (size_t i = 0; i < scene.roots.size(); i++) {
            std::vector<const Object3D*> stack;
            stack.push_back(scene.roots[i]);
            while (!stack.empty()) {
                const Object3D* cur = stack.back();
                stack.pop_back();
                idMap[cur] = idc++;
                for (size_t c = cur->children.size(); c > 0; c--)
                    stack.push_back(cur->children[c - 1]);
            }
        }
    }
    // 图层关键帧（Flash 式整层驱动）：每行 = 某层某帧某根元件的快照值
    for (size_t li = 0; li < scene.layers.size(); li++) {
        const std::vector<LayerKey>& keys = scene.layers[li].keys;
        for (size_t ki = 0; ki < keys.size(); ki++) {
            const LayerKey& k = keys[ki];
            if (k.objs.empty()) {
                wchar_t eb[96];
                swprintf(eb, 96, L"layer_key_empty layer=%d frame=%d\r\n", (int)li, k.frame);
                out += eb;
            } else {
                for (size_t i = 0; i < k.objs.size() && i < k.snaps.size(); i++) {
                    const Object3D* o = k.objs[i];
                    if (!o || !scene.ContainsObject(o) || o->parent != NULL || scene.RootLayerOf(o) != (int)li) continue;
                    int id = -1;
                    std::map<const Object3D*, int>::const_iterator it = idMap.find(o);
                    if (it != idMap.end()) id = it->second;
                    wchar_t lb[96];
                    if (id >= 0)
                        swprintf(lb, 96, L"layer_key layer=%d frame=%d id=%d tf=", (int)li, k.frame, id);
                    else
                        swprintf(lb, 96, L"layer_key layer=%d frame=%d name=%s tf=", (int)li, k.frame, o->name.c_str());
                    out += lb;
                    const KeyFrame& s = k.snaps[i];
                    AppendNum(out, s.px); out += L",";
                    AppendNum(out, s.py); out += L",";
                    AppendNum(out, s.pz); out += L",";
                    AppendNum(out, s.rx); out += L",";
                    AppendNum(out, s.ry); out += L",";
                    AppendNum(out, s.rz); out += L",";
                    AppendNum(out, s.sx); out += L",";
                    AppendNum(out, s.sy); out += L",";
                    AppendNum(out, s.sz);
                    out += L"\r\n";
                }
            }
            // 关键帧脚本（长度前缀 + 原始文本，防行内换行破坏解析）
            if (!k.script.empty()) {
                wchar_t sb2[128];
                swprintf(sb2, 128, L"layer_key_script layer=%d frame=%d len=%d\r\n",
                         (int)li, k.frame, (int)k.script.size());
                out += sb2;
                out += k.script;
                out += L"\r\n";
            }
        }
    }
    swprintf(b, 256, L"script_len=%d\r\n", (int)scene.script.size());
    out += b;
    out += scene.script;
    out += L"\r\n";
    out += L"# end\r\n";
    return out;
}

// ------------------------------------------------------------ 解析工具
static std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == L' ' || s[a] == L'\t' || s[a] == L'\r')) a++;
    while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t' || s[b - 1] == L'\r')) b--;
    return s.substr(a, b - a);
}

static void SplitWs(const std::wstring& s, std::vector<std::wstring>& out) {
    out.clear();
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) i++;
        size_t start = i;
        while (i < s.size() && s[i] != L' ' && s[i] != L'\t') i++;
        if (i > start) out.push_back(s.substr(start, i - start));
    }
}

static bool GetKV(const std::wstring& s, const std::wstring& key, std::wstring& val) {
    if (s.compare(0, key.size(), key) != 0) return false;
    size_t p = key.size();
    if (p < s.size() && s[p] == L'=') {
        val = Trim(s.substr(p + 1));
        return true;
    }
    return false;
}

static void ParseList(const std::wstring& s, double out[4], int maxN) {
    for (int k = 0; k < maxN; k++) out[k] = 0;
    size_t i = 0;
    int idx = 0;
    while (i < s.size() && idx < maxN) {
        size_t c = s.find(L',', i);
        std::wstring item = Trim(c == std::wstring::npos ? s.substr(i) : s.substr(i, c - i));
        out[idx++] = wcstod(item.c_str(), NULL);
        if (c == std::wstring::npos) break;
        i = c + 1;
    }
}

// 按分隔符 ch 分割（不 trim 内部空白，解析控制点用）
static void SplitStr(const std::wstring& s, wchar_t ch, std::vector<std::wstring>& out) {
    out.clear();
    size_t i = 0;
    while (i < s.size()) {
        size_t c = s.find(ch, i);
        if (c == std::wstring::npos) { out.push_back(s.substr(i)); break; }
        out.push_back(s.substr(i, c - i));
        i = c + 1;
    }
    if (out.empty()) out.push_back(s);
}

bool DeserializeFromString(Scene3D& scene, const std::wstring& text, std::wstring& err) {
    scene.Clear();
    // 按行切分
    std::vector<std::wstring> lines;
    size_t i = 0;
    while (i < text.size()) {
        size_t nl = text.find(L'\n', i);
        std::wstring line = (nl == std::wstring::npos) ? text.substr(i) : text.substr(i, nl - i);
        lines.push_back(Trim(line));
        if (nl == std::wstring::npos) break;
        i = nl + 1;
    }
    // 元信息
    std::vector<Layer> layerDefs;
    for (size_t l = 0; l < lines.size(); l++) {
        const std::wstring& line = lines[l];
        if (line.empty() || line[0] == L'#') continue;
        if (line.compare(0, 7, L"camera ") == 0) {
            std::wstring v;
            if (GetKV(line, L"yaw", v)) scene.cam.yaw = wcstod(v.c_str(), NULL);
            if (GetKV(line, L"pitch", v)) scene.cam.pitch = wcstod(v.c_str(), NULL);
            if (GetKV(line, L"dist", v)) scene.cam.dist = wcstod(v.c_str(), NULL);
            if (GetKV(line, L"tx", v)) scene.cam.tx = wcstod(v.c_str(), NULL);
            if (GetKV(line, L"ty", v)) scene.cam.ty = wcstod(v.c_str(), NULL);
            if (GetKV(line, L"tz", v)) scene.cam.tz = wcstod(v.c_str(), NULL);
            continue;
        }
        if (line.compare(0, 5, L"light") == 0) {
            // 行格式: light amb=... dif=... dx=... dy=... dz=...（从行中查找 kv）
            size_t p;
            p = line.find(L"amb="); if (p != std::wstring::npos) Renderer::s_lightAmbient = (float)wcstod(line.c_str()+p+4, NULL);
            p = line.find(L"dif="); if (p != std::wstring::npos) Renderer::s_lightDiffuse = (float)wcstod(line.c_str()+p+4, NULL);
            p = line.find(L"dx="); if (p != std::wstring::npos) Renderer::s_lightDirX = (float)wcstod(line.c_str()+p+3, NULL);
            p = line.find(L"dy="); if (p != std::wstring::npos) Renderer::s_lightDirY = (float)wcstod(line.c_str()+p+3, NULL);
            p = line.find(L"dz="); if (p != std::wstring::npos) Renderer::s_lightDirZ = (float)wcstod(line.c_str()+p+3, NULL);
            continue;
        }
        std::wstring v;
        if (GetKV(line, L"fps", v)) scene.fps = (int)wcstod(v.c_str(), NULL);
        else if (GetKV(line, L"frames", v)) scene.totalFrames = (int)wcstod(v.c_str(), NULL);
        else if (GetKV(line, L"loop", v)) scene.loopPlay = ((int)wcstod(v.c_str(), NULL) != 0);
        else if (GetKV(line, L"script_per_frame", v)) scene.scriptPerFrame = ((int)wcstod(v.c_str(), NULL) != 0);
        else if (GetKV(line, L"layers", v)) {
            int n = (int)wcstod(v.c_str(), NULL);
            if (n >= 1) {
                scene.layers.clear();
                for (int kk = 0; kk < n; kk++) scene.layers.push_back(Layer());
            }
        }
        else if (line.compare(0, 10, L"layer_def ") == 0) {
            std::vector<std::wstring> kv;
            SplitWs(line, kv);
            Layer def;
            for (size_t k = 1; k < kv.size(); k++) {
                std::wstring val;
                if (GetKV(kv[k], L"name", val)) def.name = val;
                else if (GetKV(kv[k], L"visible", val)) def.visible = ((int)wcstod(val.c_str(), NULL) != 0);
                else if (GetKV(kv[k], L"locked", val)) def.locked = ((int)wcstod(val.c_str(), NULL) != 0);
            }
            layerDefs.push_back(def);
        }
        else if (GetKV(line, L"cur_layer", v)) scene.curLayer = (int)wcstod(v.c_str(), NULL);
    }
    if (scene.fps < 1) scene.fps = 24;
    if (scene.totalFrames < 2) scene.totalFrames = 120;
    if (scene.frame >= scene.totalFrames) scene.frame = scene.totalFrames - 1;
    if (!layerDefs.empty()) scene.layers = layerDefs;
    scene.EnsureLayers();

    // 元件
    std::vector<Object3D*> created;
    for (size_t l = 0; l < lines.size(); l++) {
        const std::wstring& line = lines[l];
        if (line.compare(0, 4, L"obj ") != 0) continue;
        std::vector<std::wstring> kv;
        SplitWs(line, kv);
        Object3D* o = new Object3D();
        std::wstring val;
        for (size_t k = 1; k < kv.size(); k++) {
            const std::wstring& item = kv[k];
            if (GetKV(item, L"name", val)) o->name = val;
            else if (GetKV(item, L"type", val)) o->type = (int)wcstod(val.c_str(), NULL);
            else if (GetKV(item, L"visible", val)) o->visible = ((int)wcstod(val.c_str(), NULL) != 0);
            else if (GetKV(item, L"locked", val)) o->locked = ((int)wcstod(val.c_str(), NULL) != 0);
            else if (GetKV(item, L"layer", val)) o->layer = (int)wcstod(val.c_str(), NULL);
            else if (GetKV(item, L"seg", val)) o->segments = (int)wcstod(val.c_str(), NULL);
            else if (GetKV(item, L"emv", val)) {
                // 编辑网格顶点：局部坐标 x,y,z;x,y,z;...
                o->editVerts.clear();
                std::vector<std::wstring> segs;
                SplitStr(val, L';', segs);
                for (size_t si = 0; si < segs.size(); si++) {
                    double p[3] = { 0, 0, 0 };
                    ParseList(segs[si], p, 3);
                    o->editVerts.push_back(Vec3(p[0], p[1], p[2]));
                }
            }
            else if (GetKV(item, L"emf", val)) {
                // 编辑网格三角索引：i,j,k;i,j,k;...（非法索引整体丢弃字段）
                o->editTris.clear();
                std::vector<std::wstring> segs;
                SplitStr(val, L';', segs);
                bool ok = !segs.empty();
                for (size_t si = 0; ok && si < segs.size(); si++) {
                    double t[3] = { 0, 0, 0 };
                    ParseList(segs[si], t, 3);
                    int iv = (int)t[0], jv = (int)t[1], kv2 = (int)t[2];
                    if (iv < 0 || iv >= (int)o->editVerts.size() ||
                        jv < 0 || jv >= (int)o->editVerts.size() ||
                        kv2 < 0 || kv2 >= (int)o->editVerts.size()) {
                        ok = false;   // 索引越界：整条 emf 不可信，丢弃
                        break;
                    }
                    o->editTris.push_back(iv); o->editTris.push_back(jv); o->editTris.push_back(kv2);
                }
                if (!ok) o->editTris.clear();
            }
            else if (GetKV(item, L"etc", val)) {
                // 面自定义色：tri序号:8位十六进制打包色;...（0 位不存，缺省=继承物体色）
                o->editTriColor.clear();
                std::vector<std::wstring> segs;
                SplitStr(val, L';', segs);
                for (size_t si = 0; si < segs.size(); si++) {
                    size_t cp = segs[si].find(L':');
                    if (cp == std::wstring::npos) continue;
                    int ti = (int)wcstol(segs[si].substr(0, cp).c_str(), NULL, 10);
                    unsigned int cv = (unsigned int)wcstoul(segs[si].substr(cp + 1).c_str(), NULL, 16);
                    if (ti < 0) continue;
                    if ((size_t)ti >= o->editTriColor.size()) o->editTriColor.resize((size_t)ti + 1, 0u);
                    o->editTriColor[ti] = cv;
                }
            }
            else if (GetKV(item, L"eec", val)) {
                // 线自定义色：a,b:8位十六进制打包色;...（a<b）
                o->editEdgeColorKey.clear();
                o->editEdgeColorVal.clear();
                std::vector<std::wstring> segs;
                SplitStr(val, L';', segs);
                for (size_t si = 0; si < segs.size(); si++) {
                    size_t cp = segs[si].find(L':');
                    if (cp == std::wstring::npos) continue;
                    std::wstring kb = segs[si].substr(0, cp);
                    unsigned int cv = (unsigned int)wcstoul(segs[si].substr(cp + 1).c_str(), NULL, 16);
                    size_t cm = kb.find(L',');
                    if (cm == std::wstring::npos) continue;
                    int a = (int)wcstol(kb.substr(0, cm).c_str(), NULL, 10);
                    int b2 = (int)wcstol(kb.substr(cm + 1).c_str(), NULL, 10);
                    if (a < 0 || b2 < 0 || a > b2) continue;
                    o->editEdgeColorKey.push_back(std::make_pair(a, b2));
                    o->editEdgeColorVal.push_back(cv);
                }
            }
            else if (GetKV(item, L"color", val)) {
                double c[4] = { 0, 0, 0, 1 };
                ParseList(val, c, 4);
                o->r = c[0]; o->g = c[1]; o->b = c[2];
                // 旧文件只存 3 值（r,g,b）→ 保持 alpha=1 不透明；新文件 4 值含 alpha
                int commas = 0;
                for (size_t ci = 0; ci < val.size(); ci++) if (val[ci] == L',') commas++;
                if (commas >= 3) o->alpha = c[3];
            }
            else if (GetKV(item, L"params", val)) {
                ParseList(val, o->params, 4);
            }
            else if (GetKV(item, L"tf", val)) {
                double t[9] = { 0 };
                ParseList(val, t, 9);
                o->x = t[0]; o->y = t[1]; o->z = t[2];
                o->rx = t[3]; o->ry = t[4]; o->rz = t[5];
                o->sx = t[6]; o->sy = t[7]; o->sz = t[8];
            }
            else if (GetKV(item, L"pts", val)) {
                // 曲线控制点：x,y,z;x,y,z;...
                o->ctrlPts.clear();
                std::vector<std::wstring> segs;
                SplitStr(val, L';', segs);
                for (size_t si = 0; si < segs.size(); si++) {
                    double p[3] = { 0, 0, 0 };
                    ParseList(segs[si], p, 3);
                    o->ctrlPts.push_back(Vec3(p[0], p[1], p[2]));
                }
            }
            else if (GetKV(item, L"text", val)) o->text = WsUnhex(val);
            else if (GetKV(item, L"font", val)) o->font = WsUnhex(val);
            else if (GetKV(item, L"mesh", val)) {
                o->_meshVerts.clear();
                std::vector<std::wstring> segs;
                SplitStr(val, L';', segs);
                for (size_t si = 0; si < segs.size(); si++) {
                    double p[3] = { 0, 0, 0 };
                    ParseList(segs[si], p, 3);
                    o->_meshVerts.push_back(Vec3(p[0], p[1], p[2]));
                }
            }
            else if (GetKV(item, L"norm", val)) {
                o->_meshNorms.clear();
                std::vector<std::wstring> segs;
                SplitStr(val, L';', segs);
                for (size_t si = 0; si < segs.size(); si++) {
                    double p[3] = { 0, 0, 0 };
                    ParseList(segs[si], p, 3);
                    o->_meshNorms.push_back(Vec3(p[0], p[1], p[2]));
                }
                if (!o->_meshVerts.empty()) { o->_meshValid = true; o->_meshRev++; }
            }
        }
        o->SyncDisplayToLocal();
        scene.roots.push_back(o);
        created.push_back(o);
    }
    // 恢复父子关系（保存顺序父在前）
    size_t createdIdx = 0;
    for (size_t l = 0; l < lines.size(); l++) {
        const std::wstring& line = lines[l];
        if (line.compare(0, 4, L"obj ") != 0) continue;
        std::vector<std::wstring> kv;
        SplitWs(line, kv);
        std::wstring objName, parentName;
        std::wstring val;
        for (size_t k = 1; k < kv.size(); k++) {
            if (GetKV(kv[k], L"name", val)) objName = val;
            else if (GetKV(kv[k], L"parent", val)) parentName = val;
        }
        // 按 obj 行创建顺序一一对应（重名副本不能按名字 FindByName，否则错挂接/重复挂接→Clear 双重删除崩溃）
        Object3D* o = created[createdIdx++];
        if (parentName.empty()) continue;
        Object3D* p = scene.FindByName(parentName);
        if (o && p && o != p) {
            for (size_t k = 0; k < scene.roots.size(); k++) {
                if (scene.roots[k] == o) { scene.roots.erase(scene.roots.begin() + k); break; }
            }
            o->parent = p;
            p->children.push_back(o);
        }
    }
    // 关键帧
    for (size_t l = 0; l < lines.size(); l++) {
        const std::wstring& line = lines[l];
        if (line.compare(0, 4, L"key ") != 0) continue;
        std::vector<std::wstring> kv;
        SplitWs(line, kv);
        std::wstring objName, val;
        KeyFrame kf;
        for (size_t k = 1; k < kv.size(); k++) {
            const std::wstring& item = kv[k];
            if (GetKV(item, L"name", val)) objName = val;
            else if (GetKV(item, L"frame", val)) kf.frame = (int)wcstod(val.c_str(), NULL);
            else if (GetKV(item, L"tf", val)) {
                double t[9] = { 0 };
                ParseList(val, t, 9);
                kf.px = t[0]; kf.py = t[1]; kf.pz = t[2];
                kf.rx = t[3]; kf.ry = t[4]; kf.rz = t[5];
                kf.sx = t[6]; kf.sy = t[7]; kf.sz = t[8];
            }
        }
        Object3D* o = scene.FindByName(objName);
        if (o) o->keys.push_back(kf);
    }
    // 图层关键帧（Flash 式整层驱动）：layer_key layer=<li> frame=<f> name=<n> tf=...
    for (size_t l = 0; l < lines.size(); l++) {
        const std::wstring& line = lines[l];
        if (line.compare(0, 15, L"layer_key_empty") == 0) {
            std::vector<std::wstring> kv;
            SplitWs(line, kv);
            int layerIdx = -1, frame = 0;
            std::wstring val;
            for (size_t k = 1; k < kv.size(); k++) {
                const std::wstring& item = kv[k];
                if (GetKV(item, L"layer", val)) layerIdx = (int)wcstod(val.c_str(), NULL);
                else if (GetKV(item, L"frame", val)) frame = (int)wcstod(val.c_str(), NULL);
            }
            if (layerIdx >= 0 && layerIdx < (int)scene.layers.size()) {
                bool has = false;
                for (size_t k = 0; k < scene.layers[layerIdx].keys.size(); k++)
                    if (scene.layers[layerIdx].keys[k].frame == frame) { has = true; break; }
                if (!has) { LayerKey lk; lk.frame = frame; scene.layers[layerIdx].keys.push_back(lk); }
            }
            continue;
        }
        if (line.compare(0, 10, L"layer_key ") != 0) continue;
        std::vector<std::wstring> kv;
        SplitWs(line, kv);
        int layerIdx = -1, frame = 0, objId = -1;
        std::wstring objName, val;
        KeyFrame kf;
        for (size_t k = 1; k < kv.size(); k++) {
            const std::wstring& item = kv[k];
            if (GetKV(item, L"layer", val)) layerIdx = (int)wcstod(val.c_str(), NULL);
            else if (GetKV(item, L"frame", val)) frame = (int)wcstod(val.c_str(), NULL);
            else if (GetKV(item, L"id", val)) objId = (int)wcstod(val.c_str(), NULL);
            else if (GetKV(item, L"name", val)) objName = val;
            else if (GetKV(item, L"tf", val)) {
                double t[9] = { 0 };
                ParseList(val, t, 9);
                kf.px = t[0]; kf.py = t[1]; kf.pz = t[2];
                kf.rx = t[3]; kf.ry = t[4]; kf.rz = t[5];
                kf.sx = t[6]; kf.sy = t[7]; kf.sz = t[8];
            }
        }
        if (layerIdx < 0 || layerIdx >= (int)scene.layers.size()) continue;   // 越界跳过该行
        // 优先按 id 精确取对象（同名元件不再错指第一份）；旧格式无 id 时回退按名查找
        Object3D* o = NULL;
        if (objId >= 0 && (size_t)objId < created.size()) o = created[(size_t)objId];
        if (!o && !objName.empty()) o = scene.FindByName(objName);
        if (!o) continue;   // 找不到元件跳过该行
        // 找该帧 LayerKey，无则新建
        LayerKey* plk = NULL;
        for (size_t k = 0; k < scene.layers[layerIdx].keys.size(); k++) {
            if (scene.layers[layerIdx].keys[k].frame == frame) {
                plk = &scene.layers[layerIdx].keys[k];
                break;
            }
        }
        if (!plk) {
            LayerKey lk;
            lk.frame = frame;
            scene.layers[layerIdx].keys.push_back(lk);
            plk = &scene.layers[layerIdx].keys.back();
        }
        plk->objs.push_back(o);
        plk->snaps.push_back(kf);
    }
    // 关键帧排序
    {
        std::vector<Object3D*> all;
        scene.CollectAll(all);
        for (size_t k = 0; k < all.size(); k++) {
            std::vector<KeyFrame>& ks = all[k]->keys;
            for (size_t a = 1; a < ks.size(); a++)
                for (size_t b = a; b > 0 && ks[b].frame < ks[b - 1].frame; b--) {
                    KeyFrame t = ks[b]; ks[b] = ks[b - 1]; ks[b - 1] = t;
                }
        }
    }
    // 图层关键帧按 frame 升序排序
    for (size_t li = 0; li < scene.layers.size(); li++) {
        std::vector<LayerKey>& keys = scene.layers[li].keys;
        for (size_t a = 1; a < keys.size(); a++)
            for (size_t b = a; b > 0 && keys[b].frame < keys[b - 1].frame; b--) {
                LayerKey t = keys[b]; keys[b] = keys[b - 1]; keys[b - 1] = t;
            }
    }
    scene.EnsureLayers();
    for (size_t i = 0; i < scene.roots.size(); i++) {
        Object3D* r = scene.roots[i];
        if (r->layer < 0 || r->layer >= (int)scene.layers.size()) r->layer = 0;
    }
    scene.SortRootsByLayer();
    // 脚本（长度前缀，原始字符）
    size_t mark = text.rfind(L"script_len=");
    if (mark != std::wstring::npos) {
        size_t nl = text.find(L'\n', mark);
        if (nl == std::wstring::npos) nl = text.size();
        else nl += 1;
        std::wstring v;
        // 解析长度
        size_t eq = text.find(L'=', mark);
        if (eq != std::wstring::npos) {
            std::wstring numStr = Trim(text.substr(eq + 1, nl - eq - 1));
            int len = (int)wcstod(numStr.c_str(), NULL);
            if (len >= 0 && (int)(text.size() - nl) >= len) {
                scene.script = text.substr(nl, len);
            }
        }
    }
    // 关键帧脚本（layer_key_script layer=%d frame=%d len=%d + 原始文本；逐块扫描）
    {
        size_t kpos = 0;
        while ((kpos = text.find(L"layer_key_script ", kpos)) != std::wstring::npos) {
            size_t nl = text.find(L'\n', kpos);
            if (nl == std::wstring::npos) break;
            std::wstring head = Trim(text.substr(kpos, nl - kpos));
            int li = -1, fr = -1, len = -1;
            std::wstring val;
            size_t k = 0;
            while (k < head.size()) {
                size_t sp = head.find(L' ', k);
                if (sp == std::wstring::npos) sp = head.size();
                std::wstring item = head.substr(k, sp - k);
                if (GetKV(item, L"layer", val)) li = (int)wcstod(val.c_str(), NULL);
                else if (GetKV(item, L"frame", val)) fr = (int)wcstod(val.c_str(), NULL);
                else if (GetKV(item, L"len", val)) len = (int)wcstod(val.c_str(), NULL);
                if (sp >= head.size()) break;
                k = sp + 1;
            }
            size_t bodyStart = nl + 1;
            if (li >= 0 && li < (int)scene.layers.size() && len >= 0 &&
                bodyStart + (size_t)len <= text.size()) {
                std::wstring scr = text.substr(bodyStart, (size_t)len);
                LayerKey* plk = NULL;
                for (size_t kk = 0; kk < scene.layers[li].keys.size(); kk++)
                    if (scene.layers[li].keys[kk].frame == fr) { plk = &scene.layers[li].keys[kk]; break; }
                if (!plk) {   // 只有脚本没有关键帧本体时自动补一个（空白关键帧语义）
                    scene.layers[li].keys.push_back(LayerKey());
                    scene.layers[li].keys.back().frame = fr;
                    plk = &scene.layers[li].keys.back();
                }
                plk->script = scr;
            }
            kpos = bodyStart + (len >= 0 ? (size_t)len : 0) + 1;
        }
    }
    return true;
}

// ------------------------------------------------------------ 文件读写
bool WriteFileUtf8(const std::wstring& path, const std::wstring& text) {
    int cch = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
    std::vector<char> buf(cch + 3);
    buf[0] = (char)0xEF; buf[1] = (char)0xBB; buf[2] = (char)0xBF;
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &buf[3], cch, NULL, NULL);
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    ::WriteFile(h, &buf[0], (DWORD)buf.size(), &written, NULL);
    ::CloseHandle(h);
    return true;
}

bool ReadFileUtf8(const std::wstring& path, std::wstring& text) {
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD size = ::GetFileSize(h, NULL);
    std::vector<char> buf(size ? size : 1);
    DWORD read = 0;
    BOOL ok = ::ReadFile(h, &buf[0], size, &read, NULL);
    ::CloseHandle(h);
    if (!ok) return false;
    int start = 0;
    if (read >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        start = 3;
    int cch = ::MultiByteToWideChar(CP_UTF8, 0, &buf[start], read - start, NULL, 0);
    std::vector<wchar_t> wbuf(cch + 1);
    ::MultiByteToWideChar(CP_UTF8, 0, &buf[start], read - start, &wbuf[0], cch);
    wbuf[cch] = 0;
    text = &wbuf[0];
    return true;
}

std::wstring DocClassPath(const std::wstring& projectPath) {
    size_t dot = projectPath.find_last_of(L'.');
    size_t sep = projectPath.find_last_of(L"/\\");
    if (dot != std::wstring::npos && (sep == std::wstring::npos || dot > sep))
        return projectPath.substr(0, dot) + L".ss";
    return projectPath + L".ss";
}

bool SaveProject(Scene3D& scene, const std::wstring& path, std::wstring& err) {
    std::wstring text = SerializeToString(scene);
    if (!WriteFileUtf8(path, text)) {
        err = L"无法写入文件：";
        err += path;
        return false;
    }
    scene.projectPath = path;
    // 文档类独立存到同名同目录 .ss（外部 SuperScript 类文件，便于后续扩展多个类）
    std::wstring ssPath = DocClassPath(path);
    if (scene.docClass.empty()) {
        ::DeleteFileW(ssPath.c_str());   // 无文档类时清理旧 .ss，避免重开误加载
    } else if (!WriteFileUtf8(ssPath, scene.docClass)) {
        err = L"工程已保存，但无法写入文档类文件：";
        err += ssPath;
        return false;
    }
    return true;
}

bool LoadProject(Scene3D& scene, const std::wstring& path, std::wstring& err) {
    std::wstring text;
    if (!ReadFileUtf8(path, text)) {
        err = L"无法读取文件：";
        err += path;
        return false;
    }
    if (!DeserializeFromString(scene, text, err)) return false;
    scene.projectPath = path;
    // 文档类以同名 .ss 为准（外部 SuperScript 类文件）
    std::wstring ssText;
    if (ReadFileUtf8(DocClassPath(path), ssText)) {
        scene.docClass = ssText;
    }
    return true;
}

}
