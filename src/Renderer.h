// Renderer.h : OpenGL 渲染器（固定管线，不依赖 MFC）
#pragma once
#include "Scene3D.h"

class Renderer {
public:
    // 细分线框显示开关（选中图元的网格线框；默认关闭，勾选“细分”后显示）
    static bool s_showWire;

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
    static void DrawObject(const Object3D* o, bool overlay);
    static void DrawMesh(const Object3D* o);
};
