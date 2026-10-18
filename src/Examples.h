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
// Examples.h : 内置示例场景（不依赖 MFC）
#pragma once
#include "Scene3D.h"

namespace Examples {

    // 小房子（静态场景，含输出脚本示例）
    void BuildHouse(Scene3D& s);
    // 风车（圆台地基 + 细圆柱塔 + 中心十字叶片组逐帧旋转）
    void BuildWindmill(Scene3D& s);
    // 太阳系（轨道圆环 + 行星逐帧公转脚本）
    void BuildSolarSystem(Scene3D& s);
    // 弹跳球（逐帧上下跳动 + 自转脚本）
    void BuildBounceBall(Scene3D& s);
    // 3D 坦克大战（纯脚本游戏：onEnterFrame/stage 键盘事件/hudText 驱动，导出与桌面一致可玩）
    void BuildTankBattle(Scene3D& s);
    // 文档类示例（外部 .ss 文档类驱动 mc 自转 + 缩放）
    void BuildDocClass(Scene3D& s);
    // 柳条（曲线枝条 + 绿叶叶片，静态垂柳示例）
    void BuildWillow(Scene3D& s);

    // 清除并载入指定示例
    void Load(Scene3D& s, int which); // 0=House 1=Windmill 2=SolarSystem 3=BounceBall 4=DocClass 5=TankBattle 6=Willow
}
