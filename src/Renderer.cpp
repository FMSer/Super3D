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
// Renderer.cpp : OpenGL 渲染实现
#include "Renderer.h"
#include <windows.h>   // 先定义 WINGDIAPI/APIENTRY，再包含 gl.h
#include <gl/gl.h>
#include <gl/glu.h>
#include <map>

static void DrawAABBWire(const Vec3& mn, const Vec3& mx);   // 前向声明

// ------------------------------------------------------ 网格显示列表缓存
// 布尔/3D文本网格顶点量大，每帧立即模式重画会明显掉帧；编译为显示列表后按版本号复用。
struct MeshList { unsigned list; int rev; std::string key; MeshList() : list(0), rev(-1) {} };
static std::map<const Object3D*, MeshList> s_meshLists;
static std::vector<unsigned> s_pendingDelLists;   // 对象析构时无 GL 上下文，延迟到下一帧删除

// 对象销毁时调用：仅摘除缓存项，GL 列表延迟到 Render（有上下文）统一删除
static void RendererInvalidateMeshCache(const Object3D* o) {
    std::map<const Object3D*, MeshList>::iterator it = s_meshLists.find(o);
    if (it != s_meshLists.end()) {
        if (it->second.list != 0) s_pendingDelLists.push_back(it->second.list);
        s_meshLists.erase(it);
    }
}

bool Renderer::s_showWire = false;
float Renderer::s_lightAmbient = 0.4f;
float Renderer::s_lightDiffuse = 0.85f;
float Renderer::s_lightDirX = 0.55f;
float Renderer::s_lightDirY = 0.83f;
float Renderer::s_lightDirZ = 0.41f;

Vec3 Renderer::CameraEye(const Camera& c) {
    double yr = c.yaw * kDeg2Rad, pr = c.pitch * kDeg2Rad;
    Vec3 dir(std::cos(pr) * std::sin(yr), std::sin(pr), std::cos(pr) * std::cos(yr));
    return Vec3(c.tx, c.ty, c.tz) + dir * c.dist;
}

void Renderer::Render(Scene3D& s, int w, int h) {
    if (w < 1 || h < 1) return;
    g_meshCacheInvalidate = RendererInvalidateMeshCache;   // 注册析构回调（幂等）
    // 有 GL 上下文：统一删除对象析构时遗留的显示列表
    if (!s_pendingDelLists.empty()) {
        for (size_t i = 0; i < s_pendingDelLists.size(); i++) glDeleteLists(s_pendingDelLists[i], 1);
        s_pendingDelLists.clear();
    }
    glViewport(0, 0, w, h);
    glClearColor(0.88f, 0.9f, 0.94f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    double aspect = (double)w / (double)h;
    gluPerspective(45.0, aspect, 0.1, 3000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Vec3 eye = CameraEye(s.cam);
    gluLookAt(eye.x, eye.y, eye.z,
              s.cam.tx, s.cam.ty, s.cam.tz,
              0.0, 1.0, 0.0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);   // 非均匀缩放（压扁球成翅膀）后归一化法线，否则光照发白

    // 灯光
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    // 关闭 OpenGL 全局环境光（默认 0.2 会叠加到所有物体上导致白色过曝）
    GLfloat modelAmb[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, modelAmb);
    GLfloat amb[4] = { s_lightAmbient, s_lightAmbient, s_lightAmbient, 1.0f };
    GLfloat dif[4] = { s_lightDiffuse, s_lightDiffuse, s_lightDiffuse, 1.0f };
    GLfloat spc[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat pos[4] = { s_lightDirX, s_lightDirY, s_lightDirZ, 0.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spc);
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    DrawGridAndAxes();

    // 绘制元件（用显示变换）
    // 真实 3D 深度遮挡：元件前后由空间位置决定（深度测试开启）。
    // 时间轴的上下排序仅作为“深度相同/相近”时的绘制先后（上层后画、略优先）。
    // 透明材质：不透明元件先画（深度正常写入），半透明元件后画（混合 + 不写深度），
    // 避免透明物体把后面的物体错误遮挡。
    std::vector<Object3D*> all;
    s.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) {
        const Object3D* o = all[i];
        if (!s.ObjectVisible(o) || !Object3D::IsPrimitive(o->type)) continue;
        if (o->alpha >= 0.999) DrawObject(o, false);   // 始终画原来的实体 3D 图，点线面仅作为可编辑元素叠加其上
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (size_t i = 0; i < all.size(); i++) {
        const Object3D* o = all[i];
        if (!s.ObjectVisible(o) || !Object3D::IsPrimitive(o->type)) continue;
        if (o->alpha < 0.999) DrawObject(o, false);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    // 选中高亮线框：仅当“细分”勾选显示时才绘制（默认不显示，避免整体变黄）
    if (Renderer::s_showWire) {
        for (size_t i = 0; i < all.size(); i++) {
            const Object3D* o = all[i];
            if (!s.ObjectVisible(o) || !Object3D::IsPrimitive(o->type) || !o->selected) continue;
            DrawObject(o, true);
        }
    }
    // 选中的组：画整组包围盒线框（组合后作为整体选中时的可视反馈）
    for (size_t i = 0; i < all.size(); i++) {
        const Object3D* o = all[i];
        if (!s.ObjectVisible(o) || o->type != PT_NONE || !o->selected) continue;
        Vec3 mn, mx;
        if (!Object3D::GroupWorldAABB(o, mn, mx)) continue;
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 0.85f, 0.1f);
        glLineWidth(2.0f);
        DrawAABBWire(mn, mx);   // 世界坐标 AABB，与渲染同基准，贴合实际大小
        glLineWidth(1.0f);
        glEnable(GL_LIGHTING);
    }

    // 选中图元的锚点：用 OpenGL 在世界坐标画朝向相机的方块，与渲染共用同一相机。
    // 曲线 → 橙色曲度锚点；其他图元 → 橙色角点锚点（整体等比缩放）+ 蓝色面中点锚点（单轴压扁/拉伸）。
    {
        Vec3 target(s.cam.tx, s.cam.ty, s.cam.tz);
        Vec3 fwd = Normalize(target - eye);
        Vec3 right = Normalize(Cross(fwd, Vec3(0, 1, 0)));
        Vec3 up = Cross(right, fwd);
        // 锚点固定为约 16 屏幕像素：随相机距离缩放（tan(22.5°)≈0.4142，fov 45°），
        // 这样放大/缩小视角或缩放物体时锚点在屏幕上大小恒定，始终好抓取。
        double ha = (16.0 / h) * s.cam.dist * 0.41421356;
        double hf = ha * 0.65; // 面中点半边长（稍小，蓝色，与角点区分）
        glDisable(GL_LIGHTING);
        glDepthMask(GL_FALSE);   // 不写深度：多个锚点重叠时后画的覆盖先画的（深度测试保持开启，正常遮挡）
        auto quad = [&](const Vec3& c, double h, float cr, float cg, float cb) {
            Vec3 a = c - right * h - up * h;
            Vec3 b = c + right * h - up * h;
            Vec3 d = c - right * h + up * h;
            Vec3 e = c + right * h + up * h;
            glColor3f(cr, cg, cb);
            glBegin(GL_QUADS);
            glVertex3d(a.x, a.y, a.z); glVertex3d(b.x, b.y, b.z);
            glVertex3d(e.x, e.y, e.z); glVertex3d(d.x, d.y, d.z);
            glEnd();
            glColor3f(1.0f, 1.0f, 1.0f);   // 白边，醒目
            glBegin(GL_LINE_LOOP);
            glVertex3d(a.x, a.y, a.z); glVertex3d(b.x, b.y, b.z);
            glVertex3d(e.x, e.y, e.z); glVertex3d(d.x, d.y, d.z);
            glEnd();
        };
        // 锚点显示位置往外浮：沿“锚点-对象中心”方向偏移一点，让贴在组件表面上的锚点
        // 浮出表面好抓取（仍随深度正常遮挡；仅影响显示，锚点数据位置不变，
        // 偏移 ≈ 半个锚点 ≈ 9 屏幕像素，小于 20px 命中阈值，不影响点中）
        auto raised = [](const Mat4& w, const Vec3& lp, const Vec3& lc, double off) {
            Vec3 pw = w.TransformPoint(lp);
            Vec3 d = pw - w.TransformPoint(lc);
            double len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (len > 1e-9) pw = pw + d * (off / len);
            return pw;
        };
        // 世界坐标版浮出（多选外框 / 组合元件锚点共用）
        auto raisedW = [](const Vec3& pw, const Vec3& pc, double off) {
            Vec3 d = pw - pc;
            double len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (len > 1e-9) return pw + d * (off / len);
            return pw;
        };
        // 多选统一外锚框：选中数 >1 时共用一个外包围盒（8 角点=整体等比缩放、6 面中点=整体单轴缩放），
        // 不再显示各对象单独锚点（避免点到单个元件触发单独缩放）
        int selCount = 0;
        Vec3 mN(1e18, 1e18, 1e18), mX(-1e18, -1e18, -1e18);
        bool mOK = false;
        for (size_t i = 0; i < all.size(); i++) {
            const Object3D* o = all[i];
            if (!s.ObjectVisible(o) || !o->selected || o == s.editMeshObj) continue;
            selCount++;
            Vec3 b0, b1;
            bool hasBox = false;
            if (Object3D::IsPrimitive(o->type)) {
                Vec3 mnL, mxL;
                if (Object3D::PrimitiveLocalBounds(o, mnL, mxL)) {
                    Mat4 wm = o->DisplayWorldMatrix();
                    for (int c2 = 0; c2 < 8; c2++) {
                        Vec3 p(mnL.x, mnL.y, mnL.z);
                        if (c2 & 1) p.x = mxL.x;
                        if (c2 & 2) p.y = mxL.y;
                        if (c2 & 4) p.z = mxL.z;
                        Vec3 w = wm.TransformPoint(p);
                        if (!hasBox) { b0 = b1 = w; hasBox = true; }
                        else {
                            if (w.x < b0.x) b0.x = w.x; if (w.x > b1.x) b1.x = w.x;
                            if (w.y < b0.y) b0.y = w.y; if (w.y > b1.y) b1.y = w.y;
                            if (w.z < b0.z) b0.z = w.z; if (w.z > b1.z) b1.z = w.z;
                        }
                    }
                }
            } else if (o->type == PT_NONE) {
                hasBox = Object3D::GroupWorldAABB(o, b0, b1);
            }
            if (!hasBox) continue;
            if (!mOK) { mN = b0; mX = b1; mOK = true; }
            else {
                if (b0.x < mN.x) mN.x = b0.x; if (b1.x > mX.x) mX.x = b1.x;
                if (b0.y < mN.y) mN.y = b0.y; if (b1.y > mX.y) mX.y = b1.y;
                if (b0.z < mN.z) mN.z = b0.z; if (b1.z > mX.z) mX.z = b1.z;
            }
        }
        if (selCount > 1 && mOK) {
            // 外包围盒线框（与组选中一致的金黄色）；保持灯光关闭，锚点颜色与单选一致
            glColor3f(1.0f, 0.85f, 0.1f);
            glLineWidth(2.0f);
            DrawAABBWire(mN, mX);
            glLineWidth(1.0f);
            Vec3 mc((mN.x + mX.x) * 0.5, (mN.y + mX.y) * 0.5, (mN.z + mX.z) * 0.5);
            double off = ha * 0.55;
            // 8 个角点（整体等比缩放）
            for (int c = 0; c < 8; c++) {
                Vec3 p((c & 1) ? mX.x : mN.x, (c & 2) ? mX.y : mN.y, (c & 4) ? mX.z : mN.z);
                quad(raisedW(p, mc, off), ha, 1.0f, 0.62f, 0.05f);
            }
            // 6 个面中点（整体单轴压扁/拉伸）
            for (int f = 0; f < 6; f++) {
                Vec3 p((f == 0) ? mX.x : (f == 1 ? mN.x : (mN.x + mX.x) * 0.5),
                       (f == 2) ? mX.y : (f == 3 ? mN.y : (mN.y + mX.y) * 0.5),
                       (f == 4) ? mX.z : (f == 5 ? mN.z : (mN.z + mX.z) * 0.5));
                quad(raisedW(p, mc, off), hf, 0.30f, 0.60f, 1.0f);
            }
        } else {
        for (size_t i = 0; i < all.size(); i++) {
            const Object3D* o = all[i];
            if (!s.ObjectVisible(o) || !o->selected) continue;
            if (o == s.editMeshObj) continue;   // 网格编辑态：锚点全部隐藏（改用点/线/面编辑叠加层）
            if (o->type == PT_SPLINE || o->type == PT_LATHE || o->type == PT_EXTRUDE) {
                // 曲线/车削母线/挤出截面：锚点嵌在体内，沿“锚点-控制点包围盒中心”方向浮出表面
                Vec3 b0(1e18, 1e18, 1e18), b1(-1e18, -1e18, -1e18);
                for (size_t k = 0; k < o->ctrlPts.size(); k++) {
                    const Vec3& p = o->ctrlPts[k];
                    if (p.x < b0.x) b0.x = p.x;
                    if (p.y < b0.y) b0.y = p.y;
                    if (p.z < b0.z) b0.z = p.z;
                    if (p.x > b1.x) b1.x = p.x;
                    if (p.y > b1.y) b1.y = p.y;
                    if (p.z > b1.z) b1.z = p.z;
                }
                Vec3 cc = (o->ctrlPts.empty()) ? Vec3(0, 0, 0) :
                    Vec3((b0.x + b1.x) * 0.5, (b0.y + b1.y) * 0.5, (b0.z + b1.z) * 0.5);
                Mat4 wmS = o->DisplayWorldMatrix();
                for (size_t k = 0; k < o->ctrlPts.size(); k++)
                    quad(raised(wmS, o->ctrlPts[k], cc, ha * 0.55), ha, 1.0f, 0.62f, 0.05f);
            } else if (Object3D::IsPrimitive(o->type)) {
                Vec3 mnL, mxL;
                if (!Object3D::PrimitiveLocalBounds(o, mnL, mxL)) continue;
                Mat4 wm = o->DisplayWorldMatrix();
                Vec3 lc((mnL.x + mxL.x) * 0.5, (mnL.y + mxL.y) * 0.5, (mnL.z + mxL.z) * 0.5);
                double off = ha * 0.55;   // 浮出量 ≈ 半个锚点（仍小于 20px 命中阈值）
                // 8 个角点缩放锚点
                for (int c = 0; c < 8; c++) {
                    double cx = (c & 1) ? mxL.x : mnL.x;
                    double cy = (c & 2) ? mxL.y : mnL.y;
                    double cz = (c & 4) ? mxL.z : mnL.z;
                    quad(raised(wm, Vec3(cx, cy, cz), lc, off), ha, 1.0f, 0.62f, 0.05f);
                }
                // 6 个面中点（单轴压扁/拉伸）
                for (int f = 0; f < 6; f++) {
                    double fx = (f == 0) ? mxL.x : (f == 1 ? mnL.x : 0.0);
                    double fy = (f == 2) ? mxL.y : (f == 3 ? mnL.y : 0.0);
                    double fz = (f == 4) ? mxL.z : (f == 5 ? mnL.z : 0.0);
                    quad(raised(wm, Vec3(fx, fy, fz), lc, off), hf, 0.30f, 0.60f, 1.0f);
                }
            } else if (o->type == PT_NONE) {
                // 组合元件（movieclip）：整体缩放锚点 = 组世界包围盒的 8 角点 + 6 面中点
                Vec3 b0, b1;
                if (!Object3D::GroupWorldAABB(o, b0, b1)) continue;
                Vec3 mc((b0.x + b1.x) * 0.5, (b0.y + b1.y) * 0.5, (b0.z + b1.z) * 0.5);
                double off = ha * 0.55;
                for (int c = 0; c < 8; c++) {
                    Vec3 p((c & 1) ? b1.x : b0.x, (c & 2) ? b1.y : b0.y, (c & 4) ? b1.z : b0.z);
                    quad(raisedW(p, mc, off), ha, 1.0f, 0.62f, 0.05f);
                }
                for (int f = 0; f < 6; f++) {
                    Vec3 p((f == 0) ? b1.x : (f == 1 ? b0.x : (b0.x + b1.x) * 0.5),
                           (f == 2) ? b1.y : (f == 3 ? b0.y : (b0.y + b1.y) * 0.5),
                           (f == 4) ? b1.z : (f == 5 ? b0.z : (b0.z + b1.z) * 0.5));
                    quad(raisedW(p, mc, off), hf, 0.30f, 0.60f, 1.0f);
                }
            }
        }
        }
        glDepthMask(GL_TRUE);
        glEnable(GL_LIGHTING);
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
}

void Renderer::DrawGridAndAxes() {
    glDisable(GL_LIGHTING);
    glLineWidth(1.0f);
    // 网格
    glColor3f(0.75f, 0.78f, 0.82f);
    glBegin(GL_LINES);
    for (int x = -50; x <= 50; x += 5) {
        glVertex3f((GLfloat)x, 0, -50.0f);
        glVertex3f((GLfloat)x, 0, 50.0f);
    }
    for (int z = -50; z <= 50; z += 5) {
        glVertex3f(-50.0f, 0, (GLfloat)z);
        glVertex3f(50.0f, 0, (GLfloat)z);
    }
    glEnd();
    // 坐标轴
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(0.9f, 0.25f, 0.25f);
    glVertex3f(0, 0, 0); glVertex3f(3, 0, 0);   // X 红
    glColor3f(0.25f, 0.85f, 0.25f);
    glVertex3f(0, 0, 0); glVertex3f(0, 3, 0);   // Y 绿
    glColor3f(0.25f, 0.35f, 0.95f);
    glVertex3f(0, 0, 0); glVertex3f(0, 0, 3);   // Z 蓝
    glEnd();
    glLineWidth(1.0f);
    DrawAxisLabels();        // 轴端点标注 X / Y / Z
    glEnable(GL_LIGHTING);
}

// 坐标轴端点的字母标识：用 wglUseFontBitmaps 生成 3D 位图字体，
// 在轴末端（稍偏移避免贴线）用 glRasterPos 定位绘制。字体在首个 GL 上下文里惰性创建一次。
void Renderer::DrawAxisLabels() {
    static GLuint s_fontList = 0;         // 位图字体显示列表基址
    static bool s_fontReady = false;
    HDC hdc = ::wglGetCurrentDC();
    if (!hdc) return;
    if (!s_fontReady) {
        s_fontList = glGenLists(256);
        HFONT fnt = ::CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        HGDIOBJ old = ::SelectObject(hdc, fnt);
        ::wglUseFontBitmaps(hdc, 0, 256, s_fontList);
        ::SelectObject(hdc, old);
        ::DeleteObject(fnt);
        s_fontReady = true;
    }
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    // 轴端点稍拉长，文字放在末端再往外一点
    const GLfloat end = 3.2f;
    // X（红）—— 沿 +X 末端右侧
    glColor3f(0.9f, 0.25f, 0.25f);
    glRasterPos3f(end, 0.15f, 0.15f);
    glListBase(s_fontList);
    glCallLists(1, GL_UNSIGNED_BYTE, (const GLvoid*)("X"));
    // Y（绿）
    glColor3f(0.25f, 0.85f, 0.25f);
    glRasterPos3f(0.15f, end, 0.15f);
    glCallLists(1, GL_UNSIGNED_BYTE, (const GLvoid*)("Y"));
    // Z（蓝）
    glColor3f(0.25f, 0.35f, 0.95f);
    glRasterPos3f(0.15f, 0.15f, end);
    glCallLists(1, GL_UNSIGNED_BYTE, (const GLvoid*)("Z"));
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void Renderer::DrawMesh(const Object3D* o) {
    // 所有图元都用显示列表缓存：普通图元每帧重新 GenerateMesh+immediate mode 画 145 个球/圆柱太慢
    //（JS 版 VBO 一次上传后 GPU 直接画）。params/seg 变化时由 Object3D 递增 _meshRev 触发重建。
    bool cacheable = o->_meshValid || o->type == PT_BOX || o->type == PT_SPHERE || o->type == PT_CYLINDER
                     || o->type == PT_CONE || o->type == PT_TORUS || o->type == PT_PLANE
                     || o->type == PT_TUBE || o->type == PT_RTUBE || o->type == PT_CAPSULE
                     || o->type == PT_HEMISPHERE || o->type == PT_LEAF || o->type == PT_LATHE
                     || o->type == PT_EXTRUDE || o->type == PT_TEXT3D || o->type == PT_BOOLEAN;
    if (cacheable) {
        // 缓存 key：类型+参数+细分，params/seg 变化时自动重建显示列表
        std::string key = std::to_string(o->type) + "|";
        for (int qi = 0; qi < 4; qi++) key += std::to_string(o->params[qi]) + ",";
        key += "|" + std::to_string(o->segments);
        MeshList& c = s_meshLists[o];
        if (c.list != 0 && c.key == key && c.rev == o->_meshRev) {
            glCallList(c.list);
            return;
        }
        std::vector<Vec3> verts, normals;
        o->GenerateMesh(verts, normals);
        if (verts.size() < 3) return;
        if (c.list != 0) glDeleteLists(c.list, 1);
        c.list = glGenLists(1);
        c.rev = o->_meshRev;
        c.key = key;
        glNewList(c.list, GL_COMPILE_AND_EXECUTE);
        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i < verts.size(); i++) {
            glNormal3d(normals[i].x, normals[i].y, normals[i].z);
            glVertex3d(verts[i].x, verts[i].y, verts[i].z);
        }
        glEnd();
        glEndList();
        return;
    }
    std::vector<Vec3> verts, normals;
    o->GenerateMesh(verts, normals);
    size_t n = verts.size();
    if (n < 3) return;
    // 编辑网格：有自定义色的三角逐面设色（0=继承物体色），其余仍整体物体色
    bool perTri = o->HasEditMesh() && !o->editTriColor.empty();
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < n; i += 3) {
        if (perTri) {
            unsigned int c;
            if (o->GetEditTriColor((int)(i / 3), c)) {
                double r, g, b;
                Object3D::UnpackEditColor(c, r, g, b);
                glColor3d(r, g, b);
            } else {
                glColor4d(o->r, o->g, o->b, o->alpha);
            }
        }
        glNormal3d(normals[i].x, normals[i].y, normals[i].z);
        glVertex3d(verts[i].x, verts[i].y, verts[i].z);
        glNormal3d(normals[i + 1].x, normals[i + 1].y, normals[i + 1].z);
        glVertex3d(verts[i + 1].x, verts[i + 1].y, verts[i + 1].z);
        glNormal3d(normals[i + 2].x, normals[i + 2].y, normals[i + 2].z);
        glVertex3d(verts[i + 2].x, verts[i + 2].y, verts[i + 2].z);
    }
    glEnd();
}

// ------------------------------------------------------ 3D 文本位图纹理
// 用 GDI 把文本渲染成带 alpha 的 RGBA 位图（文字实心/孔洞/笔画由光栅化决定，绝对正确），
// 作为纹理贴到盒体顶面。返回 GL 纹理 ID；同时输出紧贴字形像素尺寸。
void Renderer::DrawObject(const Object3D* o, bool overlay) {
    Mat4 wm = o->DisplayWorldMatrix();
    glPushMatrix();
    glMultMatrixd(wm.m);
    if (overlay) {
        glDisable(GL_LIGHTING);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(1.0f, 0.85f, 0.1f);
        glLineWidth(2.0f);
        DrawMesh(o);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
        glEnable(GL_LIGHTING);
    } else {
        glColor4d(o->r, o->g, o->b, o->alpha);
        DrawMesh(o);
        // 编辑网格的自定义色线：贴表面单独描色（编辑态与常态都显示）
        if (o->HasEditMesh() && !o->editEdgeColorKey.empty()) {
            glDisable(GL_LIGHTING);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1.0f, -2.0f);
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            for (size_t i = 0; i < o->editEdgeColorKey.size(); i++) {
                double r, g, b;
                Object3D::UnpackEditColor(o->editEdgeColorVal[i], r, g, b);
                glColor3d(r, g, b);
                const Vec3& va = o->editVerts[o->editEdgeColorKey[i].first];
                const Vec3& vb = o->editVerts[o->editEdgeColorKey[i].second];
                glVertex3d(va.x, va.y, va.z);
                glVertex3d(vb.x, vb.y, vb.z);
            }
            glEnd();
            glLineWidth(1.0f);
            glDisable(GL_POLYGON_OFFSET_LINE);
            glEnable(GL_LIGHTING);
        }
    }
    glPopMatrix();
}

// 画一个 AABB 的 12 条边线框（在调用处已把组的世界矩阵压入栈）
static void DrawAABBWire(const Vec3& mn, const Vec3& mx) {
    float x0 = (float)mn.x, y0 = (float)mn.y, z0 = (float)mn.z;
    float x1 = (float)mx.x, y1 = (float)mx.y, z1 = (float)mx.z;
    struct P { float x, y, z; };
    P v[8] = {
        {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
        {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}
    };
    static const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    glBegin(GL_LINES);
    for (int i = 0; i < 12; i++) {
        glVertex3f(v[edges[i][0]].x, v[edges[i][0]].y, v[edges[i][0]].z);
        glVertex3f(v[edges[i][1]].x, v[edges[i][1]].y, v[edges[i][1]].z);
    }
    glEnd();
}

// ------------------------------------------------------ 网格编辑叠加层
// 点/线/面编辑态：在物体表面画可编辑元素——黑细线框（POLYGON_OFFSET_LINE 防闪面）、
// 顶点（不写深度 + POLYGON_OFFSET_POINT 浮在表面、保持正常遮挡）、选中元素橙色高亮。
// 在 Render 之后调用（相机/投影矩阵沿用），编辑网格经世界矩阵变换后绘制。
void Renderer::DrawEditMeshOverlay(Scene3D& s, int mode,
                                   const std::vector<int>& selV,
                                   const std::vector<std::pair<int, int> >& selE,
                                   const std::vector<int>& selF) {
    Object3D* o = s.editMeshObj;
    if (!o || o->editVerts.empty() || o->editTris.empty()) return;
    Mat4 wm = o->DisplayWorldMatrix();
    const std::vector<Vec3>& V = o->editVerts;
    const std::vector<int>& T = o->editTris;
    size_t nt = T.size() / 3;

    // 确保深度测试开启、函数为 GL_LESS：背面的点线面被正面实体遮住
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    // 选中集合 → 查找表
    std::map<int, bool> vs;
    for (size_t i = 0; i < selV.size(); i++) vs[selV[i]] = true;
    std::map<long long, bool> es;
    for (size_t i = 0; i < selE.size(); i++) {
        int a = selE[i].first, b = selE[i].second;
        if (a > b) { int t2 = a; a = b; b = t2; }
        es[(long long)a * 1000003LL + b] = true;
    }
    std::map<int, bool> fs;
    for (size_t i = 0; i < selF.size(); i++) fs[selF[i]] = true;

    glDisable(GL_LIGHTING);

    // 面模式：选中面半透明橙色覆盖（POLYGON_OFFSET_FILL 把覆盖层拉向相机，
    // 避免与实体表面同深度 z-fighting 导致只显示一部分；关光照保持橙色纯正）
    if (mode == 2 && !fs.empty()) {
        glDisable(GL_LIGHTING);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
        glColor4f(1.0f, 0.55f, 0.05f, 0.40f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glBegin(GL_TRIANGLES);
        for (size_t t = 0; t < nt; t++) {
            if (!fs[(int)t]) continue;
            for (int j = 0; j < 3; j++) {
                Vec3 w = wm.TransformPoint(V[T[t * 3 + j]]);
                glVertex3d(w.x, w.y, w.z);
            }
        }
        glEnd();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_LIGHTING);
    }

    // 全部网格边：贴在实体表面的黑色细线（POLYGON_OFFSET_LINE 防闪面），跳过三角剖分斜线
    glDisable(GL_LIGHTING);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -2.0f);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (size_t t = 0; t < nt; t++) {
        for (int j = 0; j < 3; j++) {
            int a = T[t * 3 + j], b = T[t * 3 + (j + 1) % 3];
            if (o->IsDiagEdge(a, b)) continue;
            unsigned int ec;
            if (o->GetEditEdgeColor(a, b, ec)) {
                double r, g, b2;
                Object3D::UnpackEditColor(ec, r, g, b2);
                glColor3d(r, g, b2);   // 自定义色线：编辑态直接按该色显示
            } else {
                glColor3f(0.05f, 0.05f, 0.05f);
            }
            Vec3 w1 = wm.TransformPoint(V[a]);
            Vec3 w2 = wm.TransformPoint(V[b]);
            glVertex3d(w1.x, w1.y, w1.z);
            glVertex3d(w2.x, w2.y, w2.z);
        }
    }
    glEnd();
    // 选中边：橙色粗线
    if (!es.empty()) {
        glColor3f(1.0f, 0.55f, 0.05f);
        glLineWidth(4.0f);
        glBegin(GL_LINES);
        for (size_t t = 0; t < nt; t++) {
            for (int j = 0; j < 3; j++) {
                int a = T[t * 3 + j], b = T[t * 3 + (j + 1) % 3];
                if (o->IsDiagEdge(a, b)) continue;
                long long key = (a < b) ? (long long)a * 1000003LL + b : (long long)b * 1000003LL + a;
                if (!es[key]) continue;
                Vec3 w1 = wm.TransformPoint(V[a]);
                Vec3 w2 = wm.TransformPoint(V[b]);
                glVertex3d(w1.x, w1.y, w1.z);
                glVertex3d(w2.x, w2.y, w2.z);
            }
        }
        glEnd();
    }
    glLineWidth(1.0f);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glEnable(GL_LIGHTING);

    // 顶点：黑底白点（选中橙色）；不写深度 + 负点偏移 → 表面可见但保留遮挡关系
    glEnable(GL_POLYGON_OFFSET_POINT);
    glPolygonOffset(-2.0f, -2.0f);
    glDepthMask(GL_FALSE);
    glPointSize(9.0f);
    glColor3f(0.05f, 0.05f, 0.05f);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < V.size(); i++) {
        Vec3 w = wm.TransformPoint(V[i]);
        glVertex3d(w.x, w.y, w.z);
    }
    glEnd();
    glPointSize(5.0f);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < V.size(); i++) {
        Vec3 w = wm.TransformPoint(V[i]);
        if (vs[i]) glColor3f(1.0f, 0.55f, 0.05f);
        else       glColor3f(1.0f, 1.0f, 1.0f);
        glVertex3d(w.x, w.y, w.z);
    }
    glEnd();
    glPointSize(1.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_POLYGON_OFFSET_POINT);

    glEnable(GL_LIGHTING);
}
