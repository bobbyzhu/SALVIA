/********************************************************************
Copyright (C) 2007-2010 Ye Wu, Minmin Gong

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

created:	2008/06/08
author:		Ye Wu

purpose:	GDI+ Device, Will Be Obseleted

Modify Log:
		
*********************************************************************/
#ifndef SALVIAXDEV_GDIPLUS_H
#define SALVIAXDEV_GDIPLUS_H

#include "salviar/include/presenter_dev.h"
#include "salviax/include/utility/inc_gdiplus.h"
#include "salviar/include/framebuffer.h"
#include <tchar.h>
#include <boost/smart_ptr.hpp>
#include <algorithm>
#include <stdio.h>

#define BEGIN_NS_SALVIAXPRESENTER() namespace softartx{ namespace presenter{
#define END_NS_SALVIAXPRESENTER() }}

BEGIN_NS_SALVIAXPRESENTER()

class dev_gdiplus;
DECL_HANDLE(dev_gdiplus, h_dev_gdiplus)

class dev_gdiplus: public salviar::device{
	dev_gdiplus(HWND hwnd);

	HWND hwnd_;
	boost::shared_ptr<Gdiplus::Bitmap> pbmp_;

public:
	~dev_gdiplus();
	static h_dev_gdiplus create_device(HWND hwnd);

	virtual void present(const salviar::surface& surf);
};

END_NS_SALVIAXPRESENTER()

class gdiplus_initializer
{
	Gdiplus::GdiplusStartupInput gdiPlusInput_;
	ULONG_PTR gdiPlusToken_;
public:
	gdiplus_initializer()
	{
#ifdef _CONSOLE
		_tprintf(_T("%s"), _T("Gdiplus Initialized.\n"));
#else
		Gdiplus::GdiplusStartup(&gdiPlusToken_, &gdiPlusInput_, NULL);
#endif
	}
	~gdiplus_initializer()
	{
#ifdef _CONSOLE
		_tprintf(_T("%s"), _T("Gdiplus Shutdown.\n"));
#else
		Gdiplus::GdiplusShutdown(gdiPlusToken_);
#endif
	}
};

#ifdef salviax_gdiplus_presenter_EXPORTS
	#define SALVIAX_API __declspec(dllexport)
#else
	#define SALVIAX_API __declspec(dllimport)
#endif

extern "C"
{
	SALVIAX_API void salviax_create_presenter_device(salviar::h_device& dev, void* param);
}

#endif //SALVIAXDEV_GDIPLUS_H
