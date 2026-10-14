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
        L"\timport super3d.Object3D;\r\n"
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
        L"\timport super3d.Object3D;\r\n"
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
        L"\timport super3d.Object3D;\r\n"
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
        L"\timport super3d.Object3D;\r\n"
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
        L"\timport super3d.Object3D;\r\n"
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
        L"\timport super3d.Object3D;\r\n"
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

void Load(Scene3D& s, int which) {
    switch (which) {
        case 0: BuildHouse(s); break;
        case 1: BuildWindmill(s); break;
        case 2: BuildSolarSystem(s); break;
        case 3: BuildBounceBall(s); break;
        case 4: BuildDocClass(s); break;
        case 5: BuildTankBattle(s); break;
        default: BuildBounceBall(s); break;
    }
    s.frame = 0;
    s.SetFrame(0);
    s.SyncDisplayToLocalAll();
}

}
