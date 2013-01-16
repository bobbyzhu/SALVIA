#include <salviar/include/renderer_impl.h>

#include <salviar/include/binary_modules.h>
#include <salviar/include/shaderregs_op.h>
#include <salviar/include/clipper.h>
#include <salviar/include/resource_manager.h>
#include <salviar/include/rasterizer.h>
#include <salviar/include/framebuffer.h>
#include <salviar/include/surface.h>
#include <salviar/include/vertex_cache.h>
#include <salviar/include/stream_assembler.h>
#include <salviar/include/shader_unit.h>
#include <salviar/include/input_layout.h>

BEGIN_NS_SALVIAR();

using namespace eflib;
using boost::shared_ptr;

//inherited
result renderer_impl::set_input_layout(const h_input_layout& layout)
{
	size_t min_slot = 0, max_slot = 0;
	layout->slot_range(min_slot, max_slot);
	vs_input_ops_ = &get_vs_input_op( static_cast<uint32_t>(max_slot) );

	assembler_->set_input_layout(layout);
	
	return result::ok;
}

result renderer_impl::set_vertex_buffers(
		size_t starts_slot,
		size_t buffers_count, h_buffer const* buffers,
		size_t const* strides, size_t const* offsets)
{
	assembler_->set_vertex_buffers(
		starts_slot,
		buffers_count, buffers,
		strides, offsets
		);
	return result::ok;
}

result renderer_impl::set_index_buffer(h_buffer const& hbuf, format index_fmt)
{
	switch (index_fmt)
	{
	case format_r16_uint:
	case format_r32_uint:
		break;
	default:
		EFLIB_ASSERT(false, "The value of index type is invalid.");
		return result::failed;
	}

	index_buffer_ = hbuf;
	index_format_ = index_fmt;

	return result::ok;
}

h_buffer renderer_impl::get_index_buffer() const{
	return index_buffer_;
}

format renderer_impl::get_index_format() const{
	return index_format_;
}

//
result renderer_impl::set_primitive_topology(primitive_topology primtopo)
{
	switch (primtopo)
	{
	case primitive_line_list:
	case primitive_line_strip:
	case primitive_triangle_list:
	case primitive_triangle_strip:
		break;
	default:
		assert( !"Invalid primitive topology." );
		return result::failed;
	}

	primtopo_ = primtopo;
	return result::ok;
}

primitive_topology renderer_impl::get_primitive_topology() const{
	return primtopo_;
}

result renderer_impl::set_vertex_shader(h_vertex_shader const& hvs)
{
	cpp_vs_ = hvs;

	uint32_t n = cpp_vs_->num_output_attributes();
	vs_output_ops_ = &get_vs_output_op(n);
	for (uint32_t i = 0; i < n; ++ i)
	{
		vs_output_ops_->attribute_modifiers[i] = cpp_vs_->output_attribute_modifiers(i);
	}

	return result::ok;
}

h_vertex_shader renderer_impl::get_vertex_shader() const
{
	return cpp_vs_;
}

result renderer_impl::set_vertex_shader_code( shared_ptr<shader_object> const& code ){
	vs_output_ops_ = &get_vs_output_op(0);

	vx_shader_ = code;
	vs_proto_.reset( new vertex_shader_unit() );
	vs_proto_->initialize( vx_shader_.get() );

	uint32_t n = vs_proto_->output_attributes_count();
	vs_output_ops_ = &get_vs_output_op(n);
	for (uint32_t i = 0; i < n; ++ i)
	{
		vs_output_ops_->attribute_modifiers[i] = vs_proto_->output_attribute_modifiers(i);
	}

	return result::ok;
}

shared_ptr<shader_object> renderer_impl::get_vertex_shader_code() const{
	return vx_shader_;
}

const vs_input_op* renderer_impl::get_vs_input_ops() const
{
	return vs_input_ops_;
}

const vs_output_op* renderer_impl::get_vs_output_ops() const
{
	return vs_output_ops_;
}

result renderer_impl::set_rasterizer_state(const h_rasterizer_state& rs)
{
	rast_state_ = rs;
	return result::ok;
}

h_rasterizer_state renderer_impl::get_rasterizer_state() const
{
	return rast_state_;
}

result renderer_impl::set_depth_stencil_state(const h_depth_stencil_state& dss, int32_t stencil_ref)
{
	ds_state_ = dss;
	stencil_ref_ = stencil_ref;
	return result::ok;
}

const h_depth_stencil_state& renderer_impl::get_depth_stencil_state() const
{
	return ds_state_;
}

int32_t renderer_impl::get_stencil_ref() const
{
	return stencil_ref_;
}

result renderer_impl::set_pixel_shader(h_pixel_shader const& hps)
{
	cpp_ps_ = hps;
	return result::ok;
}

h_pixel_shader renderer_impl::get_pixel_shader() const
{
	return cpp_ps_;
}

result renderer_impl::set_blend_shader(h_blend_shader const& hbs)
{
	cpp_bs_ = hbs;
	return result::ok;
}

h_blend_shader renderer_impl::get_blend_shader() const
{
	return cpp_bs_;
}

result renderer_impl::set_viewport(const viewport& vp)
{
	vp_ = vp;
	return result::ok;
}

viewport renderer_impl::get_viewport() const
{
	return vp_;
}

result renderer_impl::set_framebuffer_size(size_t width, size_t height, size_t num_samples)
{
	frame_buffer_->reset(width, height, num_samples, frame_buffer_->get_buffer_format());
	return result::ok;
}

eflib::rect<size_t> renderer_impl::get_framebuffer_size() const
{
	return frame_buffer_->get_rect();
}

//
result renderer_impl::set_framebuffer_format(pixel_format pxfmt)
{
	frame_buffer_->reset(frame_buffer_->get_width(), frame_buffer_->get_height(), frame_buffer_->get_num_samples(), pxfmt);
	return result::ok;
}

pixel_format renderer_impl::get_framebuffer_format(pixel_format /*pxfmt*/) const
{
	return frame_buffer_->get_buffer_format();
}

result renderer_impl::set_render_target_available(render_target tar, size_t target_index, bool valid)
{
	if(valid){
		frame_buffer_->set_render_target_enabled(tar, target_index);
	} else {
		frame_buffer_->set_render_target_disabled(tar, target_index);
	}

	return result::ok;
}

bool renderer_impl::get_render_target_available(render_target /*tar*/, size_t /*target_index*/) const
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return false;
}

//do not support get function for a while
result renderer_impl::set_render_target(render_target tar, size_t target_index, h_surface const& surf)
{
	frame_buffer_->set_render_target( tar, target_index, surf.get() );
	return result::ok;
}

h_buffer renderer_impl::create_buffer(size_t size)
{
	return buffer_pool_->create_buffer(size);
}

h_texture renderer_impl::create_tex2d(size_t width, size_t height, size_t num_samples, pixel_format fmt)
{
	return texture_pool_->create_texture_2d(width, height, num_samples, fmt);
}

h_texture renderer_impl::create_texcube(size_t width, size_t height, size_t num_samples, pixel_format fmt)
{
	return texture_pool_->create_texture_cube(width, height, num_samples, fmt);
}

h_sampler renderer_impl::create_sampler(const sampler_desc& desc)
{
	return h_sampler(new sampler(desc));
}

result renderer_impl::draw(size_t startpos, size_t primcnt)
{
	rast_->set_state(rast_state_);

	vertex_cache_->update_index_buffer(h_buffer(), index_format_, primtopo_, static_cast<uint32_t>(startpos), 0);
	vertex_cache_->transform_vertices(static_cast<uint32_t>(primcnt));
	
	rast_->draw(primcnt);
	return result::ok;
}

result renderer_impl::draw_index(size_t startpos, size_t primcnt, int basevert)
{
	rast_->set_state(rast_state_);

	vertex_cache_->update_index_buffer(index_buffer_, index_format_, primtopo_, static_cast<uint32_t>(startpos), basevert);
	vertex_cache_->transform_vertices(static_cast<uint32_t>(primcnt));

	rast_->draw(primcnt);
	return result::ok;
}

result renderer_impl::clear_color(size_t target_index, const color_rgba32f& c)
{
	frame_buffer_->clear_color(target_index, c);
	return result::ok;
}

result renderer_impl::clear_depth(float d)
{
	frame_buffer_->clear_depth(d);
	return result::ok;
}

result renderer_impl::clear_stencil(uint32_t s)
{
	frame_buffer_->clear_stencil(s);
	return result::ok;
}

result renderer_impl::clear_color(size_t target_index, const eflib::rect<size_t>& rc, const color_rgba32f& c)
{
	frame_buffer_->clear_color(target_index, rc, c);
	return result::ok;
}

result renderer_impl::clear_depth(const eflib::rect<size_t>& rc, float d)
{
	frame_buffer_->clear_depth(rc, d);
	return result::ok;
}

result renderer_impl::clear_stencil(const eflib::rect<size_t>& rc, uint32_t s)
{
	frame_buffer_->clear_stencil(rc, s);
	return result::ok;
}

result renderer_impl::flush()
{
	return result::ok;
}

result renderer_impl::present()
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return result::ok;
}

void renderer_impl::initialize()
{
	rast_->initialize(this);
	frame_buffer_->initialize(this);
}

renderer_impl::renderer_impl(const renderer_parameters* pparam, h_device hdev)
	: index_format_(format_r16_uint), primtopo_(primitive_triangle_list)
{
	native_dev_ = hdev;

	buffer_pool_.reset(new buffer_manager());
	texture_pool_.reset(new texture_manager());

	assembler_.reset(new stream_assembler() );
	clipper_.reset(new clipper());
	rast_.reset(new rasterizer());
	frame_buffer_.reset(
		new framebuffer(
		pparam->backbuffer_width,
		pparam->backbuffer_height,
		pparam->backbuffer_num_samples,
		pparam->backbuffer_format
		)
		);
	
	vertex_cache_ = create_default_vertex_cache(this);
	rast_state_.reset(new rasterizer_state(rasterizer_desc()));
	ds_state_.reset(new depth_stencil_state(depth_stencil_desc()));

	vp_.minz = 0.0f;
	vp_.maxz = 1.0f;
	vp_.w = static_cast<float>(pparam->backbuffer_width);
	vp_.h = static_cast<float>(pparam->backbuffer_height);
	vp_.x = vp_.y = 0;

	initialize();
}

h_rasterizer renderer_impl::get_rasterizer()
{
	return rast_;
}

h_framebuffer renderer_impl::get_framebuffer() const
{
	return frame_buffer_; 
}

h_device renderer_impl::get_native_device()
{
	return native_dev_;
}

h_vertex_cache renderer_impl::get_vertex_cache()
{
	return vertex_cache_;
}

h_clipper renderer_impl::get_clipper()
{
	return clipper_;
}

stream_assembler_ptr renderer_impl::get_assembler()
{
	return assembler_;
}

result renderer_impl::set_vs_variable_value( std::string const& name, void const* pvariable, size_t /*sz*/ )
{
	if( vs_proto_ ){
		vs_proto_->set_variable(name, pvariable);
		return result::ok;
	}
	return result::failed;
}

result renderer_impl::set_vs_variable_pointer( std::string const& name, void const* pvariable, size_t sz )
{
	if( vs_proto_ ){
		vs_proto_->set_variable_pointer(name, pvariable, sz);
		return result::ok;
	}
	return result::failed;
}

shared_ptr<vertex_shader_unit> renderer_impl::vs_proto() const{
	return vs_proto_;
}

h_input_layout renderer_impl::create_input_layout(
	input_element_desc const* elem_descs, size_t elems_count,
	h_shader_code const& vs )
{
	return input_layout::create( elem_descs, elems_count, vs );
}

salviar::h_input_layout renderer_impl::create_input_layout(
	input_element_desc const* elem_descs, size_t elems_count,
	h_vertex_shader const& vs )
{
	return input_layout::create( elem_descs, elems_count, vs );
}

result renderer_impl::set_pixel_shader_code( shared_ptr<shader_object> const& code )
{
	px_shader_ = code;
	ps_proto_.reset( new pixel_shader_unit() );
	ps_proto_->initialize( px_shader_.get() );

	return result::ok;
}

shared_ptr<shader_object> renderer_impl::get_pixel_shader_code() const{
	return px_shader_;
}

shared_ptr<pixel_shader_unit> renderer_impl::ps_proto() const
{
	return ps_proto_;
}

result renderer_impl::set_ps_variable( std::string const& name, void const* data, size_t /*sz*/ )
{
	if( ps_proto_ ){
		ps_proto_->set_variable(name, data);
		return result::ok;
	}
	return result::failed;
}

result renderer_impl::set_ps_sampler( std::string const& name, h_sampler const& samp )
{
	if ( ps_proto_ ){
		ps_proto_->set_sampler( name, samp );
		return result::ok;
	}
	return result::failed;
}

result renderer_impl::set_vs_sampler( std::string const& name, h_sampler const& samp )
{
	if ( vs_proto_ ){
		vs_proto_->set_sampler( name, samp );
		return result::ok;
	}
	return result::failed;
}

renderer_ptr create_renderer_impl(renderer_parameters const* pparam, h_device const& hdev)
{
	return renderer_ptr(new renderer_impl(pparam, hdev));
}

END_NS_SALVIAR();
