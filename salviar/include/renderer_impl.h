#ifndef SALVIAR_RENDERER_IMPL_H
#define SALVIAR_RENDERER_IMPL_H

#include <salviar/include/renderer.h>
#include <salviar/include/salviar_forward.h>

#include <eflib/include/utility/shared_declaration.h>

BEGIN_NS_SALVIAR();

EFLIB_DECLARE_CLASS_SHARED_PTR(renderer_impl);

class vertex_shader_unit;
class pixel_shader_unit;

struct state_block{
	viewport vp;	
};

class renderer_impl : public renderer
{
	//some states
	viewport vp_;
	cull_mode cm_;

	h_buffer_manager			hbufmgr_;
	h_texture_manager			htexmgr_;
	h_vertex_shader				hvs_;
	h_clipper					hclipper_;
	h_rasterizer				hrast_;
	h_pixel_shader				hps_;
	h_framebuffer				hfb_;
	h_device					hdev_;
	h_vertex_cache				hvertcache_;
	h_blend_shader				hbs_;

	h_buffer indexbuf_;
	format index_fmt_;

	primitive_topology primtopo_;

	h_rasterizer_state			hrs_;
	h_depth_stencil_state		hdss_;
	int32_t						stencil_ref_;

	vs_input_op*				vs_input_ops_;
	vs_output_op*				vs_output_ops_;

	boost::shared_ptr<shader_code>			vscode_;
	boost::shared_ptr<shader_code>			pscode_;
	boost::shared_ptr<vertex_shader_unit>	vs_proto_;
	boost::shared_ptr<pixel_shader_unit>	ps_proto_;
	void initialize();

public:
	//inherited
	virtual h_input_layout create_input_layout(
		input_element_desc const* elem_descs, size_t elems_count,
		h_shader_code const& vs );
	
	virtual h_input_layout create_input_layout(
		input_element_desc const* elem_descs, size_t elems_count,
		h_vertex_shader const& vs );

	virtual result set_input_layout(h_input_layout const& layout);

	virtual result set_vertex_buffers(
		size_t starts_slot,
		size_t buffers_count, h_buffer const* buffers,
		size_t const* strides, size_t const* offsets
		);

	virtual result set_index_buffer(h_buffer const& hbuf, format index_fmt);
	virtual h_buffer get_index_buffer() const;
	virtual format get_index_format() const;

	virtual result set_primitive_topology(primitive_topology primtopo);
	virtual primitive_topology get_primitive_topology() const;

	virtual result set_vertex_shader(h_vertex_shader const& hvs);
	virtual h_vertex_shader get_vertex_shader() const;
	
	virtual result set_vertex_shader_code( boost::shared_ptr<shader_code> const& );
	virtual boost::shared_ptr<shader_code> get_vertex_shader_code() const;
	virtual result set_vs_variable_value( std::string const& name, void const* pvariable, size_t sz );
	virtual result set_vs_variable_pointer( std::string const& name, void const* pvariable, size_t sz );
	virtual result set_vs_sampler( std::string const& name, h_sampler const& samp );
	virtual boost::shared_ptr<vertex_shader_unit> vs_proto() const;

	virtual const vs_input_op* get_vs_input_ops() const;
	virtual const vs_output_op* get_vs_output_ops() const;

	virtual result set_rasterizer_state(h_rasterizer_state const& rs);
	virtual h_rasterizer_state get_rasterizer_state() const;
	virtual result set_depth_stencil_state(h_depth_stencil_state const& dss, int32_t stencil_ref);
	virtual const h_depth_stencil_state& get_depth_stencil_state() const;
	virtual int32_t get_stencil_ref() const;

	virtual result set_pixel_shader(h_pixel_shader const& hps);
	virtual h_pixel_shader get_pixel_shader() const;

	virtual result set_pixel_shader_code( boost::shared_ptr<shader_code> const& );
	virtual boost::shared_ptr<shader_code> get_pixel_shader_code() const;
	virtual result set_ps_variable( std::string const& name, void const* data, size_t sz );
	virtual result set_ps_sampler( std::string const& name, h_sampler const& samp );

	virtual boost::shared_ptr<pixel_shader_unit> ps_proto() const;

	virtual result set_blend_shader(h_blend_shader const& hbs);
	virtual h_blend_shader get_blend_shader() const;

	virtual result set_viewport(viewport const& vp);
	virtual viewport get_viewport() const;

	virtual result set_framebuffer_size(size_t width, size_t height, size_t num_samples);
	virtual eflib::rect<size_t> get_framebuffer_size() const;

	virtual result set_framebuffer_format(pixel_format pxfmt);
	virtual pixel_format get_framebuffer_format(pixel_format pxfmt) const;

	virtual result set_render_target_available(render_target tar, size_t target_index, bool valid);
	virtual bool get_render_target_available(render_target tar, size_t target_index) const;

	virtual h_framebuffer get_framebuffer() const;

	//do not support get function for a while
	virtual result set_render_target(render_target tar, size_t target_index, h_surface const& surf);

	virtual h_buffer	create_buffer(size_t size);
	virtual h_texture	create_tex2d(size_t width, size_t height, size_t num_samples, pixel_format fmt);
	virtual h_texture	create_texcube(size_t width, size_t height, size_t num_samples, pixel_format fmt);
	virtual h_sampler	create_sampler(sampler_desc const& desc);

	virtual result draw(size_t startpos, size_t primcnt);
	virtual result draw_index(size_t startpos, size_t primcnt, int basevert);

	virtual result clear_color(size_t target_index, color_rgba32f const& c);
	virtual result clear_depth(float d);
	virtual result clear_stencil(uint32_t s);
	virtual result clear_color(size_t target_index, eflib::rect<size_t> const& rc, color_rgba32f const& c);
	virtual result clear_depth(eflib::rect<size_t> const& rc, float d);
	virtual result clear_stencil(eflib::rect<size_t> const& rc, uint32_t s);

	virtual result present();

	//this class for inner system
	renderer_impl(const renderer_parameters* pparam, h_device hdev);

	h_rasterizer get_rasterizer();
	
	h_device get_device();
	h_vertex_cache get_vertex_cache();
	h_clipper get_clipper();
};

renderer_ptr create_renderer_impl(renderer_parameters const* pparam, h_device const& hdev);

END_NS_SALVIAR();

#endif