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
// Examples.cpp : 内置示例场景实现
#include "Examples.h"

namespace Examples {

static Object3D* Add(Scene3D& s, int type, const std::wstring& name, Object3D* parent) {
    Object3D* o = s.AddObject(type, name, parent);
    Scene3D::SetStandingY(o);
    return o;
}

// 组内子件定位：Add() 里的 SetStandingY 是给根对象落地用的，会把子件中心抬离组锚点，
// 组旋转时子件就绕锚点扫圆（晃动）。子件必须显式归位，并同步渲染显示值 dx/dy/dz。
static void Put(Object3D* o, double x, double y, double z) {
    o->x = x; o->y = y; o->z = z;
    o->dx = x; o->dy = y; o->dz = z;
}

void BuildHouse(Scene3D& s) {
    s.Clear();
    Object3D* ground = Add(s, PT_PLANE, L"ground", NULL);
    ground->params[0] = 30.0;
    ground->r = 0.55; ground->g = 0.82; ground->b = 0.5;

    Object3D* wall = Add(s, PT_BOX, L"wall", NULL);
    wall->params[0] = 6.0; wall->params[1] = 3.6; wall->params[2] = 4.4;
    wall->y = 1.8;
    wall->r = 0.95; wall->g = 0.9; wall->b = 0.72;

    // 屋顶：四棱尖顶，底面转 45° 与墙平行（消除拧歪感），半径加大使屋顶更宽、出檐更足
    Object3D* roof = Add(s, PT_CONE, L"roof", NULL);
    roof->params[0] = 5.2; roof->params[1] = 2.4;
    roof->segments = 4;
    roof->ry = 45.0;
    roof->y = 4.8;   // 底面正好落在墙顶(y=3.6)
    roof->r = 0.85; roof->g = 0.35; roof->b = 0.3;

    Object3D* door = Add(s, PT_BOX, L"door", NULL);
    door->params[0] = 1.2; door->params[1] = 2.0; door->params[2] = 0.25;
    door->x = 0; door->z = 2.32; door->y = 1.0;
    door->r = 0.55; door->g = 0.35; door->b = 0.2;

    Object3D* w1 = Add(s, PT_BOX, L"window1", NULL);
    w1->params[0] = 1.0; w1->params[1] = 1.0; w1->params[2] = 0.25;
    w1->x = -1.7; w1->z = 2.32; w1->y = 2.4;
    w1->r = 0.6; w1->g = 0.8; w1->b = 1.0;

    Object3D* w2 = Add(s, PT_BOX, L"window2", NULL);
    w2->params[0] = 1.0; w2->params[1] = 1.0; w2->params[2] = 0.25;
    w2->x = 1.7; w2->z = 2.32; w2->y = 2.4;
    w2->r = 0.6; w2->g = 0.8; w2->b = 1.0;

    Object3D* ch = Add(s, PT_CYLINDER, L"chimney", NULL);
    ch->params[0] = 0.4; ch->params[1] = 1.6;
    ch->x = 1.6; ch->z = -1.2; ch->y = 4.4;
    ch->r = 0.7; ch->g = 0.5; ch->b = 0.35;

    // 文档类实现（替代主脚本）：构造时输出欢迎信息
    s.docClass =
        L"package {\r\n"
        L"\timport super3d.display.Object3D;\r\n"
        L"\timport super3d.events.Event;\r\n"
        L"\tpublic class main extends Object3D {\r\n"
        L"\t\tpublic function main() {\r\n"
        L"\t\t\ttrace(\"Hello! I am a small house.\");\r\n"
        L"\t\t}\r\n"
        L"\t}\r\n"
        L"}\r\n";
    s.script.clear();
    s.scriptPerFrame = false;
}

void BuildWindmill(Scene3D& s) {
    s.Clear();
    // 最原始版风车：圆台地基 + 细圆柱塔 + 十字四叶片，简洁直观
    Object3D* base = Add(s, PT_CYLINDER, L"base", NULL);
    base->params[0] = 1.5; base->params[1] = 0.3;
    base->y = 0.15;
    base->r = 0.85; base->g = 0.85; base->b = 0.8;

    // 细塔（圆柱）
    Object3D* tower = Add(s, PT_CYLINDER, L"tower", NULL);
    tower->params[0] = 0.5; tower->params[1] = 5.2;
    tower->y = 2.6;
    tower->r = 0.92; tower->g = 0.9; tower->b = 0.82;

    // 塔顶小球
    Object3D* tip = Add(s, PT_SPHERE, L"tip", NULL);
    tip->params[0] = 0.18;
    tip->y = 5.4;
    tip->r = 0.95; tip->g = 0.5; tip->b = 0.25;

    // 叶片组：塔顶中心，绕水平轴（X）旋转
    Object3D* group = Add(s, PT_NONE, L"bladesGroup", NULL);
    group->y = 5.2;

    // 轮毂：沿旋转轴的短轴
    Object3D* hub = Add(s, PT_CYLINDER, L"hub", group);
    hub->params[0] = 0.32; hub->params[1] = 0.28;
    hub->y = 0;
    hub->rz = 90.0;
    hub->r = 0.3; hub->g = 0.3; hub->b = 0.35;

    // 四片扁叶十字排列（上/下/前/后），关于轮毂中心对称
    const wchar_t* bladeNames[4] = { L"blade1", L"blade2", L"blade3", L"blade4" };
    for (int i = 0; i < 4; i++) {
        Object3D* b = Add(s, PT_BOX, bladeNames[i], group);
        if (i < 2) {
            b->params[0] = 0.4; b->params[1] = 3.0; b->params[2] = 0.1;
            b->y = (i == 0 ? 1.5 : -1.5);
        } else {
            b->params[0] = 0.1; b->params[1] = 0.4; b->params[2] = 3.0;
            b->y = 0;
            b->z = (i == 2 ? 1.5 : -1.5);
        }
        b->r = 0.9; b->g = 0.55; b->b = 0.2;
    }

    // 文档类实现（替代主脚本）：onEnterFrame 驱动叶片组绕 X 旋转
    s.docClass =
        L"package {\r\n"
        L"\timport super3d.display.Object3D;\r\n"
        L"\timport super3d.events.Event;\r\n"
        L"\tpublic class main extends Object3D {\r\n"
        L"\t\tpublic function main() {\r\n"
        L"\t\t\ttrace(\"Windmill spinning!\");\r\n"
        L"\t\t\tstage.addEventListener(Event.ENTER_FRAME, onEnterFrame);\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function onEnterFrame(e:Event):void {\r\n"
        L"\t\t\tbladesGroup.rx = frame * 5;\r\n"
        L"\t\t}\r\n"
        L"\t}\r\n"
        L"}\r\n";
    s.script.clear();
    s.scriptPerFrame = false;
}

void BuildSolarSystem(Scene3D& s) {
    s.Clear();
    Object3D* sun = Add(s, PT_SPHERE, L"sun", NULL);
    sun->params[0] = 2.5;
    sun->r = 1.0; sun->g = 0.8; sun->b = 0.2;

    const wchar_t* ringNames[3] = { L"orbit1", L"orbit2", L"orbit3" };
    double ringR[3] = { 6.0, 10.0, 14.0 };
    for (int i = 0; i < 3; i++) {
        Object3D* ring = Add(s, PT_TORUS, ringNames[i], NULL);
        ring->params[0] = ringR[i]; ring->params[1] = 0.06;
        ring->segments = 48;
        ring->r = 0.72; ring->g = 0.72; ring->b = 0.72;
    }

    Object3D* mercury = Add(s, PT_SPHERE, L"mercury", NULL);
    mercury->params[0] = 0.5;
    mercury->r = 0.8; mercury->g = 0.7; mercury->b = 0.55;

    Object3D* earth = Add(s, PT_SPHERE, L"earth", NULL);
    earth->params[0] = 0.9;
    earth->r = 0.3; earth->g = 0.6; earth->b = 1.0;

    Object3D* mars = Add(s, PT_SPHERE, L"mars", NULL);
    mars->params[0] = 0.7;
    mars->r = 0.9; mars->g = 0.5; mars->b = 0.3;

    // 文档类实现（替代主脚本）：onEnterFrame 驱动三颗行星沿轨道公转
    s.docClass =
        L"package {\r\n"
        L"\timport super3d.display.Object3D;\r\n"
        L"\timport super3d.events.Event;\r\n"
        L"\tpublic class main extends Object3D {\r\n"
        L"\t\tpublic function main() {\r\n"
        L"\t\t\ttrace(\"Solar system running!\");\r\n"
        L"\t\t\tstage.addEventListener(Event.ENTER_FRAME, onEnterFrame);\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function onEnterFrame(e:Event):void {\r\n"
        L"\t\t\tmercury.x = 6 * cos(frame * 5);\r\n"
        L"\t\t\tmercury.z = 6 * sin(frame * 5);\r\n"
        L"\t\t\tearth.x = 10 * cos(frame * 3);\r\n"
        L"\t\t\tearth.z = 10 * sin(frame * 3);\r\n"
        L"\t\t\tmars.x = 14 * cos(frame * 2);\r\n"
        L"\t\t\tmars.z = 14 * sin(frame * 2);\r\n"
        L"\t\t}\r\n"
        L"\t}\r\n"
        L"}\r\n";
    s.script.clear();
    s.scriptPerFrame = false;
}

void BuildBounceBall(Scene3D& s) {
    s.Clear();
    Object3D* ground = Add(s, PT_PLANE, L"ground", NULL);
    ground->params[0] = 20.0;
    ground->r = 0.7; ground->g = 0.8; ground->b = 0.95;

    Object3D* ball = Add(s, PT_SPHERE, L"ball", NULL);
    ball->params[0] = 1.0;
    ball->r = 1.0; ball->g = 0.55; ball->b = 0.2;

    // 文档类实现（替代主脚本）：onEnterFrame 驱动球上下跳动 + 自转
    s.docClass =
        L"package {\r\n"
        L"\timport super3d.display.Object3D;\r\n"
        L"\timport super3d.events.Event;\r\n"
        L"\tpublic class main extends Object3D {\r\n"
        L"\t\tpublic function main() {\r\n"
        L"\t\t\ttrace(\"Ball bouncing!\");\r\n"
        L"\t\t\tstage.addEventListener(Event.ENTER_FRAME, onEnterFrame);\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function onEnterFrame(e:Event):void {\r\n"
        L"\t\t\tball.y = 1 + 4 * abs(sin(frame * 8));\r\n"
        L"\t\t\tball.ry = frame * 10;\r\n"
        L"\t\t}\r\n"
        L"\t}\r\n"
        L"}\r\n";
    s.script.clear();
    s.scriptPerFrame = false;
}

void BuildSpaceStation(Scene3D& s) {
    s.Clear();

    // ---------- 背景星球（静态远景，放远作背景） ----------
    Object3D* planet = Add(s, PT_SPHERE, L"planet", NULL);
    planet->params[0] = 5.0;
    planet->x = -20.0; planet->y = -3.0; planet->z = -16.0;
    planet->r = 0.22; planet->g = 0.55; planet->b = 0.5;
    Object3D* pring = Add(s, PT_TORUS, L"planetRing", NULL);
    pring->params[0] = 7.0; pring->params[1] = 0.14;
    pring->segments = 48;         // 圆滑星环（6 会变六边形）
    pring->x = -20.0; pring->y = -3.0; pring->z = -16.0;
    pring->ry = 20.0; pring->rx = 15.0;
    pring->r = 0.8; pring->g = 0.75; pring->b = 0.6;

    // ---------- 核心舱：圆柱躺倒，连环三次布尔差挖出三个舷窗 ----------
    // 第一次差：圆柱 → 布尔结果；第二次/第三次差：布尔结果再次布尔（验证布尔可叠加）
    Object3D* core = Add(s, PT_CYLINDER, L"coreModule", NULL);
    core->params[0] = 1.0; core->params[1] = 6.0;
    core->segments = 24;
    core->rz = 90.0;              // 躺倒：轴向 X
    core->y = 3.0;                // 舱体悬浮在 y∈[2,4]
    core->r = 0.92; core->g = 0.92; core->b = 0.95;
    const wchar_t* holeNames[3] = { L"hole1", L"hole2", L"hole3" };
    const double holeX[3] = { -1.5, 0.0, 1.5 };
    for (int i = 0; i < 3; i++) {
        Object3D* hole = Add(s, PT_CYLINDER, holeNames[i], NULL);
        hole->params[0] = 0.42; hole->params[1] = 3.4;
        hole->segments = 16;
        hole->x = holeX[i]; hole->y = 3.0; hole->z = 0.0;   // 竖直贯穿躺倒的舱体
        core->RebuildBooleanMesh(core, hole, 0);            // 差集
        core->type = PT_BOOLEAN;
        s.DeleteObject(hole);
    }

    // ---------- 对接环组：圆管 + 十字天线，脚本绕 X 自转 ----------
    Object3D* dock = Add(s, PT_NONE, L"dockGroup", NULL);
    dock->x = 4.3; dock->y = 3.0; dock->z = 0.0;
    Object3D* ring = Add(s, PT_RTUBE, L"dockRing", dock);
    ring->params[0] = 1.15; ring->params[1] = 0.8; ring->params[2] = 1.0;
    ring->segments = 24;
    ring->rz = 90.0;              // 环面法向沿 X
    ring->r = 0.40; ring->g = 0.72; ring->b = 0.82;
    Object3D* n1 = Add(s, PT_BOX, L"antennaH", dock);
    n1->params[0] = 0.08; n1->params[1] = 0.08; n1->params[2] = 3.0;
    n1->r = 0.85; n1->g = 0.85; n1->b = 0.9;
    Object3D* n2 = Add(s, PT_BOX, L"antennaV", dock);
    n2->params[0] = 0.08; n2->params[1] = 3.0; n2->params[2] = 0.08;
    n2->r = 0.85; n2->g = 0.85; n2->b = 0.9;
    // 子件归位到组锚点（抵消 SetStandingY 的落地偏移），自转才绕环心不晃
    Put(ring, 0, 0, 0);
    Put(n1, 0, 0, 0);
    Put(n2, 0, 0, 0);

    // ---------- 太阳能翼组：方管桁架 + 两块翼板，脚本绕 X 翻转追光 ----------
    Object3D* wing = Add(s, PT_NONE, L"wingGroup", NULL);
    wing->x = -4.3; wing->y = 3.0; wing->z = 0.0;
    Object3D* truss = Add(s, PT_TUBE, L"truss", wing);
    truss->params[0] = 0.5; truss->params[1] = 0.3; truss->params[2] = 3.0;
    truss->rx = 90.0;             // 方管高度转向 Z
    truss->r = 0.7; truss->g = 0.7; truss->b = 0.75;
    Object3D* p1 = Add(s, PT_BOX, L"panel1", wing);
    p1->params[0] = 1.5; p1->params[1] = 0.08; p1->params[2] = 2.2;
    p1->z = 2.4;
    p1->r = 0.16; p1->g = 0.3; p1->b = 0.8;
    Object3D* p2 = Add(s, PT_BOX, L"panel2", wing);
    p2->params[0] = 1.5; p2->params[1] = 0.08; p2->params[2] = 2.2;
    p2->z = -2.4;
    p2->r = 0.16; p2->g = 0.3; p2->b = 0.8;
    // 子件归位到组锚点（桁架被 SetStandingY 抬了 1.5，翻转时会晃），只有翼板沿 Z 展开
    Put(truss, 0, 0, 0);
    Put(p1, 0, 0, 2.4);
    Put(p2, 0, 0, -2.4);

    // ---------- 雷达罩：圆锥 + 顶球，脚本摇摆 ----------
    Object3D* dish = Add(s, PT_CONE, L"dish", NULL);
    dish->params[0] = 0.7; dish->params[1] = 0.8;
    dish->x = 0.0; dish->y = 4.4; dish->z = 0.0;
    dish->r = 0.9; dish->g = 0.45; dish->b = 0.42;
    Object3D* dtip = Add(s, PT_SPHERE, L"dishTip", NULL);
    dtip->params[0] = 0.14;
    dtip->x = 0.0; dtip->y = 5.0; dtip->z = 0.0;
    dtip->r = 0.95; dtip->g = 0.5; dtip->b = 0.25;

    // ---------- 指示灯：脚本闪烁（红通道正弦变化） ----------
    Object3D* lamp = Add(s, PT_SPHERE, L"lamp", NULL);
    lamp->params[0] = 0.12;
    lamp->x = 1.0; lamp->y = 4.0; lamp->z = 0.5;
    lamp->r = 1.0; lamp->g = 0.2; lamp->b = 0.2;

    // ---------- 跳板：立方体经网格编辑固化成楔形（程序化演示拖动编辑点） ----------
    Object3D* ramp = Add(s, PT_BOX, L"ramp", NULL);
    ramp->params[0] = 2.4; ramp->params[1] = 0.9; ramp->params[2] = 1.5;
    ramp->segments = 2;
    ramp->BuildEditMeshFromCore();
    for (size_t i = 0; i < ramp->editVerts.size(); i++) {
        Vec3& v = ramp->editVerts[i];
        if (v.y > 0.449)                       // 顶面顶点 → 改写成斜面（留 0.1 薄边）
            v.y = -0.35 + (v.x + 1.2) * (0.8 / 2.4);
    }
    ramp->x = 5.5; ramp->y = 1.35; ramp->z = 0.0;
    ramp->r = 0.85; ramp->g = 0.65; ramp->b = 0.3;

    // ---------- 植物舱：小盒 + 两片绿叶 ----------
    Object3D* bio = Add(s, PT_BOX, L"bioPod", NULL);
    bio->params[0] = 1.2; bio->params[1] = 1.0; bio->params[2] = 1.2;
    bio->x = 2.4; bio->y = 1.5; bio->z = 1.7;
    bio->r = 0.75; bio->g = 0.75; bio->b = 0.8;
    Object3D* lf1 = Add(s, PT_LEAF, L"leaf1", NULL);
    lf1->params[0] = 0.55; lf1->params[1] = 0.32; lf1->params[2] = 0.05;
    lf1->x = 2.4; lf1->y = 2.05; lf1->z = 1.7;
    lf1->ry = 30.0;
    Object3D* lf2 = Add(s, PT_LEAF, L"leaf2", NULL);
    lf2->params[0] = 0.45; lf2->params[1] = 0.28; lf2->params[2] = 0.05;
    lf2->x = 2.6; lf2->y = 2.05; lf2->z = 1.55;
    lf2->ry = 95.0;
    lf2->r = 0.24; lf2->g = 0.55; lf2->b = 0.24;

    // ---------- 轨道环：曲线（样条）绕站一圈，卫星沿它飞行 ----------
    Object3D* orbit = Add(s, PT_SPLINE, L"orbitRing", NULL);
    orbit->params[0] = 0.06;
    orbit->y = 3.0;
    orbit->segments = 12;
    for (int i = 0; i < 12; i++) {
        double a = kPI * 2.0 * i / 12.0;
        orbit->ctrlPts.push_back(Vec3(7.0 * std::cos(a), 0.0, 7.0 * std::sin(a)));
    }
    orbit->r = 0.62; orbit->g = 0.62; orbit->b = 0.68;

    // ---------- 卫星组：球体 + 两块小翼板，脚本沿轨道环公转 ----------
    Object3D* sat = Add(s, PT_NONE, L"satGroup", NULL);
    sat->x = 7.0; sat->y = 3.0; sat->z = 0.0;
    Object3D* sbody = Add(s, PT_SPHERE, L"satBody", sat);
    sbody->params[0] = 0.35;
    sbody->r = 0.95; sbody->g = 0.95; sbody->b = 0.98;
    Object3D* sw1 = Add(s, PT_BOX, L"satWing1", sat);
    sw1->params[0] = 0.7; sw1->params[1] = 0.05; sw1->params[2] = 0.4;
    sw1->x = 0.55;
    sw1->r = 0.2; sw1->g = 0.4; sw1->b = 0.85;
    Object3D* sw2 = Add(s, PT_BOX, L"satWing2", sat);
    sw2->params[0] = 0.7; sw2->params[1] = 0.05; sw2->params[2] = 0.4;
    sw2->x = -0.55;
    sw2->r = 0.2; sw2->g = 0.4; sw2->b = 0.85;
    // 子件归位（同上，避免公转时翼板微晃）
    Put(sbody, 0, 0, 0);
    Put(sw1, 0.55, 0, 0);
    Put(sw2, -0.55, 0, 0);

    // ---------- 站名牌：3D 文本 ----------
    Object3D* sign = Add(s, PT_TEXT3D, L"namePlate", NULL);
    sign->params[0] = 0.4; sign->params[1] = 1.6;
    sign->text = L"SUPER3D";
    sign->font = L"宋体";
    sign->InvalidateMesh();       // 文字网格按需缓存，改完 text/font 必须重建
    sign->x = 0.0; sign->y = 7.2; sign->z = 0.0;
    sign->r = 1.0; sign->g = 0.85; sign->b = 0.2;

    // ---------- 巡检器：关键帧动画（第 0/60/120 帧 上下巡检） ----------
    Object3D* drone = Add(s, PT_SPHERE, L"drone", NULL);
    drone->params[0] = 0.28;
    drone->x = 3.5; drone->y = 1.2; drone->z = 2.5;
    drone->r = 1.0; drone->g = 0.55; drone->b = 0.15;

    // 关键帧：所有元件先在第 0 帧定格，再移动巡检器补两个帧
    s.InsertLayerKey(0, 0);
    drone->y = 4.8;
    s.InsertLayerKey(0, 60);
    drone->y = 1.2;
    s.InsertLayerKey(0, 120);

    // ---------- 场景与脚本 ----------
    s.fps = 24;
    s.totalFrames = 120;
    s.loopPlay = true;
    s.scriptPerFrame = true;
    s.script =
        L"print(\"SUPER3D space station started!\");\r\n"
        L"dockGroup.rx = frame * 4;\r\n"
        L"wingGroup.rx = frame * 2;\r\n"
        L"dish.rx = 10 * sin(frame * 1.5);\r\n"
        L"satGroup.x = 7 * cos(frame * 3);\r\n"
        L"satGroup.z = 7 * sin(frame * 3);\r\n"
        L"satGroup.y = 3;\r\n"
        L"lamp.red = 128 + 120 * sin(frame * 15);\r\n";

    // 相机：稍远俯视全站
    s.cam.yaw = -45.0; s.cam.pitch = 25.0;
    s.cam.dist = 34.0;
    s.cam.tx = 0.0; s.cam.ty = 3.0; s.cam.tz = 0.0;
}

void BuildTankBattle(Scene3D& s) {
    s.Clear();
    // ---- 场景与障碍物（参考 1307 正常版：PT_PLANE 大地面 + 中央大掩体 + 四角小障碍）----
    Object3D* ground = Add(s, PT_PLANE, L"ground", NULL);
    ground->params[0] = 42.0;
    ground->r = 0.32; ground->g = 0.42; ground->b = 0.28;

    Object3D* wall = Add(s, PT_BOX, L"wall", NULL);
    wall->params[0] = 7.0; wall->params[1] = 3.0; wall->params[2] = 7.0;
    wall->y = 1.5;
    wall->r = 0.55; wall->g = 0.55; wall->b = 0.5;

    const wchar_t* wnames[4] = { L"obstacle1", L"obstacle2", L"obstacle3", L"obstacle4" };
    const double wx[4] = { -10.0, 10.0, -10.0, 10.0 };
    const double wz[4] = { -10.0, 10.0, 12.0, -12.0 };
    for (int i = 0; i < 4; i++) {
        Object3D* w = Add(s, PT_BOX, wnames[i], NULL);
        w->params[0] = 3.0; w->params[1] = 1.6; w->params[2] = 3.0;
        w->x = wx[i]; w->z = wz[i]; w->y = 0.8;
        w->r = 0.5; w->g = 0.5; w->b = 0.45;
    }

    // ---- 玩家坦克（参考 1307 正常版造型：底盘 + 炮塔 + 炮管 + 左右履带，组节点随脚本转向）----
    // 朝向约定：ry=0 朝 -Z（脚本 -Z 数学），炮管沿 -Z 伸出
    Object3D* ptank = Add(s, PT_NONE, L"playerTank", NULL);
    ptank->x = 0; ptank->y = 0; ptank->z = 12; ptank->ry = 0;
    Object3D* pbody = Add(s, PT_BOX, L"playerBody", ptank);
    pbody->params[0] = 1.7; pbody->params[1] = 0.8; pbody->params[2] = 2.6;
    Put(pbody, 0, 0.4, 0);
    pbody->r = 0.42; pbody->g = 0.52; pbody->b = 0.22;
    Object3D* ptur = Add(s, PT_BOX, L"playerTurret", ptank);
    ptur->params[0] = 1.2; ptur->params[1] = 0.6; ptur->params[2] = 1.4;
    Put(ptur, 0, 1.1, -0.1);
    ptur->r = 0.36; ptur->g = 0.44; ptur->b = 0.2;
    Object3D* pgun = Add(s, PT_BOX, L"playerGun", ptank);
    pgun->params[0] = 0.26; pgun->params[1] = 0.26; pgun->params[2] = 2.2;
    Put(pgun, 0, 1.1, -0.9);
    pgun->r = 0.25; pgun->g = 0.25; pgun->b = 0.25;
    Object3D* plt = Add(s, PT_BOX, L"playerTrackL", ptank);
    plt->params[0] = 0.5; plt->params[1] = 0.5; plt->params[2] = 2.6;
    Put(plt, -0.75, -0.25, 0);
    plt->r = 0.18; plt->g = 0.22; plt->b = 0.18;
    Object3D* prt = Add(s, PT_BOX, L"playerTrackR", ptank);
    prt->params[0] = 0.5; prt->params[1] = 0.5; prt->params[2] = 2.6;
    Put(prt, 0.75, -0.25, 0);
    prt->r = 0.18; prt->g = 0.22; prt->b = 0.18;

    // ---- 敌方坦克 x3（1307 同款结构，仅颜色不同：红/橙/紫粉）----
    struct TankSpec { const wchar_t* nm; double x, z, cr, cg, cb; };
    TankSpec es[3] = {
        { L"enemy1", 10.0, -8.0, 0.85, 0.2, 0.18 },
        { L"enemy2", -8.0, 10.0, 0.9, 0.45, 0.15 },
        { L"enemy3", 8.0, 12.0, 0.75, 0.3, 0.55 },
    };
    for (int i = 0; i < 3; i++) {
        std::wstring nm(es[i].nm);
        Object3D* g = Add(s, PT_NONE, es[i].nm, NULL);
        g->x = es[i].x; g->y = 0; g->z = es[i].z;
        Object3D* b = Add(s, PT_BOX, (nm + L"Body").c_str(), g);
        b->params[0] = 1.7; b->params[1] = 0.8; b->params[2] = 2.6;
        Put(b, 0, 0.4, 0);
        b->r = es[i].cr; b->g = es[i].cg; b->b = es[i].cb;
        Object3D* t = Add(s, PT_BOX, (nm + L"Turret").c_str(), g);
        t->params[0] = 1.2; t->params[1] = 0.6; t->params[2] = 1.4;
        Put(t, 0, 1.1, -0.1);
        t->r = es[i].cr * 0.85; t->g = es[i].cg * 0.85; t->b = es[i].cb * 0.85;
        Object3D* gn = Add(s, PT_BOX, (nm + L"Gun").c_str(), g);
        gn->params[0] = 0.26; gn->params[1] = 0.26; gn->params[2] = 2.2;
        Put(gn, 0, 1.1, -0.9);
        gn->r = 0.25; gn->g = 0.25; gn->b = 0.25;
        Object3D* lk = Add(s, PT_BOX, (nm + L"TrackL").c_str(), g);
        lk->params[0] = 0.5; lk->params[1] = 0.5; lk->params[2] = 2.6;
        Put(lk, -0.75, -0.25, 0);
        lk->r = 0.18; lk->g = 0.22; lk->b = 0.18;
        Object3D* rk = Add(s, PT_BOX, (nm + L"TrackR").c_str(), g);
        rk->params[0] = 0.5; rk->params[1] = 0.5; rk->params[2] = 2.6;
        Put(rk, 0.75, -0.25, 0);
        rk->r = 0.18; rk->g = 0.22; rk->b = 0.18;
    }

    // ---- 炮弹（1307 正常版：球形炮弹，玩家金黄 / 敌方橙红）----
    Object3D* bullet = Add(s, PT_SPHERE, L"bullet", NULL);
    bullet->params[0] = 0.22;
    bullet->x = 0; bullet->y = 1.1; bullet->z = 12;
    bullet->r = 1.0; bullet->g = 0.85; bullet->b = 0.2;
    bullet->visible = false;

    Object3D* ebullet = Add(s, PT_SPHERE, L"enemyBullet", NULL);
    ebullet->params[0] = 0.2;
    ebullet->x = 0; ebullet->y = 1.1; ebullet->z = -9;
    ebullet->r = 1.0; ebullet->g = 0.45; ebullet->b = 0.2;
    ebullet->visible = false;

    // 游戏逻辑改为文档类实现（纯脚本驱动，桌面调试与导出 HTML 执行同一份；
    // 类体内 var 字段保存游戏状态，onEnterFrame 约定自动每帧驱动，键盘用舞台事件）
    s.docClass =
        L"package {\r\n"
        L"\timport super3d.display.Object3D;\r\n"
        L"\timport super3d.events.Event;\r\n"
        L"\timport super3d.events.KeyboardEvent;\r\n"
        L"\tpublic class main extends Object3D {\r\n"
        L"\t\tvar score = 0;\r\n"
        L"\t\tvar hp = 100;\r\n"
        L"\t\tvar en1 = 1; var en2 = 1; var en3 = 1;\r\n"
        L"\t\tvar d1 = 1; var d2 = -1; var d3 = 1;\r\n"
        L"\t\tvar ba = 0; var bx = 0; var bz = 0; var bd = 0;\r\n"
        L"\t\tvar ea = 0; var ex = 0; var ez = 0; var ed = 0;\r\n"
        L"\t\tvar cd = 0; var cde = 0; var over = 0;\r\n"
        L"\t\tvar mv = 0; var tr = 0; var fk = 0;\r\n"
        L"\t\tpublic function main() {\r\n"
        L"\t\t\tstage.addEventListener(KeyboardEvent.KEY_DOWN, onKeyDown);\r\n"
        L"\t\t\tstage.addEventListener(KeyboardEvent.KEY_UP, onKeyUp);\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function onKeyDown(e) {\r\n"
        L"\t\t\tif (e == 65 || e == 37) tr = 1;\r\n"
        L"\t\t\tif (e == 68 || e == 39) tr = -1;\r\n"
        L"\t\t\tif (e == 87 || e == 38) mv = 1;\r\n"
        L"\t\t\tif (e == 83 || e == 40) mv = -1;\r\n"
        L"\t\t\tif (e == 32) fk = 1;\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function onKeyUp(e) {\r\n"
        L"\t\t\tif (e == 65 || e == 37) { if (tr == 1) tr = 0; }\r\n"
        L"\t\t\tif (e == 68 || e == 39) { if (tr == -1) tr = 0; }\r\n"
        L"\t\t\tif (e == 87 || e == 38) { if (mv == 1) mv = 0; }\r\n"
        L"\t\t\tif (e == 83 || e == 40) { if (mv == -1) mv = 0; }\r\n"
        L"\t\t\tif (e == 32) fk = 0;\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function hitWall(x, z) {\r\n"
        L"\t\t\tif (Math.abs(x) < 3.5 && Math.abs(z) < 3.5) return true;\r\n"
        L"\t\t\tif (Math.abs(x + 10) < 1.5 && Math.abs(z + 10) < 1.5) return true;\r\n"
        L"\t\t\tif (Math.abs(x - 10) < 1.5 && Math.abs(z - 10) < 1.5) return true;\r\n"
        L"\t\t\tif (Math.abs(x + 10) < 1.5 && Math.abs(z - 12) < 1.5) return true;\r\n"
        L"\t\t\tif (Math.abs(x - 10) < 1.5 && Math.abs(z + 12) < 1.5) return true;\r\n"
        L"\t\t\treturn false;\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function playerFire() {\r\n"
        L"\t\t\tba = 1;\r\n"
        L"\t\t\tbd = playerTank.ry;\r\n"
        L"\t\t\tbx = playerTank.x - sin(bd) * 2.0;\r\n"
        L"\t\t\tbz = playerTank.z - cos(bd) * 2.0;\r\n"
        L"\t\t\tbullet.visible = true;\r\n"
        L"\t\t\tbullet.ry = bd;\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function onEnterFrame(e:Event) {\r\n"
        L"\t\t\tif (over == 1) return;\r\n"
        L"\t\t\t// 1) player turn: written by keyDown/keyUp events (A/D or Left/Right)\r\n"
        L"\t\t\tif (tr == 1) playerTank.ry = playerTank.ry + 3;\r\n"
        L"\t\t\tif (tr == -1) playerTank.ry = playerTank.ry - 3;\r\n"
        L"\t\t\t// 2) player move: mv=+-1 along heading (blocked by wall/obstacles, field bounds)\r\n"
        L"\t\t\tvar sp = 0.15;\r\n"
        L"\t\t\tvar nx = playerTank.x;\r\n"
        L"\t\t\tvar nz = playerTank.z;\r\n"
        L"\t\t\tif (mv == 1) { nx = playerTank.x - sin(playerTank.ry) * sp; nz = playerTank.z - cos(playerTank.ry) * sp; }\r\n"
        L"\t\t\tif (mv == -1) { nx = playerTank.x + sin(playerTank.ry) * sp; nz = playerTank.z + cos(playerTank.ry) * sp; }\r\n"
        L"\t\t\tif (!hitWall(nx, nz)) { playerTank.x = nx; playerTank.z = nz; }\r\n"
        L"\t\t\tif (playerTank.x > 20) playerTank.x = 20;\r\n"
        L"\t\t\tif (playerTank.x < -20) playerTank.x = -20;\r\n"
        L"\t\t\tif (playerTank.z > 20) playerTank.z = 20;\r\n"
        L"\t\t\tif (playerTank.z < -20) playerTank.z = -20;\r\n"
        L"\t\t\t// 3) fire (Space keyDown sets fk, with cooldown; gun fixed to tank heading)\r\n"
        L"\t\t\tcd = cd - 1; if (cd < 0) cd = 0;\r\n"
        L"\t\t\tif (fk == 1 && ba == 0 && cd == 0) { playerFire(); cd = 14; }\r\n"
        L"\t\t\t// 4) player bullet flight + hit test (spherical bullet, hit radius 1.5)\r\n"
        L"\t\t\tif (ba == 1) {\r\n"
        L"\t\t\t\tbx = bx - sin(bd) * 0.55;\r\n"
        L"\t\t\t\tbz = bz - cos(bd) * 0.55;\r\n"
        L"\t\t\t\tbullet.x = bx; bullet.z = bz;\r\n"
        L"\t\t\t\tif (en1 == 1 && Math.abs(bx - enemy1.x) < 1.5 && Math.abs(bz - enemy1.z) < 1.5) { en1 = 0; enemy1.visible = false; enemy1Gun.visible = false; ba = 0; bullet.visible = false; score = score + 10; }\r\n"
        L"\t\t\t\tif (en2 == 1 && Math.abs(bx - enemy2.x) < 1.5 && Math.abs(bz - enemy2.z) < 1.5) { en2 = 0; enemy2.visible = false; enemy2Gun.visible = false; ba = 0; bullet.visible = false; score = score + 10; }\r\n"
        L"\t\t\t\tif (en3 == 1 && Math.abs(bx - enemy3.x) < 1.5 && Math.abs(bz - enemy3.z) < 1.5) { en3 = 0; enemy3.visible = false; enemy3Gun.visible = false; ba = 0; bullet.visible = false; score = score + 10; }\r\n"
        L"\t\t\t\tif (hitWall(bx, bz) || Math.abs(bx) > 20 || Math.abs(bz) > 20) { ba = 0; bullet.visible = false; }\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// 5) enemy patrol (only alive ones move)\r\n"
        L"\t\t\tif (en1 == 1) { enemy1.x = enemy1.x + 0.07 * d1; if (enemy1.x > 15) d1 = -1; if (enemy1.x < -15) d1 = 1; }\r\n"
        L"\t\t\tif (en2 == 1) { enemy2.x = enemy2.x + 0.06 * d2; if (enemy2.x > 15) d2 = -1; if (enemy2.x < -15) d2 = 1; }\r\n"
        L"\t\t\tif (en3 == 1) { enemy3.x = enemy3.x + 0.065 * d3; if (enemy3.x > 15) d3 = -1; if (enemy3.x < -15) d3 = 1; }\r\n"
        L"\t\t\t// 6) enemy turns toward player (gun fixed to body, fires at player)\r\n"
        L"\t\t\tif (en1 == 1) { enemy1.ry = Math.atan2(enemy1.x - playerTank.x, enemy1.z - playerTank.z) * 180 / Math.PI; }\r\n"
        L"\t\t\tif (en2 == 1) { enemy2.ry = Math.atan2(enemy2.x - playerTank.x, enemy2.z - playerTank.z) * 180 / Math.PI; }\r\n"
        L"\t\t\tif (en3 == 1) { enemy3.ry = Math.atan2(enemy3.x - playerTank.x, enemy3.z - playerTank.z) * 180 / Math.PI; }\r\n"
        L"\t\t\t// 7) enemy fire (first alive enemy, with cooldown)\r\n"
        L"\t\t\tcde = cde - 1; if (cde < 0) cde = 0;\r\n"
        L"\t\t\tif (ea == 0 && cde == 0) {\r\n"
        L"\t\t\t\tif (en1 == 1) { ea = 1; ed = enemy1.ry; ex = enemy1.x - sin(ed) * 2.0; ez = enemy1.z - cos(ed) * 2.0; enemyBullet.visible = true; enemyBullet.ry = ed; cde = 40; }\r\n"
        L"\t\t\t\telse if (en2 == 1) { ea = 1; ed = enemy2.ry; ex = enemy2.x - sin(ed) * 2.0; ez = enemy2.z - cos(ed) * 2.0; enemyBullet.visible = true; enemyBullet.ry = ed; cde = 40; }\r\n"
        L"\t\t\t\telse if (en3 == 1) { ea = 1; ed = enemy3.ry; ex = enemy3.x - sin(ed) * 2.0; ez = enemy3.z - cos(ed) * 2.0; enemyBullet.visible = true; enemyBullet.ry = ed; cde = 40; }\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// 8) enemy bullet flight + hit player (5 HP per hit)\r\n"
        L"\t\t\tif (ea == 1) {\r\n"
        L"\t\t\t\tex = ex - sin(ed) * 0.5;\r\n"
        L"\t\t\t\tez = ez - cos(ed) * 0.5;\r\n"
        L"\t\t\t\tenemyBullet.x = ex; enemyBullet.z = ez;\r\n"
        L"\t\t\t\tif (Math.abs(ex - playerTank.x) < 1.5 && Math.abs(ez - playerTank.z) < 1.5) { ea = 0; enemyBullet.visible = false; hp = hp - 5; }\r\n"
        L"\t\t\t\tif (hitWall(ex, ez) || Math.abs(ex) > 20 || Math.abs(ez) > 20) { ea = 0; enemyBullet.visible = false; }\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// 9) HUD / win-loss\r\n"
        L"\t\t\tif (hp <= 0) { over = 1; hudText = \"Game Over! Score=\" + score + \" Click to restart\"; }\r\n"
        L"\t\t\telse if (en1 == 0 && en2 == 0 && en3 == 0) { over = 1; hudText = \"Victory! All enemies destroyed Score=\" + score + \" Click to restart\"; }\r\n"
        L"\t\t\telse { hudText = \"Tank Battle  HP=\" + hp + \"  Score=\" + score + \"   W/S move A/D turn Space fire\"; }\r\n"
        L"\t\t}\r\n"
        L"\t}\r\n"
        L"}\r\n";
    s.script.clear();
    s.scriptPerFrame = false;
    s.fps = 24; s.totalFrames = 300; s.loopPlay = true;
    s.cam.yaw = -45.0; s.cam.pitch = 30.0; s.cam.dist = 26.0;
    s.cam.tx = 0.0; s.cam.ty = 0.0; s.cam.tz = 0.0;
}

// 文档类示例：mc 立方体 + 外部 .ss 文档类（onEnterFrame 每帧自转 + 缩放）
void BuildDocClass(Scene3D& s) {
    s.Clear();
    Object3D* mc = Add(s, PT_BOX, L"mc", NULL);
    mc->params[0] = 2.0; mc->params[1] = 2.0; mc->params[2] = 2.0;
    mc->segments = 2;
    mc->y = 1.0;        // 浮于地面（s3d: tf py=1）
    mc->ry = 62.0;      // 初始朝向（s3d: tf ry=62）
    mc->r = 0.92; mc->g = 0.62; mc->b = 0.28;

    s.docClass =
        L"package {\r\n"
        L"\timport super3d.display.Object3D;\r\n"
        L"\timport super3d.events.Event;\r\n"
        L"\tpublic class main extends Object3D {\r\n"
        L"\t\tpublic function main() {\r\n"
        L"\t\t    stage.addEventListener(Event.ENTER_FRAME, onEnterFrame);\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function onEnterFrame(e:Event):void {\r\n"
        L"\t\t\tmc.rx++;\r\n"
        L"\t\t\tmc.ry++;\r\n"
        L"\t\t\tmc.rz++;\r\n"
        L"\t\t\tmc.s += 0.01;\r\n"
        L"\t\t}\r\n"
        L"\t}\r\n"
        L"}\r\n";
    s.scriptPerFrame = false;
    s.fps = 24; s.totalFrames = 120; s.loopPlay = true;
}

// 柳树示例：树干 + 8 根形态各异的垂枝（起点高度/上翘/外伸/叶片数均不同）+ 细长柳叶
// 叶片只长在枝条悬垂段（远离树干），叶柄端贴枝、叶尖下垂
// 曲线采样：6 控制点线性插值（与 ctrlPts 相同的分段）
static double WillowSeg(double t, const double* v) {
    double f = t * 5.0;
    int i0 = (int)f; if (i0 > 4) i0 = 4;
    double fr = f - i0;
    return v[i0] + (v[i0 + 1] - v[i0]) * fr;
}
void BuildWillow(Scene3D& s) {
    s.Clear();
    const double PI2 = 6.283185307179586;

    // —— 树干：粗圆柱，灰褐色，高 2.6，底贴地（SetStandingY 自动抬 y=1.3）——
    Object3D* trunk = Add(s, PT_CYLINDER, L"trunk", NULL);
    trunk->params[0] = 0.30;                  // 半径
    trunk->params[1] = 2.6;                   // 高
    trunk->r = 0.42; trunk->g = 0.31; trunk->b = 0.17;   // 棕褐色

    // —— 主枝：8 根环绕，起点高度/上翘/外伸/下垂姿态各不相同 ——
    for (int b = 0; b < 8; b++) {
        double ang = b * PI2 / 8.0 + 0.18 * (b % 3);           // 环绕角度（微错开）
        double dx = cos(ang), dz = sin(ang);
        double y0    = 2.20 + 0.30 * (b % 3) / 2.0;            // 起点高度 2.2..2.5
        double rise  = 0.50 + 0.30 * ((b * 2 + 1) % 3) / 2.0;  // 上翘 0.5..0.8
        double reach = 0.90 + 0.70 * ((b * 7) % 5) / 4.0;      // 外伸 0.9..1.6
        double bxv[6] = { 0.00, 0.50, 1.00, 1.15, 1.08, 0.92 };
        double byv[6] = { y0, y0 + rise, y0 + rise * 0.8, y0 * 0.55, y0 * 0.25, 0.00 };

        Object3D* twig = Add(s, PT_SPLINE, L"willowBranch", NULL);
        twig->params[0] = 0.050 + 0.010 * (b % 2);             // 枝管径（略有差异）
        twig->r = 0.33; twig->g = 0.42; twig->b = 0.17;        // 深绿棕
        twig->ctrlPts.clear();
        for (int i = 0; i < 6; i++)
            twig->ctrlPts.push_back(Vec3(dx * reach * bxv[i], byv[i], dz * reach * bxv[i]));

        // 细长柳叶：只长在枝条悬垂段（t=0.62..0.98），每枝叶片数不同
        int nl = 5 + (b % 3);                                   // 5..7 片
        for (int i = 0; i < nl; i++) {
            double t = 0.62 + 0.36 * i / (nl - 1);
            double side = (i % 2) ? 1.0 : -1.0;
            Object3D* lf = Add(s, PT_LEAF, L"willowLeaf", NULL);
            lf->params[0] = 0.75 + 0.35 * ((b * 5 + i * 3) % 5) / 4.0;  // 叶长 0.75..1.1（随枝变化）
            lf->params[1] = 0.08 + 0.02 * (i % 2);             // 叶宽（窄）
            lf->params[2] = 0.025;                              // 叶厚
            lf->x = dx * reach * WillowSeg(t, bxv) + side * (0.09 + 0.02 * (b % 2));
            lf->y = WillowSeg(t, byv) + 0.03;
            lf->z = dz * reach * WillowSeg(t, bxv) + side * (0.06 + 0.02 * (b % 2));
            lf->rz = 78.0 + side * 12.0 + 6.0 * (b % 3);        // 叶柄端贴枝、叶尖下垂
            lf->rx = 6.0 + 6.0 * (i % 3);                       // 微倾
            lf->r = 0.15 + 0.05 * (i % 2);                      // 两种绿
            lf->g = 0.52 + 0.08 * ((i + b) % 3);
            lf->b = 0.13 + 0.04 * (i % 2);
        }
    }
    s.scriptPerFrame = false;
    s.fps = 24; s.totalFrames = 60; s.loopPlay = true;
    // 相机：整棵树入画（树高约 3、冠幅约 3.6）
    s.cam.yaw = 50.0; s.cam.pitch = 35.0; s.cam.dist = 8.5;
    s.cam.tx = 0.0; s.cam.ty = 1.2; s.cam.tz = 0.0;
}

// 精密火箭示例：两级箭体 + 整流罩 + 舷窗/条纹/尾翼/喷嘴 + 双助推器 + 发射台塔架
// 完整发射动画（docClass onEnterFrame 逐帧驱动，160 帧循环）：
//   点火脉动 → 二次加速升空 → 助推器分离 → 高空倾斜 → 整流罩分离（载荷露出）→ 降落伞回收
//   全程火焰/烟雾特效（颜色、缩放、可见性由帧号驱动，确定性可复现）
void BuildRocket(Scene3D& s) {
    s.Clear();

    // ---------- 地面 ----------
    Object3D* ground = Add(s, PT_PLANE, L"ground", NULL);
    ground->params[0] = 40.0;
    ground->r = 0.42; ground->g = 0.46; ground->b = 0.42;   // 发射场深灰绿

    // ---------- 发射台：底座 + 四支腿 + 发射座环 ----------
    Object3D* pad = Add(s, PT_BOX, L"pad", NULL);
    pad->params[0] = 2.6; pad->params[1] = 0.35; pad->params[2] = 2.6;
    pad->r = 0.62; pad->g = 0.62; pad->b = 0.6;
    for (int i = 0; i < 4; i++) {
        Object3D* leg = Add(s, PT_BOX, L"padLeg", NULL);
        leg->params[0] = 0.14; leg->params[1] = 0.5; leg->params[2] = 0.14;
        leg->x = (i < 2 ? -1.05 : 1.05);
        leg->z = (i % 2 ? -1.05 : 1.05);
        leg->r = 0.3; leg->g = 0.3; leg->b = 0.3;
    }
    Object3D* ring = Add(s, PT_TORUS, L"padRing", NULL);
    ring->params[0] = 0.58; ring->params[1] = 0.06;
    ring->segments = 48;
    ring->y = 0.38;
    ring->r = 0.75; ring->g = 0.3; ring->b = 0.25;

    // ---------- 发射塔架：立柱 + 两条横臂 + 两根斜撑 + 指示灯 ----------
    Object3D* tower = Add(s, PT_BOX, L"tower", NULL);
    tower->params[0] = 0.5; tower->params[1] = 7.0; tower->params[2] = 0.5;
    tower->x = -3.2;
    tower->r = 0.55; tower->g = 0.55; tower->b = 0.58;
    Object3D* arm1 = Add(s, PT_BOX, L"arm1", NULL);
    arm1->params[0] = 0.12; arm1->params[1] = 0.12; arm1->params[2] = 1.9;
    arm1->x = -2.45; arm1->y = 6.2;
    arm1->r = 0.5; arm1->g = 0.5; arm1->b = 0.53;
    Object3D* arm2 = Add(s, PT_BOX, L"arm2", NULL);
    arm2->params[0] = 0.12; arm2->params[1] = 0.12; arm2->params[2] = 1.4;
    arm2->x = -2.55; arm2->y = 4.0;
    arm2->r = 0.5; arm2->g = 0.5; arm2->b = 0.53;
    for (int i = 0; i < 2; i++) {
        Object3D* br = Add(s, PT_BOX, L"brace", NULL);
        br->params[0] = 0.1; br->params[1] = 2.3; br->params[2] = 0.1;
        br->x = -3.2 + (i ? 0.95 : -0.95);
        br->y = 2.0;
        br->rz = (i ? -30.0 : 30.0);
        br->r = 0.5; br->g = 0.5; br->b = 0.53;
    }
    Object3D* padLight = Add(s, PT_SPHERE, L"padLight", NULL);
    padLight->params[0] = 0.1;
    padLight->x = -0.8; padLight->y = 0.6; padLight->z = -0.8;
    padLight->r = 0.9; padLight->g = 0.3; padLight->b = 0.2;

    // ---------- 火箭组（锚点=发射台顶 y0.35，脚本每帧抬 y 实现升空） ----------
    Object3D* rocket = Add(s, PT_NONE, L"rocket", NULL);
    rocket->y = 0.35;

    // 一级箭体
    Object3D* body = Add(s, PT_CYLINDER, L"body", rocket);
    body->params[0] = 0.45; body->params[1] = 3.2;
    body->segments = 24;
    Put(body, 0, 1.6, 0);
    body->r = 0.93; body->g = 0.93; body->b = 0.95;   // 白色箭体
    // 整流罩（圆锥尖顶，底接箭体顶）
    Object3D* nose = Add(s, PT_CONE, L"nose", rocket);
    nose->params[0] = 0.45; nose->params[1] = 1.6;
    nose->segments = 24;
    Put(nose, 0, 4.0, 0);
    nose->r = 0.93; nose->g = 0.93; nose->b = 0.95;
    // 鼻锥球
    Object3D* tip = Add(s, PT_SPHERE, L"tip", rocket);
    tip->params[0] = 0.12;
    Put(tip, 0, 4.9, 0);
    tip->r = 0.85; tip->g = 0.2; tip->b = 0.2;
    // 舷窗 ×2
    for (int i = 0; i < 2; i++) {
        Object3D* win = Add(s, PT_SPHERE, L"window", rocket);
        win->params[0] = 0.15;
        Put(win, (i ? 0.44 : -0.44), 2.1, 0);
        win->r = 0.2; win->g = 0.5; win->b = 0.9;
    }
    // 色带条纹 ×2（红 / 蓝）
    for (int i = 0; i < 2; i++) {
        Object3D* band = Add(s, PT_TORUS, L"band", rocket);
        band->params[0] = 0.46; band->params[1] = 0.035;
        band->segments = 48;
        Put(band, 0, (i ? 2.7 : 1.1), 0);
        if (i == 0) { band->r = 0.85; band->g = 0.2; band->b = 0.2; }
        else { band->r = 0.2; band->g = 0.45; band->b = 0.85; }
    }
    // 尾翼 ×4（十字分布，薄板）
    for (int i = 0; i < 4; i++) {
        Object3D* fin = Add(s, PT_BOX, L"fin", rocket);
        fin->params[0] = 0.07; fin->params[1] = 0.6; fin->params[2] = 0.34;
        if (i < 2) { Put(fin, (i ? 0.47 : -0.47), 0.55, 0); }
        else { fin->params[0] = 0.34; fin->params[1] = 0.6; fin->params[2] = 0.07;
               Put(fin, 0, 0.55, (i == 2 ? 0.47 : -0.47)); }
        fin->r = 0.85; fin->g = 0.2; fin->b = 0.2;   // 红尾翼
    }
    // 喷嘴：倒圆锥（尖朝下），大圆嵌进箭体底、尖突出箭体 0.225
    Object3D* nozzle = Add(s, PT_CONE, L"nozzle", rocket);
    nozzle->params[0] = 0.3; nozzle->params[1] = 0.45;
    nozzle->segments = 20;
    nozzle->rx = 180.0;
    Put(nozzle, 0, 0.0, 0);
    nozzle->r = 0.25; nozzle->g = 0.25; nozzle->b = 0.27;

    // 主火焰：外层橙锥（尖朝下喷）+ 内芯亮黄球，脚本控制缩放脉动
    Object3D* flameOuter = Add(s, PT_CONE, L"flameOuter", rocket);
    flameOuter->params[0] = 0.34; flameOuter->params[1] = 1.5;
    flameOuter->segments = 20;
    flameOuter->rx = 180.0;
    Put(flameOuter, 0, -1.05, 0);
    flameOuter->r = 1.0; flameOuter->g = 0.55; flameOuter->b = 0.15;
    Object3D* flameCore = Add(s, PT_SPHERE, L"flameCore", rocket);
    flameCore->params[0] = 0.24;
    Put(flameCore, 0, -0.7, 0);
    flameCore->r = 1.0; flameCore->g = 0.85; flameCore->b = 0.3;

    // 双助推器（根级独立组：筒身 + 头锥 + 喷嘴 + 各自火焰）。
    // 分离前脚本让它随火箭同步上升，分离后外飞 + 下落（真正脱离火箭）
    for (int i = 0; i < 2; i++) {
        Object3D* bs = Add(s, PT_NONE, (i ? L"boosterR" : L"boosterL"), NULL);
        bs->x = (i ? 1.15 : -1.15); bs->y = 1.75; bs->z = 0;
        Object3D* bb = Add(s, PT_CYLINDER, (i ? L"boosterRBody" : L"boosterLBody"), bs);
        bb->params[0] = 0.2; bb->params[1] = 2.2;
        bb->segments = 18;
        Put(bb, 0, 1.1, 0);
        bb->r = 0.85; bb->g = 0.85; bb->b = 0.88;
        Object3D* bn = Add(s, PT_CONE, (i ? L"boosterRNose" : L"boosterLNose"), bs);
        bn->params[0] = 0.2; bn->params[1] = 0.7;
        bn->segments = 18;
        Put(bn, 0, 2.5, 0);
        bn->r = 0.85; bn->g = 0.85; bn->b = 0.88;
        Object3D* bz = Add(s, PT_CONE, (i ? L"boosterRNozzle" : L"boosterLNozzle"), bs);
        bz->params[0] = 0.14; bz->params[1] = 0.22;
        bz->segments = 14;
        bz->rx = 180.0;
        Put(bz, 0, -0.1, 0);
        bz->r = 0.25; bz->g = 0.25; bz->b = 0.27;
        // 助推器火焰：外锥 + 内芯
        Object3D* bf = Add(s, PT_CONE, (i ? L"boosterFlameR" : L"boosterFlameL"), bs);
        bf->params[0] = 0.16; bf->params[1] = 0.75;
        bf->segments = 14;
        bf->rx = 180.0;
        Put(bf, 0, -0.7, 0);
        bf->r = 1.0; bf->g = 0.5; bf->b = 0.12;
        Object3D* bc = Add(s, PT_SPHERE, (i ? L"boosterFlameRCore" : L"boosterFlameLCore"), bs);
        bc->params[0] = 0.11;
        Put(bc, 0, -0.5, 0);
        bc->r = 1.0; bc->g = 0.8; bc->b = 0.25;
    }

    // ---------- 载荷（整流罩内卫星，分离后露出） ----------
    Object3D* payload = Add(s, PT_NONE, L"payload", NULL);
    payload->visible = false;
    Object3D* sat = Add(s, PT_SPHERE, L"satellite", payload);
    sat->params[0] = 0.28;
    Put(sat, 0, 0, 0);
    sat->r = 0.75; sat->g = 0.75; sat->b = 0.8;
    for (int i = 0; i < 2; i++) {
        Object3D* sp = Add(s, PT_BOX, L"satPanel", payload);
        sp->params[0] = 0.55; sp->params[1] = 0.03; sp->params[2] = 1.3;
        Put(sp, 0, 0.1, (i ? 1.0 : -1.0));
        sp->r = 0.2; sp->g = 0.45; sp->b = 0.85;
    }

    // ---------- 降落伞（回收阶段张开） ----------
    Object3D* para = Add(s, PT_NONE, L"para", NULL);
    para->visible = false;
    Object3D* canopy = Add(s, PT_CONE, L"canopy", para);
    canopy->params[0] = 1.1; canopy->params[1] = 0.8;
    canopy->segments = 24;
    Put(canopy, 0, 0.8, 0);
    canopy->r = 0.95; canopy->g = 0.35; canopy->b = 0.3;   // 红白伞面（红）
    for (int i = 0; i < 3; i++) {
        Object3D* line = Add(s, PT_CYLINDER, L"paraLines", para);
        line->params[0] = 0.02; line->params[1] = 0.9;
        Put(line, (i - 1) * 0.45, 0.0, 0);
        line->r = 0.85; line->g = 0.85; line->b = 0.9;
    }

    // ---------- 烟雾 ×4（发射时喷出，上升扩散变淡） ----------
    // 名字必须唯一（smoke1..smoke4）：脚本按对象名驱动每个烟雾
    const double smX[4] = { 0.7, -0.7, 0.0, 1.2 };
    const double smZ[4] = { 0.3, 0.6, -1.0, 0.2 };
    for (int i = 0; i < 4; i++) {
        Object3D* sm = Add(s, PT_SPHERE, (std::wstring(L"smoke") + std::to_wstring(i + 1)).c_str(), NULL);
        sm->params[0] = 0.5;
        sm->x = smX[i]; sm->z = smZ[i];
        sm->y = 0.5;
        sm->r = 0.75; sm->g = 0.75; sm->b = 0.75;
        sm->visible = false;
    }

    // ---------- 文档类动画脚本（160 帧循环：点火→升空→分离→整流罩→降落伞） ----------
    s.docClass =
        L"package {\r\n"
        L"\timport super3d.display.Object3D;\r\n"
        L"\timport super3d.events.Event;\r\n"
        L"\tpublic class main extends Object3D {\r\n"
        L"\t\tpublic function main() {\r\n"
        L"\t\t\tstage.addEventListener(Event.ENTER_FRAME, onEnterFrame);\r\n"
        L"\t\t}\r\n"
        L"\t\tpublic function onEnterFrame(e:Event):void {\r\n"
        L"\t\t\tvar f = frame;\r\n"
        L"\t\t\tvar p = 0; var t = 0; var t2 = 0; var d = 0; var g = 0;\r\n"
        L"\t\t\t// ---- 主火焰：点火后随火箭，缩放脉动 ----\r\n"
        L"\t\t\tflameOuter.visible = (f >= 2);\r\n"
        L"\t\t\tflameCore.visible = (f >= 2);\r\n"
        L"\t\t\tif (f < 15) {\r\n"
        L"\t\t\t\tp = f / 15;\r\n"
        L"\t\t\t\tflameOuter.s = 0.25 + 0.75 * p + 0.3 * sin(f * 3);\r\n"
        L"\t\t\t\tflameCore.s = flameOuter.s * 0.6;\r\n"
        L"\t\t\t} else {\r\n"
        L"\t\t\t\tflameOuter.s = 1.15 + 0.45 * sin(f * 0.9);\r\n"
        L"\t\t\t\tflameCore.s = flameOuter.s * 0.62;\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// ---- 助推器火焰：分离(62帧)后熄灭 ----\r\n"
        L"\t\t\tboosterFlameL.visible = (f >= 2 && f < 62);\r\n"
        L"\t\t\tboosterFlameR.visible = (f >= 2 && f < 62);\r\n"
        L"\t\t\tboosterFlameL.s = 0.9 + 0.35 * sin(f * 1.2);\r\n"
        L"\t\t\tboosterFlameR.s = 0.9 + 0.35 * sin(f * 1.2);\r\n"
        L"\t\t\t// ---- 升空：二次加速 0.35→20，60帧后缓升+微倾，100帧后远去 ----\r\n"
        L"\t\t\tif (f < 15) {\r\n"
        L"\t\t\t\trocket.y = 0.35; rocket.rz = 0;\r\n"
        L"\t\t\t} else if (f < 60) {\r\n"
        L"\t\t\t\tt = (f - 15) / 45;\r\n"
        L"\t\t\t\trocket.y = 0.35 + 20 * t * t; rocket.rz = 0;\r\n"
        L"\t\t\t} else if (f < 100) {\r\n"
        L"\t\t\t\tt2 = (f - 60) / 40;\r\n"
        L"\t\t\t\trocket.y = 20 + 3 * t2; rocket.rz = 6 * t2;\r\n"
        L"\t\t\t} else {\r\n"
        L"\t\t\t\trocket.y = 23 + (f - 100) * 0.3; rocket.rz = 6 + (f - 100) * 0.2;\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// ---- 助推器：分离前随火箭同步上升，60-75帧外飞下坠，随后真正下落 ----\r\n"
        L"\t\t\tif (f < 60) {\r\n"
        L"\t\t\t\tboosterL.x = -1.15; boosterL.y = rocket.y + 1.4; boosterL.rz = 0;\r\n"
        L"\t\t\t\tboosterR.x = 1.15; boosterR.y = rocket.y + 1.4; boosterR.rz = 0;\r\n"
        L"\t\t\t} else if (f < 75) {\r\n"
        L"\t\t\t\td = (f - 60) / 15;\r\n"
        L"\t\t\t\tboosterL.x = -1.15 - 3.2 * d; boosterL.y = rocket.y + 1.4 - 0.4 * d; boosterL.rz = -18 * d;\r\n"
        L"\t\t\t\tboosterR.x = 1.15 + 3.2 * d; boosterR.y = rocket.y + 1.4 - 0.4 * d; boosterR.rz = 18 * d;\r\n"
        L"\t\t\t} else {\r\n"
        L"\t\t\t\tboosterL.x = -4.35 - (f - 75) * 0.06; boosterL.y = 22.1 - (f - 75) * 0.2; boosterL.rz = -18;\r\n"
        L"\t\t\t\tboosterR.x = 4.35 + (f - 75) * 0.06; boosterR.y = 22.1 - (f - 75) * 0.2; boosterR.rz = 18;\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// ---- 整流罩分离 + 载荷露出（100帧） ----\r\n"
        L"\t\t\tnose.visible = (f < 100);\r\n"
        L"\t\t\tpayload.visible = (f >= 100);\r\n"
        L"\t\t\tif (f >= 100) {\r\n"
        L"\t\t\t\tpayload.y = rocket.y - 2.0;\r\n"
        L"\t\t\t\tpayload.ry = (f - 100) * 3;\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// ---- 降落伞回收（115帧） ----\r\n"
        L"\t\t\tpara.visible = (f >= 115);\r\n"
        L"\t\t\tif (f >= 115) {\r\n"
        L"\t\t\t\tpara.y = payload.y + 0.8;\r\n"
        L"\t\t\t\tpara.ry = f * 5;\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// ---- 烟雾：上升扩散变淡，95帧后消散 ----\r\n"
        L"\t\t\tif (f >= 5 && f < 95) {\r\n"
        L"\t\t\t\tg = (f - 5) / 90;\r\n"
        L"\t\t\t\tsmoke1.y = 0.4 + g * 6; smoke1.s = 0.8 + g * 1.6;\r\n"
        L"\t\t\t\tsmoke1.red = 191 + g * 51; smoke1.green = 191 + g * 51; smoke1.blue = 191 + g * 51;\r\n"
        L"\t\t\t\tsmoke2.y = 0.4 + g * 5 + 0.5 * sin(f * 0.7); smoke2.s = 0.9 + g * 1.4;\r\n"
        L"\t\t\t\tsmoke2.red = 178 + g * 64; smoke2.green = 178 + g * 64; smoke2.blue = 178 + g * 64;\r\n"
        L"\t\t\t\tsmoke3.x = 0.7 + g * 0.8; smoke3.y = 0.4 + g * 5.5; smoke3.s = 0.7 + g * 1.5;\r\n"
        L"\t\t\t\tsmoke3.red = 204 + g * 38; smoke3.green = 204 + g * 38; smoke3.blue = 204 + g * 38;\r\n"
        L"\t\t\t\tsmoke4.x = 1.2 - g * 1.0; smoke4.y = 0.4 + g * 5; smoke4.s = 0.75 + g * 1.4;\r\n"
        L"\t\t\t\tsmoke4.red = 184 + g * 56; smoke4.green = 184 + g * 56; smoke4.blue = 184 + g * 56;\r\n"
        L"\t\t\t\tsmoke1.visible = true; smoke2.visible = true; smoke3.visible = true; smoke4.visible = true;\r\n"
        L"\t\t\t} else {\r\n"
        L"\t\t\t\tsmoke1.visible = false; smoke2.visible = false; smoke3.visible = false; smoke4.visible = false;\r\n"
        L"\t\t\t}\r\n"
        L"\t\t\t// ---- 发射台指示灯闪烁（red 0~255） ----\r\n"
        L"\t\t\tpadLight.red = 102 + 153 * abs(sin(f * 0.8));\r\n"
        L"\t\t}\r\n"
        L"\t}\r\n"
        L"}\r\n";
    s.script.clear();
    s.scriptPerFrame = false;
    s.fps = 24; s.totalFrames = 160; s.loopPlay = true;
    // 相机：侧前视角，视野中心对准发射台中上部（火箭最高约 40 末尾远去，分离/整流罩/降落伞全程可见）
    s.cam.yaw = -38.0; s.cam.pitch = 24.0; s.cam.dist = 42.0;
    s.cam.tx = 0.0; s.cam.ty = 10.5; s.cam.tz = 0.0;
}

void Load(Scene3D& s, int which) {
    s.frame = 0;                     // 先归零：内置示例对象统一挂到第 0 帧关键帧，避免挂到上次停留帧
    switch (which) {
        case 0: BuildHouse(s); break;
        case 1: BuildWindmill(s); break;
        case 2: BuildSolarSystem(s); break;
        case 3: BuildBounceBall(s); break;
        case 4: BuildDocClass(s); break;
        case 5: BuildTankBattle(s); break;
        case 6: BuildWillow(s); break;
        case 7: BuildRocket(s); break;
        default: BuildBounceBall(s); break;
    }
    // 位置已全部设置完：用最新本地值刷新第 0 帧关键帧快照
    // （AddObject 挂帧时快照的是 CreateByType 默认值，不刷新会被 SetFrame(0) 拉回原点）
    for (size_t i = 0; i < s.roots.size(); i++) {
        Object3D* r = s.roots[i];
        if (r && !r->parent) s.AddObjectToLayerKey(r->layer, 0, r);
    }
    s.SetFrame(0);
    s.SyncDisplayToLocalAll();
}

}
