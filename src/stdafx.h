// stdafx.h : 预编译头
#pragma once

#define VC_EXTRALEAN
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define _WIN32_IE  0x0501

#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>
#include <afxdlgs.h>
#include <afxrich.h>   // CRichEditCtrl / AfxInitRichEdit（.ss 编辑器语法着色）
#ifndef EM_SETREDRAW
#define EM_SETREDRAW 0x00B3   // RichEdit 抑制/恢复重绘（部分头文件组合下未声明）
#endif

#include <gl/gl.h>
#include <gl/glu.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <cstdio>
