/********************************************************************
Copyright (C) 2007-2010 Ye Wu

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
		
*********************************************************************/

#pragma once

#include <salviax/include/resource/resource_forward.h>
#include <salviax/include/utility/inc_d3d9x.h>
#include <salviax/include/resource/mesh/sa/mesh.h>
#include <salviax/include/utility/d3d9_utilities.h>
#include <salviar/include/decl.h>

BEGIN_NS_SALVIAX_RESOURCE();

mesh_ptr create_mesh_from_dx9mesh(salviar::renderer* psr, LPD3DXMESH dx_mesh);
mesh_ptr create_mesh_from_xfile(salviar::renderer* psr, salviax::utility::d3d9_device* dev, const std::_tstring& filename);

END_NS_SALVIAX_RESOURCE();

