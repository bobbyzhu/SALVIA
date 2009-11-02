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

purpose:	�ļ�������ƽ̨�޹ص��������д������������

Modify Log:
		
*********************************************************************/

#ifndef SOFTARTX_TEX_IO_H
#define SOFTARTX_TEX_IO_H

#include "softart/include/surface.h"
#include "softart/include/decl.h"

#include "eflib/include/math.h"

//����ΪRaw��ʽ
void save_surface_to_raw(surface& surf, const std::_tstring& filename, pixel_format format);

//����ΪRaw���ı���ʽ����ΪRGBA32F���ڲ���ʽ
//channel ���ڿ������˳��channel = ��RGBA�����������RGBA�����Ϊ��RGB'�����RGB�����Ϊ'BGR'�����BGR
void save_surface_to_raw_text(surface& surf, const std::_tstring& filename, uint32_t channel);

#endif //SOFTARTX_TEX_IO_H