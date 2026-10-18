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
// gen_samples.cpp : 一次性工具——把内置示例（Examples.cpp 的 BuildXXX）生成为
// sample\<名字>\<名字>.s3d 独立文件夹形式（每个示例一个文件夹），同名 .ss 文档类脚本。
// 以后内置示例改为直接提供 sample\<名字>\<名字>.s3d，程序示例菜单动态扫描子目录。
#include <cstdio>
#include <string>
#include <windows.h>
#include "Scene3D.h"
#include "ProjectIO.h"
#include "Examples.h"

int main() {
    const wchar_t* kNames[] = {
        L"House", L"Windmill", L"SolarSystem", L"BounceBall",
        L"DocClass", L"TankBattle", L"Willow", L"Rocket"
    };
    const int kCount = 8;
    CreateDirectoryW(L"sample", NULL);
    int ok = 0;
    for (int i = 0; i < kCount; i++) {
        Scene3D s;
        Examples::Load(s, i);            // 构建 + 挂第 0 帧关键帧
        std::wstring sub = std::wstring(L"sample\\") + kNames[i];
        CreateDirectoryW(sub.c_str(), NULL);
        std::wstring path = sub + L"\\" + kNames[i] + L".s3d";
        std::wstring err;
        if (ProjectIO::SaveProject(s, path, err)) {
            wprintf(L"[OK] %s\n", path.c_str());
            ok++;
        } else {
            wprintf(L"[FAIL] %s : %s\n", path.c_str(), err.c_str());
        }
    }
    wprintf(L"%d/%d samples generated.\n", ok, kCount);
    return ok == kCount ? 0 : 1;
}
