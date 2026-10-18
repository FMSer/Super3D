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
// Renderer.h : OpenGL 渲染器（固定管线，不依赖 MFC）
#pragma once
#include "Scene3D.h"

class Renderer {
public:
    // 细分线框显示开关（选中图元的网格线框；默认关闭，勾选”细分”后显示）
    static bool s_showWire;

    // 灯光参数（可在”视图→灯光设置”中调节）
    static float s_lightAmbient;    // 环境光强度 0~1
    static float s_lightDiffuse;    // 方向光强度 0~1
    static float s_lightDirX, s_lightDirY, s_lightDirZ;  // 灯光方向（归一化）

    // 渲染一帧；要求当前线程已绑定 OpenGL 上下文
    static void Render(Scene3D& s, int w, int h);

    // 网格编辑叠加层（Blender 式点/线/面编辑）：Render 之后、SwapBuffers 之前调用，
    // 沿用当前相机矩阵。mode: 0=点 1=线 2=面；选中集合由视口传入
    static void DrawEditMeshOverlay(Scene3D& s, int mode,
                                    const std::vector<int>& selV,
                                    const std::vector<std::pair<int, int> >& selE,
                                    const std::vector<int>& selF);

    // 相机辅助：返回相机位置
    static Vec3 CameraEye(const Camera& c);

private:
    static void DrawGridAndAxes();
    static void DrawAxisLabels();     // 坐标轴端点绘制 X/Y/Z 文字标识（OpenGL 位图字体）
    static void DrawObject(const Object3D* o, bool overlay);
    static void DrawMesh(const Object3D* o);
};
