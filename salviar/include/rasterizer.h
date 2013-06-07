#pragma once

#include <salviar/include/salviar_forward.h>

#include <salviar/include/decl.h>
#include <salviar/include/enums.h>
#include <salviar/include/shader.h>
#include <salviar/include/framebuffer.h>
#include <salviar/include/raster_state.h>
#include <salviar/include/geom_setup_engine.h>

#include <eflib/include/memory/atomic.h>
#include <eflib/include/memory/pool.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/array.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

BEGIN_NS_SALVIAR();

typedef eflib::pool::preserved_pool<vs_output> vs_output_pool;

struct scanline_info;
class  pixel_shader_unit;
class  vs_output;
struct vs_output_op;
struct clip_context;

struct drawing_context
{
	int					vp_left, vp_top;
	int					rgn_left, rgn_top, rgn_right, rgn_bottom;
	eflib::vec4 const*	edge_factors;
	size_t				num_samples;
	bool				has_centroid;
	vs_output const*	v0;
	vs_output const*	ddx;
	vs_output const*	ddy;
	vs_output const*	pixels;
	uint32_t const*		masks;

	vs_output_op const*	vs_output_ops;
	cpp_pixel_shader*	cpp_ps;
	pixel_shader_unit*	ps_unit;
	cpp_blend_shader*	cpp_bs;
	float const*		aa_z_offset;
};

struct rasterize_multi_prim_context
{
	std::vector<uint32_t> const*	sorted_prims;
	viewport const*					tile_vp;
	cpp_pixel_shader*				cpp_ps;
	pixel_shader_unit*				psu;
};

struct rasterize_prim_context
{
	uint32_t						prim_id;
	viewport const*					tile_vp;
	cpp_pixel_shader*				cpp_ps;
	pixel_shader_unit*				psu;
};

class rasterizer : public render_stage
{
private:
	static const int MAX_NUM_MULTI_SAMPLES = 4;

	// Status per drawing.
	raster_state_ptr				state_;
	uint32_t						num_vs_output_attributes_;

	framebuffer_ptr					frame_buffer_;
	cpp_blend_shader*				cpp_bs_;

	viewport const*					vp_;
	vs_output_op const*				vso_ops_;

	// Intermediate data
	prim_type						prim_;
	uint32_t						prim_size_;
	eflib::vec2						samples_pattern_[MAX_NUM_MULTI_SAMPLES];

	std::vector<std::vector<std::vector<uint32_t>>>
									threaded_tiled_prims_;		// vector<prim> prims = thread_tiled_prims[ThreadID][TileID]
	std::vector<eflib::vec3>		edge_factors_;

	vs_output**						clipped_verts_;
	size_t							clipped_verts_count_;
	size_t							clipped_prims_count_;

	size_t							tile_x_count_;
	size_t							tile_y_count_;
	size_t							tile_count_;

	std::vector<cpp_pixel_shader*>	threaded_cpp_ps_;
	std::vector<pixel_shader_unit*>	threaded_psu_;					

	void threaded_dispatch_primitive(thread_context const*);
	void threaded_rasterize_multi_prim(thread_context const*);

	boost::function< void (rasterizer*, rasterize_multi_prim_context*)>
		rasterize_prims_;

	void draw_whole_tile(
		int left, int top, int right, int bottom,
		size_t num_samples, const vs_output& v0,
		const vs_output& ddx, const vs_output& ddy,
		cpp_pixel_shader* cpp_ps, pixel_shader_unit* psu, cpp_blend_shader* cpp_bs,
		const float* aa_z_offset );
	void draw_pixels(
		int left0, int top0, int left, int top,
		const eflib::vec4* edge_factors, size_t num_samples, bool has_centroid,
		const vs_output& v0, const vs_output& ddx, const vs_output& ddy,
		cpp_pixel_shader* cpp_ps, pixel_shader_unit* psu, cpp_blend_shader* cpp_bs,
		const float* aa_z_offset);
	void subdivide_tile(int left, int top, const eflib::rect<uint32_t>& cur_region, const eflib::vec4* edge_factors,
		uint32_t* test_regions, uint32_t& test_region_size, float x_min, float x_max, float y_min, float y_max,
		const float* rej_to_acc, const float* evalue, const float* step_x, const float* step_y);

	void draw_full_package(
		vs_output* pixels,
		uint32_t top, uint32_t left, size_t num_samples,
		cpp_pixel_shader* cpp_ps, pixel_shader_unit* psu, cpp_blend_shader* cpp_bs,
		float const* aa_z_offset );
	void draw_package(
		vs_output* pixels,
		uint32_t top, uint32_t left, size_t num_samples,
		cpp_pixel_shader* cpp_ps, pixel_shader_unit* psu, cpp_blend_shader* cpp_bs,
		uint32_t const* masks, float const* aa_z_offset );

	void viewport_and_project_transform(vs_output** vertexes, size_t num_verts);

public:
	//inherited
	void initialize(renderer_impl* pparent);

	//constructor
	rasterizer();
	~rasterizer();

	//state_seter
	void set_state(const raster_state_ptr& state);
	const raster_state_ptr& get_state() const;

	//drawer
	void rasterize_line(rasterize_prim_context const*);
	void rasterize_triangle(rasterize_prim_context const*);

	void rasterize_multi_line(rasterize_multi_prim_context const*);
	void rasterize_multi_triangle(rasterize_multi_prim_context const*);

	void draw(size_t prim_count);

	void update_prim_info();
};

END_NS_SALVIAR();