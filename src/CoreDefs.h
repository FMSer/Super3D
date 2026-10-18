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
// CoreDefs.h : 核心基础类型（不依赖 MFC，可独立编译测试）
#pragma once
#include <vector>
#include <string>
#include <cmath>

const double kPI = 3.14159265358979323846;
const double kDeg2Rad = kPI / 180.0;
const double kRad2Deg = 180.0 / kPI;

// 三维向量
struct Vec3 {
    double x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(double a, double b, double c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
};

inline double Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline double Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }
inline Vec3 Normalize(const Vec3& v) {
    double L = Length(v);
    if (L < 1e-12) return Vec3(0, 0, 0);
    return v * (1.0 / L);
}

// 图元类型
enum PrimType {
    PT_NONE = 0,     // 组（容器，不画几何体）
    PT_BOX,          // 立方体/长方体
    PT_SPHERE,       // 球
    PT_CYLINDER,     // 圆柱
    PT_CONE,         // 圆锥
    PT_TORUS,        // 圆环
    PT_PLANE,        // 平面
    PT_SPLINE,       // 曲线（Catmull-Rom 管状线，拖线绘制 + 锚点编辑）
    PT_LEAF,         // 绿叶（扁椭球叶片）
    PT_TEXT3D,       // 3D 文本（GDI 轮廓挤出成体字，可设文字/字体/字高/厚度）
    PT_TUBE,         // 方形空管（外方柱-内方柱，空心方管）
    PT_RTUBE,        // 圆形空管（外圆柱-内圆柱，空心圆管）
    PT_BOOLEAN,      // 布尔差结果（立方体挖孔：网格固定，顶点序列化，可再移动/缩放）
    PT_CAPSULE,      // 胶囊（圆柱段+两端半球，做肢体/舱段/胶囊状零件）
    PT_HEMISPHERE,   // 半球/球冠（圆顶，可配平底盖，做火箭头/头盔/整流罩）
    PT_ROUNDBOX,     // 圆角长方体（8 角为 1/8 球、12 棱为 1/4 圆柱，做圆角零件外壳）
    PT_LATHE,        // 车削回转体（ctrlPts 存母线折线：x=半径, y=高度；绕 Y 轴旋转成体，喷管/瓶碗/螺栓）
    PT_EXTRUDE       // 挤出体（ctrlPts 存 x,z 平面闭合截面折线，沿 Y 拉伸 params[0] 厚度，齿轮/型材/导轨）
};

// 相机（环绕视角）
struct Camera {
    double yaw, pitch, dist;   // 度
    double tx, ty, tz;         // 观察目标
    Camera() { Reset(); }
    void Reset() { yaw = -45.0; pitch = 30.0; dist = 20.0; tx = 0.0; ty = 0.0; tz = 0.0; }
};

// 关键帧（位置/旋转/缩放 共 9 通道）
struct KeyFrame {
    int frame;
    double px, py, pz;
    double rx, ry, rz;   // 度
    double sx, sy, sz;
    KeyFrame() {
        frame = 0;
        px = py = pz = rx = ry = rz = 0.0;
        sx = sy = sz = 1.0;
    }
    void CopyFrom(double _x, double _y, double _z,
                  double _rx, double _ry, double _rz,
                  double _sx, double _sy, double _sz) {
        px = _x; py = _y; pz = _z;
        rx = _rx; ry = _ry; rz = _rz;
        sx = _sx; sy = _sy; sz = _sz;
    }
};
