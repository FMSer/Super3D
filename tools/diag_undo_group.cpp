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
// diag_undo_group.cpp : 复现"进组删除子对象后撤销"的崩溃（核心链路，不含 MFC UI）
#include <cstdio>
#include <string>
#include "Scene3D.h"
#include "ProjectIO.h"
#include "Examples.h"

int main() {
    Scene3D s;
    Examples::Load(s, 5);   // TankBattle
    Object3D* tank = NULL, * gun = NULL;
    std::vector<Object3D*> all;
    s.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) {
        if (all[i]->name == L"playerTank") tank = all[i];
        if (all[i]->name == L"playerGun") gun = all[i];
    }
    wprintf(L"tank=%p gun=%p gunParentIsTank=%s\n", (void*)tank, (void*)gun,
        (gun && tank && gun->parent == tank) ? L"YES" : L"NO");
    if (!tank || !gun) return 2;

    // 模拟双击进入组
    s.editRoot = tank;

    // 1) PushUndo：快照删除前状态
    std::wstring snapText = ProjectIO::SerializeToString(s, true);
    wprintf(L"snapText=%d chars, gunInSnap=%s\n", (int)snapText.size(),
        (snapText.find(L"playerGun") != std::wstring::npos) ? L"YES" : L"NO");

    // 2) DoDelete：删除炮筒
    s.RemoveObjectFromAllLayerKeys(gun);
    s.DeleteObject(gun);
    wprintf(L"after delete: gun=%s tankChildren=%d\n",
        (s.FindByName(L"playerGun") != NULL) ? L"still?" : L"gone",
        (int)tank->children.size());

    // 3) CollectUndoSnap(cur)（删除后状态序列化）
    std::wstring curText = ProjectIO::SerializeToString(s, true);
    wprintf(L"curText=%d chars\n", (int)curText.size());

    // 4) RestoreSnap：反序列化回删除前
    std::wstring err;
    if (!ProjectIO::DeserializeFromString(s, snapText, err)) {
        wprintf(L"DESERIALIZE FAIL: %s\n", err.c_str());
        return 3;
    }
    wprintf(L"restored: playerGun=%s tank=%s\n",
        (s.FindByName(L"playerGun") != NULL) ? L"YES" : L"NO",
        (s.FindByName(L"playerTank") != NULL) ? L"YES" : L"NO");
    s.SetFrame(0);   // 应用关键帧（模拟 RestoreSnap 的 SetFrame）
    wprintf(L"SetFrame(0) OK, editRoot=%s\n", s.editRoot ? L"set" : L"NULL");
    wprintf(L"ALL OK\n");
    return 0;
}
