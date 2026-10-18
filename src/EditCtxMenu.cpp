// EditCtxMenu.cpp : context-menu (copy/cut/clear) + Ctrl+X for read-only edit controls.
#include "stdafx.h"
#include "EditCtxMenu.h"

BEGIN_MESSAGE_MAP(CEditCtxMenu, CEdit)
    ON_WM_CONTEXTMENU()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()
