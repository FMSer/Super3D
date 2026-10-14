// Scene3D.cpp : 场景数据模型实现
#include "Scene3D.h"
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <windows.h>   // 3D 文本：GetGlyphOutlineW 提取字形轮廓
// ---------------------------------------------------------------- Object3D
Object3D::Object3D()
    : type(PT_NONE), visible(true), locked(false), layer(0), r(0.8f), g(0.8f), b(0.8f), segments(24),
      x(0), y(0), z(0), rx(0), ry(0), rz(0), sx(1), sy(1), sz(1),
      dx(0), dy(0), dz(0), drx(0), dry(0), drz(0), dsx(1), dsy(1), dsz(1),
      parent(NULL), selected(false), _meshValid(false), _meshRev(0), _texID(0), _texW(0), _texH(0), _texValid(false) {
    params[0] = params[1] = params[2] = params[3] = 0.0;
    segments = 1;
}

void Object3D::InvalidateMesh() {
    _meshValid = false;
    _texValid = false;   // 文字/尺寸变化 → 纹理需重建（ID 复用，下次渲染重传像素）
}

// 测量文本位图尺寸：pxH 字号下渲染整串文本，返回紧贴字形的最小像素宽高（不带留白）
bool Object3D::MeasureTextBitmap(const std::wstring& text, const std::wstring& font,
                                 int pxH, int& wpx, int& hpx) {
    wpx = hpx = 0;
    if (text.empty() || pxH <= 0) return false;
    HDC hdc = ::CreateCompatibleDC(NULL);
    if (!hdc) return false;
    HFONT fnt = ::CreateFontW(-pxH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE,
                              font.empty() ? L"微软雅黑" : font.c_str());
    HGDIOBJ old = ::SelectObject(hdc, fnt);
    SIZE sz; ::GetTextExtentPoint32W(hdc, text.c_str(), (int)text.size(), &sz);
    if (sz.cx <= 0 || sz.cy <= 0) { sz.cx = pxH; sz.cy = pxH; }
    int bw = sz.cx + 8, bh = sz.cy + 8;
    BITMAPINFO bi; ::memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bw;
    bi.bmiHeader.biHeight = -bh;              // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = NULL;
    HBITMAP bmp = ::CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) { ::SelectObject(hdc, old); ::DeleteObject(fnt); ::DeleteDC(hdc); return false; }
    HGDIOBJ ob = ::SelectObject(hdc, bmp);
    // 白底黑字（便于后续用亮度生成 alpha）
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, RGB(0, 0, 0));
    RECT rc = { 0, 0, bw, bh };
    ::FillRect(hdc, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH));
    ::TextOutW(hdc, 4, 4, text.c_str(), (int)text.size());
    ::GdiFlush();
    // 扫描 alpha：白底 alpha=0，文字(变暗) alpha 依亮度。找文字 bbox
    const unsigned char* p = (const unsigned char*)bits;
    int mnx = bw, mny = bh, mxx = -1, mxy = -1;
    for (int y = 0; y < bh; y++) {
        for (int x = 0; x < bw; x++) {
            const unsigned char* q = p + (y * bw + x) * 4;   // BGRA
            unsigned char b = q[0], g = q[1], r = q[2];
            int lum = ((int)b + g + r) / 3;
            if (lum < 245) {   // 非白（文字/抗锯齿）
                if (x < mnx) mnx = x; if (x > mxx) mxx = x;
                if (y < mny) mny = y; if (y > mxy) mxy = y;
            }
        }
    }
    ::SelectObject(hdc, ob); ::DeleteObject(bmp);
    ::SelectObject(hdc, old); ::DeleteObject(fnt); ::DeleteDC(hdc);
    if (mxx < 0) return false;
    wpx = mxx - mnx + 1; hpx = mxy - mny + 1;
    return true;
}

Object3D::~Object3D() {
    if (g_meshCacheInvalidate) g_meshCacheInvalidate(this);   // 摘除显示列表缓存，避免悬空指针键被新对象复用
    for (size_t i = 0; i < children.size(); i++) delete children[i];
    children.clear();
}

void (*g_meshCacheInvalidate)(const Object3D*) = NULL;

const wchar_t* Object3D::TypeName(int t) {
    switch (t) {
        case PT_NONE:    return L"组";
        case PT_BOX:     return L"立方体";
        case PT_SPHERE:  return L"球";
        case PT_CYLINDER:return L"圆柱";
        case PT_CONE:    return L"圆锥";
        case PT_TORUS:   return L"圆环";
        case PT_PLANE:   return L"平面";
        case PT_SPLINE:  return L"曲线";
        case PT_LEAF:    return L"绿叶";
        case PT_TEXT3D:  return L"3D文本";
        case PT_TUBE:    return L"方管";
        case PT_RTUBE:   return L"圆管";
        case PT_BOOLEAN: return L"布尔孔";
    }
    return L"未知";
}

Object3D* Object3D::CreateByType(int t, const std::wstring& name) {
    Object3D* o = new Object3D();
    o->name = name;
    o->type = t;
    switch (t) {
        case PT_NONE:
            o->r = 0.75; o->g = 0.75; o->b = 0.8;
            o->segments = 1;
            break;
        case PT_BOX:
            o->params[0] = 2.0; o->params[1] = 2.0; o->params[2] = 2.0;
            o->r = 0.92; o->g = 0.62; o->b = 0.28;
            o->segments = 2;          // 每面 3×3 网格，焊接后约 26 个顶点（原 seg=8 为 386）
            break;
        case PT_SPHERE:
            o->params[0] = 1.0;
            o->r = 0.35; o->g = 0.62; o->b = 0.95;
            o->segments = 24;         // 原始细分：经纬 24 段，球面平滑
            break;
        case PT_CYLINDER:
            o->params[0] = 0.8; o->params[1] = 2.0;
            o->r = 0.52; o->g = 0.78; o->b = 0.42;
            o->segments = 24;         // 原始细分：截面 24 边形，接近圆
            break;
        case PT_CONE:
            o->params[0] = 1.0; o->params[1] = 2.0;
            o->r = 0.95; o->g = 0.45; o->b = 0.42;
            o->segments = 24;         // 原始细分：底面 24 边形，接近圆
            break;
        case PT_TORUS:
            o->params[0] = 1.2; o->params[1] = 0.35;
            o->r = 0.85; o->g = 0.5; o->b = 0.82;
            o->segments = 6;
            break;
        case PT_PLANE:
            o->params[0] = 10.0;
            o->r = 0.55; o->g = 0.82; o->b = 0.5;
            o->segments = 1;
            break;
        case PT_SPLINE:
            // 曲线：半径=params[0]，默认 3 个控制点（起点/中间/终点），形成一条弧形柳条
            o->params[0] = 0.12;
            o->r = 0.36; o->g = 0.58; o->b = 0.22;   // 枝条绿
            o->segments = 12;
            o->ctrlPts.push_back(Vec3(-1.5, 0, 0));
            o->ctrlPts.push_back(Vec3(0, 0.6, 0));
            o->ctrlPts.push_back(Vec3(1.5, 0, 0));
            break;
        case PT_LEAF:
            // 绿叶：长/宽/厚 半轴 = params[0..2]，扁椭球叶片
            o->params[0] = 0.8; o->params[1] = 0.45; o->params[2] = 0.06;
            o->r = 0.30; o->g = 0.62; o->b = 0.30;
            o->segments = 4;          // 点数约为 seg=12 时的 1/9
            break;
        case PT_TEXT3D:
            // 3D 文本：params[0]=厚度, params[1]=字高（单行高度），text=文字, font=字体
            o->params[0] = 0.3; o->params[1] = 2.0;
            o->text = L"3D文本";
            o->font = L"宋体";
            o->r = 0.10; o->g = 0.10; o->b = 0.12;
            o->segments = 1;
            break;
        case PT_TUBE:
            // 方形空管：params[0]=外边长, params[1]=内边长, params[2]=高度
            o->params[0] = 2.0; o->params[1] = 1.2; o->params[2] = 2.0;
            o->r = 0.62; o->g = 0.45; o->b = 0.85;
            o->segments = 1;
            break;
        case PT_RTUBE:
            // 圆形空管：params[0]=外半径, params[1]=内半径, params[2]=高度
            o->params[0] = 1.0; o->params[1] = 0.6; o->params[2] = 2.0;
            o->r = 0.40; o->g = 0.72; o->b = 0.82;
            o->segments = 24;         // 截面 24 边形，接近圆
            break;
        case PT_BOOLEAN:
            // 布尔差结果：params 保留原立方体尺寸，网格由序列化/生成填充
            o->params[0] = 2.0; o->params[1] = 2.0; o->params[2] = 2.0;
            o->r = 0.92; o->g = 0.62; o->b = 0.28;
            o->segments = 1;
            break;
    }
    return o;
}

Mat4 Object3D::LocalMatrix() const {
    Mat4 t = Mat4::Translation(x, y, z);
    Mat4 r = Mat4::RotationXYZ(rx, ry, rz);
    Mat4 s = Mat4::Scale(sx, sy, sz);
    return t.Mul(r).Mul(s);
}

Mat4 Object3D::WorldMatrix() const {
    if (parent) return parent->WorldMatrix().Mul(LocalMatrix());
    return LocalMatrix();
}

Mat4 Object3D::DisplayMatrix() const {
    Mat4 t = Mat4::Translation(dx, dy, dz);
    Mat4 r = Mat4::RotationXYZ(drx, dry, drz);
    Mat4 s = Mat4::Scale(dsx, dsy, dsz);
    return t.Mul(r).Mul(s);
}

Mat4 Object3D::DisplayWorldMatrix() const {
    if (parent) return parent->DisplayWorldMatrix().Mul(DisplayMatrix());
    return DisplayMatrix();
}

void Object3D::SyncDisplayToLocal() {
    dx = x; dy = y; dz = z;
    drx = rx; dry = ry; drz = rz;
    dsx = sx; dsy = sy; dsz = sz;
}

void Object3D::CopyLocalFromDisplay() {
    x = dx; y = dy; z = dz;
    rx = drx; ry = dry; rz = drz;
    sx = dsx; sy = dsy; sz = dsz;
}

void Object3D::CopyLocalToKey(KeyFrame& k) const {
    k.CopyFrom(x, y, z, rx, ry, rz, sx, sy, sz);
}

void Object3D::CopyKeyToLocal(const KeyFrame& k) {
    x = k.px; y = k.py; z = k.pz;
    rx = k.rx; ry = k.ry; rz = k.rz;
    sx = k.sx; sy = k.sy; sz = k.sz;
}

bool Object3D::HasKeyAt(int frame) const {
    for (size_t i = 0; i < keys.size(); i++)
        if (keys[i].frame == frame) return true;
    return false;
}

void Object3D::InsertKey(int frame) {
    KeyFrame k;
    k.frame = frame;
    CopyLocalToKey(k);
    InsertKeyFrame(k);
}

void Object3D::InsertKeyFrame(const KeyFrame& k) {
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame == k.frame) { keys[i] = k; return; }
    }
    keys.push_back(k);
    // 按 frame 排序
    for (size_t i = keys.size() - 1; i > 0; i--) {
        if (keys[i].frame < keys[i - 1].frame) {
            KeyFrame tmp = keys[i]; keys[i] = keys[i - 1]; keys[i - 1] = tmp;
        } else break;
    }
}

void Object3D::RemoveKey(int frame) {
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame == frame) {
            keys.erase(keys.begin() + i);
            return;
        }
    }
}

// ------------------------------------------------------------ 网格生成
// 输出 verts/normals 配对（每三角形 3 顶 + 3 法线）
static void EmitTri(std::vector<Vec3>& v, std::vector<Vec3>& n,
                    const Vec3& a, const Vec3& b, const Vec3& c,
                    const Vec3& na, const Vec3& nb, const Vec3& nc) {
    v.push_back(a); n.push_back(na);
    v.push_back(b); n.push_back(nb);
    v.push_back(c); n.push_back(nc);
}

// 一个四边形（两个三角形），平面法线
static void EmitQuad(std::vector<Vec3>& v, std::vector<Vec3>& n,
                     const Vec3& nl,
                     const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    EmitTri(v, n, a, b, c, nl, nl, nl);
    EmitTri(v, n, a, c, d, nl, nl, nl);
}

// 把四边形（角点按环绕顺序 q0→q1→q2→q3）细分为 seg×seg 网格面片
// 双线性插值保证细分点严格落在原平面上（相邻面共享边完全重合，焊接后不开裂）
static void EmitQuadGrid(std::vector<Vec3>& v, std::vector<Vec3>& n,
                         const Vec3& nl,
                         const Vec3& q0, const Vec3& q1, const Vec3& q2, const Vec3& q3,
                         int seg) {
    for (int j = 0; j < seg; j++) {
        double fv0 = (double)j / seg, fv1 = (double)(j + 1) / seg;
        for (int i = 0; i < seg; i++) {
            double fu0 = (double)i / seg, fu1 = (double)(i + 1) / seg;
            Vec3 a = q0 + (q1 - q0) * fu0; Vec3 b = q3 + (q2 - q3) * fu0;
            Vec3 p00 = a + (b - a) * fv0;
            Vec3 c = q0 + (q1 - q0) * fu1; Vec3 d = q3 + (q2 - q3) * fu1;
            Vec3 p10 = c + (d - c) * fv0;
            Vec3 p11 = c + (d - c) * fv1;
            Vec3 p01 = a + (b - a) * fv1;
            EmitQuad(v, n, nl, p00, p10, p11, p01);
        }
    }
}

// ------------------------------------------------------------ 布尔挖孔（CSG 差：立方体 − 实心圆柱/方柱）
struct Plane { Vec3 n; double d; };   // 半空间：dot(n,p)+d <= 0 视为体内

// 用平面把凸多边形裁成 in(体内侧)/out(体外侧) 两段（Sutherland-Hodgman）
static void ClipPolyPlane(const std::vector<Vec3>& poly, const Vec3& n, double d,
                          std::vector<Vec3>& in, std::vector<Vec3>& out) {
    size_t m = poly.size();
    for (size_t i = 0; i < m; i++) {
        const Vec3& p0 = poly[i];
        const Vec3& p1 = poly[(i + 1) % m];
        double d0 = Dot(n, p0) + d, d1 = Dot(n, p1) + d;
        bool i0 = (d0 <= 1e-9), i1 = (d1 <= 1e-9);
        if (i0) in.push_back(p0); else out.push_back(p0);
        if (i0 != i1) {
            double t = d0 / (d0 - d1);
            Vec3 p = p0 + (p1 - p0) * t;
            in.push_back(p); out.push_back(p);
        }
    }
}
// 严格内侧裁剪：只有 dot(n,p)+d < -eps 才算体内（剔除与面共面的边界，用于孔壁）
static void ClipPolyPlaneStrict(const std::vector<Vec3>& poly, const Vec3& n, double d,
                                std::vector<Vec3>& in, std::vector<Vec3>& out) {
    size_t m = poly.size();
    for (size_t i = 0; i < m; i++) {
        const Vec3& p0 = poly[i];
        const Vec3& p1 = poly[(i + 1) % m];
        double d0 = Dot(n, p0) + d, d1 = Dot(n, p1) + d;
        bool i0 = (d0 < -1e-9), i1 = (d1 < -1e-9);
        if (i0) in.push_back(p0); else out.push_back(p0);
        if (i0 != i1) {
            double t = d0 / (d0 - d1);
            Vec3 p = p0 + (p1 - p0) * t;
            in.push_back(p); out.push_back(p);
        }
    }
}
static void EmitPolyTri(std::vector<Vec3>& v, std::vector<Vec3>& n,
                        const std::vector<Vec3>& poly, const Vec3& nrm) {
    if (poly.size() < 3) return;
    for (size_t i = 1; i + 1 < poly.size(); i++) {
        v.push_back(poly[0]); v.push_back(poly[i]); v.push_back(poly[i + 1]);
        n.push_back(nrm); n.push_back(nrm); n.push_back(nrm);
    }
}

static double Dist2(const Vec3& a, const Vec3& b) {
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
// 2D 线段严格相交（不含端点）
static bool SegIntersect2D(double x1, double y1, double x2, double y2,
                           double x3, double y3, double x4, double y4) {
    double d = (x2 - x1) * (y4 - y3) - (y2 - y1) * (x4 - x3);
    if (fabs(d) < 1e-12) return false;
    double t = ((x3 - x1) * (y4 - y3) - (y3 - y1) * (x4 - x3)) / d;
    double u = ((x3 - x1) * (y2 - y1) - (y3 - y1) * (x2 - x1)) / d;
    return t > 1e-9 && t < 1.0 - 1e-9 && u > 1e-9 && u < 1.0 - 1e-9;
}
// 点是否在多边形内（含边界）
static bool PtInPoly2D(double px, double py, const std::vector<double>& x, const std::vector<double>& y) {
    bool in = false;
    for (size_t i = 0, j = x.size() - 1; i < x.size(); j = i++) {
        if (((y[i] > py) != (y[j] > py)) &&
            (px < (x[j] - x[i]) * (py - y[i]) / (y[j] - y[i]) + x[i]))
            in = !in;
    }
    return in;
}
static bool PtInTri2D(double px, double py,
                      double ax, double ay, double bx, double by, double cx, double cy) {
    double d1 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    double d2 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
    double d3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
    // 严格内部（不含边界）：桥接环含重复顶点，边界点不得阻挡耳切
    return (d1 > 1e-12 && d2 > 1e-12 && d3 > 1e-12) ||
           (d1 < -1e-12 && d2 < -1e-12 && d3 < -1e-12);
}

// 把洞（顺时针）桥接进外环（逆时针），形成单轮廓，供耳切
// 输出环：ring[0..bi], hole[bj..bj+h-1], hole[bj], ring[bi+1..] —— 洞起终点各出现一次，无多余重复
static void BridgeHole(std::vector<Vec3>& ring, const std::vector<Vec3>& hole) {
    if (hole.size() < 3) return;
    int bi = -1, bj = -1; double bd = 1e30;
    for (int i = 0; i < (int)ring.size(); i++)
        for (int j = 0; j < (int)hole.size(); j++) {
            double d = Dist2(ring[i], hole[j]);
            if (d < bd) { bd = d; bi = i; bj = j; }
        }
    if (bi < 0 || bj < 0) return;
    std::vector<Vec3> nr;
    for (int k = 0; k <= bi; k++) nr.push_back(ring[k]);           // 外环到 bridge 点
    int h = (int)hole.size();
    for (int c = 0; c < h; c++) nr.push_back(hole[(bj + c) % h]); // 洞一圈
    nr.push_back(hole[bj]);                                        // 洞闭合回 bridge 点
    for (int k = bi + 1; k < (int)ring.size(); k++) nr.push_back(ring[k]); // 外环剩余
    ring = nr;
}

// 耳切三角化单轮廓（投影到 nrm 平面 2D），顶点为逆时针
// hole：原洞多边形（投影同平面），跨洞/覆盖洞的耳朵被拒绝（保证孔不被填充）
static void EarClipTri(std::vector<Vec3>& v, std::vector<Vec3>& n,
                       std::vector<Vec3> ring, const Vec3& nrm,
                       const std::vector<Vec3>& hole) {
    if (ring.size() < 3) return;
    if (ring.size() == 3) {
        v.push_back(ring[0]); v.push_back(ring[1]); v.push_back(ring[2]);
        n.push_back(nrm); n.push_back(nrm); n.push_back(nrm);
        return;
    }
    Vec3 u = Normalize(Cross(nrm, Vec3(1, 0, 0)));
    if (Length(u) < 1e-9) u = Normalize(Cross(nrm, Vec3(0, 1, 0)));
    Vec3 w = Cross(nrm, u);
    std::vector<double> px(ring.size()), py(ring.size());
    for (size_t i = 0; i < ring.size(); i++) { px[i] = Dot(ring[i], u); py[i] = Dot(ring[i], w); }
    // 规范化环绕方向为逆时针：保证凸角判定（cross<0）一致
    double area = 0;
    for (size_t i = 0; i < ring.size(); i++) {
        size_t j = (i + 1) % ring.size();
        area += px[i] * py[j] - px[j] * py[i];
    }
    if (area < 0) {
        std::reverse(ring.begin(), ring.end());
        for (size_t i = 0; i < ring.size(); i++) { px[i] = Dot(ring[i], u); py[i] = Dot(ring[i], w); }
    }
    // 洞投影
    std::vector<double> hx(hole.size()), hy(hole.size());
    for (size_t i = 0; i < hole.size(); i++) { hx[i] = Dot(hole[i], u); hy[i] = Dot(hole[i], w); }
    // 耳朵是否覆盖/跨越孔洞（跨洞耳朵会填孔 → 拒绝）
    auto earBlocked = [&](size_t im, size_t ic, size_t ip) -> bool {
        // 1) 耳朵任一边与洞边严格相交
        for (int e = 0; e < 3; e++) {
            size_t a = (e == 0) ? im : (e == 1) ? ic : ip;
            size_t b = (e == 0) ? ic : (e == 1) ? ip : im;
            for (size_t q = 0; q < hole.size(); q++) {
                size_t r = (q + 1) % hole.size();
                if (SegIntersect2D(px[a], py[a], px[b], py[b], hx[q], hy[q], hx[r], hy[r]))
                    return true;
            }
        }
        // 2) 洞完全在耳朵内
        if (hole.size() >= 3) {
            bool allIn = true;
            for (size_t q = 0; q < hole.size(); q++)
                if (!PtInTri2D(hx[q], hy[q], px[ic], py[ic], px[ip], py[ip], px[im], py[im])) { allIn = false; break; }
            if (allIn) return true;
        }
        // 3) 耳朵中心落在洞内（无边相交但覆盖洞的情形）
        double cxm = (px[im] + px[ic] + px[ip]) / 3.0, cym = (py[im] + py[ic] + py[ip]) / 3.0;
        if (hole.size() >= 3 && PtInPoly2D(cxm, cym, hx, hy)) return true;
        return false;
    };
    std::vector<int> idx;
    for (int i = 0; i < (int)ring.size(); i++) idx.push_back(i);
    int guard = 0;
    while (idx.size() > 3 && guard < 100000) {
        guard++;
        bool clipped = false;
        for (size_t k = 0; k < idx.size(); k++) {
            size_t im = idx[(k + idx.size() - 1) % idx.size()];
            size_t ic = idx[k];
            size_t ip = idx[(k + 1) % idx.size()];
            double cross = (px[ip] - px[ic]) * (py[im] - py[ic]) - (py[ip] - py[ic]) * (px[im] - px[ic]);
            if (cross >= 0) continue;   // 非凸角（逆时针凸角叉积为负）
            bool contains = false;
            for (size_t q = 0; q < idx.size(); q++) {
                if (q == k || q == (k + 1) % idx.size() || q == (k + idx.size() - 1) % idx.size()) continue;
                size_t qi = idx[q];
                if (PtInTri2D(px[qi], py[qi], px[ic], py[ic], px[ip], py[ip], px[im], py[im])) { contains = true; break; }
            }
            if (!contains && !earBlocked(im, ic, ip)) {
                v.push_back(ring[im]); v.push_back(ring[ic]); v.push_back(ring[ip]);
                n.push_back(nrm); n.push_back(nrm); n.push_back(nrm);
                idx.erase(idx.begin() + k);
                clipped = true;
                break;
            }
        }
        if (!clipped) break;
    }
    if (idx.size() >= 3)
        for (size_t k = 1; k + 1 < idx.size(); k++) {
            v.push_back(ring[idx[0]]); v.push_back(ring[idx[k]]); v.push_back(ring[idx[k + 1]]);
            n.push_back(nrm); n.push_back(nrm); n.push_back(nrm);
        }
}

// 三角化带洞多边形（外环逆时针 + 洞顺时针）
static void TriangulateFace(std::vector<Vec3>& v, std::vector<Vec3>& n,
                            std::vector<Vec3> outer, std::vector<Vec3> hole, const Vec3& nrm) {
    if (hole.empty()) { EmitPolyTri(v, n, outer, nrm); return; }
    BridgeHole(outer, hole);
    EarClipTri(v, n, outer, nrm, hole);
}

// 孔体类型：凸体（盒/球/柱/锥/方管/圆管，走凸包平面）+ 布尔结果（走网格体内判定）
static bool IsHoleType(int t) {
    return t == PT_BOX || t == PT_SPHERE || t == PT_CYLINDER || t == PT_CONE || t == PT_TUBE || t == PT_RTUBE || t == PT_BOOLEAN;
}
// 可布尔目标类型（图元 + 布尔结果：布尔结果可再次参与布尔运算）
static bool IsBoolTargetType(int t) {
    return (t >= PT_BOX && t <= PT_RTUBE) || t == PT_BOOLEAN;
}

// 生成孔体 B 的表面三角形（A 局部，孔壁素材）与凸包平面（A 局部，减体判定）。B 为凸体。
static void BuildHoleSolid(const Object3D* B,
                           const std::function<Vec3(const Vec3&)>& toA,
                           const std::function<Vec3(const Vec3&)>& dirToA,
                           std::vector<Vec3>& bV, std::vector<Vec3>& bN,
                           std::vector<Plane>& Bplanes,
                           int op = 0) {
    bV.clear(); bN.clear(); Bplanes.clear();
    if (B->type == PT_BOX) {
        // 实心立方体：6 面全参与凸包 + 孔壁
        double hx = B->params[0] * 0.5, hy = B->params[1] * 0.5, hz = B->params[2] * 0.5;
        if (hx <= 0) hx = 1; if (hy <= 0) hy = 1; if (hz <= 0) hz = 1;
        Vec3 c[8] = { Vec3(-hx,-hy,-hz),Vec3( hx,-hy,-hz),Vec3( hx,-hy, hz),Vec3(-hx,-hy, hz),
                      Vec3(-hx, hy,-hz),Vec3( hx, hy,-hz),Vec3( hx, hy, hz),Vec3(-hx, hy, hz) };
        struct F { int a, b, c, d; Vec3 n; };
        F fs[6] = { {0,3,2,1,Vec3(0,-1,0)}, {4,5,6,7,Vec3(0,1,0)},
                    {0,4,7,3,Vec3(-1,0,0)}, {1,2,6,5,Vec3(1,0,0)},
                    {0,1,5,4,Vec3(0,0,-1)}, {3,7,6,2,Vec3(0,0,1)} };
        for (int i = 0; i < 6; i++) {
            const F& f = fs[i];
            Vec3 nA = dirToA(f.n);
            Bplanes.push_back({ nA, -Dot(nA, toA(c[f.a])) });
            EmitTri(bV, bN, toA(c[f.a]), toA(c[f.b]), toA(c[f.c]), nA, nA, nA);
            EmitTri(bV, bN, toA(c[f.a]), toA(c[f.c]), toA(c[f.d]), nA, nA, nA);
        }
    } else if (B->type == PT_SPHERE) {
        // 实心球：经纬网格表面（孔壁素材）；凸包平面用轴对齐盒近似（球孔为方形近似）
        double r = B->params[0];
        if (r <= 0) r = 1;
        const int N = 16, S = 12;
        for (int i = 0; i < S; i++) {
            double lat0 = ((double)i / S) * kPI - kPI * 0.5;
            double lat1 = ((double)(i + 1) / S) * kPI - kPI * 0.5;
            double s0 = std::sin(lat0), c0 = std::cos(lat0);
            double s1 = std::sin(lat1), c1 = std::cos(lat1);
            for (int j = 0; j < N; j++) {
                double lon0 = ((double)j / N) * 2.0 * kPI;
                double lon1 = ((double)(j + 1) / N) * 2.0 * kPI;
                Vec3 n00(c0 * std::cos(lon0), s0, c0 * std::sin(lon0));
                Vec3 n10(c1 * std::cos(lon0), s1, c1 * std::sin(lon0));
                Vec3 n11(c1 * std::cos(lon1), s1, c1 * std::sin(lon1));
                Vec3 n01(c0 * std::cos(lon1), s0, c0 * std::sin(lon1));
                Vec3 nA00 = dirToA(n00), nA10 = dirToA(n10), nA11 = dirToA(n11), nA01 = dirToA(n01);
                EmitTri(bV, bN, toA(n00 * r), toA(n10 * r), toA(n11 * r), nA00, nA10, nA11);
                EmitTri(bV, bN, toA(n00 * r), toA(n11 * r), toA(n01 * r), nA00, nA11, nA01);
            }
        }
        Vec3 na[6] = { Vec3(1,0,0),Vec3(-1,0,0),Vec3(0,1,0),Vec3(0,-1,0),Vec3(0,0,1),Vec3(0,0,-1) };
        for (int i = 0; i < 6; i++) {
            Vec3 nA = dirToA(na[i]);
            Bplanes.push_back({ nA, -Dot(nA, toA(na[i] * r)) });
        }
    } else if (B->type == PT_CYLINDER) {
        // 实心圆柱：侧面棱柱 + 顶底圆盘
        // N 跟随 B->segments（与 GenerateMeshCore 同样下限4），使布尔孔形与视觉网格一致
        double r = B->params[0], bh = B->params[1] * 0.5;
        if (r <= 0) r = 1; if (bh <= 0) bh = 1;
        int N = B->segments < 4 ? 4 : (B->segments > 64 ? 64 : B->segments);
        for (int i = 0; i < N; i++) {
            double a0 = 2.0 * kPI * i / N, a1 = 2.0 * kPI * (i + 1) / N;
            Vec3 p0l(r * cos(a0), -bh, r * sin(a0)), p0h(r * cos(a0), bh, r * sin(a0));
            Vec3 p1l(r * cos(a1), -bh, r * sin(a1)), p1h(r * cos(a1), bh, r * sin(a1));
            Vec3 sideN = Normalize(Vec3(std::sin(a1) - std::sin(a0), 0, -(std::cos(a1) - std::cos(a0))));
            Vec3 nA = dirToA(sideN);
            EmitTri(bV, bN, toA(p0l), toA(p1l), toA(p1h), nA, nA, nA);
            EmitTri(bV, bN, toA(p0l), toA(p1h), toA(p0h), nA, nA, nA);
            Bplanes.push_back({ nA, -Dot(nA, toA(p0l)) });
        }
        Vec3 upA = dirToA(Vec3(0, 1, 0)), dnA = dirToA(Vec3(0, -1, 0));
        for (int i = 0; i < N; i++) {
            double a0 = 2.0 * kPI * i / N, a1 = 2.0 * kPI * (i + 1) / N;
            Vec3 c0(r * cos(a0), bh, r * sin(a0)), c1(r * cos(a1), bh, r * sin(a1));
            EmitTri(bV, bN, toA(c0), toA(c1), toA(Vec3(0, bh, 0)), upA, upA, upA);
            Vec3 d0(r * cos(a0), -bh, r * sin(a0)), d1(r * cos(a1), -bh, r * sin(a1));
            EmitTri(bV, bN, toA(d0), toA(Vec3(0, -bh, 0)), toA(d1), dnA, dnA, dnA);
        }
        Bplanes.push_back({ upA, -Dot(upA, toA(Vec3(0, bh, 0))) });
        Bplanes.push_back({ dnA, -Dot(dnA, toA(Vec3(0, -bh, 0))) });
    } else if (B->type == PT_CONE) {
        // 实心圆锥：侧面 + 底面（圆锥为凸体）
        // N 跟随 B->segments，使布尔孔形与视觉网格一致
        double r = B->params[0], bh = B->params[1] * 0.5;
        if (r <= 0) r = 1; if (bh <= 0) bh = 1;
        int N = B->segments < 4 ? 4 : (B->segments > 64 ? 64 : B->segments);
        Vec3 apex(0, bh, 0);
        for (int i = 0; i < N; i++) {
            double a0 = 2.0 * kPI * i / N, a1 = 2.0 * kPI * (i + 1) / N;
            Vec3 pb0(r * cos(a0), -bh, r * sin(a0)), pb1(r * cos(a1), -bh, r * sin(a1));
            Vec3 sideN = Normalize(Cross(pb1 - pb0, apex - pb0));
            Vec3 nA = dirToA(sideN);
            EmitTri(bV, bN, toA(apex), toA(pb0), toA(pb1), nA, nA, nA);
            Bplanes.push_back({ nA, -Dot(nA, toA(pb0)) });
        }
        Vec3 dnA = dirToA(Vec3(0, -1, 0));
        for (int i = 0; i < N; i++) {
            double a0 = 2.0 * kPI * i / N, a1 = 2.0 * kPI * (i + 1) / N;
            Vec3 d0(r * cos(a0), -bh, r * sin(a0)), d1(r * cos(a1), -bh, r * sin(a1));
            EmitTri(bV, bN, toA(d0), toA(Vec3(0, -bh, 0)), toA(d1), dnA, dnA, dnA);
        }
        Bplanes.push_back({ dnA, -Dot(dnA, toA(Vec3(0, -bh, 0))) });
    } else if (B->type == PT_RTUBE) {
        // 圆形空管：外圆柱(params[0]) − 内圆柱(params[1]) 空心壳
        // 差集(op=0)：按"外圆柱实心"减体（挖出能放进整根管的圆孔，与方管口径一致）
        // 并/交(op!=0)：按"管壳"参与——交集=管壁圆环框架，并集=实心段+空心管伸出段
        double ro = B->params[0], ri = B->params[1], bh = B->params[2] * 0.5;
        if (ro <= 0) ro = 1;
        if (bh <= 0) bh = 1;
        if (ri <= 0 || ri >= ro) ri = ro * 0.6;
        int N = B->segments < 4 ? 4 : (B->segments > 64 ? 64 : B->segments);
        Vec3 upA = dirToA(Vec3(0, 1, 0)), dnA = dirToA(Vec3(0, -1, 0));
        for (int i = 0; i < N; i++) {
            double a0 = 2.0 * kPI * i / N, a1 = 2.0 * kPI * (i + 1) / N;
            Vec3 n0(std::cos(a0), 0, std::sin(a0)), n1(std::cos(a1), 0, std::sin(a1));
            Vec3 ol0(n0.x * ro, -bh, n0.z * ro), ol1(n1.x * ro, -bh, n1.z * ro);
            Vec3 oh0(n0.x * ro,  bh, n0.z * ro), oh1(n1.x * ro,  bh, n1.z * ro);
            Vec3 nA = dirToA(Normalize(Vec3(std::sin(a1) - std::sin(a0), 0, -(std::cos(a1) - std::cos(a0)))));
            Bplanes.push_back({ nA, -Dot(nA, toA(ol0)) });   // 外圆柱侧面凸包平面
            // 管壳表面：外侧面（朝外）
            EmitTri(bV, bN, toA(ol0), toA(ol1), toA(oh1), nA, nA, nA);
            EmitTri(bV, bN, toA(ol0), toA(oh1), toA(oh0), nA, nA, nA);
            // 内侧面（朝轴心）
            Vec3 il0(n0.x * ri, -bh, n0.z * ri), il1(n1.x * ri, -bh, n1.z * ri);
            Vec3 ih0(n0.x * ri,  bh, n0.z * ri), ih1(n1.x * ri,  bh, n1.z * ri);
            EmitTri(bV, bN, toA(il0), toA(ih1), toA(ih0), -nA, -nA, -nA);
            EmitTri(bV, bN, toA(il0), toA(il1), toA(ih1), -nA, -nA, -nA);
            // 顶盖环 / 底盖环（外圈↔内圈）
            EmitTri(bV, bN, toA(oh0), toA(oh1), toA(ih1), upA, upA, upA);
            EmitTri(bV, bN, toA(oh0), toA(ih1), toA(ih0), upA, upA, upA);
            EmitTri(bV, bN, toA(ol0), toA(il0), toA(il1), dnA, dnA, dnA);
            EmitTri(bV, bN, toA(ol0), toA(il1), toA(ol1), dnA, dnA, dnA);
        }
        Bplanes.push_back({ upA, -Dot(upA, toA(Vec3(0, bh, 0))) });
        Bplanes.push_back({ dnA, -Dot(dnA, toA(Vec3(0, -bh, 0))) });
    } else {  // PT_TUBE：方管 = 外方柱(params[0]) − 内方柱(params[1]) 空心壳
        // 差集(op=0)：按"外方柱实心"减体（挖出能放进整根管的方孔，用户验收效果）
        // 并/交(op!=0)：按"管壳"参与——交集=管壁框架，并集=圆柱+空心管伸出段
        double w = B->params[0] * 0.5, bh = B->params[2] * 0.5;
        double wi = B->params[1] * 0.5;
        if (w <= 0) w = 1; if (bh <= 0) bh = 1;
        if (wi <= 0) wi = w * 0.6; if (wi >= w) wi = w * 0.6;
        Vec3 c[8] = { Vec3(-w,-bh,-w),Vec3( w,-bh,-w),Vec3( w,-bh, w),Vec3(-w,-bh, w),
                      Vec3(-w, bh,-w),Vec3( w, bh,-w),Vec3( w, bh, w),Vec3(-w, bh, w) };
        struct F { int a, b, c, d; Vec3 n; };
        F fs[6] = { {0,3,2,1,Vec3(0,-1,0)}, {4,5,6,7,Vec3(0,1,0)},
                    {0,4,7,3,Vec3(-1,0,0)}, {1,2,6,5,Vec3(1,0,0)},
                    {0,1,5,4,Vec3(0,0,-1)}, {3,7,6,2,Vec3(0,0,1)} };
        for (int i = 0; i < 6; i++) {
            const F& f = fs[i];
            Vec3 nA = dirToA(f.n);
            Bplanes.push_back({ nA, -Dot(nA, toA(c[f.a])) });   // 外方柱 6 平面（凸包）
        }
        {
            // 管壳表面（三种运算都生成；下游按 A 裁剪：差集=挖槽，外侧面成槽外壁、内侧面成管腔方柱侧壁）
            for (int i = 2; i < 6; i++) {
                const F& f = fs[i];
                Vec3 nA = dirToA(f.n);
                EmitTri(bV, bN, toA(c[f.a]), toA(c[f.b]), toA(c[f.c]), nA, nA, nA);
                EmitTri(bV, bN, toA(c[f.a]), toA(c[f.c]), toA(c[f.d]), nA, nA, nA);
            }
            Vec3 ci[8] = { Vec3(-wi,-bh,-wi),Vec3( wi,-bh,-wi),Vec3( wi,-bh, wi),Vec3(-wi,-bh, wi),
                           Vec3(-wi, bh,-wi),Vec3( wi, bh,-wi),Vec3( wi, bh, wi),Vec3(-wi, bh, wi) };
            F gs[4] = { {0,3,7,4,Vec3( 1,0,0)},   // 内左侧 x=-wi，管腔在 +x
                        {1,2,6,5,Vec3(-1,0,0)},   // 内右侧 x=+wi，管腔在 -x
                        {0,1,5,4,Vec3(0,0, 1)},   // 内前侧 z=-wi，管腔在 +z
                        {3,7,6,2,Vec3(0,0,-1)} }; // 内后侧 z=+wi，管腔在 -z
            for (int i = 0; i < 4; i++) {
                const F& g = gs[i];
                Vec3 nA = dirToA(g.n);
                EmitTri(bV, bN, toA(ci[g.a]), toA(ci[g.b]), toA(ci[g.c]), nA, nA, nA);
                EmitTri(bV, bN, toA(ci[g.a]), toA(ci[g.c]), toA(ci[g.d]), nA, nA, nA);
            }
            // 顶盖环（y=+bh）与底盖环（y=-bh）：外环 ↔ 内环 4 梯形
            Vec3 upA = dirToA(Vec3(0, 1, 0)), dnA = dirToA(Vec3(0, -1, 0));
            for (int i = 0; i < 4; i++) {
                int te0 = 4 + i, te1 = 4 + (i + 1) % 4;    // 顶外环（c[4..7]）
                int ti0 = 4 + i, ti1 = 4 + (i + 1) % 4;    // 顶内环（ci[4..7]）
                EmitTri(bV, bN, toA(c[te0]), toA(c[te1]), toA(ci[ti1]), upA, upA, upA);
                EmitTri(bV, bN, toA(c[te0]), toA(ci[ti1]), toA(ci[ti0]), upA, upA, upA);
                int be0 = i, be1 = (i + 1) % 4;            // 底外环（c[0..3]）
                int bi0 = i, bi1 = (i + 1) % 4;            // 底内环（ci[0..3]）
                EmitTri(bV, bN, toA(c[be0]), toA(c[be1]), toA(ci[bi1]), dnA, dnA, dnA);
                EmitTri(bV, bN, toA(c[be0]), toA(ci[bi1]), toA(ci[bi0]), dnA, dnA, dnA);
            }
        }
    }
}

// A 的凸包平面（A 局部），孔壁裁剪用。仅 BOX/TUBE（平面精确）；其余类型返回空 → 孔壁用中心点解析判定。
static void BuildASolid(const Object3D* A, std::vector<Plane>& Aplanes) {
    Aplanes.clear();
    if (A->type == PT_BOX) {
        double hx = A->params[0] * 0.5, hy = A->params[1] * 0.5, hz = A->params[2] * 0.5;
        if (hx <= 0) hx = 1; if (hy <= 0) hy = 1; if (hz <= 0) hz = 1;
        Aplanes.push_back({ Vec3( 1,0,0),-hx }); Aplanes.push_back({ Vec3(-1,0,0),-hx });
        Aplanes.push_back({ Vec3(0, 1,0),-hy }); Aplanes.push_back({ Vec3(0,-1,0),-hy });
        Aplanes.push_back({ Vec3(0,0, 1),-hz }); Aplanes.push_back({ Vec3(0,0,-1),-hz });
    } else if (A->type == PT_TUBE) {
        double w = A->params[0] * 0.5, bh = A->params[2] * 0.5;
        if (w <= 0) w = 1; if (bh <= 0) bh = 1;
        Aplanes.push_back({ Vec3( 1,0,0),-w }); Aplanes.push_back({ Vec3(-1,0,0),-w });
        Aplanes.push_back({ Vec3(0, 1,0),-bh }); Aplanes.push_back({ Vec3(0,-1,0),-bh });
        Aplanes.push_back({ Vec3(0,0, 1),-w }); Aplanes.push_back({ Vec3(0,0,-1),-w });
    }
}

// 点（A 局部）是否在 A 几何内部（解析判定，供非 BOX/TUBE 的孔壁整三角形保留）
static bool PointInsideA(const Object3D* A, const Vec3& p) {
    switch (A->type) {
        case PT_SPHERE: {
            double r = A->params[0]; if (r <= 0) r = 1;
            return p.x * p.x + p.y * p.y + p.z * p.z <= r * r;
        }
        case PT_CYLINDER: {
            double r = A->params[0], bh = A->params[1] * 0.5;
            if (r <= 0) r = 1; if (bh <= 0) bh = 1;
            return p.x * p.x + p.z * p.z <= r * r && std::fabs(p.y) <= bh;
        }
        case PT_CONE: {
            double r = A->params[0], bh = A->params[1] * 0.5;
            if (r <= 0) r = 1; if (bh <= 0) bh = 1;
            if (p.y < -bh || p.y > bh) return false;
            double rr = r * (bh - p.y) / (2.0 * bh);
            if (rr < 0) rr = 0;
            return p.x * p.x + p.z * p.z <= rr * rr;
        }
        case PT_TORUS: {
            double R = A->params[0], r = A->params[1];
            if (R <= 0) R = 1; if (r <= 0) r = 0.3;
            double d = std::sqrt(p.x * p.x + p.z * p.z);
            double dd = std::sqrt((d - R) * (d - R) + p.y * p.y);
            return dd <= r;
        }
        case PT_PLANE: {
            double hx = A->params[0] * 0.5, hz = A->params[0] * 0.5;
            if (hx <= 0) hx = 0.5; if (hz <= 0) hz = 0.5;
            return std::fabs(p.x) <= hx && std::fabs(p.y) <= 0.1 && std::fabs(p.z) <= hz;
        }
        default:
            return false;   // TEXT3D/SPLINE/LEAF：不做孔壁
    }
}

// 4-1 细分三角形直到最大边长 <= maxE（用于非平面 A 的 B 表面近似判定）
static void SubdivTri(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& n,
                      double maxE, std::vector<Vec3>& v, std::vector<Vec3>& nrm) {
    Vec3 d1(b.x - a.x, b.y - a.y, b.z - a.z);
    Vec3 d2(c.x - b.x, c.y - b.y, c.z - b.z);
    Vec3 d3(a.x - c.x, a.y - c.y, a.z - c.z);
    double l1 = sqrt(Dot(d1, d1)), l2 = sqrt(Dot(d2, d2)), l3 = sqrt(Dot(d3, d3));
    double mx = l1 > l2 ? (l1 > l3 ? l1 : l3) : (l2 > l3 ? l2 : l3);
    if (mx <= maxE) {
        v.push_back(a); v.push_back(b); v.push_back(c);
        nrm.push_back(n); nrm.push_back(n); nrm.push_back(n);
        return;
    }
    Vec3 ab((a.x + b.x) * 0.5, (a.y + b.y) * 0.5, (a.z + b.z) * 0.5);
    Vec3 bc((b.x + c.x) * 0.5, (b.y + c.y) * 0.5, (b.z + c.z) * 0.5);
    Vec3 ca((c.x + a.x) * 0.5, (c.y + a.y) * 0.5, (c.z + a.z) * 0.5);
    SubdivTri(a, ab, ca, n, maxE, v, nrm);
    SubdivTri(b, bc, ab, n, maxE, v, nrm);
    SubdivTri(c, ca, bc, n, maxE, v, nrm);
    SubdivTri(ab, bc, ca, n, maxE, v, nrm);
}

// 曲面感知细分：中点投影回 A 的解析曲面（消除直线中点弦切裂缝），per-vertex 法线
static void ProjectMidSurface(const Object3D* A, const Vec3& p0, const Vec3& p1, Vec3& m, Vec3& nm) {
    switch (A->type) {
        case PT_SPHERE: {
            double r = A->params[0]; if (r <= 0) r = 1;
            double l = sqrt(Dot(m, m));
            if (l > 1e-9) {
                m = Vec3(m.x / l * r, m.y / l * r, m.z / l * r);
                nm = Vec3(m.x / r, m.y / r, m.z / r);
            }
            break;
        }
        case PT_CYLINDER: {
            double r = A->params[0], hh = A->params[1] * 0.5;
            if (r <= 0) r = 1; if (hh <= 0) hh = 1;
            if (std::fabs(m.y) < hh - 0.02 && std::fabs(p0.y) < hh + 0.01 && std::fabs(p1.y) < hh + 0.01) {
                double l = hypot(m.x, m.z);
                if (l > 1e-9) { m.x = m.x / l * r; m.z = m.z / l * r; nm = Vec3(m.x / r, 0, m.z / r); }
            } else if (std::fabs(p0.y) >= hh - 0.01 && std::fabs(p1.y) >= hh - 0.01) {
                double r0 = hypot(p0.x, p0.z), r1 = hypot(p1.x, p1.z);
                if (r0 > r - 0.03 && r1 > r - 0.03) {
                    double l = hypot(m.x, m.z);
                    if (l > 1e-9) { m.x = m.x / l * r; m.z = m.z / l * r; }
                }
            }
            break;
        }
        case PT_CONE: {
            double r = A->params[0], hh = A->params[1] * 0.5;
            if (r <= 0) r = 1; if (hh <= 0) hh = 1;
            if (std::fabs(m.y) < hh - 0.02 && std::fabs(p0.y) < hh + 0.01 && std::fabs(p1.y) < hh + 0.01) {
                double rr = r * (hh - m.y) / (2 * hh);
                double l = hypot(m.x, m.z);
                if (l > 1e-9) {
                    Vec3 rad(m.x / l, 0, m.z / l);
                    m.x = rad.x * rr; m.z = rad.z * rr;
                    Vec3 g(rad.x, r / (2 * hh), rad.z);
                    double gl = sqrt(Dot(g, g));
                    nm = Vec3(g.x / gl, g.y / gl, g.z / gl);
                }
            } else if (p0.y <= -hh + 0.01 && p1.y <= -hh + 0.01) {
                double r0 = hypot(p0.x, p0.z), r1 = hypot(p1.x, p1.z);
                if (r0 > r - 0.03 && r1 > r - 0.03) {
                    double l = hypot(m.x, m.z);
                    if (l > 1e-9) { m.x = m.x / l * r; m.z = m.z / l * r; }
                }
            }
            break;
        }
        default: break;
    }
}
static void SubdivProject(const Object3D* A, const Vec3& a, const Vec3& b, const Vec3& c,
                          const Vec3& na, const Vec3& nb, const Vec3& nc,
                          double maxE, std::vector<Vec3>& v, std::vector<Vec3>& nrm) {
    Vec3 d1(b.x - a.x, b.y - a.y, b.z - a.z);
    Vec3 d2(c.x - b.x, c.y - b.y, c.z - b.z);
    Vec3 d3(a.x - c.x, a.y - c.y, a.z - c.z);
    double l1 = sqrt(Dot(d1, d1)), l2 = sqrt(Dot(d2, d2)), l3 = sqrt(Dot(d3, d3));
    double mx = l1 > l2 ? (l1 > l3 ? l1 : l3) : (l2 > l3 ? l2 : l3);
    if (mx <= maxE) {
        v.push_back(a); v.push_back(b); v.push_back(c);
        nrm.push_back(na); nrm.push_back(nb); nrm.push_back(nc);
        return;
    }
    Vec3 ab((a.x + b.x) * .5, (a.y + b.y) * .5, (a.z + b.z) * .5);
    Vec3 bc((b.x + c.x) * .5, (b.y + c.y) * .5, (b.z + c.z) * .5);
    Vec3 ca((c.x + a.x) * .5, (c.y + a.y) * .5, (c.z + a.z) * .5);
    Vec3 nab((na.x + nb.x) * .5, (na.y + nb.y) * .5, (na.z + nb.z) * .5);
    Vec3 nbc((nb.x + nc.x) * .5, (nb.y + nc.y) * .5, (nb.z + nc.z) * .5);
    Vec3 nca((nc.x + na.x) * .5, (nc.y + na.y) * .5, (nc.z + na.z) * .5);
    ProjectMidSurface(A, a, b, ab, nab);
    ProjectMidSurface(A, b, c, bc, nbc);
    ProjectMidSurface(A, c, a, ca, nca);
    SubdivProject(A, a, ab, ca, na, nab, nca, maxE, v, nrm);
    SubdivProject(A, b, bc, ab, nb, nbc, nab, maxE, v, nrm);
    SubdivProject(A, c, ca, bc, nc, nca, nbc, maxE, v, nrm);
    SubdivProject(A, ab, bc, ca, nab, nbc, nca, maxE, v, nrm);
}

// 点（A 局部）在 A 内部的隐式符号值（<=0 内、>0 外）。用于孔壁跨边曲面裁剪（边缘平滑）。
static double SignedA(const Object3D* A, const Vec3& p) {
    switch (A->type) {
        case PT_SPHERE: {
            double r = A->params[0]; if (r <= 0) r = 1;
            return p.x * p.x + p.y * p.y + p.z * p.z - r * r;
        }
        case PT_CYLINDER: {
            double r = A->params[0], bh = A->params[1] * 0.5;
            if (r <= 0) r = 1; if (bh <= 0) bh = 1;
            double s1 = p.x * p.x + p.z * p.z - r * r;
            double s2 = p.y * p.y - bh * bh;
            return s1 > s2 ? s1 : s2;
        }
        case PT_CONE: {
            double r = A->params[0], bh = A->params[1] * 0.5;
            if (r <= 0) r = 1; if (bh <= 0) bh = 1;
            double sy = p.y * p.y - bh * bh;
            double rr = r * (bh - p.y) / (2.0 * bh); if (rr < 0) rr = 0;
            double sc = p.x * p.x + p.z * p.z - rr * rr;
            return sy > sc ? sy : sc;
        }
        case PT_TORUS: {
            double R = A->params[0], r = A->params[1];
            if (R <= 0) R = 1; if (r <= 0) r = 0.3;
            double d = std::sqrt(p.x * p.x + p.z * p.z);
            double dd = (d - R) * (d - R) + p.y * p.y;
            return dd - r * r;
        }
        case PT_PLANE: {
            double hx = A->params[0] * 0.5, hz = A->params[0] * 0.5;
            if (hx <= 0) hx = 0.5; if (hz <= 0) hz = 0.5;
            double s1 = std::fabs(p.x) - hx, s2 = std::fabs(p.y) - 0.1, s3 = std::fabs(p.z) - hz;
            double m = s1 > s2 ? s1 : s2;
            return m > s3 ? m : s3;
        }
        default:
            return 1.0;   // 视为外侧
    }
}

// 把三角形 a b c 裁剪到 A 曲面内部（keepOut=false 保留内 / true 保留外），跨边用隐式零点线性插值，
// 使孔壁边缘贴合球面/柱面等曲面，避免"细分中心点二值判定"造成的毛刺锯齿。
static void EmitTriClampedToA(const Object3D* A, const Vec3& a, const Vec3& b, const Vec3& c,
                              const Vec3& n, bool keepOut,
                              std::vector<Vec3>& v, std::vector<Vec3>& nrm) {
    double sa = SignedA(A, a), sb = SignedA(A, b), sc = SignedA(A, c);
    bool ia = sa <= 1e-6, ib = sb <= 1e-6, ic = sc <= 1e-6;
    int cnt = (ia ? 1 : 0) + (ib ? 1 : 0) + (ic ? 1 : 0);
    if (keepOut) {
        if (cnt == 0) { EmitTri(v, nrm, a, b, c, n, n, n); return; }
        if (cnt == 3) return;
    } else {
        if (cnt == 3) { EmitTri(v, nrm, a, b, c, n, n, n); return; }
        if (cnt == 0) return;
    }
    // 混合情况：跨边求隐式零点，构建裁剪多边形（保留侧顶点 + 交点）
    std::vector<Vec3> poly;
    auto addCross = [&](const Vec3& p0, double s0, const Vec3& p1, double s1) {
        if (s0 * s1 < 0) {
            double t = s0 / (s0 - s1);
            if (t < 0) t = 0; if (t > 1) t = 1;
            poly.push_back(Vec3(p0.x + (p1.x - p0.x) * t,
                                p0.y + (p1.y - p0.y) * t,
                                p0.z + (p1.z - p0.z) * t));
        }
    };
    if (keepOut ? !ia : ia) poly.push_back(a);
    addCross(a, sa, b, sb);
    if (keepOut ? !ib : ib) poly.push_back(b);
    addCross(b, sb, c, sc);
    if (keepOut ? !ic : ic) poly.push_back(c);
    addCross(c, sc, a, sa);
    if (poly.size() < 3) return;
    for (size_t i = 1; i + 1 < poly.size(); i++)
        EmitTri(v, nrm, poly[0], poly[i], poly[i + 1], n, n, n);
}

// A 局部坐标下曲面图元的解析外法线（与 SignedA 隐式口径一致），用于布尔结果 A 表面的平滑着色，
// 避免"整块单法线 flat 着色"使球面/柱面呈块状。
static Vec3 NormalA(const Object3D* A, const Vec3& p) {
    switch (A->type) {
        case PT_SPHERE: {
            double l = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
            if (l < 1e-9) return Vec3(0, 1, 0);
            return Vec3(p.x / l, p.y / l, p.z / l);
        }
        case PT_CYLINDER: {
            double r = A->params[0], bh = A->params[1] * 0.5;
            if (r <= 0) r = 1; if (bh <= 0) bh = 1;
            double s1 = p.x * p.x + p.z * p.z - r * r;
            double s2 = p.y * p.y - bh * bh;
            if (s1 >= s2) {
                double l = std::sqrt(p.x * p.x + p.z * p.z);
                if (l < 1e-9) return Vec3(1, 0, 0);
                return Vec3(p.x / l, 0, p.z / l);
            }
            return Vec3(0, p.y > 0 ? 1.0 : -1.0, 0);
        }
        case PT_CONE: {
            double r = A->params[0], bh = A->params[1] * 0.5;
            if (r <= 0) r = 1; if (bh <= 0) bh = 1;
            double sy = p.y * p.y - bh * bh;
            double k = r / (2.0 * bh);
            double rry = k * (bh - p.y);
            double sc = p.x * p.x + p.z * p.z - rry * rry;
            if (sy >= sc || rry < 1e-9)
                return Vec3(0, p.y > 0 ? 1.0 : -1.0, 0);
            double nx = p.x, ny = k * k * (bh - p.y), nz = p.z;
            double l = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (l < 1e-9) return Vec3(0, 1, 0);
            return Vec3(nx / l, ny / l, nz / l);
        }
        case PT_TORUS: {
            double R = A->params[0], r = A->params[1];
            if (R <= 0) R = 1; if (r <= 0) r = 0.3;
            double d = std::sqrt(p.x * p.x + p.z * p.z);
            if (d < 1e-9) return Vec3(0, p.y > 0 ? 1.0 : -1.0, 0);
            double nx = (d - R) * p.x / d, ny = p.y, nz = (d - R) * p.z / d;
            double l = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (l < 1e-9) return Vec3(0, 1, 0);
            return Vec3(nx / l, ny / l, nz / l);
        }
        default:
            return Vec3(0, 1, 0);   // 平面体：调用方回退 flatN
    }
}
static bool IsCurvedA(int t) {
    return t == PT_SPHERE || t == PT_CYLINDER || t == PT_CONE || t == PT_TORUS;
}
// 发射多边形三角化：曲面图元每顶点用解析法线（平滑），平面图元（BOX/PLANE/TUBE）回退单法线 flat。
static void EmitPolyTriSmooth(std::vector<Vec3>& v, std::vector<Vec3>& n,
                              const std::vector<Vec3>& poly, const Vec3& flatN,
                              const Object3D* A) {
    if (poly.size() < 3) return;
    if (!IsCurvedA(A->type)) { EmitPolyTri(v, n, poly, flatN); return; }
    for (size_t i = 1; i + 1 < poly.size(); i++) {
        v.push_back(poly[0]); v.push_back(poly[i]); v.push_back(poly[i + 1]);
        n.push_back(NormalA(A, poly[0]));
        n.push_back(NormalA(A, poly[i]));
        n.push_back(NormalA(A, poly[i + 1]));
    }
}

// 孔壁（B 表面）裁剪到 A 内/外，逐顶点用 B 的解析法线（圆柱/球/锥曲面平滑、平面 B 回退 flatN），
// 消除"SubdivTri 单法线 flat 着色"导致的洞壁块状。flip=true（差集）法线朝孔内。
// keepOut=false 保 A 内（差/交）、true 保 A 外（并集）。
static void EmitHoleClampedSmooth(const Object3D* A, const Object3D* B,
        const std::function<Vec3(const Vec3&)>& toB,
        const std::function<Vec3(const Vec3&)>& dirToA,
        bool flip, const Vec3& flatN,
        const Vec3& a, const Vec3& b, const Vec3& c,
        bool keepOut, std::vector<Vec3>& v, std::vector<Vec3>& nrm) {
    auto hn = [&](const Vec3& p) -> Vec3 {
        if (IsCurvedA(B->type)) {
            Vec3 nA = dirToA(NormalA(B, toB(p)));
            return flip ? Vec3(-nA.x, -nA.y, -nA.z) : nA;
        }
        return flatN;
    };
    double sa = SignedA(A, a), sb = SignedA(A, b), sc = SignedA(A, c);
    bool ia = sa <= 1e-6, ib = sb <= 1e-6, ic = sc <= 1e-6;
    int cnt = (ia ? 1 : 0) + (ib ? 1 : 0) + (ic ? 1 : 0);
    if (keepOut) { if (cnt == 0) { EmitTri(v, nrm, a, b, c, hn(a), hn(b), hn(c)); return; } if (cnt == 3) return; }
    else { if (cnt == 3) { EmitTri(v, nrm, a, b, c, hn(a), hn(b), hn(c)); return; } if (cnt == 0) return; }
    std::vector<Vec3> poly;
    auto addCross = [&](const Vec3& p0, double s0, const Vec3& p1, double s1) {
        if (s0 * s1 < 0) {
            double t = s0 / (s0 - s1); if (t < 0) t = 0; if (t > 1) t = 1;
            poly.push_back(Vec3(p0.x + (p1.x - p0.x) * t, p0.y + (p1.y - p0.y) * t, p0.z + (p1.z - p0.z) * t));
        }
    };
    if (keepOut ? !ia : ia) poly.push_back(a);
    addCross(a, sa, b, sb);
    if (keepOut ? !ib : ib) poly.push_back(b);
    addCross(b, sb, c, sc);
    if (keepOut ? !ic : ic) poly.push_back(c);
    addCross(c, sc, a, sa);
    if (poly.size() < 3) return;
    for (size_t i = 1; i + 1 < poly.size(); i++)
        EmitTri(v, nrm, poly[0], poly[i], poly[i + 1], hn(poly[0]), hn(poly[i]), hn(poly[i + 1]));
}

// 三角网格体内判定加速结构：+X 射线奇偶法，YZ 二维网格预筛候选三角。
// 供"布尔结果作为 A/B"时的点在体内查询（无解析隐式/凸包平面可用）。
struct MeshInside {
    const std::vector<Vec3>* src;
    std::vector<float> maxX;           // 每个三角的最大 x（射线 t>0 预筛）
    double mny, mnz, cely, celz;
    int gy, gz;
    std::vector<std::vector<int> > cells;
    void Build(const std::vector<Vec3>& mesh) {
        src = &mesh;
        double y0 = 1e18, z0 = 1e18, y1 = -1e18, z1 = -1e18;
        for (size_t i = 0; i < mesh.size(); i++) {
            if (mesh[i].y < y0) y0 = mesh[i].y; if (mesh[i].y > y1) y1 = mesh[i].y;
            if (mesh[i].z < z0) z0 = mesh[i].z; if (mesh[i].z > z1) z1 = mesh[i].z;
        }
        gy = gz = 24;
        mny = y0; mnz = z0;
        cely = (y1 - y0) / gy + 1e-9; celz = (z1 - z0) / gz + 1e-9;
        cells.assign((size_t)gy * gz, std::vector<int>());
        maxX.assign(mesh.size() / 3, 0.0f);
        for (size_t t = 0; t + 2 < mesh.size(); t += 3) {
            double ty0 = mesh[t].y, ty1 = ty0, tz0 = mesh[t].z, tz1 = tz0, mx = mesh[t].x;
            for (int j = 1; j < 3; j++) {
                const Vec3& p = mesh[t + j];
                if (p.y < ty0) ty0 = p.y; if (p.y > ty1) ty1 = p.y;
                if (p.z < tz0) tz0 = p.z; if (p.z > tz1) tz1 = p.z;
                if (p.x > mx) mx = p.x;
            }
            maxX[t / 3] = (float)mx;
            int cy0 = (int)((ty0 - mny) / cely), cy1 = (int)((ty1 - mny) / cely);
            int cz0 = (int)((tz0 - mnz) / celz), cz1 = (int)((tz1 - mnz) / celz);
            if (cy0 < 0) cy0 = 0; if (cy1 >= gy) cy1 = gy - 1;
            if (cz0 < 0) cz0 = 0; if (cz1 >= gz) cz1 = gz - 1;
            for (int cy = cy0; cy <= cy1; cy++)
                for (int cz = cz0; cz <= cz1; cz++)
                    cells[(size_t)cz * gy + cy].push_back((int)t);
        }
    }
    bool Inside(const Vec3& p) const {
        // 起点沿 y/z 微扰：射线 rd=(1,0,0) 精确穿过网格顶点/缝/与三角共面时命中数奇偶会错
        // （如球网格顶点行 y=sin(lat)、经线缝 z=0）。δ 远小于网格边长、远大于浮点容差，
        // y/z 用不同比例错开，避免微扰后仍对齐另一方向的结构线。
        Vec3 ps(p.x, p.y + cely * 1e-4, p.z + celz * 1.7e-4);
        int cy = (int)((ps.y - mny) / cely), cz = (int)((ps.z - mnz) / celz);
        if (cy < 0) cy = 0; if (cy >= gy) cy = gy - 1;
        if (cz < 0) cz = 0; if (cz >= gz) cz = gz - 1;
        const std::vector<int>& cand = cells[(size_t)cz * gy + cy];
        Vec3 rd(1, 0, 0);
        int hits = 0;
        for (size_t q = 0; q < cand.size(); q++) {
            int t = cand[q];
            if ((double)maxX[t / 3] < ps.x) continue;
            double tt;
            if (RayTriangleHit(ps, rd, (*src)[t], (*src)[t + 1], (*src)[t + 2], tt) && tt > 1e-7) hits++;
        }
        return (hits & 1) != 0;
    }
    // 跨内外边界的边二分求交（无符号距离，用体内判定逼近）
    Vec3 Cross(const Vec3& p0, bool in0, const Vec3& p1) const {
        Vec3 lo = p0, hi = p1; bool inLo = in0;
        for (int it = 0; it < 12; it++) {
            Vec3 mid((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, (lo.z + hi.z) * 0.5);
            if (Inside(mid) == inLo) lo = mid; else hi = mid;
        }
        return Vec3((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, (lo.z + hi.z) * 0.5);
    }
};

// 布尔运算 A op B ，在 A 局部坐标生成网格。op: 0=差集(A−B 挖孔) 1=并集(A∪B) 2=交集(A∩B)。
// A/B 均可为布尔结果（网格体内判定路径）；凸体 B 走解析凸包平面（更精确）。
// 返回是否成功。调用方把结果写入 PT_BOOLEAN 的网格缓存。
static bool BuildBooleanMesh(const Object3D* A, const Object3D* B, int op,
                             std::vector<Vec3>& outV, std::vector<Vec3>& outN) {
    if (!A || !B) return false;
    if (!IsBoolTargetType(A->type)) return false;
    if (!IsHoleType(B->type)) return false;
    Mat4 AwInv = A->WorldMatrix().Inverse();
    Mat4 Bw = B->WorldMatrix();
    // B 局部→A 局部
    auto toA = [&](const Vec3& p) { return AwInv.TransformPoint(Bw.TransformPoint(p)); };
    auto dirToA = [&](const Vec3& v) { return Normalize(AwInv.TransformDir(Bw.TransformDir(v))); };
    // A 局部→B 局部（孔壁点求 B 解析法线用）
    Mat4 Aw = A->WorldMatrix();
    Mat4 BwInv = Bw.Inverse();
    auto toB = [&](const Vec3& p) { return BwInv.TransformPoint(Aw.TransformPoint(p)); };

    // ---- B 表面（法线朝外）+ 凸包平面（凸体半空间）；布尔结果 B 直接读缓存网格
    // 球体与布尔结果走网格体内判定：球用外接盒平面近似会挖出方孔（与视觉球不符），
    // 布尔结果非凸无凸包平面。圆柱/圆锥保持切平面近似（N=segments，孔形=视觉棱柱横截面）。
    const bool useMeshB = (B->type == PT_BOOLEAN || B->type == PT_SPHERE);
    std::vector<Vec3> bV, bN;
    std::vector<Plane> Bplanes;
    if (useMeshB) {
        std::vector<Vec3> bv, bn;
        B->GenerateMesh(bv, bn);
        for (size_t q = 0; q < bv.size(); q++) bV.push_back(toA(bv[q]));
        for (size_t q = 0; q < bn.size(); q++) bN.push_back(dirToA(bn[q]));
    } else {
        BuildHoleSolid(B, toA, dirToA, bV, bN, Bplanes, op);
    }
    if (useMeshB ? bV.size() < 3 : Bplanes.empty()) return false;
    MeshInside bGrid;                     // 布尔结果 B 的体内判定加速结构
    if (useMeshB) bGrid.Build(bV);

    // 管壳模式：TUBE/RTUBE 参与并/交时，B = 空心壳（外壁内 且 不在内壁内）。
    // 差集(op=0) 保持"外壁实心"减体（挖出贯穿孔，已验收）。
    // B=TUBE/RTUBE 时 B 始终是空心管壳（非凸），三种运算都需内腔半空间：
    // 方管内腔=内方柱 6 平面；圆管内腔=N 棱柱近似（N=segments，与视觉网格一致）。
    const bool tubeShell = (B->type == PT_TUBE || B->type == PT_RTUBE);
    std::vector<Plane> InnerPlanes;
    if (tubeShell) {
        double bh = B->params[2] * 0.5;
        double ro = B->params[0], ri = B->params[1];
        if (ro <= 0) ro = 1;
        if (bh <= 0) bh = 1;
        if (ri <= 0 || ri >= ro) ri = ro * 0.6;
        if (B->type == PT_RTUBE) {
            // 圆管内腔：N 棱柱侧面 + 上下底面（法线朝外，与 TUBE 内方柱约定一致）
            int N = B->segments < 4 ? 4 : (B->segments > 64 ? 64 : B->segments);
            for (int i = 0; i < N; i++) {
                double a0 = 2.0 * kPI * i / N, a1 = 2.0 * kPI * (i + 1) / N;
                Vec3 nrm = Normalize(Vec3(std::sin(a1) - std::sin(a0), 0, -(std::cos(a1) - std::cos(a0))));
                Vec3 pA(ri * cos(a0), 0, ri * sin(a0));
                Vec3 nA = dirToA(nrm);
                InnerPlanes.push_back({ nA, -Dot(nA, toA(pA)) });
            }
            Vec3 upA = dirToA(Vec3(0, 1, 0)), dnA = dirToA(Vec3(0, -1, 0));
            InnerPlanes.push_back({ upA, -Dot(upA, toA(Vec3(0, bh, 0))) });
            InnerPlanes.push_back({ dnA, -Dot(dnA, toA(Vec3(0, -bh, 0))) });
        } else {
            double wi = ri * 0.5, w = ro * 0.5;
            Vec3 c8[8] = { Vec3(-wi,-bh,-wi),Vec3( wi,-bh,-wi),Vec3( wi,-bh, wi),Vec3(-wi,-bh, wi),
                           Vec3(-wi, bh,-wi),Vec3( wi, bh,-wi),Vec3( wi, bh, wi),Vec3(-wi, bh, wi) };
            struct F { int a; Vec3 n; };
            F fs[6] = { {0,Vec3(0,-1,0)}, {4,Vec3(0,1,0)}, {0,Vec3(-1,0,0)},
                        {1,Vec3(1,0,0)}, {0,Vec3(0,0,-1)}, {3,Vec3(0,0,1)} };
            for (int i = 0; i < 6; i++) {
                Vec3 nA = dirToA(fs[i].n);
                InnerPlanes.push_back({ nA, -Dot(nA, toA(c8[fs[i].a])) });
            }
        }
    }

    // ---- A 表面三角形（A 局部网格）
    std::vector<Vec3> aV, aN;
    A->GenerateMesh(aV, aN);
    if (aV.size() < 3) return false;
    // 布尔结果作为 A：保留一份原始网格供"体内判定"（射线奇偶法）
    std::vector<Vec3> aMesh;
    if (A->type == PT_BOOLEAN) aMesh = aV;

    // 统一细分到边长≤0.35（TORUS 保持原始网格）：平面体直线中点、曲面中点投影回解析曲面。
    // 细分后每个小三角不会同时跨越管壳"外方柱外/管壁/管腔"多个区域，后续裁剪正确、孔口圆滑。
    // 布尔结果（PT_BOOLEAN）的网格已是前次 CSG 的细分小三角，无需再细分（也无解析曲面可投影）。
    if (A->type != PT_TORUS && A->type != PT_BOOLEAN) {
        std::vector<Vec3> fv, fn;
        fv.reserve(aV.size() * 4); fn.reserve(aN.size() * 4);
        for (size_t i = 0; i + 2 < aV.size(); i += 3)
            SubdivProject(A, aV[i], aV[i + 1], aV[i + 2], aN[i], aN[i + 1], aN[i + 2], 0.35, fv, fn);
        aV.swap(fv); aN.swap(fn);
    }

    std::vector<Vec3> rV, rN;
    auto inOuter = [&](const Vec3& p) {
        for (auto& pl : Bplanes) if (Dot(pl.n, p) + pl.d > 1e-6) return false;
        return true;
    };
    auto inInner = [&](const Vec3& p) {
        for (auto& pl : InnerPlanes) if (Dot(pl.n, p) + pl.d > 1e-6) return false;
        return true;
    };
    auto inB = [&](const Vec3& p) {
        if (useMeshB) return bGrid.Inside(p);
        if (!inOuter(p)) return false;
        if (tubeShell && inInner(p)) return false;   // 管腔不属于管壳
        return true;
    };
    bool isDiff = (op == 0), isInter = (op == 2);
    for (size_t i = 0; i + 2 < aV.size(); i += 3) {
        Vec3 T0 = aV[i], T1 = aV[i + 1], T2 = aV[i + 2];
        Vec3 nT = aN[i];
        if (useMeshB) {
            // B 为布尔结果（非凸）：逐顶点网格体内判定，跨界二分求交；交集保 B 内、差/并保 B 外
            bool i0 = bGrid.Inside(T0), i1 = bGrid.Inside(T1), i2 = bGrid.Inside(T2);
            int cnt = (i0 ? 1 : 0) + (i1 ? 1 : 0) + (i2 ? 1 : 0);
            bool keepIn = isInter;
            if (keepIn ? (cnt == 3) : (cnt == 0)) {
                std::vector<Vec3> whole; whole.push_back(T0); whole.push_back(T1); whole.push_back(T2);
                EmitPolyTriSmooth(rV, rN, whole, nT, A);
                continue;
            }
            if (keepIn ? (cnt == 0) : (cnt == 3)) continue;
            std::vector<Vec3> poly;
            if (keepIn ? i0 : !i0) poly.push_back(T0);
            if (i0 != i1) poly.push_back(bGrid.Cross(T0, i0, T1));
            if (keepIn ? i1 : !i1) poly.push_back(T1);
            if (i1 != i2) poly.push_back(bGrid.Cross(T1, i1, T2));
            if (keepIn ? i2 : !i2) poly.push_back(T2);
            if (i2 != i0) poly.push_back(bGrid.Cross(T2, i2, T0));
            EmitPolyTriSmooth(rV, rN, poly, nT, A);
            continue;
        }
        if (isInter) {
            // 交集：A 表面 ∩ B 体
            if (tubeShell) {
                // 先保"外方柱内"，再保"内方柱外"（管壁处），管腔处丢弃
                std::vector<Vec3> cur = { T0, T1, T2 };
                for (size_t p = 0; p < Bplanes.size(); p++) {
                    std::vector<Vec3> inP, outP;
                    ClipPolyPlane(cur, Bplanes[p].n, Bplanes[p].d, inP, outP);
                    cur = inP;
                }
                if (cur.size() >= 3) {
                    for (size_t p = 0; p < InnerPlanes.size(); p++) {
                        std::vector<Vec3> inP2, outP2;
                        ClipPolyPlane(cur, InnerPlanes[p].n, InnerPlanes[p].d, inP2, outP2);
                        EmitPolyTriSmooth(rV, rN, outP2, nT, A);
                        cur = inP2;
                    }
                }
            } else {
                std::vector<Vec3> cur = { T0, T1, T2 };
                for (size_t p = 0; p < Bplanes.size(); p++) {
                    std::vector<Vec3> inP, outP;
                    ClipPolyPlane(cur, Bplanes[p].n, Bplanes[p].d, inP, outP);
                    cur = inP;
                }
                EmitPolyTriSmooth(rV, rN, cur, nT, A);
            }
        } else if (tubeShell) {
            // 差集/并集：A 表面保留在管壳外 = 外方柱外 ∪ 管腔（内方柱内）；管壁环被挖空
            // 差集：外方柱外=A 外壁，管腔=保留的方柱顶面（管壁环被挖、露出管腔方柱侧壁）
            if (inB(T0) && inB(T1) && inB(T2)) continue;
            std::vector<Vec3> cur = { T0, T1, T2 };
            for (size_t p = 0; p < Bplanes.size(); p++) {
                std::vector<Vec3> inP, outP;
                ClipPolyPlane(cur, Bplanes[p].n, Bplanes[p].d, inP, outP);
                EmitPolyTriSmooth(rV, rN, outP, nT, A);
                cur = inP;
            }
            for (size_t p = 0; p < InnerPlanes.size(); p++) {
                std::vector<Vec3> inP2, outP2;
                ClipPolyPlane(cur, InnerPlanes[p].n, InnerPlanes[p].d, inP2, outP2);
                cur = inP2;   // 管壁处 A 表面丢弃（属于 B）
            }
            EmitPolyTriSmooth(rV, rN, cur, nT, A);   // 管腔处 A 表面保留
        } else {
            // 差集（及非管壳并集）：保留 A 表面在 B（外方柱）外
            if (inB(T0) && inB(T1) && inB(T2)) continue;
            std::vector<Vec3> cur = { T0, T1, T2 };
            for (size_t p = 0; p < Bplanes.size(); p++) {
                std::vector<Vec3> inP, outP;
                ClipPolyPlane(cur, Bplanes[p].n, Bplanes[p].d, inP, outP);
                EmitPolyTriSmooth(rV, rN, outP, nT, A);
                cur = inP;
            }
        }
    }

    // ---- B 表面：差集保留 A 内（孔壁，法线朝孔内）；交集保留 A 内（法线朝外）；并集保留 A 外（法线朝外）
    std::vector<Plane> Aplanes;
    BuildASolid(A, Aplanes);
    // A 为布尔结果：无解析隐式/凸包平面，用 A 网格射线奇偶法做体内判定（网格加速）
    MeshInside aGrid;
    if (!aMesh.empty()) aGrid.Build(aMesh);
    auto inMeshA = [&](const Vec3& p) -> bool { return aGrid.Inside(p); };
    auto crossMeshA = [&](const Vec3& p0, bool in0, const Vec3& p1) -> Vec3 { return aGrid.Cross(p0, in0, p1); };
    // 孔壁多边形逐顶点 B 解析法线发射（平面 A 分支用；曲面 B 平滑、平面 B 回退 flatN）
    auto emitHolePolySmooth = [&](const std::vector<Vec3>& poly, bool flip, const Vec3& flatN) {
        if (poly.size() < 3) return;
        auto hn = [&](const Vec3& p) -> Vec3 {
            if (IsCurvedA(B->type)) { Vec3 nA = dirToA(NormalA(B, toB(p))); return flip ? Vec3(-nA.x, -nA.y, -nA.z) : nA; }
            return flatN;
        };
        for (size_t i = 1; i + 1 < poly.size(); i++)
            EmitTri(rV, rN, poly[0], poly[i], poly[i + 1], hn(poly[0]), hn(poly[i]), hn(poly[i + 1]));
    };
    for (size_t i = 0; i + 2 < bV.size(); i += 3) {
        Vec3 hN = isDiff ? Vec3(-bN[i].x, -bN[i].y, -bN[i].z) : bN[i];
        if (!Aplanes.empty()) {
            // 严格交集：B 表面 ∩ A 内部
            std::vector<Vec3> cur = { bV[i], bV[i + 1], bV[i + 2] };
            for (size_t p = 0; p < Aplanes.size(); p++) {
                std::vector<Vec3> inP, outP;
                ClipPolyPlaneStrict(cur, Aplanes[p].n, Aplanes[p].d, inP, outP);
                cur = inP;
                if (cur.size() < 3) break;
            }
            bool inA = (cur.size() >= 3);
            if (isDiff && inA) emitHolePolySmooth(cur, true, hN);             // 孔壁（A 内，朝孔内）
            else if (isInter && inA) emitHolePolySmooth(cur, false, hN);      // 交集 B 面（A 内，朝外）
            else if (!isDiff && !isInter) {                             // 并集：保留 A 外（outP）
                std::vector<Vec3> oo = { bV[i], bV[i + 1], bV[i + 2] };
                for (size_t p = 0; p < Aplanes.size(); p++) {
                    std::vector<Vec3> inP2, outP2;
                    ClipPolyPlane(oo, Aplanes[p].n, Aplanes[p].d, inP2, outP2);
                    emitHolePolySmooth(outP2, false, hN);
                    oo = inP2;
                }
            }
        } else if (A->type == PT_BOOLEAN && aMesh.size() >= 3) {
            // A 为布尔结果：细分 B 表面后逐小三角做网格体内判定，跨界二分求交（对齐 SignedA 路径的结构）
            std::vector<Vec3> sv, sn;
            SubdivTri(bV[i], bV[i + 1], bV[i + 2], bN[i], 0.5, sv, sn);
            bool flip = isDiff;
            bool keepOut = (!isDiff && !isInter);
            for (size_t k = 0; k + 2 < sv.size(); k += 3) {
                const Vec3& a = sv[k]; const Vec3& b = sv[k + 1]; const Vec3& c = sv[k + 2];
                auto hn = [&](const Vec3& p) -> Vec3 {
                    if (IsCurvedA(B->type)) { Vec3 nA = dirToA(NormalA(B, toB(p))); return flip ? Vec3(-nA.x, -nA.y, -nA.z) : nA; }
                    return hN;
                };
                bool ia = inMeshA(a), ib = inMeshA(b), ic = inMeshA(c);
                int cnt = (ia ? 1 : 0) + (ib ? 1 : 0) + (ic ? 1 : 0);
                if (keepOut) { if (cnt == 0) { EmitTri(rV, rN, a, b, c, hn(a), hn(b), hn(c)); continue; } if (cnt == 3) continue; }
                else { if (cnt == 3) { EmitTri(rV, rN, a, b, c, hn(a), hn(b), hn(c)); continue; } if (cnt == 0) continue; }
                std::vector<Vec3> poly;
                if (keepOut ? !ia : ia) poly.push_back(a);
                if (ia != ib) poly.push_back(crossMeshA(a, ia, b));
                if (keepOut ? !ib : ib) poly.push_back(b);
                if (ib != ic) poly.push_back(crossMeshA(b, ib, c));
                if (keepOut ? !ic : ic) poly.push_back(c);
                if (ic != ia) poly.push_back(crossMeshA(c, ic, a));
                if (poly.size() < 3) continue;
                for (size_t q = 1; q + 1 < poly.size(); q++)
                    EmitTri(rV, rN, poly[0], poly[q], poly[q + 1], hn(poly[0]), hn(poly[q]), hn(poly[q + 1]));
            }
        } else {
            // 非平面 A（解析曲面）：细分 B 表面到小片，逐顶点 B 解析法线 + 跨边曲面裁剪（孔壁平滑、边缘贴合、无毛刺）
            std::vector<Vec3> sv, sn;
            SubdivTri(bV[i], bV[i + 1], bV[i + 2], bN[i], 0.35, sv, sn);
            bool flip = isDiff;
            bool keepOut = (!isDiff && !isInter);
            for (size_t k = 0; k + 2 < sv.size(); k += 3)
                EmitHoleClampedSmooth(A, B, toB, dirToA, flip, hN, sv[k], sv[k + 1], sv[k + 2], keepOut, rV, rN);
        }
    }

    outV = rV; outN = rN;
    return true;
}

// 生成/重建布尔网格缓存（op: 0=差集 1=并集 2=交集）。结果写回 _meshVerts/_meshNorms 并标记有效。
void Object3D::RebuildBooleanMesh(const Object3D* A, const Object3D* B, int op) {
    std::vector<Vec3> v, n;
    if (BuildBooleanMesh(A, B, op, v, n)) {
        _meshVerts = v; _meshNorms = n;
        _meshValid = true;
        _meshRev++;
    }
}


bool Object3D::ChainVisible() const {
    if (!visible) return false;
    if (parent) return parent->ChainVisible();
    return true;
}

// ------------------------------------------------------------ 3D 文本网格
// 方案：直接用 GDI 把整行文本光栅化成白底黑字位图(mask)，字形实心/孔洞由系统光栅化保证。
// 前后面用"逐行扫描梯形条带"三角化填充区域（完全基于 mask，孔洞天然正确，无需判断环）；
// 侧面用 Marching Squares 的边界边直接挤出。文本是真正的实体网格，无背景贴图。
struct TextP2 { double x, y; TextP2() : x(0), y(0) {} TextP2(double a, double b) : x(a), y(b) {} };

// 渲染整行文本 → 二值 mask（四周留 1px 空白保证边界闭合）。scale=世界单位/像素
// 采样二次贝塞尔到线段点（追加到 out）
static void TextSampleQuad(const TextP2& s, const TextP2& c, const TextP2& e,
                           std::vector<TextP2>& out, int seg) {
    TextP2 prev = s;
    for (int i = 1; i <= seg; i++) {
        double t = (double)i / seg, mt = 1.0 - t;
        TextP2 p(mt*mt*s.x + 2.0*mt*t*c.x + t*t*e.x,
                 mt*mt*s.y + 2.0*mt*t*c.y + t*t*e.y);
        if (p.x != prev.x || p.y != prev.y) { out.push_back(p); prev = p; }
    }
}
static inline TextP2 TextFX(const POINTFX& p) {
    // POINTFX 是 16.16 定点：value 为整数部分（像素），fract 为小数部分
    return TextP2((double)p.x.value + (double)p.x.fract / 65536.0,
                  (double)p.y.value + (double)p.y.fract / 65536.0);
}

// 用 GetGlyphOutlineW(GGO_NATIVE) 提取整行文本的所有 TrueType 轮廓环（贝塞尔已采样成多边形）
static bool TextCollectContours(const Object3D* o, int pxH,
                                std::vector<std::vector<TextP2>>& rings) {
    rings.clear();
    if (o->text.empty()) return false;
    HDC hdc = ::CreateCompatibleDC(NULL);
    HFONT fnt = ::CreateFontW(-pxH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE,
                              o->font.empty() ? L"微软雅黑" : o->font.c_str());
    HGDIOBJ oldf = ::SelectObject(hdc, fnt);
    MAT2 mat; ::memset(&mat, 0, sizeof(mat));
    mat.eM11.value = 1; mat.eM11.fract = 0;   // 1.0（单位矩阵，取原生字形轮廓）
    mat.eM22.value = 1; mat.eM22.fract = 0;
    double penX = 0.0;   // 字符前进宽度（advance）累计，避免所有字符重叠
    for (size_t ci = 0; ci < o->text.size(); ci++) {
        wchar_t ch = o->text[ci];
        GLYPHMETRICS gm;
        DWORD sz = ::GetGlyphOutlineW(hdc, ch, GGO_NATIVE, &gm, 0, NULL, &mat);
        if (sz == GDI_ERROR || sz == 0) continue;
        std::vector<BYTE> buf(sz);
        if (!::GetGlyphOutlineW(hdc, ch, GGO_NATIVE, &gm, sz, &buf[0], &mat)) continue;
        BYTE* p = &buf[0]; BYTE* end = p + sz;
        while (p + sizeof(TTPOLYGONHEADER) <= end) {
            TTPOLYGONHEADER* ph = (TTPOLYGONHEADER*)p;
            if (ph->cb < sizeof(TTPOLYGONHEADER) || (BYTE*)p + ph->cb > end) break;
            std::vector<TextP2> ring;
            TextP2 start = TextFX(ph->pfxStart);
            ring.push_back(start);
            TextP2 cur = start;
            BYTE* q = p + sizeof(TTPOLYGONHEADER);
            BYTE* qend = p + ph->cb;
            while (q + 4 <= qend) {
                TTPOLYCURVE* pc = (TTPOLYCURVE*)q;
                DWORD blk = 4 + (DWORD)pc->cpfx * sizeof(POINTFX);
                if ((BYTE*)q + blk > qend) break;
                if (pc->wType == TT_PRIM_LINE) {
                    for (WORD i = 0; i < pc->cpfx; i++) { cur = TextFX(pc->apfx[i]); ring.push_back(cur); }
                } else if (pc->wType == TT_PRIM_QSPLINE) {
                    WORD n = pc->cpfx;
                    TextP2 s = cur;
                    for (WORD i = 0; i + 1 < n; i += 2) {
                        TextP2 ctrl = TextFX(pc->apfx[i]);
                        TextP2 e = TextFX(pc->apfx[i + 1]);
                        TextSampleQuad(s, ctrl, e, ring, 10);
                        s = e;
                    }
                    if (n % 2 == 1) {   // 奇数：末尾孤立点按直线点近似（罕见）
                        TextP2 lastp = TextFX(pc->apfx[n - 1]);
                        if (lastp.x != cur.x || lastp.y != cur.y) ring.push_back(lastp);
                        cur = lastp;
                    } else if (n >= 2) cur = TextFX(pc->apfx[n - 1]);
                }
                q += blk;
            }
            if (ring.back().x != ring[0].x || ring.back().y != ring[0].y)
                ring.push_back(ring[0]);
            if (ring.size() >= 4) {
                for (size_t i = 0; i < ring.size(); i++) ring[i].x += penX;   // 按字距排开
                rings.push_back(ring);
            }
            p += ph->cb;
        }
        penX += (double)gm.gmCellIncX;   // 累计该字符前进宽度
    }
    ::SelectObject(hdc, oldf); ::DeleteObject(fnt); ::DeleteDC(hdc);
    return !rings.empty();
}

// 真矢量 3D 文本：奇偶规则扫描线三角化前后面（孔洞天然正确，无需判断环类型）+ 轮廓边挤出侧面
static void BuildText3DMesh(std::vector<Vec3>& v, std::vector<Vec3>& n, const Object3D* o) {
    v.clear(); n.clear();
    double hgt = (o->params[1] > 0) ? o->params[1] : 2.0;
    int pxH = (int)(hgt * 110.0); if (pxH < 96) pxH = 96; if (pxH > 1024) pxH = 1024;
    std::vector<std::vector<TextP2>> rings;
    if (!TextCollectContours(o, pxH, rings)) return;
    std::vector<std::pair<TextP2, TextP2>> edges;
    double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
    for (size_t r = 0; r < rings.size(); r++) {
        const std::vector<TextP2>& rg = rings[r];
        for (size_t i = 0; i + 1 < rg.size(); i++) {
            const TextP2& a = rg[i]; const TextP2& b = rg[i + 1];
            edges.push_back(std::make_pair(a, b));
            if (a.x < minx) minx = a.x; if (a.x > maxx) maxx = a.x;
            if (a.y < miny) miny = a.y; if (a.y > maxy) maxy = a.y;
            if (b.x < minx) minx = b.x; if (b.x > maxx) maxx = b.x;
            if (b.y < miny) miny = b.y; if (b.y > maxy) maxy = b.y;
        }
    }
    if (minx > maxx || miny > maxy) return;
    double scale = hgt / (maxy - miny);
    double cx = (minx + maxx) * 0.5, cy = (miny + maxy) * 0.5;
    auto conv = [&](double px, double py, Vec3& out) {
        out.x = (px - cx) * scale;   // 横向 -> x
        out.y = (py - cy) * scale;   // 纵向 -> y（文字立在 XY 平面，厚度沿 z）
    };
    double thick = (o->params[0] > 0) ? o->params[0] : 0.3;
    double hz = thick * 0.5;
    const Vec3 nf(0, 0, 1), nb(0, 0, -1);
    int ns = 320;
    double ys0 = miny, ys1 = maxy;
    double dy = (ys1 - ys0) / (double)ns;
    if (dy <= 0) return;
    std::vector<std::pair<double, double>> prevRuns;
    for (int k = 0; k <= ns; k++) {
        double yp = ys0 + dy * (double)k;
        std::vector<double> xs;
        for (size_t i = 0; i < edges.size(); i++) {
            double ay = edges[i].first.y, by = edges[i].second.y;
            if ((ay <= yp && by > yp) || (by <= yp && ay > yp)) {
                double t = (yp - ay) / (by - ay);
                xs.push_back(edges[i].first.x + t * (edges[i].second.x - edges[i].first.x));
            }
        }
        std::sort(xs.begin(), xs.end());
        std::vector<std::pair<double, double>> runs;
        for (size_t i = 0; i + 1 < xs.size(); i += 2)
            if (xs[i + 1] > xs[i]) runs.push_back(std::make_pair(xs[i], xs[i + 1]));
        if (k > 0 && !prevRuns.empty() && !runs.empty()) {
            double yprev = ys0 + dy * (double)(k - 1);
            for (size_t a = 0; a < prevRuns.size(); a++)
                for (size_t b = 0; b < runs.size(); b++) {
                    double L = prevRuns[a].first > runs[b].first ? prevRuns[a].first : runs[b].first;
                    double R = prevRuns[a].second < runs[b].second ? prevRuns[a].second : runs[b].second;
                    if (L > R) continue;
                    Vec3 A, B, C, D;
                    conv(L, yprev, A); conv(R, yprev, B);
                    conv(R, yp, C); conv(L, yp, D);
                    EmitQuad(v, n, nf, Vec3(A.x, A.y, hz), Vec3(B.x, B.y, hz), Vec3(C.x, C.y, hz), Vec3(D.x, D.y, hz));
                    EmitQuad(v, n, nb, Vec3(A.x, A.y, -hz), Vec3(D.x, D.y, -hz), Vec3(C.x, C.y, -hz), Vec3(B.x, B.y, -hz));
                }
        }
        prevRuns = runs;
    }
    // 侧面：沿每条轮廓边挤出
    for (size_t i = 0; i < edges.size(); i++) {
        Vec3 A, B;
        conv(edges[i].first.x, edges[i].first.y, A);
        conv(edges[i].second.x, edges[i].second.y, B);
        double ex = B.y - A.y, ey = -(B.x - A.x);
        double len = std::sqrt(ex * ex + ey * ey);
        if (len < 1e-9) continue;
        ex /= len; ey /= len;
        Vec3 nl(ex, ey, 0);
        EmitQuad(v, n, nl, Vec3(A.x, A.y, -hz), Vec3(B.x, B.y, -hz),
                 Vec3(B.x, B.y, hz), Vec3(A.x, A.y, hz));
    }
}


// ---------------- Blender 式网格编辑（点/线/面） ----------------
// 进入编辑时把程序化网格固化成 editVerts/editTris（空间哈希焊接重复顶点），
// 此后 GenerateMesh 直接输出固化网格；法线按硬/软边重算（盒体平直、球体平滑）。

// 该类型是否可进入网格编辑：普通图元（排除曲线/3D文字/布尔）
bool Object3D::EditableType(int t) {
    return IsPrimitive(t) && t != PT_SPLINE && t != PT_TEXT3D && t != PT_BOOLEAN;
}

// 由程序化网格生成编辑网格：三角汤 → 空间哈希焊接（容差=包围盒对角线×1e-6）→ 索引化
void Object3D::BuildEditMeshFromCore() {
    std::vector<Vec3> v, n;
    GenerateMeshCore(v, n);
    editVerts.clear();
    editTris.clear();
    if (v.empty()) return;
    double mnx = v[0].x, mny = v[0].y, mnz = v[0].z;
    double mxx = v[0].x, mxy = v[0].y, mxz = v[0].z;
    for (size_t i = 1; i < v.size(); i++) {
        if (v[i].x < mnx) mnx = v[i].x;  if (v[i].x > mxx) mxx = v[i].x;
        if (v[i].y < mny) mny = v[i].y;  if (v[i].y > mxy) mxy = v[i].y;
        if (v[i].z < mnz) mnz = v[i].z;  if (v[i].z > mxz) mxz = v[i].z;
    }
    double diag = std::sqrt((mxx - mnx) * (mxx - mnx) + (mxy - mny) * (mxy - mny) + (mxz - mnz) * (mxz - mnz));
    double eps = diag * 1e-6;
    if (eps < 1e-9) eps = 1e-9;
    // 空间哈希：cell 尺寸 = eps；重复顶点必落在相邻 cell（27 邻域查找）。
    // key 沿各维线性可分（相邻 cell 的 key 差恒为 ±P^k），跨维碰撞只会多查不会漏。
    const long long P = 8388603;                    // 大质数基，降低跨维碰撞概率
    std::map<long long, std::vector<int> > grid;
    std::vector<int> remap(v.size(), -1);
    for (size_t i = 0; i < v.size(); i++) {
        long long cx = (long long)std::floor(v[i].x / eps) + (1LL << 20);
        long long cy = (long long)std::floor(v[i].y / eps) + (1LL << 20);
        long long cz = (long long)std::floor(v[i].z / eps) + (1LL << 20);
        int found = -1;
        for (long long da = -1; da <= 1 && found < 0; da++)
        for (long long db = -1; db <= 1 && found < 0; db++)
        for (long long dc = -1; dc <= 1 && found < 0; dc++) {
            long long key = (cx + da) + (cy + db) * P + (cz + dc) * P * P;
            std::map<long long, std::vector<int> >::iterator it = grid.find(key);
            if (it == grid.end()) continue;
            for (size_t k = 0; k < it->second.size(); k++) {
                const Vec3& w = editVerts[it->second[k]];
                double ddx = w.x - v[i].x, ddy = w.y - v[i].y, ddz = w.z - v[i].z;
                if (ddx * ddx + ddy * ddy + ddz * ddz <= eps * eps) { found = it->second[k]; break; }
            }
        }
        if (found < 0) {
            found = (int)editVerts.size();
            editVerts.push_back(v[i]);
            grid[cx + cy * P + cz * P * P].push_back(found);
        }
        remap[i] = found;
    }
    std::vector<int> triMap(v.size() / 3, -1);  // 原三角序号 → 焊接后三角序号（-1=退化被丢弃）
    for (size_t i = 0; i + 2 < v.size(); i += 3) {
        int a = remap[i], b = remap[i + 1], c = remap[i + 2];
        if (a == b || b == c || a == c) continue;   // 焊接后退化为线/点的三角丢弃
        triMap[i / 3] = (int)(editTris.size() / 3);
        editTris.push_back(a);
        editTris.push_back(b);
        editTris.push_back(c);
    }
    // 识别三角剖分斜线：程序化网格的四边形由 EmitQuad 发射为"连续两个三角"（原数组序号
    // (2k, 2k+1)），经 triMap 映射到焊接后三角——不受退化三角丢弃导致的索引错位影响。
    // 两三角共享一条边且近似共面（法线点积>0.5，排除端盖扇面/侧面+底面这类非四边形连对），
    // 则共享边为剖分斜线。这样不会把盒体同一平面上相邻四边形的公共棱边误判为斜线。
    editDiagEdges.clear();
    size_t nt = editTris.size() / 3;
    // 面法线预计算
    std::vector<Vec3> triN(nt);
    for (size_t t = 0; t < nt; t++) {
        int a = editTris[t * 3], b = editTris[t * 3 + 1], c = editTris[t * 3 + 2];
        Vec3 e1 = editVerts[b] - editVerts[a], e2 = editVerts[c] - editVerts[a];
        Vec3 n = Cross(e1, e2);
        double l = std::sqrt(Dot(n, n));
        if (l > 1e-12) n = Vec3(n.x / l, n.y / l, n.z / l);
        triN[t] = n;
    }
    size_t origNt = v.size() / 3;
    for (size_t t = 0; t + 1 < origNt; t += 2) {
        int t0 = triMap[t], t1 = triMap[t + 1];
        if (t0 < 0 || t1 < 0) continue;
        if (Dot(triN[t0], triN[t1]) < 0.5) continue;
        int tri0[3] = { editTris[t0 * 3], editTris[t0 * 3 + 1], editTris[t0 * 3 + 2] };
        int tri1v[3] = { editTris[t1 * 3], editTris[t1 * 3 + 1], editTris[t1 * 3 + 2] };
        int shared[2] = { -1, -1 }; int sc = 0;
        for (int p = 0; p < 3 && sc < 2; p++)
            for (int q = 0; q < 3; q++)
                if (tri0[p] == tri1v[q]) { shared[sc++] = tri0[p]; break; }
        if (sc != 2) continue;
        int ea = shared[0], eb = shared[1];
        if (ea > eb) { int tmp = ea; ea = eb; eb = tmp; }
        editDiagEdges.push_back(std::make_pair(ea, eb));
    }
    std::sort(editDiagEdges.begin(), editDiagEdges.end());
    editDiagEdges.erase(std::unique(editDiagEdges.begin(), editDiagEdges.end()), editDiagEdges.end());
    // 面/线颜色表初始化：全部继承物体色
    editTriColor.assign(nt, 0u);
    editEdgeColorKey.clear();
    editEdgeColorVal.clear();
}

// ------------------------------------------------------------ 面/线独立着色
unsigned int Object3D::PackEditColor(double r, double g, double b) {
    if (r < 0) r = 0; if (r > 1) r = 1;
    if (g < 0) g = 0; if (g > 1) g = 1;
    if (b < 0) b = 0; if (b > 1) b = 1;
    return 0x80000000u |
        ((unsigned int)(r * 255.0 + 0.5) << 16) |
        ((unsigned int)(g * 255.0 + 0.5) << 8) |
        (unsigned int)(b * 255.0 + 0.5);
}

void Object3D::UnpackEditColor(unsigned int c, double& r, double& g, double& b) {
    r = ((c >> 16) & 0xFF) / 255.0;
    g = ((c >> 8) & 0xFF) / 255.0;
    b = (c & 0xFF) / 255.0;
}

void Object3D::SetEditTriColors(const std::vector<int>& tris, unsigned int c) {
    if (editTriColor.empty()) editTriColor.assign(editTris.size() / 3, 0u);
    int nt = (int)(editTris.size() / 3);
    for (size_t i = 0; i < tris.size(); i++)
        if (tris[i] >= 0 && tris[i] < nt) editTriColor[tris[i]] = c;
}

void Object3D::SetEditEdgeColors(const std::vector<std::pair<int, int> >& edges, unsigned int c) {
    for (size_t i = 0; i < edges.size(); i++) {
        int a = edges[i].first, b = edges[i].second;
        if (a > b) { int t = a; a = b; b = t; }
        std::pair<int, int> key(a, b);
        // 已存在则改色；否则按序插入（保持 key 升序，供二分查找）
        size_t lo = 0, hi = editEdgeColorKey.size();
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (editEdgeColorKey[mid] < key) lo = mid + 1; else hi = mid;
        }
        if (lo < editEdgeColorKey.size() && editEdgeColorKey[lo] == key) {
            editEdgeColorVal[lo] = c;
        } else {
            editEdgeColorKey.insert(editEdgeColorKey.begin() + lo, key);
            editEdgeColorVal.insert(editEdgeColorVal.begin() + lo, c);
        }
    }
}

bool Object3D::GetEditTriColor(int t, unsigned int& c) const {
    if (t < 0 || t >= (int)editTriColor.size() || editTriColor[t] == 0) return false;
    c = editTriColor[t];
    return true;
}

bool Object3D::GetEditEdgeColor(int a, int b, unsigned int& c) const {
    if (a > b) { int t = a; a = b; b = t; }
    std::pair<int, int> key(a, b);
    size_t lo = 0, hi = editEdgeColorKey.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (editEdgeColorKey[mid] < key) lo = mid + 1; else hi = mid;
    }
    if (lo < editEdgeColorKey.size() && editEdgeColorKey[lo] == key) { c = editEdgeColorVal[lo]; return true; }
    return false;
}

// 取三角 triIdx 所属四边形的全部三角：找到与该三角共享"斜线"的另一三角。
void Object3D::GetQuadTris(int triIdx, std::vector<int>& out) const {
    out.clear();
    int nt = (int)(editTris.size() / 3);
    if (triIdx < 0 || triIdx >= nt) return;
    out.push_back(triIdx);
    int a = editTris[triIdx * 3], b = editTris[triIdx * 3 + 1], c = editTris[triIdx * 3 + 2];
    int edges[3][2] = { {a,b}, {b,c}, {c,a} };
    for (int k = 0; k < 3; k++) {
        if (!IsDiagEdge(edges[k][0], edges[k][1])) continue;
        // 这条边是斜线，找共享它的另一个三角
        int ea = edges[k][0], eb = edges[k][1];
        for (int t = 0; t < nt; t++) {
            if (t == triIdx) continue;
            int i0 = editTris[t * 3], i1 = editTris[t * 3 + 1], i2 = editTris[t * 3 + 2];
            bool hasA = (i0 == ea || i1 == ea || i2 == ea);
            bool hasB = (i0 == eb || i1 == eb || i2 == eb);
            if (hasA && hasB) { out.push_back(t); return; }
        }
    }
}

// 编辑网格法线：面法线 + 按“软边连通簇”的顶点平滑法线。
// 每个顶点把邻接三角分簇：两三角共享以该顶点为端点的边、且二面角软（dot>cos30°）→ 同簇；
// corner 取所在簇的平均法线。盒体棱边/角点两侧的面簇互不连通 → 各自保持面法线（硬边平直），
// 球体/圆柱侧面全软边 → 整簇平滑；开口边界只有单簇，取面法线平均。
static void ComputeEditNormals(const std::vector<Vec3>& verts, const std::vector<int>& tris,
                               std::vector<Vec3>& out) {
    size_t nv = verts.size();
    size_t nt = tris.size() / 3;
    out.assign(tris.size(), Vec3(0, 0, 0));
    if (nt == 0 || nv == 0) return;
    std::vector<Vec3> fn(nt);
    for (size_t t = 0; t < nt; t++) {
        const Vec3& A = verts[tris[t * 3]];
        const Vec3& B = verts[tris[t * 3 + 1]];
        const Vec3& C = verts[tris[t * 3 + 2]];
        // 项目绕序约定下 Cross(B-A,C-A) 指向面反面（与 EmitQuad 传入法线反向），
        // 取 Cross(C-A,B-A) 使重算法线与 GenerateMeshCore 的朝外法线一致
        fn[t] = Normalize(Cross(C - A, B - A));
    }
    // 顶点 → 邻接三角表
    std::vector<std::vector<int> > vertTris(nv);
    for (size_t t = 0; t < nt; t++)
        for (int j = 0; j < 3; j++)
            vertTris[tris[t * 3 + j]].push_back((int)t);
    // 逐顶点分簇（BFS），corner 写入所在簇的平均法线
    const double COS30 = 0.86602540378443865;
    std::vector<int> cid;                       // 邻接表局部簇号
    std::vector<Vec3> csum;                     // 每簇法线和
    for (size_t v = 0; v < nv; v++) {
        const std::vector<int>& adj = vertTris[v];
        size_t na = adj.size();
        if (na == 0) continue;
        // 每个邻接三角中除 v 外的另外两个端点（用于判断是否共享以 v 为端点的边）
        std::vector<int> o1(na), o2(na);
        for (size_t i = 0; i < na; i++) {
            int t = adj[i];
            int a = tris[t * 3], b = tris[t * 3 + 1], c = tris[t * 3 + 2];
            if (a == (int)v)      { o1[i] = b; o2[i] = c; }
            else if (b == (int)v) { o1[i] = a; o2[i] = c; }
            else                  { o1[i] = a; o2[i] = b; }
        }
        cid.assign(na, -1);
        csum.clear();
        for (size_t i = 0; i < na; i++) {
            if (cid[i] >= 0) continue;
            int cn = (int)csum.size();
            Vec3 sum(0, 0, 0);
            std::vector<int> stack;
            stack.push_back((int)i);
            cid[i] = cn;
            while (!stack.empty()) {
                int cur = stack.back();
                stack.pop_back();
                sum = sum + fn[adj[cur]];
                for (size_t k = 0; k < na; k++) {
                    if (cid[k] >= 0) continue;
                    // 共享边 (v, o)：cur 与 k 在 v 之外的端点有交集
                    bool share = (o1[cur] == o1[k] || o1[cur] == o2[k] ||
                                  o2[cur] == o1[k] || o2[cur] == o2[k]);
                    if (!share) continue;
                    if (Dot(fn[adj[cur]], fn[adj[k]]) <= COS30) continue;   // 硬边不连通
                    cid[k] = cn;
                    stack.push_back((int)k);
                }
            }
            csum.push_back(sum);
        }
        for (size_t i = 0; i < na; i++) {
            Vec3 nn = Normalize(csum[cid[i]]);
            int t = adj[i];
            for (int j = 0; j < 3; j++)
                if (tris[t * 3 + j] == (int)v) out[t * 3 + j] = nn;
        }
    }
}

// 编辑网格输出：展开为顶点/法线成对的三角汤（与 GenerateMeshCore 输出约定一致，
// 渲染/布尔/拾取等既有消费方零改动）
void Object3D::EmitEditMesh(std::vector<Vec3>& verts, std::vector<Vec3>& normals) const {
    verts.clear();
    normals.clear();
    if (editTris.empty() || editVerts.empty()) return;
    std::vector<Vec3> cn;
    ComputeEditNormals(editVerts, editTris, cn);
    verts.reserve(editTris.size());
    normals.reserve(editTris.size());
    for (size_t i = 0; i < editTris.size(); i++) {
        verts.push_back(editVerts[editTris[i]]);
        normals.push_back(cn[i]);
    }
}

// 删除指定三角（tris 元素 = editTris 中的第 i 个三角序号）
void Object3D::DeleteEditTris(const std::vector<int>& tris) {
    if (tris.empty() || editTris.empty()) return;
    std::vector<bool> kill(editTris.size() / 3, false);
    for (size_t i = 0; i < tris.size(); i++)
        if (tris[i] >= 0 && tris[i] < (int)(editTris.size() / 3)) kill[tris[i]] = true;
    std::vector<int> keep;
    keep.reserve(editTris.size());
    std::vector<unsigned int> keepCol;
    if (!editTriColor.empty()) keepCol.reserve(editTriColor.size());
    for (size_t t = 0; t < editTris.size() / 3; t++) {
        if (kill[t]) continue;
        keep.push_back(editTris[t * 3]);
        keep.push_back(editTris[t * 3 + 1]);
        keep.push_back(editTris[t * 3 + 2]);
        if (!editTriColor.empty()) keepCol.push_back(editTriColor[t]);
    }
    editTris.swap(keep);
    if (!editTriColor.empty()) editTriColor.swap(keepCol);
}

// 删除指定顶点及其关联三角（顶点表重排，三角索引同步更新）
void Object3D::DeleteEditVerts(const std::vector<int>& verts) {
    if (verts.empty() || editVerts.empty()) return;
    std::vector<bool> killV(editVerts.size(), false);
    for (size_t i = 0; i < verts.size(); i++)
        if (verts[i] >= 0 && verts[i] < (int)editVerts.size()) killV[verts[i]] = true;
    std::vector<int> keep;
    keep.reserve(editTris.size());
    std::vector<unsigned int> keepCol;
    if (!editTriColor.empty()) keepCol.reserve(editTriColor.size());
    for (size_t t = 0; t + 2 < editTris.size(); t += 3) {
        if (killV[editTris[t]] || killV[editTris[t + 1]] || killV[editTris[t + 2]]) continue;
        keep.push_back(editTris[t]);
        keep.push_back(editTris[t + 1]);
        keep.push_back(editTris[t + 2]);
        if (!editTriColor.empty()) keepCol.push_back(editTriColor[t / 3]);
    }
    editTris.swap(keep);
    if (!editTriColor.empty()) editTriColor.swap(keepCol);
    std::vector<int> remap(editVerts.size(), -1);
    std::vector<Vec3> nv2;
    nv2.reserve(editVerts.size());
    for (size_t i = 0; i < editVerts.size(); i++) {
        if (killV[i]) continue;
        remap[i] = (int)nv2.size();
        nv2.push_back(editVerts[i]);
    }
    editVerts.swap(nv2);
    for (size_t i = 0; i < editTris.size(); i++) editTris[i] = remap[editTris[i]];
    // 边颜色表按新索引重建（端点被删的边丢弃）
    if (!editEdgeColorKey.empty()) {
        std::vector<std::pair<std::pair<int, int>, unsigned int> > tmp;
        tmp.reserve(editEdgeColorKey.size());
        for (size_t i = 0; i < editEdgeColorKey.size(); i++) {
            int ra = remap[editEdgeColorKey[i].first];
            int rb = remap[editEdgeColorKey[i].second];
            if (ra < 0 || rb < 0) continue;
            if (ra > rb) { int tt = ra; ra = rb; rb = tt; }
            tmp.push_back(std::make_pair(std::make_pair(ra, rb), editEdgeColorVal[i]));
        }
        std::sort(tmp.begin(), tmp.end());
        editEdgeColorKey.clear();
        editEdgeColorVal.clear();
        for (size_t i = 0; i < tmp.size(); i++) {
            if (!editEdgeColorKey.empty() && editEdgeColorKey.back() == tmp[i].first)
                editEdgeColorVal.back() = tmp[i].second;
            else {
                editEdgeColorKey.push_back(tmp[i].first);
                editEdgeColorVal.push_back(tmp[i].second);
            }
        }
    }
}

// 生成本地坐标系网格：有编辑网格时直接输出固化网格（点/线/面编辑结果），
// 否则按参数程序化生成。布尔运算取 A/B 时调用 GenerateMesh（编辑网格自动参与布尔）。
void Object3D::GenerateMesh(std::vector<Vec3>& verts, std::vector<Vec3>& normals) const {
    if (HasEditMesh()) { EmitEditMesh(verts, normals); return; }
    GenerateMeshCore(verts, normals);
}

void Object3D::GenerateMeshCore(std::vector<Vec3>& verts, std::vector<Vec3>& normals) const {
    verts.clear(); normals.clear();
    if (!IsPrimitive(type)) return;

    if (type == PT_BOX) {
        double hw = params[0] * 0.5, hh = params[1] * 0.5, hd = params[2] * 0.5;
        // 各面按「细分」划分网格：网格编辑需要足够顶点才能拖出平滑形状
        int seg = segments < 1 ? 1 : segments;
        EmitQuadGrid(verts, normals, Vec3(1, 0, 0),
                     Vec3(hw, -hh, -hd), Vec3(hw, -hh, hd), Vec3(hw, hh, hd), Vec3(hw, hh, -hd), seg);
        EmitQuadGrid(verts, normals, Vec3(-1, 0, 0),
                     Vec3(-hw, -hh, hd), Vec3(-hw, -hh, -hd), Vec3(-hw, hh, -hd), Vec3(-hw, hh, hd), seg);
        EmitQuadGrid(verts, normals, Vec3(0, 1, 0),
                     Vec3(-hw, hh, -hd), Vec3(hw, hh, -hd), Vec3(hw, hh, hd), Vec3(-hw, hh, hd), seg);
        EmitQuadGrid(verts, normals, Vec3(0, -1, 0),
                     Vec3(-hw, -hh, hd), Vec3(hw, -hh, hd), Vec3(hw, -hh, -hd), Vec3(-hw, -hh, -hd), seg);
        EmitQuadGrid(verts, normals, Vec3(0, 0, 1),
                     Vec3(hw, -hh, hd), Vec3(-hw, -hh, hd), Vec3(-hw, hh, hd), Vec3(hw, hh, hd), seg);
        EmitQuadGrid(verts, normals, Vec3(0, 0, -1),
                     Vec3(-hw, -hh, -hd), Vec3(hw, -hh, -hd), Vec3(hw, hh, -hd), Vec3(-hw, hh, -hd), seg);
        return;
    }

    if (type == PT_PLANE) {
        double hs = params[0] * 0.5;
        Vec3 n(0, 1, 0);
        EmitTri(verts, normals, Vec3(-hs, 0, -hs), Vec3(hs, 0, -hs), Vec3(hs, 0, hs), n, n, n);
        EmitTri(verts, normals, Vec3(-hs, 0, -hs), Vec3(hs, 0, hs), Vec3(-hs, 0, hs), n, n, n);
        return;
    }

    int seg = segments < 4 ? 4 : segments;
    if (seg > 64) seg = 64;

    if (type == PT_SPHERE) {
        double rad = params[0];
        if (rad <= 0) rad = 1.0;
        int stacks = seg / 2;
        if (stacks < 3) stacks = 3;
        for (int i = 0; i < stacks; i++) {
            double lat0 = ((double)i / stacks) * kPI - kPI * 0.5;
            double lat1 = ((double)(i + 1) / stacks) * kPI - kPI * 0.5;
            double s0 = std::sin(lat0), c0 = std::cos(lat0);
            double s1 = std::sin(lat1), c1 = std::cos(lat1);
            for (int j = 0; j < seg; j++) {
                double lon0 = ((double)j / seg) * 2.0 * kPI;
                double lon1 = ((double)(j + 1) / seg) * 2.0 * kPI;
                double sl0 = std::sin(lon0), cl0 = std::cos(lon0);
                double sl1 = std::sin(lon1), cl1 = std::cos(lon1);
                Vec3 p00(c0 * cl0, s0, c0 * sl0);
                Vec3 p10(c1 * cl0, s1, c1 * sl0);
                Vec3 p11(c1 * cl1, s1, c1 * sl1);
                Vec3 p01(c0 * cl1, s0, c0 * sl1);
                EmitTri(verts, normals, p00 * rad, p10 * rad, p11 * rad, p00, p10, p11);
                EmitTri(verts, normals, p00 * rad, p11 * rad, p01 * rad, p00, p11, p01);
            }
        }
        return;
    }

    if (type == PT_CYLINDER) {
        double rad = params[0], hh = params[1] * 0.5;
        if (rad <= 0) rad = 1.0;
        // 侧面沿高度按「细分」分段：自由弯曲需要中间环才能把整个柱体弯成平滑圆弧
        // （只有上下两圈顶点时弯不动，只能整体倾斜）；顶/底盖是横截面，弯曲时整体
        // 刚性旋转（形状不变、只改朝向），无需细分。
        Vec3 up(0, 1, 0), dn(0, -1, 0);
        for (int j = 0; j < seg; j++) {
            double a0 = ((double)j / seg) * 2.0 * kPI;
            double a1 = ((double)(j + 1) / seg) * 2.0 * kPI;
            Vec3 n0(std::cos(a0), 0, std::sin(a0));
            Vec3 n1(std::cos(a1), 0, std::sin(a1));
            for (int i = 0; i < seg; i++) {
                double y0 = -hh + (2.0 * hh) * i / seg;
                double y1 = -hh + (2.0 * hh) * (i + 1) / seg;
                Vec3 p0b(n0.x * rad, y0, n0.z * rad), p0t(n0.x * rad, y1, n0.z * rad);
                Vec3 p1b(n1.x * rad, y0, n1.z * rad), p1t(n1.x * rad, y1, n1.z * rad);
                EmitTri(verts, normals, p0b, p0t, p1t, n0, n0, n1);
                EmitTri(verts, normals, p0b, p1t, p1b, n0, n1, n1);
            }
            // 顶盖/底盖
            Vec3 pT0(n0.x * rad, hh, n0.z * rad), pT1(n1.x * rad, hh, n1.z * rad);
            Vec3 pB0(n0.x * rad, -hh, n0.z * rad), pB1(n1.x * rad, -hh, n1.z * rad);
            EmitTri(verts, normals, Vec3(0, hh, 0), pT1, pT0, up, up, up);
            EmitTri(verts, normals, Vec3(0, -hh, 0), pB0, pB1, dn, dn, dn);
        }
        return;
    }

    if (type == PT_RTUBE) {
        // 圆形空管：外圆柱 − 内圆柱（空心），params[0]=外半径, params[1]=内半径, params[2]=高度
        double ro = params[0], ri = params[1], hh = params[2] * 0.5;
        if (ro <= 0) ro = 1.0;
        if (ri <= 0 || ri >= ro) ri = ro * 0.6;
        Vec3 up(0, 1, 0), dn(0, -1, 0);
        for (int j = 0; j < seg; j++) {
            double a0 = ((double)j / seg) * 2.0 * kPI;
            double a1 = ((double)(j + 1) / seg) * 2.0 * kPI;
            Vec3 n0(std::cos(a0), 0, std::sin(a0));
            Vec3 n1(std::cos(a1), 0, std::sin(a1));
            // 外侧面（朝外）：沿高度按「细分」分段，网格编辑可拖出平滑形状
            for (int i = 0; i < seg; i++) {
                double y0 = -hh + (2.0 * hh) * i / seg;
                double y1 = -hh + (2.0 * hh) * (i + 1) / seg;
                Vec3 p0b(n0.x * ro, y0, n0.z * ro), p0t(n0.x * ro, y1, n0.z * ro);
                Vec3 p1b(n1.x * ro, y0, n1.z * ro), p1t(n1.x * ro, y1, n1.z * ro);
                EmitTri(verts, normals, p0b, p0t, p1t, n0, n0, n1);
                EmitTri(verts, normals, p0b, p1t, p1b, n0, n1, n1);
            }
            // 内侧面（朝轴心）：与外侧面相反的绕向
            for (int i = 0; i < seg; i++) {
                double y0 = -hh + (2.0 * hh) * i / seg;
                double y1 = -hh + (2.0 * hh) * (i + 1) / seg;
                Vec3 p0b(n0.x * ri, y0, n0.z * ri), p0t(n0.x * ri, y1, n0.z * ri);
                Vec3 p1b(n1.x * ri, y0, n1.z * ri), p1t(n1.x * ri, y1, n1.z * ri);
                EmitTri(verts, normals, p0b, p1t, p0t, -n0, -n1, -n0);
                EmitTri(verts, normals, p0b, p1b, p1t, -n0, -n1, -n1);
            }
            // 顶盖环（外顶→内顶，法线朝上）/ 底盖环（法线朝下）
            Vec3 oT0(n0.x * ro, hh, n0.z * ro), oT1(n1.x * ro, hh, n1.z * ro);
            Vec3 iT0(n0.x * ri, hh, n0.z * ri), iT1(n1.x * ri, hh, n1.z * ri);
            EmitTri(verts, normals, oT0, oT1, iT1, up, up, up);
            EmitTri(verts, normals, oT0, iT1, iT0, up, up, up);
            Vec3 oB0(n0.x * ro, -hh, n0.z * ro), oB1(n1.x * ro, -hh, n1.z * ro);
            Vec3 iB0(n0.x * ri, -hh, n0.z * ri), iB1(n1.x * ri, -hh, n1.z * ri);
            EmitTri(verts, normals, oB0, iB0, iB1, dn, dn, dn);
            EmitTri(verts, normals, oB0, iB1, oB1, dn, dn, dn);
        }
        return;
    }

    if (type == PT_CONE) {
        double rad = params[0], hh = params[1] * 0.5;
        if (rad <= 0) rad = 1.0;
        Vec3 apex(0, hh, 0);
        Vec3 dn(0, -1, 0);
        for (int j = 0; j < seg; j++) {
            double a0 = ((double)j / seg) * 2.0 * kPI;
            double a1 = ((double)(j + 1) / seg) * 2.0 * kPI;
            Vec3 p0(std::cos(a0) * rad, -hh, std::sin(a0) * rad);
            Vec3 p1(std::cos(a1) * rad, -hh, std::sin(a1) * rad);
            // 侧面法线用面法线（平坦着色）
            Vec3 nrm = Normalize(Cross(p1 - apex, p0 - apex));
            if (nrm.y < 0) nrm = -nrm;
            EmitTri(verts, normals, apex, p0, p1, nrm, nrm, nrm);
            // 底面
            EmitTri(verts, normals, Vec3(0, -hh, 0), p0, p1, dn, dn, dn);
        }
        return;
    }

    if (type == PT_TORUS) {
        double R = params[0], tube = params[1];
        if (R <= 0) R = 1.0;
        if (tube <= 0) tube = 0.3;
        int minor = seg / 2;
        if (minor < 4) minor = 4;
        for (int i = 0; i < seg; i++) {
            double u0 = ((double)i / seg) * 2.0 * kPI;
            double u1 = ((double)(i + 1) / seg) * 2.0 * kPI;
            double cu0 = std::cos(u0), su0 = std::sin(u0);
            double cu1 = std::cos(u1), su1 = std::sin(u1);
            for (int j = 0; j < minor; j++) {
                double v0 = ((double)j / minor) * 2.0 * kPI;
                double v1 = ((double)(j + 1) / minor) * 2.0 * kPI;
                double cv0 = std::cos(v0), sv0 = std::sin(v0);
                double cv1 = std::cos(v1), sv1 = std::sin(v1);
                Vec3 n00(cv0 * cu0, sv0, cv0 * su0);
                Vec3 n10(cv0 * cu1, sv0, cv0 * su1);
                Vec3 n11(cv1 * cu1, sv1, cv1 * su1);
                Vec3 n01(cv1 * cu0, sv1, cv1 * su0);
                Vec3 p00 = Vec3((R + tube * cv0) * cu0, tube * sv0, (R + tube * cv0) * su0);
                Vec3 p10 = Vec3((R + tube * cv0) * cu1, tube * sv0, (R + tube * cv0) * su1);
                Vec3 p11 = Vec3((R + tube * cv1) * cu1, tube * sv1, (R + tube * cv1) * su1);
                Vec3 p01 = Vec3((R + tube * cv1) * cu0, tube * sv1, (R + tube * cv1) * su0);
                EmitTri(verts, normals, p00, p10, p11, n00, n10, n11);
                EmitTri(verts, normals, p00, p11, p01, n00, n11, n01);
            }
        }
        return;
    }

    if (type == PT_SPLINE) {
        double rad = params[0]; if (rad <= 0) rad = 0.12;
        int ring = segments; if (ring < 3) ring = 3; if (ring > 20) ring = 20;
        int sub = segments;  if (sub < 4) sub = 4;   if (sub > 20) sub = 20;
        std::vector<Vec3> pts;   // 平滑后的中心线点列
        size_t n = ctrlPts.size();
        if (n == 0) return;
        if (n == 1) { pts.push_back(ctrlPts[0]); }
        else if (n == 2) {
            for (int s = 0; s <= sub; s++) {
                double t = (double)s / sub;
                pts.push_back(ctrlPts[0] + (ctrlPts[1] - ctrlPts[0]) * t);
            }
        } else {
            for (size_t i = 0; i + 1 < n; i++) {
                Vec3 p0 = (i == 0) ? ctrlPts[0] : ctrlPts[i - 1];
                Vec3 p1 = ctrlPts[i];
                Vec3 p2 = ctrlPts[i + 1];
                Vec3 p3 = (i + 2 < n) ? ctrlPts[i + 2] : ctrlPts[i + 1];
                for (int s = 0; s < sub; s++) {
                    double t = (double)s / sub;
                    double t2 = t * t, t3 = t2 * t;
                    Vec3 q = (p1 * 2.0 + (p2 - p0) * t +
                              (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * t2 +
                              (p3 - p0 + p1 * 3.0 - p2 * 3.0) * t3) * 0.5;
                    pts.push_back(q);
                }
            }
            pts.push_back(ctrlPts[n - 1]);
        }
        size_t m = pts.size();
        if (m < 2) return;
        // 管壁：逐帧用方向生成环绕圆环
        for (size_t i = 0; i + 1 < m; i++) {
            Vec3 dir = (i + 1 < m) ? Normalize(pts[i + 1] - pts[i]) : Vec3(1, 0, 0);
            Vec3 up  = (std::fabs(dir.y) < 0.99) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
            Vec3 tan = Normalize(Cross(dir, up));
            Vec3 bit = Normalize(Cross(dir, tan));
            Vec3 p = pts[i];
            Vec3 p2 = pts[i + 1];
            Vec3 dir2 = (i + 2 < m) ? Normalize(pts[i + 2] - pts[i + 1]) : dir;
            Vec3 up2  = (std::fabs(dir2.y) < 0.99) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
            Vec3 tan2 = Normalize(Cross(dir2, up2));
            Vec3 bit2 = Normalize(Cross(dir2, tan2));
            for (int k = 0; k < ring; k++) {
                double a0 = 2.0 * kPI * k / ring;
                double a1 = 2.0 * kPI * (k + 1) / ring;
                Vec3 n0 = tan * std::cos(a0) + bit * std::sin(a0);
                Vec3 n1 = tan * std::cos(a1) + bit * std::sin(a1);
                Vec3 n20 = tan2 * std::cos(a0) + bit2 * std::sin(a0);
                Vec3 n21 = tan2 * std::cos(a1) + bit2 * std::sin(a1);
                Vec3 q0 = p  + n0  * rad;
                Vec3 q1 = p  + n1  * rad;
                Vec3 q20 = p2 + n20 * rad;
                Vec3 q21 = p2 + n21 * rad;
                EmitTri(verts, normals, q0, q20, q21, n0, n20, n21);
                EmitTri(verts, normals, q0, q21, q1, n0, n21, n1);
            }
        }
        // 起点/终点圆盖
        Vec3 d0 = Normalize(pts[1] - pts[0]);
        Vec3 d1 = Normalize(pts[m - 1] - pts[m - 2]);
        Vec3 up0 = (std::fabs(d0.y) < 0.99) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        Vec3 tan0 = Normalize(Cross(d0, up0)), bit0 = Normalize(Cross(d0, tan0));
        Vec3 up1 = (std::fabs(d1.y) < 0.99) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        Vec3 tan1 = Normalize(Cross(d1, up1)), bit1 = Normalize(Cross(d1, tan1));
        for (int k = 0; k < ring; k++) {
            double a0 = 2.0 * kPI * k / ring;
            double a1 = 2.0 * kPI * (k + 1) / ring;
            Vec3 r0 = tan0 * std::cos(a0) + bit0 * std::sin(a0);
            Vec3 r1 = tan0 * std::cos(a1) + bit0 * std::sin(a1);
            EmitTri(verts, normals, pts[0], pts[0] + r0 * rad, pts[0] + r1 * rad, -d0, -d0, -d0);
            Vec3 s0 = tan1 * std::cos(a0) + bit1 * std::sin(a0);
            Vec3 s1 = tan1 * std::cos(a1) + bit1 * std::sin(a1);
            EmitTri(verts, normals, pts[m - 1], pts[m - 1] + s1 * rad, pts[m - 1] + s0 * rad, d1, d1, d1);
        }
        return;
    }

    if (type == PT_LEAF) {
        double a = params[0] > 0 ? params[0] : 0.8;   // 长半轴 x
        double b = params[1] > 0 ? params[1] : 0.45;  // 宽半轴 y
        double c = params[2] > 0 ? params[2] : 0.06;  // 厚半轴 z
        int seg = segments; if (seg < 4) seg = 4; if (seg > 64) seg = 64;
        int stacks = seg / 2; if (stacks < 3) stacks = 3;
        for (int i = 0; i < stacks; i++) {
            double lat0 = ((double)i / stacks) * kPI - kPI * 0.5;
            double lat1 = ((double)(i + 1) / stacks) * kPI - kPI * 0.5;
            double s0 = std::sin(lat0), c0 = std::cos(lat0);
            double s1 = std::sin(lat1), c1 = std::cos(lat1);
            for (int j = 0; j < seg; j++) {
                double lon0 = ((double)j / seg) * 2.0 * kPI;
                double lon1 = ((double)(j + 1) / seg) * 2.0 * kPI;
                double sl0 = std::sin(lon0), cl0 = std::cos(lon0);
                double sl1 = std::sin(lon1), cl1 = std::cos(lon1);
                Vec3 n00(c0 * cl0, s0, c0 * sl0);
                Vec3 n10(c1 * cl0, s1, c1 * sl0);
                Vec3 n11(c1 * cl1, s1, c1 * sl1);
                Vec3 n01(c0 * cl1, s0, c0 * sl1);
                Vec3 p00(n00.x * a, n00.y * b, n00.z * c);
                Vec3 p10(n10.x * a, n10.y * b, n10.z * c);
                Vec3 p11(n11.x * a, n11.y * b, n11.z * c);
                Vec3 p01(n01.x * a, n01.y * b, n01.z * c);
                EmitTri(verts, normals, p00, p10, p11, n00, n10, n11);
                EmitTri(verts, normals, p00, p11, p01, n00, n11, n01);
            }
        }
        return;
    }

    if (type == PT_TEXT3D) {
        if (!_meshValid) {
            _meshVerts.clear(); _meshNorms.clear();
            // 文本转图形：光栅化 mask → 梯形条带填充前后面 + Marching Squares 边界挤出侧面。
            // 文字是真实实体网格，无背景贴图，天然透明背景。
            BuildText3DMesh(_meshVerts, _meshNorms, this);
            _meshValid = true;
            _meshRev++;
        }
        verts = _meshVerts;
        normals = _meshNorms;
        return;
    }

    if (type == PT_BOOLEAN) {
        // 布尔差结果：网格缓存由布尔操作生成或序列化恢复，直接返回
        verts = _meshVerts;
        normals = _meshNorms;
        return;
    }

    if (type == PT_TUBE) {
        // 方形空管：外方柱 - 内方柱（空心），params[0]=外边长, params[1]=内边长, params[2]=高度
        double wo = params[0] * 0.5, wi = params[1] * 0.5, hh = params[2] * 0.5;
        if (wo <= 0) wo = 1.0;
        if (wi <= 0 || wi >= wo) wi = wo * 0.6;
        Vec3 obL(-wo, -hh, -wo), obR(wo, -hh, -wo), obR2(wo, -hh, wo), obL2(-wo, -hh, wo);
        Vec3 otL(-wo,  hh, -wo), otR(wo,  hh, -wo), otR2(wo,  hh, wo), otL2(-wo,  hh, wo);
        Vec3 ibL(-wi, -hh, -wi), ibR(wi, -hh, -wi), ibR2(wi, -hh, wi), ibL2(-wi, -hh, wi);
        Vec3 itL(-wi,  hh, -wi), itR(wi,  hh, -wi), itR2(wi,  hh, wi), itL2(-wi,  hh, wi);
        // 外侧面（朝外）
        EmitQuad(verts, normals, Vec3(0, 0, -1), obL, obR, otR, otL);
        EmitQuad(verts, normals, Vec3(1, 0, 0), obR, obR2, otR2, otR);
        EmitQuad(verts, normals, Vec3(0, 0, 1), obR2, obL2, otL2, otR2);
        EmitQuad(verts, normals, Vec3(-1, 0, 0), obL2, obL, otL, otL2);
        // 内侧面（朝内）
        EmitQuad(verts, normals, Vec3(0, 0, 1), ibL, itL, itR, ibR);
        EmitQuad(verts, normals, Vec3(-1, 0, 0), ibR, itR, itR2, ibR2);
        EmitQuad(verts, normals, Vec3(0, 0, -1), ibR2, itR2, itL2, ibL2);
        EmitQuad(verts, normals, Vec3(1, 0, 0), ibL2, itL2, itL, ibL);
        // 顶面环（外顶→内顶，法线朝上）
        Vec3 up(0, 1, 0), dn(0, -1, 0);
        EmitQuad(verts, normals, up, otL, otR, itR, itL);
        EmitQuad(verts, normals, up, otR, otR2, itR2, itR);
        EmitQuad(verts, normals, up, otR2, otL2, itL2, itR2);
        EmitQuad(verts, normals, up, otL2, otL, itL, itL2);
        // 底面环（外底→内底，法线朝下）
        EmitQuad(verts, normals, dn, obL, ibL, ibR, obR);
        EmitQuad(verts, normals, dn, obR, ibR, ibR2, obR2);
        EmitQuad(verts, normals, dn, obR2, ibR2, ibL2, obL2);
        EmitQuad(verts, normals, dn, obL2, ibL2, ibL, obL);
        return;
    }
}

// ------------------------------------------------------------ 复制
Object3D* Object3D::CloneDeep(const std::wstring& newName, bool appendSuffix) const {
    Object3D* c = new Object3D();
    c->name = appendSuffix ? newName + L"副本" : newName;
    c->type = type;
    c->visible = visible;
    c->locked = locked;
    c->layer = layer;
    c->r = r; c->g = g; c->b = b;
    for (int i = 0; i < 4; i++) c->params[i] = params[i];
    c->segments = segments;
    c->ctrlPts = ctrlPts;
    c->text = text;
    c->font = font;
    c->editVerts = editVerts;
    c->editTris = editTris;
    c->x = x; c->y = y; c->z = z;
    c->rx = rx; c->ry = ry; c->rz = rz;
    c->sx = sx; c->sy = sy; c->sz = sz;
    // 显示变换必须一并复制：渲染走 DisplayMatrix（读 dx/dy/dz/dsx...），
    // 缺则新对象用默认（原点+单位缩放），导致复制后先显示原始尺寸在原点、拖动后才变正确。
    c->dx = dx; c->dy = dy; c->dz = dz;
    c->drx = drx; c->dry = dry; c->drz = drz;
    c->dsx = dsx; c->dsy = dsy; c->dsz = dsz;
    c->keys = keys;
    for (size_t i = 0; i < children.size(); i++) {
        Object3D* ch = children[i]->CloneDeep(children[i]->name, appendSuffix);
        ch->parent = c;
        c->children.push_back(ch);
    }
    return c;
}

// ---------------------------------------------------------------- Scene3D
Scene3D::Scene3D() {
    fps = 24;
    totalFrames = 120;
    loopPlay = true;
    frame = 0;
    scriptPerFrame = false;
    editRoot = NULL;
    editMeshObj = NULL;
    Layer l;
    l.name = L"图层1";
    layers.push_back(l);
    curLayer = 0;
}

Scene3D::~Scene3D() { Clear(); }

void Scene3D::Clear() {
    for (size_t i = 0; i < roots.size(); i++) delete roots[i];
    roots.clear();
    cam.Reset();
    frame = 0;
    script.clear();
    scriptPerFrame = false;
    projectPath.clear();
    editRoot = NULL;
    editMeshObj = NULL;
    layers.clear();
    {
        Layer l;
        l.name = L"图层1";
        layers.push_back(l);
    }
    curLayer = 0;
}

void Scene3D::Swap(Scene3D& o) {
    roots.swap(o.roots);
    Camera tmpCam = cam; cam = o.cam; o.cam = tmpCam;
    int ti = fps; fps = o.fps; o.fps = ti;
    ti = totalFrames; totalFrames = o.totalFrames; o.totalFrames = ti;
    bool tb = loopPlay; loopPlay = o.loopPlay; o.loopPlay = tb;
    ti = frame; frame = o.frame; o.frame = ti;
    script.swap(o.script);
    tb = scriptPerFrame; scriptPerFrame = o.scriptPerFrame; o.scriptPerFrame = tb;
    docClass.swap(o.docClass);   // 文档类必须随工程交换，否则打开工程后文档类丢失（播放不执行/菜单显示模板）
    projectPath.swap(o.projectPath);
    layers.swap(o.layers);
    ti = curLayer; curLayer = o.curLayer; o.curLayer = ti;
    editMeshObj = NULL;            // 指针不交换：装载工程后旧指针失效，统一置空退出编辑态
    o.editMeshObj = NULL;
}

int Scene3D::CountType(int t) const {
    std::vector<Object3D*> all;
    CollectAll(all);
    int n = 0;
    for (size_t i = 0; i < all.size(); i++)
        if (all[i]->type == t) n++;
    return n;
}

std::wstring Scene3D::AutoName(int t) const {
    const wchar_t* base = Object3D::TypeName(t);
    wchar_t buf[128];
    swprintf(buf, 128, L"%s%d", base, CountType(t) + 1);
    return std::wstring(buf);
}

Object3D* Scene3D::AddObject(int t, const std::wstring& name, Object3D* parent) {
    EnsureLayers();
    Object3D* o = Object3D::CreateByType(t, name);
    o->parent = parent;
    if (parent) parent->children.push_back(o);
    else { o->layer = curLayer; roots.push_back(o); SortRootsByLayer(); }
    return o;
}

void Scene3D::DeleteObject(Object3D* o) {
    if (!o) return;
    if (o == editMeshObj) editMeshObj = NULL;   // 删除的是编辑态元件 → 退出编辑态
    if (o->parent) {
        std::vector<Object3D*>& ch = o->parent->children;
        for (size_t i = 0; i < ch.size(); i++) {
            if (ch[i] == o) { ch.erase(ch.begin() + i); break; }
        }
    } else {
        for (size_t i = 0; i < roots.size(); i++) {
            if (roots[i] == o) { roots.erase(roots.begin() + i); break; }
        }
    }
    delete o;
}

Object3D* Scene3D::DuplicateObject(Object3D* o) {
    if (!o) return NULL;
    Object3D* c = o->CloneDeep(o->name, true);
    c->parent = o->parent;
    c->x += 1.5; c->y += 0.0; c->z += 0.0;
    if (o->parent) o->parent->children.push_back(c);
    else { roots.push_back(c); SortRootsByLayer(); }
    return c;
}

// 打散：把组内的元件移到组外（挂在组原来的父级下），并保留它们在场景中的位置
bool Scene3D::Ungroup(Object3D* g) {
    if (!g || g->type != PT_NONE) return false;
    Object3D* par = g->parent;
    std::vector<Object3D*> kids = g->children;   // 拷贝，迭代中不修改原数组
    if (kids.empty()) {
        DeleteObject(g);
        return true;
    }
    Mat4 gm = g->LocalMatrix();                  // 组相对父空间的变换
    for (size_t i = 0; i < kids.size(); i++) {
        Object3D* c = kids[i];
        // 1) 本地编辑值变换到父空间
        Vec3 p = gm.TransformPoint(Vec3(c->x, c->y, c->z));
        c->x = p.x; c->y = p.y; c->z = p.z;
        c->rx += g->rx; c->ry += g->ry; c->rz += g->rz;
        c->sx *= g->sx; c->sy *= g->sy; c->sz *= g->sz;
        // 2) 关键帧同步变换（保持动画相对位置）
        for (size_t k = 0; k < c->keys.size(); k++) {
            KeyFrame& kf = c->keys[k];
            Vec3 kp = gm.TransformPoint(Vec3(kf.px, kf.py, kf.pz));
            kf.px = kp.x; kf.py = kp.y; kf.pz = kp.z;
            kf.rx += g->rx; kf.ry += g->ry; kf.rz += g->rz;
            kf.sx *= g->sx; kf.sy *= g->sy; kf.sz *= g->sz;
        }
        // 3) 挂到组的父级
        c->parent = par;
        if (par) par->children.push_back(c);
        else { c->layer = curLayer; roots.push_back(c); }
    }
    g->children.clear();
    DeleteObject(g);
    SortRootsByLayer();
    return true;
}

// 从仿射矩阵分解出 位置/旋转(度)/缩放（软件旋转顺序 Rz*Ry*Rx）
static void DecomposeTRS(const Mat4& M, double& px, double& py, double& pz,
                         double& rx, double& ry, double& rz,
                         double& sx, double& sy, double& sz) {
    px = M.m[12]; py = M.m[13]; pz = M.m[14];
    // 列向量（列主序）
    double c0x = M.m[0],  c0y = M.m[1],  c0z = M.m[2];
    double c1x = M.m[4],  c1y = M.m[5],  c1z = M.m[6];
    double c2x = M.m[8],  c2y = M.m[9],  c2z = M.m[10];
    sx = std::sqrt(c0x * c0x + c0y * c0y + c0z * c0z);
    sy = std::sqrt(c1x * c1x + c1y * c1y + c1z * c1z);
    sz = std::sqrt(c2x * c2x + c2y * c2y + c2z * c2z);
    if (sx < 1e-12) sx = 1.0;
    if (sy < 1e-12) sy = 1.0;
    if (sz < 1e-12) sz = 1.0;
    // 归一化旋转部分
    double r00 = c0x / sx, r10 = c0y / sx, r20 = c0z / sx;
    double r01 = c1x / sy, r11 = c1y / sy, r21 = c1z / sy;
    double r02 = c2x / sz, r12 = c2y / sz, r22 = c2z / sz;
    ry = std::asin(-r20);
    double cy = std::cos(ry);
    if (std::fabs(cy) > 1e-9) {
        rz = std::atan2(r10, r00);
        rx = std::atan2(r21, r22);
    } else {
        // 万向锁：仅给 rz（绕 Y 轴 ±90° 的情况）
        rz = std::atan2(-r01, r11);
        rx = 0.0;
    }
    rx *= kRad2Deg; ry *= kRad2Deg; rz *= kRad2Deg;
}

// 组旋转后的子件补偿：直接子件新旋转 = Inverse(组新旋转) * 组旧旋转 * 子件旧旋转，
// 使子件世界朝向保持不变——旋转 movieclip 时成员只绕组原点公转、自身角度不转。
// rotOld 为组旋转变化前的旋转矩阵（调用时组自身 rx/ry/rz 已是新值）。
// 子件关键帧旋转同样补偿，保证每个关键帧的世界朝向都不变。
void Object3D::CompensateChildRotations(const Mat4& rotOld) {
    if (children.empty()) return;
    Mat4 pre = Mat4::RotationXYZ(rx, ry, rz).Inverse().Mul(rotOld);
    double px, py, pz, rxx, ryy, rzz, sx, sy, sz;
    for (size_t i = 0; i < children.size(); i++) {
        Object3D* c = children[i];
        if (!c) continue;
        DecomposeTRS(pre.Mul(Mat4::RotationXYZ(c->rx, c->ry, c->rz)),
                     px, py, pz, rxx, ryy, rzz, sx, sy, sz);
        c->rx = rxx; c->ry = ryy; c->rz = rzz;
        for (size_t k = 0; k < c->keys.size(); k++) {
            KeyFrame& kf = c->keys[k];
            DecomposeTRS(pre.Mul(Mat4::RotationXYZ(kf.rx, kf.ry, kf.rz)),
                         px, py, pz, rxx, ryy, rzz, sx, sy, sz);
            kf.rx = rxx; kf.ry = ryy; kf.rz = rzz;
        }
    }
}

// 把已存在的元件移入组（保持世界位置不变；关键帧位置同步变换）
bool Scene3D::MoveInto(Object3D* o, Object3D* g) {
    if (!o || !g || g->type != PT_NONE) return false;
    if (o == g) return false;
    // 防止把组拖进自己的后代
    for (Object3D* p = g; p; p = p->parent)
        if (p == o) return false;
    // 已直接挂在 g 下则无需操作
    if (o->parent == g) return false;
    // 从旧父移除
    if (o->parent) {
        std::vector<Object3D*>& ch = o->parent->children;
        for (size_t i = 0; i < ch.size(); i++) {
            if (ch[i] == o) { ch.erase(ch.begin() + i); break; }
        }
    } else {
        for (size_t i = 0; i < roots.size(); i++) {
            if (roots[i] == o) { roots.erase(roots.begin() + i); break; }
        }
    }
    // 新本地 = Inverse(组世界) * 元件世界
    Mat4 M = g->WorldMatrix().Inverse().Mul(o->WorldMatrix());
    double px, py, pz, rx, ry, rz, sx, sy, sz;
    DecomposeTRS(M, px, py, pz, rx, ry, rz, sx, sy, sz);
    o->x = px; o->y = py; o->z = pz;
    o->rx = rx; o->ry = ry; o->rz = rz;
    o->sx = sx; o->sy = sy; o->sz = sz;
    // 关键帧位置同步变换（旋转/缩放沿用旧值；组无旋转缩放时完全精确）
    for (size_t k = 0; k < o->keys.size(); k++) {
        KeyFrame& kf = o->keys[k];
        Vec3 kp = M.TransformPoint(Vec3(kf.px, kf.py, kf.pz));
        kf.px = kp.x; kf.py = kp.y; kf.pz = kp.z;
    }
    o->parent = g;
    g->children.push_back(o);
    return true;
}

// 仅改父级、保持元件全部参数（位置/旋转/缩放/关键帧）不变。
// 用于“转换为元件”：新组总是与子元件同层且变换恒等，子元件参数无需换算。
void Scene3D::Reparent(Object3D* o, Object3D* g) {
    if (!o || !g || g->type != PT_NONE || o == g) return;
    for (Object3D* p = g; p; p = p->parent)
        if (p == o) return;   // 防止移入自己的后代
    if (o->parent == g) return;
    // 从旧父移除
    if (o->parent) {
        std::vector<Object3D*>& ch = o->parent->children;
        for (size_t i = 0; i < ch.size(); i++) {
            if (ch[i] == o) { ch.erase(ch.begin() + i); break; }
        }
    } else {
        for (size_t i = 0; i < roots.size(); i++) {
            if (roots[i] == o) { roots.erase(roots.begin() + i); break; }
        }
    }
    o->parent = g;
    g->children.push_back(o);
}

// 调整同一父级下元件的绘制顺序（时间轴层级）。时间轴最上面的行 = 最上层（遮挡下层）。
// dir: -1 上移一层，1 下移一层，2 置顶（最上层），3 置底（最底层）
// 置顶/置底用“取出后插入”而非交换：原最上层依次后移，符合 Flash 层级习惯。
bool Scene3D::MoveLayer(Object3D* o, int dir) {
    if (!o) return false;
    std::vector<Object3D*>* v = o->parent ? &o->parent->children : &roots;
    int idx = -1;
    for (size_t i = 0; i < v->size(); i++)
        if ((*v)[i] == o) { idx = (int)i; break; }
    if (idx < 0) return false;
    int n = (int)v->size();
    if (dir == 2) {            // 置顶：移到最前，其余依次后移
        if (idx == 0) return false;
        v->erase(v->begin() + idx);
        v->insert(v->begin(), o);
        return true;
    }
    if (dir == 3) {            // 置底：移到最后，其余依次前移
        if (idx == n - 1) return false;
        v->erase(v->begin() + idx);
        v->push_back(o);
        return true;
    }
    int ni = idx + dir;        // dir=-1 上移一位(向数组前=更上层) / dir=1 下移一位
    if (ni < 0 || ni >= n) return false;
    std::swap((*v)[idx], (*v)[ni]);
    return true;
}

void Scene3D::EnsureLayers() {
    if (layers.empty()) {
        Layer l;
        l.name = L"图层1";
        layers.push_back(l);
    }
    for (size_t i = 0; i < layers.size(); i++) {
        if (layers[i].name.empty()) {
            wchar_t b[32];
            swprintf(b, 32, L"图层%d", (int)i + 1);
            layers[i].name = b;
        }
    }
    if (curLayer < 0) curLayer = 0;
    if (curLayer >= (int)layers.size()) curLayer = (int)layers.size() - 1;
}

int Scene3D::RootLayerOf(const Object3D* o) const {
    if (!o) return 0;
    const Object3D* r = o;
    while (r->parent) r = r->parent;
    int li = r->layer;
    if (li < 0 || (size_t)li >= layers.size()) li = 0;
    return li;
}

bool Scene3D::LayerVisible(const Object3D* o) const {
    if (layers.empty()) return true;
    return layers[RootLayerOf(o)].visible;
}

bool Scene3D::LayerLocked(const Object3D* o) const {
    if (layers.empty()) return false;
    return layers[RootLayerOf(o)].locked;
}

bool Scene3D::ObjectVisible(const Object3D* o) const {
    if (!o || !o->ChainVisible() || !LayerVisible(o)) return false;
    // 图层关键帧决定内容：该层在当前帧只显示当前关键帧内容里的元件（支持不同关键帧不同元件集合）
    int li = RootLayerOf(o);
    if (li >= 0 && li < (int)layers.size() && !LayerContainsAt(li, o, frame)) return false;
    return true;
}

bool Scene3D::ObjectEditable(const Object3D* o) const {
    return o && ObjectVisible(o) && !o->locked && !LayerLocked(o);
}

bool Scene3D::SetCurrentLayer(int idx) {
    EnsureLayers();
    if (idx < 0 || idx >= (int)layers.size()) return false;
    if (curLayer == idx) return false;
    curLayer = idx;
    return true;
}

bool Scene3D::MoveToLayer(Object3D* o, int layerIdx) {
    EnsureLayers();
    if (!o || o->parent) return false;
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return false;
    if (o->layer == layerIdx) return false;
    o->layer = layerIdx;
    SortRootsByLayer();
    return true;
}

bool Scene3D::MoveLayerOrder(int layerIdx, int dir) {
    EnsureLayers();
    int n = (int)layers.size();
    if (layerIdx < 0 || layerIdx >= n) return false;
    int src = layerIdx, dst = -1;
    if (dir == 2) { if (src == 0) return false; dst = 0; }
    else if (dir == 3) { if (src == n - 1) return false; dst = n - 1; }
    else { dst = src + dir; if (dst < 0 || dst >= n) return false; }
    int oldCur = curLayer;
    Layer t = layers[src];
    layers.erase(layers.begin() + src);
    layers.insert(layers.begin() + dst, t);
    if (oldCur == src) curLayer = dst;
    else if (src < oldCur && dst >= oldCur) curLayer = oldCur - 1;
    else if (src > oldCur && dst <= oldCur) curLayer = oldCur + 1;
    else curLayer = oldCur;
    SortRootsByLayer();
    return true;
}

int Scene3D::AddLayer() {
    EnsureLayers();
    int k = 1;
    for (;;) {
        wchar_t b[32];
        swprintf(b, 32, L"图层%d", k);
        bool dup = false;
        for (size_t i = 0; i < layers.size(); i++)
            if (layers[i].name == b) { dup = true; break; }
        if (!dup) {
            Layer l;
            l.name = b;
            layers.push_back(l);
            curLayer = (int)layers.size() - 1;
            return curLayer;
        }
        k++;
    }
}

bool Scene3D::DeleteLayer(int idx) {
    EnsureLayers();
    if (idx < 0 || idx >= (int)layers.size()) return false;
    if ((int)layers.size() <= 1) return false;
    for (size_t i = 0; i < roots.size(); i++)
        if (RootLayerOf(roots[i]) == idx) return false;
    layers.erase(layers.begin() + idx);
    for (size_t i = 0; i < roots.size(); i++) {
        Object3D* r = roots[i];
        while (r->parent) r = r->parent;
        if (r->layer > idx) r->layer--;
    }
    if (curLayer >= (int)layers.size()) curLayer = (int)layers.size() - 1;
    if (curLayer < 0) curLayer = 0;
    return true;
}

void Scene3D::SortRootsByLayer() {
    EnsureLayers();
    std::vector<Object3D*> sorted;
    for (size_t li = 0; li < layers.size(); li++) {
        for (size_t i = 0; i < roots.size(); i++) {
            Object3D* o = roots[i];
            int l = o->layer;
            if (l < 0 || (size_t)l >= layers.size()) l = 0;
            if ((size_t)l == li) sorted.push_back(o);
        }
    }
    roots = sorted;
}

Object3D* Scene3D::FindByName(const std::wstring& n) const {
    std::vector<Object3D*> all;
    CollectAll(all);
    for (size_t i = 0; i < all.size(); i++)
        if (all[i]->name == n) return all[i];
    return NULL;
}

void Scene3D::CollectAll(std::vector<Object3D*>& out) const {
    out.clear();
    for (size_t i = 0; i < roots.size(); i++) {
        // 深度优先
        std::vector<Object3D*> stack;
        stack.push_back(roots[i]);
        while (!stack.empty()) {
            Object3D* o = stack.back();
            stack.pop_back();
            out.push_back(o);
            for (size_t j = o->children.size(); j > 0; j--)
                stack.push_back(o->children[j - 1]);
        }
    }
}

bool Scene3D::ContainsObject(const Object3D* o) const {
    if (!o) return false;
    std::vector<Object3D*> all;
    CollectAll(all);
    for (size_t i = 0; i < all.size(); i++)
        if (all[i] == o) return true;
    return false;
}

void Scene3D::EditChildren(std::vector<Object3D*>& out) const {
    out.clear();
    if (editRoot) {
        out = editRoot->children;
    } else {
        out = roots;
    }
}

bool Scene3D::IsDirectEditChild(const Object3D* o) const {
    std::vector<Object3D*> kids;
    EditChildren(kids);
    for (size_t i = 0; i < kids.size(); i++)
        if (kids[i] == o) return true;
    return false;
}

// ---- Flash 式图层关键帧（整层驱动） ----
bool Scene3D::LayerHasKeyAt(int layerIdx, int frame) const {
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return false;
    const std::vector<LayerKey>& keys = layers[layerIdx].keys;
    for (size_t i = 0; i < keys.size(); i++)
        if (keys[i].frame == frame) return true;
    return false;
}

bool Scene3D::LayerBlankAt(int layerIdx, int frame) const {
    // 空关键帧区间：该帧之前最近的图层关键帧是空关键帧（objs 为空），或还没有任何关键帧但存在关键帧。
    // 无任何关键帧 = 编辑态（该层元件正常显示），返回 false。
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return false;
    const std::vector<LayerKey>& keys = layers[layerIdx].keys;
    if (keys.empty()) return false;
    const LayerKey* k0 = NULL;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame <= frame) k0 = &keys[i];
        else break;
    }
    if (k0) return k0->objs.empty();   // 最近关键帧是空关键帧 → 该帧该层无元件
    return true;                        // 在第一个关键帧之前 → Flash 语义该层空白
}

bool Scene3D::LayerContainsAt(int layerIdx, const Object3D* o, int frame) const {
    // 该层在帧 frame 是否显示元件 o：无关键帧=编辑态全部显示；
    // 有关键帧则只看当前关键帧区间（k0）的内容是否含 o（的根）——支持不同关键帧承载不同元件集合。
    if (!o) return false;
    const Object3D* r = o;
    while (r->parent) r = r->parent;
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return true;
    const std::vector<LayerKey>& keys = layers[layerIdx].keys;
    if (keys.empty()) return true;                    // 编辑态：显示
    const LayerKey* k0 = NULL;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame <= frame) k0 = &keys[i];
        else break;
    }
    if (!k0) return false;                            // 首关键帧之前：空白
    if (k0->objs.empty()) return false;               // 空关键帧：无元件
    for (size_t j = 0; j < k0->objs.size(); j++)
        if (k0->objs[j] == r) return true;
    return false;                                     // 该元件不属于当前关键帧内容
}

int Scene3D::CurrentIntervalKeyFrame(int layerIdx, int frame) const {
    if (layerIdx < 0 || layerIdx >= (int)layers.size() || layers[layerIdx].keys.empty()) return -1;
    const std::vector<LayerKey>& keys = layers[layerIdx].keys;
    int best = -1;
    for (size_t i = 0; i < keys.size(); i++) { if (keys[i].frame <= frame) best = keys[i].frame; else break; }
    if (best < 0) return keys[0].frame;
    return best;
}

void Scene3D::RemoveObjectFromAllLayerKeys(Object3D* o) {
    if (!o) return;
    Object3D* r = o; while (r->parent) r = r->parent;
    for (size_t li = 0; li < layers.size(); li++) {
        std::vector<LayerKey>& keys = layers[li].keys;
        for (size_t i = 0; i < keys.size(); i++) {
            for (size_t j = 0; j < keys[i].objs.size(); j++) {
                if (keys[i].objs[j] == r) {
                    keys[i].objs.erase(keys[i].objs.begin() + j);
                    if (j < keys[i].snaps.size()) keys[i].snaps.erase(keys[i].snaps.begin() + j);
                    j--;
                }
            }
        }
    }
}

void Scene3D::InsertLayerKey(int layerIdx, int frame) {
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return;
    if (frame < 0) frame = 0;
    // 收集该图层"在当前帧 frame 可见"的根元件（所见即所得 + 尊重空关键帧切断）：
    // 只记录该帧区间实际显示的内容，被空关键帧切断的元件（如第40帧空白后第20帧的立方体/球）不会被带出。
    std::vector<Object3D*> collect;
    for (size_t i = 0; i < roots.size(); i++) {
        Object3D* r = roots[i];
        if (!r || r->parent) continue;
        if (r->layer != layerIdx) continue;
        if (LayerContainsAt(layerIdx, r, frame)) collect.push_back(r);
    }
    LayerKey lk;
    lk.frame = frame;
    for (size_t i = 0; i < collect.size(); i++) {
        lk.objs.push_back(collect[i]);
        KeyFrame kf; collect[i]->CopyLocalToKey(kf);
        lk.snaps.push_back(kf);
    }
    std::vector<LayerKey>& keys = layers[layerIdx].keys;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame == frame) { keys[i] = lk; return; }   // 已存在则整体覆盖替换
    }
    keys.push_back(lk);
    // 按 frame 升序插入
    for (size_t i = keys.size() - 1; i > 0; i--) {
        if (keys[i].frame < keys[i - 1].frame) {
            LayerKey tmp = keys[i]; keys[i] = keys[i - 1]; keys[i - 1] = tmp;
        } else break;
    }
}

// 添加新元件（新增/复制/移入图层）时的关键帧归属：
// 无关键帧=编辑态全显示不处理；实心关键帧区间→加入该关键帧；空白区间（空关键帧之后或首关键帧前）→在当前帧新建仅含该元件的关键帧，不填充前面的空关键帧
void Scene3D::AddNewObjectToCurrentKey(Object3D* o) {
    if (!o || o->parent) return;
    int li = o->layer;
    if (li < 0 || li >= (int)layers.size()) return;
    int f = frame;
    if (f < 0) f = 0;
    const std::vector<LayerKey>& keys = layers[li].keys;
    if (keys.empty()) return;
    int k0idx = -1;
    for (size_t i = 0; i < keys.size(); i++) { if (keys[i].frame <= f) k0idx = (int)i; else break; }
    if (k0idx >= 0 && !keys[k0idx].objs.empty()) { AddObjectToLayerKey(li, keys[k0idx].frame, o); return; }
    Object3D* r = o; while (r->parent) r = r->parent;
    for (size_t i = 0; i < layers[li].keys.size(); i++) { if (layers[li].keys[i].frame == f) { AddObjectToLayerKey(li, f, r); return; } }
    LayerKey lk; lk.frame = f; lk.objs.push_back(r);
    KeyFrame kf; r->CopyLocalToKey(kf); lk.snaps.push_back(kf);
    layers[li].keys.push_back(lk);
    for (size_t i = layers[li].keys.size() - 1; i > 0; i--) {
        if (layers[li].keys[i].frame < layers[li].keys[i - 1].frame) {
            LayerKey tmp = layers[li].keys[i]; layers[li].keys[i] = layers[li].keys[i - 1]; layers[li].keys[i - 1] = tmp;
        } else break;
    }
}

void Scene3D::InsertEmptyLayerKey(int layerIdx, int frame) {
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return;
    if (frame < 0) frame = 0;
    LayerKey lk;             // objs/snaps 为空 → 空关键帧（该帧该层无元件）
    lk.frame = frame;
    std::vector<LayerKey>& keys = layers[layerIdx].keys;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame == frame) { keys[i] = lk; return; }   // 同帧则覆盖为空帧
    }
    keys.push_back(lk);
    for (size_t i = keys.size() - 1; i > 0; i--) {
        if (keys[i].frame < keys[i - 1].frame) {
            LayerKey tmp = keys[i]; keys[i] = keys[i - 1]; keys[i - 1] = tmp;
        } else break;
    }
}

bool Scene3D::AddObjectToLayerKey(int layerIdx, int frame, Object3D* o) {
    if (layerIdx < 0 || layerIdx >= (int)layers.size() || !o) return false;
    Object3D* r = o; while (r->parent) r = r->parent;
    if (frame < 0) frame = 0;
    std::vector<LayerKey>& keys = layers[layerIdx].keys;
    LayerKey* k = NULL;
    for (size_t i = 0; i < keys.size(); i++) if (keys[i].frame == frame) { k = &keys[i]; break; }
    if (!k) {
        LayerKey lk; lk.frame = frame; lk.objs.push_back(r);
        KeyFrame kf; r->CopyLocalToKey(kf); lk.snaps.push_back(kf);
        keys.push_back(lk);
        for (size_t i = keys.size() - 1; i > 0; i--) {
            if (keys[i].frame < keys[i - 1].frame) { LayerKey tmp = keys[i]; keys[i] = keys[i - 1]; keys[i - 1] = tmp; }
            else break;
        }
        return true;
    }
    for (size_t i = 0; i < k->objs.size(); i++) if (k->objs[i] == r) return true;
    k->objs.push_back(r);
    KeyFrame kf; r->CopyLocalToKey(kf); k->snaps.push_back(kf);
    return true;
}

bool Scene3D::RemoveObjectFromLayerKey(int layerIdx, int frame, Object3D* o) {
    if (layerIdx < 0 || layerIdx >= (int)layers.size() || !o) return false;
    Object3D* r = o; while (r->parent) r = r->parent;
    std::vector<LayerKey>& keys = layers[layerIdx].keys;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame != frame) continue;
        for (size_t j = 0; j < keys[i].objs.size(); j++) {
            if (keys[i].objs[j] == r) {
                keys[i].objs.erase(keys[i].objs.begin() + j);
                if (j < keys[i].snaps.size()) keys[i].snaps.erase(keys[i].snaps.begin() + j);
                return true;
            }
        }
    }
    return false;
}

void Scene3D::RemoveLayerKey(int layerIdx, int frame) {
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return;
    std::vector<LayerKey>& keys = layers[layerIdx].keys;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame == frame) { keys.erase(keys.begin() + i); return; }
    }
}

void Scene3D::MoveLayerKey(int layerIdx, int oldFrame, int newFrame) {
    if (layerIdx < 0 || layerIdx >= (int)layers.size()) return;
    if (newFrame < 0) newFrame = 0;
    std::vector<LayerKey>& keys = layers[layerIdx].keys;
    size_t idx = (size_t)-1;
    for (size_t i = 0; i < keys.size(); i++)
        if (keys[i].frame == oldFrame) { idx = i; break; }
    if (idx == (size_t)-1) return;   // oldFrame 不存在则忽略
    LayerKey lk = keys[idx];        // 快照值原样保留
    keys.erase(keys.begin() + idx);
    lk.frame = newFrame;
    // 删除目标帧已有同帧关键帧（防御：newFrame 已有则覆盖）
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].frame == newFrame) { keys.erase(keys.begin() + i); break; }
    }
    keys.push_back(lk);
    for (size_t i = keys.size() - 1; i > 0; i--) {
        if (keys[i].frame < keys[i - 1].frame) {
            LayerKey tmp = keys[i]; keys[i] = keys[i - 1]; keys[i - 1] = tmp;
        } else break;
    }
}

int Scene3D::LastLayerKeyFrame() const {
    int last = -1;
    for (size_t li = 0; li < layers.size(); li++)
        for (size_t i = 0; i < layers[li].keys.size(); i++)
            if (layers[li].keys[i].frame > last) last = layers[li].keys[i].frame;
    return last;
}

void Scene3D::UpdateLayerKeySnap(Object3D* o, int frame) {
    if (!o) return;
    // 取根元件
    Object3D* r = o;
    while (r->parent) r = r->parent;
    int li = RootLayerOf(r);
    if (li < 0 || li >= (int)layers.size()) return;
    std::vector<LayerKey>& keys = layers[li].keys;
    for (size_t k = 0; k < keys.size(); k++) {
        if (keys[k].frame != frame) continue;
        LayerKey& lk = keys[k];
        for (size_t j = 0; j < lk.objs.size(); j++) {
            if (lk.objs[j] == r) {
                if (j < lk.snaps.size()) r->CopyLocalToKey(lk.snaps[j]);
                return;
            }
        }
        // 该帧关键帧存在但快照无此元件（插入快照后新拖入该层）→ 末尾追加
        KeyFrame kf;
        r->CopyLocalToKey(kf);
        lk.objs.push_back(r);
        lk.snaps.push_back(kf);
        return;
    }
    // 无该帧关键帧则忽略
}

void Scene3D::SetFrame(int f) {
    if (f < 0) f = 0;
    if (f >= totalFrames) f = totalFrames - 1;
    frame = f;
    // 图层驱动：关键帧属于图层，作用于该图层所有根元件（及其子树）
    for (size_t li = 0; li < layers.size(); li++) {
        Layer& layer = layers[li];
        if (layer.keys.empty()) {
            // 该层无关键帧：元件保持本地当前值（显示值不同步则对齐，不写回）
            // 脚本逐帧模式（scriptPerFrame）下完全保留脚本写入的显示值（dx/dy/dz 等），
            // 不做任何写回——否则松开按键/脚本不写的那帧 dx 会被拉回初始值（"动一下又退回"）
            if (!scriptPerFrame) {
                for (size_t i = 0; i < roots.size(); i++) {
                    Object3D* o = roots[i];
                    if (!o || RootLayerOf(o) != (int)li) continue;
                    o->SyncDisplayToLocal();
                }
            }
            continue;
        }
        // 找前后关键帧：k0 = frame<=f 的最近一个，k1 = frame>f 的第一个
        const LayerKey* k0 = NULL;
        const LayerKey* k1 = NULL;
        for (size_t j = 0; j < layer.keys.size(); j++) {
            if (layer.keys[j].frame <= f) k0 = &layer.keys[j];
            else { k1 = &layer.keys[j]; break; }
        }
        if (!k0) k0 = k1;
        if (!k1) k1 = k0;
        // 关键帧脚本（Flash 帧动作）：最近关键帧挂有脚本 → 该帧起位置由脚本控制，
        // 跳过插值写回（保持脚本写入的显示值），直到下一个无脚本的关键帧
        if (k0->script.empty() == false) continue;
        bool freeze = (k0 == k1) || (k0->frame == k1->frame);
        // 对该层每个快照项插值写回（跳过已删除/已移层/已移入组的元件）
        for (size_t i = 0; i < k0->objs.size() && i < k0->snaps.size(); i++) {
            Object3D* o = k0->objs[i];
            if (!o || !ContainsObject(o) || o->parent != NULL || RootLayerOf(o) != (int)li) continue;
            const KeyFrame& a = k0->snaps[i];
            if (freeze) {
                o->CopyKeyToLocal(a);
            } else {
                // 在 k1->objs 中找 o 的对应快照 b
                const KeyFrame* pb = NULL;
                for (size_t j = 0; j < k1->objs.size() && j < k1->snaps.size(); j++) {
                    if (k1->objs[j] == o) { pb = &k1->snaps[j]; break; }
                }
                if (!pb) {
                    o->CopyKeyToLocal(a);
                } else {
                    double t = (double)(f - k0->frame) / (double)(k1->frame - k0->frame);
                    // smoothstep 缓动
                    t = t * t * (3.0 - 2.0 * t);
                    o->x = a.px + (pb->px - a.px) * t;
                    o->y = a.py + (pb->py - a.py) * t;
                    o->z = a.pz + (pb->pz - a.pz) * t;
                    o->rx = a.rx + (pb->rx - a.rx) * t;
                    o->ry = a.ry + (pb->ry - a.ry) * t;
                    o->rz = a.rz + (pb->rz - a.rz) * t;
                    o->sx = a.sx + (pb->sx - a.sx) * t;
                    o->sy = a.sy + (pb->sy - a.sy) * t;
                    o->sz = a.sz + (pb->sz - a.sz) * t;
                }
            }
            // 脚本逐帧模式（scriptPerFrame）下：关键帧插值只写本地 x，不 SyncDisplayToLocal——
            // 显示值由脚本每帧决定，避免把脚本写入的 dx 拉回
            if (!scriptPerFrame) o->SyncDisplayToLocal();
        }
    }
}

void Scene3D::SyncDisplayToLocalAll() {
    std::vector<Object3D*> all;
    CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) all[i]->SyncDisplayToLocal();
}

void Scene3D::CopyLocalFromDisplayAll() {
    std::vector<Object3D*> all;
    CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) all[i]->CopyLocalFromDisplay();
}

// ------------------------------------------------------------ 拾取
bool RayTriangleHit(const Vec3& o, const Vec3& d,
                    const Vec3& a, const Vec3& b, const Vec3& c, double& t) {
    Vec3 e1 = b - a, e2 = c - a;
    Vec3 p = Cross(d, e2);
    double det = Dot(e1, p);
    if (std::fabs(det) < 1e-9) return false;
    double inv = 1.0 / det;
    Vec3 s = o - a;
    double u = Dot(s, p) * inv;
    if (u < 0.0 || u > 1.0) return false;
    Vec3 q = Cross(s, e1);
    double v = Dot(d, q) * inv;
    if (v < 0.0 || u + v > 1.0) return false;
    double tt = Dot(e2, q) * inv;
    if (tt < 1e-6) return false;
    t = tt;
    return true;
}

bool RayTriangleHit(const Vec3& o, const Vec3& d,
                    const Vec3& v0, const Vec3& v1, const Vec3& v2, double& t);
bool RayAABB(const Vec3& o, const Vec3& d, const Vec3& mn, const Vec3& mx, double& t);

bool Scene3D::PickObject(const Vec3& rayOrg, const Vec3& rayDir, Object3D*& hit, double& tmin) const {
    hit = NULL;
    tmin = 0.0;
    // 只拾取"当前编辑层级"的直接子级：
    //  - 图元：三角形精确拾取
    //  - 组：按整组包围盒拾取（作为整体，不进入其子元件）
    // 在场景根层编辑时，命中的是顶层元件（组合后就拖整个组，不会误选组内子元件）；
    // 双击进入某个组后，才能拾取其内部的子元件。
    std::vector<Object3D*> kids;
    EditChildren(kids);
    double best = 1e18;
    std::vector<Vec3> verts, normals;
    for (size_t i = 0; i < kids.size(); i++) {
        Object3D* o = kids[i];
        if (!ObjectVisible(o) || o->locked || LayerLocked(o)) continue;
        if (Object3D::IsPrimitive(o->type)) {
            Mat4 inv = o->DisplayWorldMatrix().Inverse();
            Vec3 ro = inv.TransformPoint(rayOrg);
            Vec3 rd = inv.TransformDir(rayDir);
            double rl = Length(rd);
            if (rl < 1e-12) continue;
            rd = rd * (1.0 / rl);
            o->GenerateMesh(verts, normals);
            for (size_t j = 0; j + 2 < verts.size(); j += 3) {
                double t;
                if (RayTriangleHit(ro, rd, verts[j], verts[j + 1], verts[j + 2], t)) {
                    Vec3 hitL = ro + rd * t;
                    Vec3 hitW = o->DisplayWorldMatrix().TransformPoint(hitL);
                    double dist2 = Dot(hitW - rayOrg, hitW - rayOrg);
                    if (dist2 < best) {
                        best = dist2;
                        hit = o;
                        tmin = std::sqrt(dist2);
                    }
                }
            }
        }
    }
    // 无图元命中时，拾取当前层的组（包围盒，作为整体）
    if (hit == NULL) {
        double bestGroup = 1e18;
        Object3D* hitGroup = NULL;
        for (size_t i = 0; i < kids.size(); i++) {
            Object3D* g = kids[i];
            if (!ObjectVisible(g) || Object3D::IsPrimitive(g->type) || !g->HasChildren() || g->locked || LayerLocked(g)) continue;
            Vec3 mn, mx;
            if (!g->GroupLocalAABB(mn, mx)) continue;
            Mat4 inv = g->DisplayWorldMatrix().Inverse();
            Vec3 ro = inv.TransformPoint(rayOrg);
            Vec3 rd = inv.TransformDir(rayDir);
            double rl = Length(rd);
            if (rl < 1e-12) continue;
            rd = rd * (1.0 / rl);
            double t;
            if (RayAABB(ro, rd, mn, mx, t) && t >= 0) {
                Vec3 hitL = ro + rd * t;
                Vec3 hitW = g->DisplayWorldMatrix().TransformPoint(hitL);
                double dist2 = Dot(hitW - rayOrg, hitW - rayOrg);
                if (dist2 < bestGroup) {
                    bestGroup = dist2;
                    hitGroup = g;
                }
            }
        }
        if (hitGroup) {
            hit = hitGroup;
            tmin = std::sqrt(bestGroup);
        }
    }
    return hit != NULL;
}

bool Object3D::PrimitiveLocalBounds(const Object3D* o, Vec3& mn, Vec3& mx) {
    if (!o || !IsPrimitive(o->type)) return false;
    double hx = 0, hy = 0, hz = 0;
    switch (o->type) {
        case PT_BOX:     hx = o->params[0] * 0.5; hy = o->params[1] * 0.5; hz = o->params[2] * 0.5; break;
        case PT_SPHERE:  hx = hy = hz = o->params[0]; break;
        case PT_CYLINDER:hx = hz = o->params[0]; hy = o->params[1] * 0.5; break;
        case PT_CONE:    hx = hz = o->params[0]; hy = o->params[1] * 0.5; break;
        case PT_TORUS:   hx = hz = o->params[0] + o->params[1]; hy = o->params[0] + o->params[1]; break;
        case PT_PLANE:   hx = hz = o->params[0] * 0.5; hy = 0; break;
        case PT_SPLINE: {
            // 控制点最小/最大 ± 半径
            double rad = o->params[0]; if (rad <= 0) rad = 0.12;
            Vec3 lo(1e18, 1e18, 1e18), hi(-1e18, -1e18, -1e18);
            if (o->ctrlPts.empty()) { lo = hi = Vec3(0, 0, 0); }
            for (size_t k = 0; k < o->ctrlPts.size(); k++) {
                const Vec3& p = o->ctrlPts[k];
                if (p.x < lo.x) lo.x = p.x; if (p.x > hi.x) hi.x = p.x;
                if (p.y < lo.y) lo.y = p.y; if (p.y > hi.y) hi.y = p.y;
                if (p.z < lo.z) lo.z = p.z; if (p.z > hi.z) hi.z = p.z;
            }
            mn = Vec3(lo.x - rad, lo.y - rad, lo.z - rad);
            mx = Vec3(hi.x + rad, hi.y + rad, hi.z + rad);
            return true;
        }
        case PT_LEAF:    hx = o->params[0]; hy = o->params[1]; hz = o->params[2]; break;
        case PT_TUBE:    hx = hz = o->params[0] * 0.5; hy = o->params[2] * 0.5; break;
        case PT_RTUBE:   hx = hz = o->params[0]; hy = o->params[2] * 0.5; break;
        case PT_BOOLEAN: {
            if (o->_meshValid && !o->_meshVerts.empty()) {
                Vec3 lo(1e18, 1e18, 1e18), hi(-1e18, -1e18, -1e18);
                for (size_t k = 0; k < o->_meshVerts.size(); k++) {
                    const Vec3& p = o->_meshVerts[k];
                    if (p.x < lo.x) lo.x = p.x; if (p.x > hi.x) hi.x = p.x;
                    if (p.y < lo.y) lo.y = p.y; if (p.y > hi.y) hi.y = p.y;
                    if (p.z < lo.z) lo.z = p.z; if (p.z > hi.z) hi.z = p.z;
                }
                mn = lo; mx = hi;
                return true;
            }
            hx = o->params[0] * 0.5; hy = o->params[1] * 0.5; hz = o->params[2] * 0.5;
            break;
        }
        case PT_TEXT3D: {
            // 用缓存网格求实际 AABB（渲染已填缓存）；未生成时按字高占位
            if (o->_meshValid && !o->_meshVerts.empty()) {
                Vec3 lo(1e18, 1e18, 1e18), hi(-1e18, -1e18, -1e18);
                for (size_t k = 0; k < o->_meshVerts.size(); k++) {
                    const Vec3& p = o->_meshVerts[k];
                    if (p.x < lo.x) lo.x = p.x; if (p.x > hi.x) hi.x = p.x;
                    if (p.y < lo.y) lo.y = p.y; if (p.y > hi.y) hi.y = p.y;
                    if (p.z < lo.z) lo.z = p.z; if (p.z > hi.z) hi.z = p.z;
                }
                mn = lo; mx = hi;
                return true;
            }
            hx = (o->text.size() > 0 ? (double)o->text.size() : 1.0) * (o->params[1] > 0 ? o->params[1] : 2.0) * 0.5;
            hy = (o->params[0] > 0 ? o->params[0] : 0.3) * 0.5;   // 厚度沿 Y
            hz = (o->params[1] > 0 ? o->params[1] : 2.0) * 0.5;   // 字高沿 Z
            break;
        }
        default: return false;
    }
    mn = Vec3(-hx, -hy, -hz);
    mx = Vec3( hx,  hy,  hz);
    return true;
}

bool Object3D::GroupLocalAABB(Vec3& mn, Vec3& mx) const {
    bool any = false;
    for (size_t i = 0; i < children.size(); i++) {
        const Object3D* ch = children[i];
        if (!ch->ChainVisible()) continue;
        Vec3 cmn, cmx;
        if (Object3D::IsPrimitive(ch->type)) {
            if (!Object3D::PrimitiveLocalBounds(ch, cmn, cmx)) continue;
        } else {
            if (!ch->GroupLocalAABB(cmn, cmx)) continue;
        }
        Mat4 m = ch->DisplayMatrix();   // 子元件相对当前组局部坐标的变换
        for (int k = 0; k < 8; k++) {
            Vec3 c((k & 1) ? cmx.x : cmn.x,
                   (k & 2) ? cmx.y : cmn.y,
                   (k & 4) ? cmx.z : cmn.z);
            Vec3 w = m.TransformPoint(c);
            if (!any) { mn = mx = w; any = true; }
            else {
                if (w.x < mn.x) mn.x = w.x; if (w.x > mx.x) mx.x = w.x;
                if (w.y < mn.y) mn.y = w.y; if (w.y > mx.y) mx.y = w.y;
                if (w.z < mn.z) mn.z = w.z; if (w.z > mx.z) mx.z = w.z;
            }
        }
    }
    return any;
}

bool Object3D::GroupWorldAABB(const Object3D* g, Vec3& mn, Vec3& mx) {
    bool any = false;
    // 用与渲染完全一致的 WorldMatrix（局部值链）计算所有后代图元的世界包围盒
    std::vector<const Object3D*> stack;
    stack.push_back(g);
    while (!stack.empty()) {
        const Object3D* o = stack.back();
        stack.pop_back();
        for (size_t i = 0; i < o->children.size(); i++) {
            const Object3D* ch = o->children[i];
            if (!ch->ChainVisible()) continue;
            if (IsPrimitive(ch->type)) {
                // 直接用实际渲染网格顶点取世界 AABB，与渲染完全一致
                // （低细分数图元如四棱锥不能用“轴对齐 ±radius 盒”近似，否则旋转后包围盒偏大）
                std::vector<Vec3> verts, normals;
                ch->GenerateMesh(verts, normals);
                if (verts.empty()) continue;
                Mat4 wm = ch->DisplayWorldMatrix();
                for (size_t v = 0; v < verts.size(); v++) {
                    Vec3 w = wm.TransformPoint(verts[v]);
                    if (!any) { mn = mx = w; any = true; }
                    else {
                        if (w.x < mn.x) mn.x = w.x; if (w.x > mx.x) mx.x = w.x;
                        if (w.y < mn.y) mn.y = w.y; if (w.y > mx.y) mx.y = w.y;
                        if (w.z < mn.z) mn.z = w.z; if (w.z > mx.z) mx.z = w.z;
                    }
                }
            } else {
                stack.push_back(ch);   // 递归嵌套组
            }
        }
    }
    return any;
}

bool RayAABB(const Vec3& o, const Vec3& d, const Vec3& mn, const Vec3& mx, double& t) {
    double tmin = 0.0, tmax = 1e18;
    for (int i = 0; i < 3; i++) {
        double oi = (i == 0) ? o.x : (i == 1 ? o.y : o.z);
        double di = (i == 0) ? d.x : (i == 1 ? d.y : d.z);
        double mni = (i == 0) ? mn.x : (i == 1 ? mn.y : mn.z);
        double mxi = (i == 0) ? mx.x : (i == 1 ? mx.y : mx.z);
        if (std::fabs(di) < 1e-12) {
            if (oi < mni || oi > mxi) return false;
        } else {
            double t1 = (mni - oi) / di, t2 = (mxi - oi) / di;
            if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    t = tmin;
    return true;
}

// ------------------------------------------------------------ 工具
void Scene3D::SetStandingY(Object3D* o) {
    if (!o || !Object3D::IsPrimitive(o->type)) return;
    // 用真实网格顶点取局部最低点（比轴对齐包围盒更精确，圆环等低细分数图元不会误判），
    // 把最低点抬到 y=0（考虑缩放 sy），使底面恰好落在水平面上。
    std::vector<Vec3> verts, normals;
    o->GenerateMesh(verts, normals);
    if (verts.empty()) return;
    double miny = 1e18;
    for (size_t i = 0; i < verts.size(); i++)
        if (verts[i].y < miny) miny = verts[i].y;
    o->y = -miny * o->sy;
    o->dy = o->y;   // 渲染用显示值 dy，必须同步，否则落地不显示（组件中心仍在 y=0 一半沉）
}
