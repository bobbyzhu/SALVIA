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

purpose:	�ṩ������ģ�͵Ľӿ�������ƽ̨�޹ض����ļ���

Modify Log:
		
*********************************************************************/

#ifndef SOFTARTX_MESH_H
#define SOFTARTX_MESH_H

#include "softart/include/decl.h"
#include "softart/include/stream_assembler.h"

#include <boost/smart_ptr.hpp>

#include <vector>

class base_mesh
{
public:
	virtual size_t get_buffer_count() = 0;
	virtual size_t get_face_count() = 0;

	virtual h_buffer get_buffer(size_t buf_id) = 0;
	virtual h_buffer get_index_buffer() = 0;
	virtual h_buffer get_vertex_buffer() = 0;

	virtual void gen_adjancency() = 0;

	virtual void render(const input_layout_decl& layout) = 0;
	virtual void render() = 0;
};

class mesh : public base_mesh
{
	std::vector<h_buffer> bufs_;
	h_buffer adjacancies_;

	renderer* pdev_;

	size_t idxbufid_;
	size_t vertbufid_;

	size_t primcount_;
	index_type idxtype_;

	std::vector<input_element_decl> default_layout_;

public:
	mesh(renderer* psr);

	/*
	inherited
	*/
	virtual size_t get_buffer_count();
	virtual size_t get_face_count();

	virtual h_buffer get_buffer(size_t buf_id);
	virtual h_buffer get_index_buffer();
	virtual h_buffer get_vertex_buffer();

	virtual void gen_adjancency();

	virtual void render(const input_layout_decl& layout);
	virtual void render();

	/*
	mesh
	*/
	virtual void set_buffer_count(size_t bufcount);
	virtual h_buffer create_buffer(size_t bufid, size_t size);
	
	virtual void set_index_buf_id(size_t bufid);
	virtual void set_vertex_buf_id(size_t bufid);

	virtual void set_primitive_count(size_t primcount);
	virtual void set_index_type(index_type idxtype);

	virtual void set_default_layout(const std::vector<input_element_decl>& layout);
};

DECL_HANDLE(base_mesh, h_mesh);

#endif