#ifndef SALVIAX_MESH_H
#define SALVIAX_MESH_H

#include <salviax/include/resource/resource_forward.h>
#include <salviar/include/decl.h>
#include <salviar/include/stream_assembler.h>
#include <salviar/include/input_layout.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/shared_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

#include <vector>

BEGIN_NS_SALVIAX_RESOURCE();

class attached_data{
public:
	virtual ~attached_data() = 0{}
};

DECL_HANDLE(attached_data, h_attached_data);

class mesh
{
public:
	virtual size_t get_buffer_count() = 0;
	virtual size_t get_face_count() = 0;

	virtual salviar::h_buffer get_index_buffer() = 0;
	virtual salviar::h_buffer get_vertex_buffer( size_t buffer_index ) = 0;
	
	virtual h_attached_data get_attached() = 0;

	virtual void gen_adjancency() = 0;

	virtual void render() = 0;

	virtual ~mesh(){}
};
DECL_HANDLE(mesh, h_mesh);

class skin_mesh
{
public:
	virtual size_t	submesh_count() = 0;
	virtual void	render( uint32_t submesh_id ) = 0;
	virtual void	update_time(float t) = 0;
	virtual void	set_time(float t) = 0;
	virtual std::vector<eflib::mat44> joint_matrices() = 0;
	virtual std::vector<eflib::mat44> bind_inv_matrices() = 0;

	virtual ~skin_mesh(){}
};
DECL_HANDLE(skin_mesh, h_skin_mesh);

END_NS_SALVIAX_RESOURCE();

#endif

/********************************************************************
Copyright (C) 2007-2012 Ye Wu

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
*********************************************************************/
