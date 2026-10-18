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
// diag_keyframe.cpp : 验证删除组内子对象时父组关键帧是否被误移除
#include <cstdio>
#include <string>
#include "Scene3D.h"
#include "ProjectIO.h"
#include "Examples.h"

static int CountKeyRefs(Scene3D& s, Object3D* target) {
    int n = 0;
    for (size_t li = 0; li < s.layers.size(); li++) {
        for (size_t i = 0; i < s.layers[li].keys.size(); i++) {
            for (size_t j = 0; j < s.layers[li].keys[i].objs.size(); j++) {
                if (s.layers[li].keys[i].objs[j] == target) n++;
            }
        }
    }
    return n;
}

int main() {
    Scene3D s;
    Examples::Load(s, 7);   // Rocket
    // 找 rocket 组和顶部红球 tip
    Object3D* rocket = NULL, * tip = NULL;
    std::vector<Object3D*> all;
    s.CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) {
        if (all[i]->name == L"rocket") rocket = all[i];
        if (all[i]->name == L"tip") tip = all[i];
    }
    if (!rocket || !tip) { wprintf(L"NOT FOUND rocket=%p tip=%p\n", (void*)rocket, (void*)tip); return 2; }
    wprintf(L"tip->parent == rocket : %s\n", (tip->parent == rocket) ? L"YES" : L"NO");
    wprintf(L"before: keyRefs(rocket)=%d keyRefs(tip)=%d\n", CountKeyRefs(s, rocket), CountKeyRefs(s, tip));
    // 模拟 DoDelete 删除 tip 前的调用
    s.RemoveObjectFromAllLayerKeys(tip);
    wprintf(L"after RemoveObjectFromAllLayerKeys(tip): keyRefs(rocket)=%d keyRefs(tip)=%d\n",
        CountKeyRefs(s, rocket), CountKeyRefs(s, tip));
    s.DeleteObject(tip);
    wprintf(L"after DeleteObject(tip): rocket still in scene=%s, rocket->children=%d\n",
        (s.FindByName(L"rocket") != NULL) ? L"YES" : L"NO", (int)rocket->children.size());
    return 0;
}
