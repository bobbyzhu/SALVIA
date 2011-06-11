/*
Copyright (C) 2007-2010 Minmin Gong, Ye Wu

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
*/

#ifndef SALVIAXDEV_D3D9_H
#define SALVIAXDEV_D3D9_H

#include "salviar/include/presenter_dev.h"
#include "salviax/include/utility/d3d9_utilities.h"
#include <eflib/include/math/math.h>
#include <boost/shared_ptr.hpp>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#define BEGIN_NS_SALVIAXPRESENTER() namespace softartx{ namespace presenter{
#define END_NS_SALVIAXPRESENTER() }}

BEGIN_NS_SALVIAXPRESENTER()

class dev_d3d9;
DECL_HANDLE(dev_d3d9, h_dev_d3d9);

class dev_d3d9 : public salviar::device
{
public:
	static h_dev_d3d9 create_device(HWND hwnd, softartx::utility::h_d3d9_device dev = softartx::utility::h_d3d9_device());

	//inherited
	virtual void present(const salviar::surface& surf);

	~dev_d3d9();

private:
	dev_d3d9(HWND hwnd, softartx::utility::h_d3d9_device dev);
	void init_device();

	HWND hwnd_;
	softartx::utility::h_d3d9_device dev_;
	IDirect3DTexture9* buftex_;

	IDirect3DVertexBuffer9* vb_;
};

END_NS_SALVIAXPRESENTER()

#ifdef salviax_d3d9_presenter_EXPORTS
	#define SALVIAX_API __declspec(dllexport)
#else
	#define SALVIAX_API __declspec(dllimport)
#endif

extern "C"
{
	SALVIAX_API void salviax_create_presenter_device(salviar::h_device& dev, void* param);
}

#endif //SALVIAXDEV_D3D9_H
