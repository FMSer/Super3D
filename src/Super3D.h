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
// Super3D.h : 应用程序主头文件
#pragma once

#ifndef __AFXWIN_H__
#error "请包含 stdafx.h 之后再包含此文件"
#endif

class CSuper3DApp : public CWinApp {
public:
    CSuper3DApp();
    virtual BOOL InitInstance();
};

extern CSuper3DApp theApp;
