// Mat4.h : 4x4 仿射矩阵（列主序，与 OpenGL 一致），不依赖 MFC
#pragma once
#include "CoreDefs.h"

struct Mat4 {
    double m[16]; // 列主序：元素 m[col*4+row]

    Mat4() { Identity(); }

    void Identity() {
        for (int i = 0; i < 16; i++) m[i] = 0.0;
        m[0] = m[5] = m[10] = m[15] = 1.0;
    }

    static Mat4 Translation(double x, double y, double z) {
        Mat4 r;
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 Scale(double sx, double sy, double sz) {
        Mat4 r;
        r.m[0] = sx; r.m[5] = sy; r.m[10] = sz;
        return r;
    }

    // 绕 X/Y/Z 轴旋转（度）
    static Mat4 RotX(double deg) {
        Mat4 r;
        double a = deg * kDeg2Rad, c = std::cos(a), s = std::sin(a);
        r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
        return r;
    }
    static Mat4 RotY(double deg) {
        Mat4 r;
        double a = deg * kDeg2Rad, c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
        return r;
    }
    static Mat4 RotZ(double deg) {
        Mat4 r;
        double a = deg * kDeg2Rad, c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
        return r;
    }

    // 旋转顺序：先 X 再 Y 再 Z（结果 = Rz*Ry*Rx）
    static Mat4 RotationXYZ(double rx, double ry, double rz) {
        return RotZ(rz).Mul(RotY(ry)).Mul(RotX(rx));
    }

    // this * b
    Mat4 Mul(const Mat4& b) const {
        Mat4 r;
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                double s = 0.0;
                for (int k = 0; k < 4; k++)
                    s += m[k * 4 + row] * b.m[col * 4 + k];
                r.m[col * 4 + row] = s;
            }
        }
        return r;
    }

    Vec3 TransformPoint(const Vec3& v) const {
        return Vec3(
            m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12],
            m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]);
    }

    Vec3 TransformDir(const Vec3& v) const {
        return Vec3(
            m[0] * v.x + m[4] * v.y + m[8]  * v.z,
            m[1] * v.x + m[5] * v.y + m[9]  * v.z,
            m[2] * v.x + m[6] * v.y + m[10] * v.z);
    }

    // 仿射逆矩阵（平移 + 3x3 可逆线性部分）
    Mat4 Inverse() const {
        // 提取 3x3 子矩阵 A（列主序：A[c*3+r] = m[c*4+r]）
        double A[9];
        for (int c = 0; c < 3; c++)
            for (int r = 0; r < 3; r++)
                A[c * 3 + r] = m[c * 4 + r];
        // 行主序三行：a b c / d e f / g h i
        double a = A[0], b = A[3], c = A[6];
        double d = A[1], e = A[4], f = A[7];
        double g = A[2], h = A[5], i = A[8];
        double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
        Mat4 r;
        if (det == 0.0) return r; // 退化，返回单位阵
        double id = 1.0 / det;
        // 伴随矩阵转置 → 逆矩阵（行主序元素）
        double inv[9];
        inv[0] = (e * i - f * h) * id;   // [0][0]
        inv[1] = (c * h - b * i) * id;   // [0][1]
        inv[2] = (b * f - c * e) * id;   // [0][2]
        inv[3] = (f * g - d * i) * id;   // [1][0]
        inv[4] = (a * i - c * g) * id;   // [1][1]
        inv[5] = (c * d - a * f) * id;   // [1][2]
        inv[6] = (d * h - e * g) * id;   // [2][0]
        inv[7] = (b * g - a * h) * id;   // [2][1]
        inv[8] = (a * e - b * d) * id;   // [2][2]
        // 行主序 → 列主序 r.m[col*4+row]
        r.m[0] = inv[0]; r.m[1] = inv[3]; r.m[2] = inv[6];
        r.m[4] = inv[1]; r.m[5] = inv[4]; r.m[6] = inv[7];
        r.m[8] = inv[2]; r.m[9] = inv[5]; r.m[10] = inv[8];
        // 平移 = -R^-1 * t（列主序：第 i 行元素是 r.m[i], r.m[4+i], r.m[8+i]）
        r.m[12] = -(r.m[0] * m[12] + r.m[4] * m[13] + r.m[8] * m[14]);
        r.m[13] = -(r.m[1] * m[12] + r.m[5] * m[13] + r.m[9] * m[14]);
        r.m[14] = -(r.m[2] * m[12] + r.m[6] * m[13] + r.m[10] * m[14]);
        return r;
    }
};
