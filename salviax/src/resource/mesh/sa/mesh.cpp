/*
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
*/

#include <salviax/include/resource/mesh/sa/mesh.h>
#include <salviar/include/renderer_impl.h>
#include <salviar/include/resource_manager.h>

#include <eflib/include/diagnostics/assert.h>

using namespace std;
using namespace eflib;
using namespace salviar;
BEGIN_NS_SALVIAXRESOURCE()

mesh::mesh(salviar::renderer* psr)
{
	EFLIB_ASSERT(psr, "");

	pdev_ = psr;
}

/*
inherited
*/
size_t mesh::get_buffer_count()
{
	return bufs_.size();
}

size_t mesh::get_face_count()
{
	return primcount_;
}

salviar::h_buffer mesh::get_buffer(size_t buf_id)
{
	EFLIB_ASSERT(buf_id < get_buffer_count(), "");
	
	if(buf_id < get_buffer_count()){return bufs_[buf_id];}
	return salviar::h_buffer();
}

salviar::h_buffer mesh::get_index_buffer()
{
	return get_buffer(idxbufid_);
}

salviar::h_buffer mesh::get_vertex_buffer()
{
	return get_buffer(vertbufid_);
}

void mesh::gen_adjancency(){
	EFLIB_ASSERT_UNIMPLEMENTED();
}

void mesh::render(const salviar::input_layout_decl& layout)
{
	EFLIB_ASSERT(pdev_, "");
	if(!pdev_) return;

	for(size_t i = 0; i < bufs_.size(); ++i){
		if(i == idxbufid_) continue;
		pdev_->set_stream(stream_index(i), get_buffer(i));
	}

	pdev_->set_index_buffer(get_index_buffer(), idxtype_);
	pdev_->set_input_layout(layout);
	pdev_->set_primitive_topology(primitive_triangle_list);

	pdev_->draw_index(0, primcount_, 0);
}

void mesh::render(){
	render(default_layout_);
}

/*
mesh
*/
void mesh::set_buffer_count(size_t bufcount)
{
	bufs_.resize(bufcount);
}

salviar::h_buffer mesh::create_buffer(size_t bufid, size_t size)
{
	EFLIB_ASSERT(bufid < bufs_.size(), "");
	EFLIB_ASSERT(pdev_, "");

	if(bufid < bufs_.size() && pdev_){
		bufs_[bufid] = pdev_->create_buffer(size);
		return bufs_[bufid];
	}

	return salviar::h_buffer();
}

void mesh::set_index_buf_id(size_t bufid)
{
	idxbufid_ = bufid;
}

void mesh::set_vertex_buf_id(size_t bufid)
{
	vertbufid_ = bufid;
}

void mesh::set_primitive_count(size_t primcount)
{
	primcount_ = primcount;
}

void mesh::set_index_type(salviar::index_type idxtype)
{
	switch(idxtype){
		case index_int16:
		case index_int32:
			idxtype_ = idxtype;
			break;
		default:
			EFLIB_ASSERT(false, "");
	}
}

void mesh::set_default_layout(const salviar::input_layout_decl& layout)
{
	default_layout_ = layout;
}

END_NS_SALVIAXRESOURCE()