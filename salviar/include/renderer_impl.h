#pragma once

#include <salviar/include/salviar_forward.h>

#include <salviar/include/renderer.h>

#include <eflib/include/utility/shared_declaration.h>

BEGIN_NS_SALVIAR();

struct vs_input_op;
struct vs_output_op;

EFLIB_DECLARE_CLASS_SHARED_PTR(host);
EFLIB_DECLARE_CLASS_SHARED_PTR(renderer_impl);
EFLIB_DECLARE_CLASS_SHARED_PTR(shader_object);
EFLIB_DECLARE_CLASS_SHARED_PTR(vertex_shader_unit);
EFLIB_DECLARE_CLASS_SHARED_PTR(pixel_shader_unit);
EFLIB_DECLARE_CLASS_SHARED_PTR(stream_assembler);

struct state_block
{
	viewport vp;	
};

class renderer_impl : public renderer
{
	//Rendering States
	viewport				vp_;
	cull_mode				cm_;
	primitive_topology		primtopo_;
	buffer_ptr				index_buffer_;
	format					index_format_;
	raster_state_ptr		rast_state_;
	depth_stencil_state_ptr	ds_state_;
	int32_t					stencil_ref_;
	
	// Stages
	host_ptr				host_;
	vertex_shader_ptr			cpp_vs_;
	pixel_shader_ptr			cpp_ps_;
	blend_shader_ptr			cpp_bs_;

	stream_assembler_ptr	assembler_;
	clipper_ptr				clipper_;
	vertex_cache_ptr			vertex_cache_;
	rasterizer_ptr			rast_;
	
	framebuffer_ptr			frame_buffer_;
	
	vs_input_op*			vs_input_ops_;
	vs_output_op*			vs_output_ops_;

	shader_object_ptr		vx_shader_;
	shader_object_ptr		px_shader_;
	vertex_shader_unit_ptr	vs_proto_;
	pixel_shader_unit_ptr	ps_proto_;

	// Resources
	buffer_manager_ptr		buffer_pool_;
	texture_manager_ptr		texture_pool_;
	device_ptr				native_dev_;

	void initialize();

public:
	//inherited
	virtual input_layout_ptr create_input_layout(
		input_element_desc const* elem_descs, size_t elems_count,
		shader_object_ptr const& vs );
	
	virtual input_layout_ptr create_input_layout(
		input_element_desc const* elem_descs, size_t elems_count,
		vertex_shader_ptr const& vs );

	virtual result set_input_layout(input_layout_ptr const& layout);

	virtual result set_vertex_buffers(
		size_t starts_slot,
		size_t buffers_count, buffer_ptr const* buffers,
		size_t const* strides, size_t const* offsets
		);

	virtual result set_index_buffer(buffer_ptr const& hbuf, format index_fmt);
	virtual buffer_ptr get_index_buffer() const;
	virtual format get_index_format() const;

	virtual result set_primitive_topology(primitive_topology primtopo);
	virtual primitive_topology get_primitive_topology() const;

	virtual result set_vertex_shader(vertex_shader_ptr const& hvs);
	virtual vertex_shader_ptr get_vertex_shader() const;
	
	virtual result					set_vertex_shader_code( boost::shared_ptr<shader_object> const& );
	virtual shader_object_ptr		get_vertex_shader_code() const;
	virtual result					set_vs_variable_value( std::string const& name, void const* pvariable, size_t sz );
	virtual result					set_vs_variable_pointer( std::string const& name, void const* pvariable, size_t sz );
	virtual result					set_vs_sampler( std::string const& name, sampler_ptr const& samp );
	virtual vertex_shader_unit_ptr	vs_proto() const;

	virtual const vs_input_op* get_vs_input_ops() const;
	virtual const vs_output_op* get_vs_output_ops() const;

	virtual result set_rasterizer_state(raster_state_ptr const& rs);
	virtual raster_state_ptr get_rasterizer_state() const;
	virtual result set_depth_stencil_state(depth_stencil_state_ptr const& dss, int32_t stencil_ref);
	virtual const depth_stencil_state_ptr& get_depth_stencil_state() const;
	virtual int32_t get_stencil_ref() const;

	virtual result set_pixel_shader(pixel_shader_ptr const& hps);
	virtual pixel_shader_ptr get_pixel_shader() const;

	virtual result set_pixel_shader_code( boost::shared_ptr<shader_object> const& );
	virtual shader_object_ptr get_pixel_shader_code() const;
	virtual result set_ps_variable( std::string const& name, void const* data, size_t sz );
	virtual result set_ps_sampler( std::string const& name, sampler_ptr const& samp );

	virtual pixel_shader_unit_ptr ps_proto() const;

	virtual result set_blend_shader(blend_shader_ptr const& hbs);
	virtual blend_shader_ptr get_blend_shader() const;

	virtual result set_viewport(viewport const& vp);
	virtual viewport get_viewport() const;

	virtual result set_framebuffer_size(size_t width, size_t height, size_t num_samples);
	virtual eflib::rect<size_t> get_framebuffer_size() const;

	virtual result set_framebuffer_format(pixel_format pxfmt);
	virtual pixel_format get_framebuffer_format(pixel_format pxfmt) const;

	virtual result set_render_target_available(render_target tar, size_t target_index, bool valid);
	virtual bool get_render_target_available(render_target tar, size_t target_index) const;

	virtual framebuffer_ptr get_framebuffer() const;

	//do not support get function for a while
	virtual result set_render_target(render_target tar, size_t target_index, surface_ptr const& surf);

	virtual buffer_ptr	create_buffer(size_t size);
	virtual texture_ptr	create_tex2d(size_t width, size_t height, size_t num_samples, pixel_format fmt);
	virtual texture_ptr	create_texcube(size_t width, size_t height, size_t num_samples, pixel_format fmt);
	virtual sampler_ptr	create_sampler(sampler_desc const& desc);

	virtual result draw(size_t startpos, size_t primcnt);
	virtual result draw_index(size_t startpos, size_t primcnt, int basevert);

	virtual result clear_color(size_t target_index, color_rgba32f const& c);
	virtual result clear_depth(float d);
	virtual result clear_stencil(uint32_t s);
	virtual result clear_color(size_t target_index, eflib::rect<size_t> const& rc, color_rgba32f const& c);
	virtual result clear_depth(eflib::rect<size_t> const& rc, float d);
	virtual result clear_stencil(eflib::rect<size_t> const& rc, uint32_t s);

	virtual result flush();
	virtual result present();

	//this class for inner system
	renderer_impl(const renderer_parameters* pparam, device_ptr hdev);

	host_ptr				get_host();
	stream_assembler_ptr	get_assembler();
	rasterizer_ptr			get_rasterizer();
	device_ptr				get_native_device();
	vertex_cache_ptr			get_vertex_cache();
	clipper_ptr				get_clipper();
};

renderer_ptr create_renderer_impl(renderer_parameters const* pparam, device_ptr const& hdev);

END_NS_SALVIAR();