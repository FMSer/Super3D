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
