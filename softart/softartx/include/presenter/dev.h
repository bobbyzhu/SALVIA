/********************************************************************
Copyright (C) 2007-2008 Ye Wu

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

purpose:	���ļ��ṩ��ƽ̨�޹ص���ʾ�豸�Ľӿڡ�ͨ����ʾ�豸���Խ������Ⱦ���ĺ󱸻�����Ⱦ����Ļ�ϡ�

Modify Log:
		
*********************************************************************/

#ifndef SOFTARTX_DEV_H
#define SOFTARTX_DEV_H

#include "softart/include/dev.h"

enum device_type
{
	devtype_d3d9 = 0,
	devtype_d3d10 = 1,
	devtype_opengl = 2,
	devtype_gdiplus = 3
};

struct IDirect3DDevice9;

struct device_info
{
	intptr_t pdevice;
	device_type dev_type;
};

struct device_impl : public device
{
	virtual const device_info& get_physical_device() = 0;
};

#endif //SOFTARTX_DEV_H