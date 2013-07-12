#ifndef SALVIAR_RENDERER_H
#define SALVIAR_RENDERER_H

#include <salviar/include/decl.h>
#include <salviar/include/enums.h>
#include <salviar/include/colors.h>
#include <salviar/include/format.h>
#include <salviar/include/shader.h>
#include <salviar/include/viewport.h>

#include <eflib/include/math/collision_detection.h>
#include <eflib/include/utility/shared_declaration.h>

#include <eflib/include/platform/disable_warnings.h>
#include <boost/any.hpp>
#include <boost/shared_ptr.hpp>
#include <eflib/include/platform/enable_warnings.h>

#include <vector>
#include <salviar/include/salviar_forward.h>

BEGIN_NS_SALVIAR();

struct shader_profile;
struct input_element_desc;

EFLIB_DECLARE_CLASS_SHARED_PTR(renderer);
EFLIB_DECLARE_CLASS_SHARED_PTR(shader_object);
EFLIB_DECLARE_CLASS_SHARED_PTR(shader_log);

struct renderer_parameters
{
	size_t backbuffer_width;
	size_t backbuffer_height;
	size_t backbuffer_num_samples;

	pixel_format backbuffer_format;
};

class renderer
{
public:
	//state accessors
	/////////////////////////////////////////////
	virtual result set_input_layout( input_layout_ptr const& layout) = 0;

	virtual input_layout_ptr create_input_layout(
		input_element_desc const* elem_descs, size_t elems_count,
		shader_object_ptr const& code ) = 0;

	virtual input_layout_ptr create_input_layout(
		input_element_desc const* elem_descs, size_t elems_count,
		cpp_vertex_shader_ptr const& vs ) = 0;

	virtual result set_vertex_buffers(
		size_t starts_slot,
		size_t buffers_count, buffer_ptr const* buffers,
		size_t const* strides, size_t const* offsets ) = 0;

	virtual result		set_index_buffer(buffer_ptr const& hbuf, format index_fmt) = 0;
	virtual buffer_ptr	get_index_buffer() const = 0;
	virtual format		get_index_format() const = 0;

	virtual result set_primitive_topology(primitive_topology primtopo) = 0;
	virtual primitive_topology get_primitive_topology() const = 0;

	virtual result set_vertex_shader(cpp_vertex_shader_ptr const& hvs) = 0;
	virtual cpp_vertex_shader_ptr get_vertex_shader() const = 0;

	virtual result set_vertex_shader_code( shader_object_ptr const& ) = 0;
	virtual shader_object_ptr get_vertex_shader_code() const = 0;

	template <typename T>
	result set_vs_variable( std::string const& name, T const* data )
	{
		return set_vs_variable_value( name, static_cast<void const*>(data), sizeof(T) );
	}
	
	virtual result set_vs_variable_value( std::string const& name, void const* pvariable, size_t sz ) = 0;
	virtual result set_vs_variable_pointer( std::string const& name, void const* pvariable, size_t sz ) = 0;

	virtual result set_vs_sampler( std::string const& name, sampler_ptr const& samp ) = 0;

	virtual result set_rasterizer_state(raster_state_ptr const& rs) = 0;
	virtual raster_state_ptr get_rasterizer_state() const = 0;

	virtual result set_pixel_shader(cpp_pixel_shader_ptr const& hps) = 0;
	virtual cpp_pixel_shader_ptr get_pixel_shader() const = 0;

	virtual result set_pixel_shader_code( shader_object_ptr const& ) = 0;
	virtual shader_object_ptr get_pixel_shader_code() const = 0;

	template <typename T>
	result set_ps_variable( std::string const& name, T const* data )
	{
		return set_ps_variable( name, static_cast<void const*>(data), sizeof(T) );
	}
	virtual result set_ps_variable( std::string const& name, void const* data, size_t sz ) = 0;
	virtual result set_ps_sampler( std::string const& name, sampler_ptr const& samp ) = 0;

	virtual result set_blend_shader(cpp_blend_shader_ptr const& hbs) = 0;
	virtual cpp_blend_shader_ptr get_blend_shader() const = 0;

	virtual result		set_viewport(viewport const& vp) = 0;
	virtual viewport	get_viewport() const = 0;

	virtual result set_framebuffer_size(size_t width, size_t height, size_t num_samples) = 0;
	virtual eflib::rect<size_t> get_framebuffer_size() const = 0;

	virtual result set_framebuffer_format(pixel_format pxfmt) = 0;
	virtual pixel_format get_framebuffer_format(pixel_format pxfmt) const = 0;

	virtual result set_render_target_available(render_target tar, size_t target_index, bool valid) = 0;
	virtual bool get_render_target_available(render_target tar, size_t target_index) const = 0;

	virtual framebuffer_ptr get_framebuffer() const = 0;

	virtual result set_render_target(render_target tar, size_t target_index, surface_ptr const& surf) = 0;
	virtual result set_depth_stencil_state(depth_stencil_state_ptr const& dss, int32_t stencil_ref) = 0;
	//resource managements
	//////////////////////////////////////////////
	virtual buffer_ptr  create_buffer(size_t size) = 0;
	virtual texture_ptr create_tex2d(size_t width, size_t height, size_t num_samples, pixel_format fmt) = 0;
	virtual texture_ptr create_texcube(size_t width, size_t height, size_t num_samples, pixel_format fmt) = 0;
	virtual sampler_ptr create_sampler(sampler_desc const& desc) = 0;

	//render operations
	//////////////////////////////////////////////
	virtual result draw(size_t startpos, size_t primcnt) = 0;
	virtual result draw_index(size_t startpos, size_t primcnt, int basevert) = 0;

	virtual result clear_color(size_t target_index, color_rgba32f const& c) = 0;
	virtual result clear_depth(float d) = 0;
	virtual result clear_stencil(uint32_t s) = 0;

	virtual result clear_color(size_t target_index, eflib::rect<size_t> const& rc, color_rgba32f const& c) = 0;
	virtual result clear_depth(eflib::rect<size_t> const& rc, float d) = 0;
	virtual result clear_stencil(eflib::rect<size_t> const& rc, uint32_t s) = 0;

	virtual result flush() = 0;
	virtual result present() = 0;
};

renderer_ptr			create_software_renderer(renderer_parameters const* pparam, device_ptr const& hdev);
renderer_ptr			create_benchmark_renderer(renderer_parameters const* pparam, device_ptr const& hdev);

shader_object_ptr	compile(std::string const& code, shader_profile const& profile, shader_log_ptr& logs);
shader_object_ptr	compile(std::string const& code, shader_profile const& profile);
shader_object_ptr	compile(std::string const& code, languages lang);

END_NS_SALVIAR();

#endif