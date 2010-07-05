#include "../include/rasterizer.h"

#include "../include/shaderregs_op.h"
#include "../include/framebuffer.h"
#include "../include/renderer_impl.h"
#include "../include/cpuinfo.h"
#include "../include/clipper.h"
#include "../include/vertex_cache.h"
#include "../include/thread_pool.h"

#include "eflib/include/slog.h"

#include <algorithm>
#include <boost/format.hpp>
#include <boost/bind.hpp>
BEGIN_NS_SOFTART()


using namespace std;
using namespace efl;
using namespace boost;

const int TILE_SIZE = 64;
const int GEOMETRY_SETUP_PACKAGE_SIZE = 1;
const int DISPATCH_PRIMITIVE_PACKAGE_SIZE = 1;
const int RASTERIZE_PRIMITIVE_PACKAGE_SIZE = 1;
const int COMPACT_CLIPPED_VERTS_PACKAGE_SIZE = 1;

bool cull_mode_none(float /*area*/)
{
	return false;
}

bool cull_mode_ccw(float area)
{
	return area <= 0;
}

bool cull_mode_cw(float area)
{
	return area >= 0;
}

void fill_wireframe_clipping(uint32_t& num_clipped_verts, vs_output* clipped_verts, uint32_t* clipped_indices, uint32_t base_vertex, const h_clipper& clipper, const viewport& vp, const vs_output** pv, float area)
{
	num_clipped_verts = 0;
	uint32_t num_out_clipped_verts;

	const bool front_face = area > 0;

	clipper->clip(&clipped_verts[num_clipped_verts], num_out_clipped_verts, vp, *pv[0], *pv[1]);
	for (uint32_t j = 0; j < num_out_clipped_verts; ++ j){
		clipped_indices[num_clipped_verts + j] = base_vertex + num_clipped_verts + j;
		clipped_verts[num_clipped_verts + j].front_face = front_face;
	}
	num_clipped_verts += num_out_clipped_verts;

	clipper->clip(&clipped_verts[num_clipped_verts], num_out_clipped_verts, vp, *pv[1], *pv[2]);
	for (uint32_t j = 0; j < num_out_clipped_verts; ++ j){
		clipped_indices[num_clipped_verts + j] = base_vertex + num_clipped_verts + j;
		clipped_verts[num_clipped_verts + j].front_face = front_face;
	}
	num_clipped_verts += num_out_clipped_verts;
						
	clipper->clip(&clipped_verts[num_clipped_verts], num_out_clipped_verts, vp, *pv[2], *pv[0]);
	for (uint32_t j = 0; j < num_out_clipped_verts; ++ j){
		clipped_indices[num_clipped_verts + j] = base_vertex + num_clipped_verts + j;
		clipped_verts[num_clipped_verts + j].front_face = front_face;
	}
	num_clipped_verts += num_out_clipped_verts;
}

void fill_solid_clipping(uint32_t& num_clipped_verts, vs_output* clipped_verts, uint32_t* clipped_indices, uint32_t base_vertex, const h_clipper& clipper, const viewport& vp, const vs_output** pv, float area)
{
	uint32_t num_out_clipped_verts;
	clipper->clip(clipped_verts, num_out_clipped_verts, vp, *pv[0], *pv[1], *pv[2]);
	custom_assert(num_out_clipped_verts <= 6, "");

	num_clipped_verts = (0 == num_out_clipped_verts) ? 0 : (num_out_clipped_verts - 2) * 3;

	const bool front_face = area > 0;

	for (uint32_t i = 0; i < num_out_clipped_verts; ++ i){
		clipped_verts[i].front_face = front_face;
	}

	for(int i_tri = 1; i_tri < static_cast<int>(num_out_clipped_verts) - 1; ++ i_tri){
		clipped_indices[(i_tri - 1) * 3 + 0] = base_vertex + 0;
		if (front_face){
			clipped_indices[(i_tri - 1) * 3 + 1] = base_vertex + i_tri + 0;
			clipped_indices[(i_tri - 1) * 3 + 2] = base_vertex + i_tri + 1;
		}
		else{
			clipped_indices[(i_tri - 1) * 3 + 1] = base_vertex + i_tri + 1;
			clipped_indices[(i_tri - 1) * 3 + 2] = base_vertex + i_tri + 0;
		}
	}
}

void fill_wireframe_triangle_rasterize_func(uint32_t& prim_size, boost::function<void (rasterizer*, const std::vector<uint32_t>&, const std::vector<vs_output>&, const std::vector<uint32_t>&, const viewport&, const h_pixel_shader&)>& rasterize_func)
{
	prim_size = 2;
	rasterize_func = boost::mem_fn(&rasterizer::rasterize_line_func);
}

void fill_solid_triangle_rasterize_func(uint32_t& prim_size, boost::function<void (rasterizer*, const std::vector<uint32_t>&, const std::vector<vs_output>&, const std::vector<uint32_t>&, const viewport&, const h_pixel_shader&)>& rasterize_func)
{
	prim_size = 3;
	rasterize_func = boost::mem_fn(&rasterizer::rasterize_triangle_func);
}

rasterizer_state::rasterizer_state(const rasterizer_desc& desc)
	: desc_(desc)
{
	switch (desc.cm)
	{
	case cull_none:
		cm_func_ = cull_mode_none;
		break;

	case cull_front:
		cm_func_ = desc.front_ccw ? cull_mode_ccw : cull_mode_cw;
		break;

	case cull_back:
		cm_func_ = desc.front_ccw ? cull_mode_cw : cull_mode_ccw;
		break;

	default:
		custom_assert(false, "");
		break;
	}
	switch (desc.fm)
	{
	case fill_wireframe:
		clipping_func_ = fill_wireframe_clipping;
		triangle_rast_func_ = fill_wireframe_triangle_rasterize_func;
		break;

	case fill_solid:
		clipping_func_ = fill_solid_clipping;
		triangle_rast_func_ = fill_solid_triangle_rasterize_func;
		break;

	default:
		custom_assert(false, "");
		break;
	}
}

const rasterizer_desc& rasterizer_state::get_desc() const
{
	return desc_;
}

bool rasterizer_state::cull(float area) const
{
	return cm_func_(area);
}

void rasterizer_state::clipping(uint32_t& num_clipped_verts, vs_output* clipped_verts, uint32_t* clipped_indices, uint32_t base_vertex, const h_clipper& clipper, const viewport& vp, const vs_output** pv, float area) const
{
	clipping_func_(num_clipped_verts, clipped_verts, clipped_indices, base_vertex, clipper, vp, pv, area);
}

void rasterizer_state::triangle_rast_func(uint32_t& prim_size, boost::function<void (rasterizer*, const std::vector<uint32_t>&, const std::vector<vs_output>&, const std::vector<uint32_t>&, const viewport&, const h_pixel_shader&)>& rasterize_func) const
{
	triangle_rast_func_(prim_size, rasterize_func);
}

//inherited
void rasterizer::initialize(renderer_impl* pparent)
{
	pparent_ = pparent;
	hfb_ = pparent->get_framebuffer();
}

/*************************************************
 *   �߶εĹ�դ�����裺
 *			1 Ѱ�������򣬻��������������������������ϵĲ��
 *			2 ����ddx��ddy������mip��ѡ��
 *			3 �����������������ּ�������λ�ü�vs_output
 *			4 ִ��pixel shader
 *			5 ��������Ⱦ��framebuffer��
 *
 *   Note: 
 *			1 ������postion��λ�ڴ�������ϵ��
 *			2 wpos��x y z�����Ѿ�������clip w
 *			3 positon.wΪ1.0f / clip w
 **************************************************/
void rasterizer::rasterize_line(uint32_t /*prim_id*/, const vs_output& v0, const vs_output& v1, const viewport& vp, const h_pixel_shader& pps)
{
	vs_output diff = project(v1) - project(v0);
	const efl::vec4& dir = diff.position;
	float diff_dir = abs(dir.x) > abs(dir.y) ? dir.x : dir.y;

	h_blend_shader hbs = pparent_->get_blend_shader();

	//������
	vs_output ddx = diff * (diff.position.x / (diff.position.xy().length_sqr()));
	vs_output ddy = diff * (diff.position.y / (diff.position.xy().length_sqr()));

	int vpleft = fast_floori(max(0.0f, vp.x));
	int vptop = fast_floori(max(0.0f, vp.y));
	int vpright = fast_floori(min(vp.x+vp.w, (float)(hfb_->get_width())));
	int vpbottom = fast_floori(min(vp.y+vp.h, (float)(hfb_->get_height())));

	ps_output px_out;

	//��Ϊx major��y majorʹ��DDA������
	if( abs(dir.x) > abs(dir.y))
	{

		//�������յ㣬ʹ�������
		const vs_output *start, *end;
		if(dir.x < 0){
			start = &v1;
			end = &v0;
			diff_dir = -diff_dir;
		} else {
			start = &v0;
			end = &v1;
		}

		triangle_info info;
		info.set(start->position, ddx, ddy);
		pps->ptriangleinfo_ = &info;

		float fsx = fast_floor(start->position.x + 0.5f);

		int sx = fast_floori(fsx);
		int ex = fast_floori(end->position.x - 0.5f);

		//��ȡ����Ļ��
		sx = efl::clamp<int>(sx, vpleft, int(vpright - 1));
		ex = efl::clamp<int>(ex, vpleft, int(vpright));

		//��������vs_output
		vs_output px_start(project(*start));
		vs_output px_end(project(*end));
		float step = sx + 0.5f - start->position.x;
		vs_output px_in = lerp(px_start, px_end, step / diff_dir);

		//x-major ���߻���
		vs_output unprojed;
		for(int iPixel = sx; iPixel < ex; ++iPixel)
		{
			//���Բ���vp��Χ�ڵ�����
			if(px_in.position.y >= vpbottom){
				if(dir.y > 0) break;
				continue;
			}
			if(px_in.position.y < 0){
				if(dir.y < 0) break;
				continue;
			}

			//����������Ⱦ
			unproject(unprojed, px_in);
			if(pps->execute(unprojed, px_out)){
				hfb_->render_sample(hbs, iPixel, fast_floori(px_in.position.y), 0, px_out, px_out.depth);
			}

			//��ֵ���
			++ step;
			px_in = lerp(px_start, px_end, step / diff_dir);
		}
	}
	else //y major
	{
		//�����������ݷ���
		const vs_output *start, *end;
		if(dir.y < 0){
			start = &v1;
			end = &v0;
			diff_dir = -diff_dir;
		} else {
			start = &v0;
			end = &v1;
		}

		triangle_info info;
		info.set(start->position, ddx, ddy);
		pps->ptriangleinfo_ = &info;

		float fsy = fast_floor(start->position.y + 0.5f);

		int sy = fast_floori(fsy);
		int ey = fast_floori(end->position.y - 0.5f);

		//��ȡ����Ļ��
		sy = efl::clamp<int>(sy, vptop, int(vpbottom - 1));
		ey = efl::clamp<int>(ey, vptop, int(vpbottom));

		//��������vs_output
		vs_output px_start(project(*start));
		vs_output px_end(project(*end));
		float step = sy + 0.5f - start->position.y;
		vs_output px_in = lerp(px_start, px_end, step / diff_dir);

		//x-major ���߻���
		vs_output unprojed;
		for(int iPixel = sy; iPixel < ey; ++iPixel)
		{
			//���Բ���vp��Χ�ڵ�����
			if(px_in.position.x >= vpright){
				if(dir.x > 0) break;
				continue;
			}
			if(px_in.position.x < 0){
				if(dir.x < 0) break;
				continue;
			}

			//����������Ⱦ
			unproject(unprojed, px_in);
			if(pps->execute(unprojed, px_out)){
				hfb_->render_sample(hbs, fast_floori(px_in.position.x), iPixel, 0, px_out, px_out.depth);
			}

			//��ֵ���
			++ step;
			px_in = lerp(px_start, px_end, step / diff_dir);
		}
	}
}

void rasterizer::draw_whole_tile(int left, int top, int right, int bottom, const vs_output& v0, const vs_output& projed_v0,
		const vs_output& ddx, const vs_output& ddy, const h_pixel_shader& pps, const h_blend_shader& hbs, size_t num_samples){
	const float offsetx = left + 0.5f - v0.position.x;
	const float offsety = top + 0.5f - v0.position.y;

	//���û�׼ɨ���ߵ�����
	vs_output base_vert = projed_v0;
	integral(base_vert, offsety, ddy);
	integral(base_vert, offsetx, ddx);

	for(int iy = top; iy < bottom; ++iy)
	{
		//��դ��
		vs_output px_in(base_vert);
		ps_output px_out;
		vs_output unprojed;
		for(size_t ix = left; ix < right; ++ix)
		{
			unproject(unprojed, px_in);
			if(pps->execute(unprojed, px_out)){
				if (1 == num_samples){
					hfb_->render_sample(hbs, ix, iy, 0, px_out, px_in.position.z);
				}
				else{
					for (int i_sample = 0; i_sample < num_samples; ++ i_sample){
						const vec2& sp = samples_pattern_[i_sample];
						const float ddxz = (sp.x - 0.5f) * ddx.position.z;
						const float ddyz = (sp.y - 0.5f) * ddy.position.z;
						float sample_depth = px_in.position.z + ddxz + ddyz;
						hfb_->render_sample(hbs, ix, iy, i_sample, px_out, sample_depth);
					}
				}
			}

			integral(px_in, ddx);
		}

		//��ֵ���
		integral(base_vert, ddy);
	}
}

void rasterizer::draw_pixels(int left, int top, const vs_output& v0, const vs_output& projed_v0,
		const vs_output& ddx, const vs_output& ddy, const efl::vec3* edge_factors, const h_pixel_shader& pps, const h_blend_shader& hbs, size_t num_samples){
	const float offsetx = left + 0.5f - v0.position.x;
	const float offsety = top + 0.5f - v0.position.y;

	//���û�׼ɨ���ߵ�����
	vs_output base_vert = projed_v0;
	integral(base_vert, offsety, ddy);
	integral(base_vert, offsetx, ddx);

	float evalue[3];
	for (int e = 0; e < 3; ++ e){
		evalue[e] = edge_factors[e].z - (left * edge_factors[e].x + top * edge_factors[e].y);
	}

#ifndef EFLIB_NO_SIMD
	vs_output px_ins[4];
	px_ins[0] = base_vert;
	integral(px_ins[1], px_ins[0], ddx);
	integral(px_ins[2], base_vert, ddy);
	integral(px_ins[3], px_ins[2], ddx);

	const __m128 mtx = _mm_set_ps(1, 0, 1, 0);
	const __m128 mty = _mm_set_ps(1, 1, 0, 0);

	const __m128 mdepth = _mm_set_ps(px_ins[3].position.z, px_ins[2].position.z, px_ins[1].position.z, px_ins[0].position.z);
	const __m128 mbaseddxz = _mm_set1_ps(ddx.position.z);
	const __m128 mbaseddyz = _mm_set1_ps(ddy.position.z);
	ALIGN16 float samples_depth[MAX_NUM_MULTI_SAMPLES * 4];
	int samples_inside[MAX_NUM_MULTI_SAMPLES];
	int any_sample_inside = 0;
	for (int i_sample = 0; i_sample < num_samples; ++ i_sample){
		const vec2& sp = samples_pattern_[i_sample];
		__m128 mspx = _mm_set1_ps(sp.x);
		__m128 mspy = _mm_set1_ps(sp.y);

		__m128 mask_rej = _mm_setzero_ps();
		for (int e = 0; e < 3; ++ e){
			__m128 mstepx = _mm_set_ps1(edge_factors[e].x);
			__m128 mstepy = _mm_set_ps1(edge_factors[e].y);
			__m128 mx = _mm_add_ps(mtx, mspx);
			__m128 my = _mm_add_ps(mty, mspy);
			__m128 msteprej = _mm_add_ps(_mm_mul_ps(mx, mstepx), _mm_mul_ps(my, mstepy));

			__m128 mevalue = _mm_set1_ps(evalue[e]);

			mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(msteprej, mevalue));
		}

		samples_inside[i_sample] = ~_mm_movemask_ps(mask_rej);
		any_sample_inside |= samples_inside[i_sample];

		__m128 mdz;
		if (1 == num_samples){
			mdz = mdepth;
		}
		else{
			__m128 mhalf = _mm_set1_ps(0.5f);
			mspx = _mm_sub_ps(mspx, mhalf);
			mspy = _mm_sub_ps(mspy, mhalf);
			__m128 mddxz = _mm_mul_ps(mspx, mbaseddxz);
			__m128 mddyz = _mm_mul_ps(mspy, mbaseddyz);
			mdz = _mm_add_ps(mddxz, mddyz);
			mdz = _mm_add_ps(mdepth, mdz);
		}

		_mm_store_ps(&samples_depth[i_sample * 4], mdz);
	}
	any_sample_inside &= 0xF;

	ps_output px_out;
	vs_output unprojed;
	unsigned long t;
	while (_BitScanForward(&t, any_sample_inside)){
		custom_assert(t < 4, "");

		size_t ix = left + (t & 1);
		size_t iy = top + (t >> 1);

		unproject(unprojed, px_ins[t]);
		if (pps->execute(unprojed, px_out)){
			for (int i_sample = 0; i_sample < num_samples; ++ i_sample){
				if ((samples_inside[i_sample] >> t) & 1){
					hfb_->render_sample(hbs, ix, iy, i_sample, px_out, samples_depth[i_sample * 4 + t]);
				}
			}
		}

		any_sample_inside &= any_sample_inside - 1;
	}
#else
	for(int iy = 0; iy < 2; ++iy)
	{
		//��դ��
		vs_output px_in(base_vert);
		ps_output px_out;
		vs_output unprojed;
		for(size_t ix = 0; ix < 2; ++ix)
		{
			float samples_depth[MAX_NUM_MULTI_SAMPLES];
			bool samples_inside[MAX_NUM_MULTI_SAMPLES];
			bool any_sample_inside = false;
			for (int i_sample = 0; i_sample < num_samples; ++ i_sample){
				const vec2& sp = samples_pattern_[i_sample];
				const float fx = ix + sp.x;
				const float fy = iy + sp.y;
				bool inside = true;
				for (int e = 0; e < 3; ++ e){
					if (fx * edge_factors[e].x + fy * edge_factors[e].y < evalue[e]){
						inside = false;
						break;
					}
				}
				if (inside){
					samples_inside[i_sample] = true;
					any_sample_inside = true;
					const float ddxz = (sp.x - 0.5f) * ddx.position.z;
					const float ddyz = (sp.y - 0.5f) * ddy.position.z;
					samples_depth[i_sample] = px_in.position.z + ddxz + ddyz;
				}
			}

			if (any_sample_inside){
				unproject(unprojed, px_in);
				if(pps->execute(unprojed, px_out)){
					for (int i_sample = 0; i_sample < num_samples; ++ i_sample){
						if (samples_inside[i_sample]){
							hfb_->render_sample(hbs, ix + vpleft, iy + vptop, i_sample, px_out, samples_depth[i_sample]);
						}
					}
				}
			}

			integral(px_in, ddx);
		}

		//��ֵ���
		integral(base_vert, ddy);
	}
#endif
}

void rasterizer::subdivide_tile(int left, int top, const efl::rect<uint32_t>& cur_region, const vec3* edge_factors, const bool* mark_x, const bool* mark_y,
		uint32_t* test_regions, uint32_t& test_region_size){
	const uint32_t new_w = std::max<uint32_t>(1, cur_region.w / 2);
	const uint32_t new_h = std::max<uint32_t>(1, cur_region.h / 2);

	int2 base_corner[3];
	float evalue[3];
	float step_x[3];
	float step_y[3];
	float rej_to_acc[3];
	for (int e = 0; e < 3; ++ e){
		base_corner[e] = int2(left + mark_x[e] * new_w, top + mark_y[e] * new_h);
		evalue[e] = edge_factors[e].z - (base_corner[e].x * edge_factors[e].x + base_corner[e].y * edge_factors[e].y);
		step_x[e] = new_w * edge_factors[e].x;
		step_y[e] = new_h * edge_factors[e].y;
		rej_to_acc[e] = (mark_x[e] ? -step_x[e] : step_x[e]) + (mark_y[e] ? -step_y[e] : step_y[e]);
	}

#ifndef EFLIB_NO_SIMD
	const __m128 mtx = _mm_set_ps(1, 0, 1, 0);
	const __m128 mty = _mm_set_ps(1, 1, 0, 0);

	__m128 mask_rej = _mm_setzero_ps();
	__m128 mask_acc = _mm_setzero_ps();
	// Trival rejection & acception
	for (int e = 0; e < 3; ++ e){
		__m128 mstepx = _mm_set1_ps(step_x[e]);
		__m128 mstepy = _mm_set1_ps(step_y[e]);

		__m128 msteprej = _mm_add_ps(_mm_mul_ps(mtx, mstepx), _mm_mul_ps(mty, mstepy));
		__m128 mstepacc = _mm_add_ps(msteprej, _mm_set1_ps(rej_to_acc[e]));

		__m128 mevalue = _mm_set1_ps(evalue[e]);

		mask_rej = _mm_or_ps(mask_rej, _mm_cmplt_ps(msteprej, mevalue));
		mask_acc = _mm_or_ps(mask_acc, _mm_cmplt_ps(mstepacc, mevalue));
	}

	int rejections = ~_mm_movemask_ps(mask_rej) & 0xF;
	int accpetions = ~_mm_movemask_ps(mask_acc) & 0xF;
	unsigned long t;
	while (_BitScanForward(&t, rejections)){
		custom_assert(t < 4, "");

		uint32_t x = cur_region.x + new_w * (t & 1);
		uint32_t y = cur_region.y + new_h * (t >> 1);

		test_regions[test_region_size] = x + (y << 8) + (new_w << 16) + (new_h << 24) + (((accpetions >> t) & 1) << 31);
		++ test_region_size;

		rejections &= rejections - 1;
	}
#else
	for (int ty = 0; ty < 2; ++ ty){
		uint32_t y = cur_region.y + new_h * ty;
		for (int tx = 0; tx < 2; ++ tx){
			uint32_t x = cur_region.x + new_w * tx;

			int rejection = 0;
			int acception = 1;

			// Trival rejection & acception
			for (int e = 0; e < 3; ++ e){
				float step = tx * step_x[e] + ty * step_y[e];
				rejection |= (step < evalue[e]);
				acception &= (step + rej_to_acc[e] >= evalue[e]);
			}

			if (!rejection){
				test_regions[dst_stage][test_region_size[dst_stage]] = x + (y << 8) + (new_w << 16) + (new_h << 24) + (acception << 31);
				++ test_region_size[dst_stage];
			}
		}
	}
#endif
}

/*************************************************
*   �����εĹ�դ�����裺
*			1 ��դ������ɨ���߼�ɨ���߲����Ϣ
*			2 rasterizer_scanline_impl����ɨ����
*			3 ����������ص�vs_output
*			4 ִ��pixel shader
*			5 ��������Ⱦ��framebuffer��
*
*   Note: 
*			1 ������postion��λ�ڴ�������ϵ��
*			2 wpos��x y z�����Ѿ�������clip w
*			3 positon.wΪ1.0f / clip w
**************************************************/
void rasterizer::rasterize_triangle(uint32_t prim_id, const vs_output& v0, const vs_output& v1, const vs_output& v2, const viewport& vp, const h_pixel_shader& pps)
{

	//{
	//	boost::mutex::scoped_lock lock(logger_mutex_);
	//
	//	typedef slog<text_log_serializer> slog_type;
	//	log_serializer_indent_scope<log_system<slog_type>::slog_type> scope(&log_system<slog_type>::instance());

	//	//��¼�����ε���Ļ����ϵ���㡣
	//	log_system<slog_type>::instance().write(_EFLIB_T("wv0"),
	//		to_tstring(str(format("( %1%, %2%, %3%)") % v0.wpos.x % v0.wpos.y % v0.wpos.z)), LOGLEVEL_MESSAGE
	//		);
	//	log_system<slog_type>::instance().write(_EFLIB_T("wv1"), 
	//		to_tstring(str(format("( %1%, %2%, %3%)") % v1.wpos.x % v1.wpos.y % v1.wpos.z)), LOGLEVEL_MESSAGE
	//		);
	//	log_system<slog_type>::instance().write(_EFLIB_T("wv2"), 
	//		to_tstring(str(format("( %1%, %2%, %3%)") % v2.wpos.x % v2.wpos.y % v2.wpos.z)), LOGLEVEL_MESSAGE
	//		);
	//}

	/**********************************************************
	*        �����㰴��y��С�������������������
	**********************************************************/
	const vs_output* pvert[3] = {&v0, &v1, &v2};
	const vec3* edge_factors = &edge_factors_[prim_id * 3];
	const bool mark_x[3] = {
		edge_factors[0].x > 0, edge_factors[1].x > 0, edge_factors[2].x > 0
	};
	const bool mark_y[3] = {
		edge_factors[0].y > 0, edge_factors[1].y > 0, edge_factors[2].y > 0
	};		
	
	enum TRI_VS_TILE {
		TVT_FULL,
		TVT_PARTIAL,
		TVT_EMPTY,
		TVT_PIXEL
	};

	TRI_VS_TILE intersect = TVT_FULL;
	{
		const int min_corner[3] = {
			mark_y[0] * 2 + mark_x[0],
			mark_y[1] * 2 + mark_x[1],
			mark_y[2] * 2 + mark_x[2]
		};

		const int vpleft = fast_floori(max(0.0f, vp.x));
		const int vptop = fast_floori(max(0.0f, vp.y));
		const int vpright = fast_floori(min(vp.x + vp.w, static_cast<float>(hfb_->get_width())));
		const int vpbottom = fast_floori(min(vp.y + vp.h, static_cast<float>(hfb_->get_height())));

		const int2 corners[] = {
			int2(vpleft, vptop),
			int2(vpright, vptop),
			int2(vpleft, vpbottom),
			int2(vpright, vpbottom)
		};

		// Trival rejection
		for (int e = 0; e < 3; ++ e){
			int min_c = min_corner[e];
			if (corners[min_c].x * edge_factors[e].x
					+ corners[min_c].y * edge_factors[e].y
					< edge_factors[e].z){
				intersect = TVT_EMPTY;
				break;
			}
		}

		// Trival acception
		if (intersect != TVT_EMPTY){
			for (int e = 0; e < 3; ++ e){
				int max_c = 3 - min_corner[e];
				if (corners[max_c].x * edge_factors[e].x
						+ corners[max_c].y * edge_factors[e].y
						< edge_factors[e].z){
					intersect = TVT_PARTIAL;
					break;
				}
			}
		}
		else{
			return;
		}
	}

	vs_output projed_vert0 = project(*(pvert[0]));

	//��ʼ���߼��������ԵĲ�
	vs_output e01 = project(*(pvert[1])) - projed_vert0;
	vs_output e02 = project(*(pvert[2])) - projed_vert0;

	//�������
	float area = cross_prod2(e02.position.xy(), e01.position.xy());
	float inv_area = 1.0f / area;
	if(equal<float>(area, 0.0f)) return;

	/**********************************************************
	*  ���������ԵĲ��ʽ
	*********************************************************/
	vs_output ddx((e02 * e01.position.y - e02.position.y * e01)*inv_area);
	vs_output ddy((e01 * e02.position.x - e01.position.x * e02)*inv_area);

	triangle_info info;
	info.set(pvert[0]->position, ddx, ddy);
	pps->ptriangleinfo_ = &info;

	/*************************************************
	*   ��ʼ���ƶ���Ρ�
	*	The algorithm is from Larrabee
	*************************************************/

	const h_blend_shader& hbs = pparent_->get_blend_shader();
	const size_t num_samples = hfb_->get_num_samples();

	uint32_t test_regions[2][TILE_SIZE / 2 * TILE_SIZE / 2];
	uint32_t test_region_size[2] = { 0, 0 };
	test_regions[0][0] = (fast_floori(vp.w) << 16) + (fast_floori(vp.h) << 24) + ((TVT_FULL == intersect) << 31);
	test_region_size[0] = 1;
	int src_stage = 0;
	int dst_stage = !src_stage;

	while (test_region_size[src_stage] > 0){
		test_region_size[dst_stage] = 0;

		for (size_t ivp = 0; ivp < test_region_size[src_stage]; ++ ivp){
			const uint32_t packed_region = test_regions[src_stage][ivp];
			efl::rect<uint32_t> cur_region(packed_region & 0xFF, (packed_region >> 8) & 0xFF,
				(packed_region >> 16) & 0xFF, (packed_region >> 24) & 0x7F);
			TRI_VS_TILE intersect = (packed_region >> 31) ? TVT_FULL : TVT_PARTIAL;

			const int vpleft = fast_floori(max(0.0f, vp.x + cur_region.x));
			const int vptop = fast_floori(max(0.0f, vp.y + cur_region.y));
			const int vpright = fast_floori(min(vp.x + cur_region.x + cur_region.w, static_cast<float>(hfb_->get_width())));
			const int vpbottom = fast_floori(min(vp.y + cur_region.y + cur_region.h, static_cast<float>(hfb_->get_height())));

			// For one pixel region
			if ((TVT_PARTIAL == intersect) && (cur_region.w <= 2) && (cur_region.h <= 2)){
				intersect = TVT_PIXEL;
			}

			switch (intersect)
			{
			case TVT_EMPTY:
				break;

			case TVT_FULL: 
				this->draw_whole_tile(vpleft, vptop, vpright, vpbottom, *pvert[0], projed_vert0, ddx, ddy, pps, hbs, num_samples);
				break;

			case TVT_PIXEL:
				this->draw_pixels(vpleft, vptop, *pvert[0], projed_vert0, ddx, ddy, edge_factors, pps, hbs, num_samples);
				break;

			default:
				this->subdivide_tile(vpleft, vptop, cur_region, edge_factors, mark_x, mark_y, test_regions[dst_stage], test_region_size[dst_stage]);
				break;
			}
		}

		src_stage = (src_stage + 1) & 1;
		dst_stage = !src_stage;
	}
}

rasterizer::rasterizer()
{
	state_.reset(new rasterizer_state(rasterizer_desc()));
}
rasterizer::~rasterizer()
{
}

void rasterizer::set_state(const h_rasterizer_state& state)
{
	state_ = state;
}

const h_rasterizer_state& rasterizer::get_state() const
{
	return state_;
}

void rasterizer::geometry_setup_func(std::vector<uint32_t>& num_clipped_verts, std::vector<vs_output>& clipped_verts, std::vector<uint32_t>& cliped_indices,
		int32_t prim_count, primitive_topology primtopo, atomic<int32_t>& working_package, int32_t package_size){

	const int32_t num_packages = (prim_count + package_size - 1) / package_size;

	const h_vertex_cache& dvc = pparent_->get_vertex_cache();
	const h_clipper& clipper = pparent_->get_clipper();
	const viewport& vp = pparent_->get_viewport();

	uint32_t prim_size = 0;
	switch(primtopo)
	{
	case primitive_line_list:
	case primitive_line_strip:
		prim_size = 2;
		break;
	case primitive_triangle_list:
	case primitive_triangle_strip:
		prim_size = 3;
		break;
	default:
		custom_assert(false, "ö��ֵ��Ч����Ч��Primitive Topology");
		return;
	}

	int32_t local_working_package = working_package ++;
	while (local_working_package < num_packages){
		const int32_t start = local_working_package * package_size;
		const int32_t end = min(prim_count, start + package_size);
		for (int32_t i = start; i < end; ++ i){
			if (3 == prim_size){
				const vs_output* pv[3];
				vec2 pv_2d[3];
				for (size_t j = 0; j < 3; ++ j){
					const vs_output& v = dvc->fetch(i * 3 + j);
					pv[j] = &v;
					const float abs_w = abs(v.position.w);
					const float x = v.position.x / abs_w;
					const float y = v.position.y / abs_w;
					pv_2d[j] = vec2(x, y);
				}

				const float area = cross_prod2(pv_2d[2] - pv_2d[0], pv_2d[1] - pv_2d[0]);
				if (!state_->cull(area)){
					state_->clipping(num_clipped_verts[i], &clipped_verts[i * 6], &cliped_indices[i * 12], i * 6, clipper, vp, pv, area);
				}
				else{
					num_clipped_verts[i] = 0;
				}
			}
			else if (2 == prim_size){
				vs_output pv[2];
				for (size_t j = 0; j < prim_size; ++ j){
					pv[j] = dvc->fetch(i * prim_size + j);
				}

				vs_output tmp_verts[6];
				uint32_t num_out_clipped_verts;
				clipper->clip(tmp_verts, num_out_clipped_verts, vp, pv[0], pv[1]);
				num_clipped_verts[i] = num_out_clipped_verts;
				for (uint32_t j = 0; j < num_out_clipped_verts; ++ j){
					clipped_verts[i * 6 + j] = tmp_verts[j];
					cliped_indices[i * 12 + j] = static_cast<uint32_t>(i * 6 + j);
				}
			}
		}

		local_working_package = working_package ++;
	}
}

void rasterizer::dispatch_primitive_func(std::vector<lockfree_queue<uint32_t> >& tiles,
		const std::vector<uint32_t>& clipped_indices, const std::vector<vs_output>& clipped_verts_full, int32_t prim_count, uint32_t stride, atomic<int32_t>& working_package, int32_t package_size){

	const viewport& vp = pparent_->get_viewport();
	int num_tiles_x = static_cast<size_t>(vp.w + TILE_SIZE - 1) / TILE_SIZE;
	int num_tiles_y = static_cast<size_t>(vp.h + TILE_SIZE - 1) / TILE_SIZE;

	const int32_t num_packages = (prim_count + package_size - 1) / package_size;
	
	float x_min;
	float x_max;
	float y_min;
	float y_max;
	int32_t local_working_package = working_package ++;
	while (local_working_package < num_packages)
	{
		const int32_t start = local_working_package * package_size;
		const int32_t end = min(prim_count, start + package_size);
		for (int32_t i = start; i < end; ++ i)
		{
			vec2 pv[3];
			for (size_t j = 0; j < stride; ++ j){
				pv[j] = clipped_verts_full[clipped_indices[i * stride + j]].position.xy();
			}

			if (3 == stride){
				// x * (y1 - y0) - y * (x1 - x0) - (y1 * x0 - x1 * y0)
				vec3* edge_factors = &edge_factors_[i * 3];
				for (int e = 0; e < 3; ++ e){
					const int se = e;
					const int ee = (e + 1) % 3;
					edge_factors[e].x = pv[se].y - pv[ee].y;
					edge_factors[e].y = pv[ee].x - pv[se].x;
					edge_factors[e].z = pv[ee].x * pv[se].y - pv[ee].y * pv[se].x;
				}
			}

			x_min = pv[0].x;
			x_max = pv[0].x;
			y_min = pv[0].y;
			y_max = pv[0].y;
			for (size_t j = 1; j < stride; ++ j){
				x_min = min(x_min, pv[j].x);
				x_max = max(x_max, pv[j].x);
				y_min = min(y_min, pv[j].y);
				y_max = max(y_max, pv[j].y);
			}

			const int sx = std::min(fast_floori(std::max(0.0f, x_min) / TILE_SIZE), num_tiles_x);
			const int sy = std::min(fast_floori(std::max(0.0f, y_min) / TILE_SIZE), num_tiles_y);
			const int ex = std::min(fast_ceili(std::max(0.0f, x_max) / TILE_SIZE) + 1, num_tiles_x);
			const int ey = std::min(fast_ceili(std::max(0.0f, y_max) / TILE_SIZE) + 1, num_tiles_y);
			for (int y = sy; y < ey; ++ y){
				for (int x = sx; x < ex; ++ x){
					tiles[y * num_tiles_x + x].enqueue(i);
				}
			}
		}

		local_working_package = working_package ++;
	}
}

void rasterizer::rasterize_primitive_func(std::vector<lockfree_queue<uint32_t> >& tiles, int num_tiles_x,
		const std::vector<uint32_t>& clipped_indices, const std::vector<vs_output>& clipped_verts_full, const h_pixel_shader& pps, atomic<int32_t>& working_package, int32_t package_size)
{
	const int32_t num_tiles = static_cast<int32_t>(tiles.size());
	const int32_t num_packages = (num_tiles + package_size - 1) / package_size;

	const viewport& vp = pparent_->get_viewport();

	viewport tile_vp;
	tile_vp.w = TILE_SIZE;
	tile_vp.h = TILE_SIZE;
	tile_vp.minz = vp.minz;
	tile_vp.maxz = vp.maxz;

	int32_t local_working_package = working_package ++;
	while (local_working_package < num_packages){
		const int32_t start = local_working_package * package_size;
		const int32_t end = min(num_tiles, start + package_size);
		for (int32_t i = start; i < end; ++ i){
			lockfree_queue<uint32_t>& prims = tiles[i];

			std::vector<uint32_t> sorted_prims;
			prims.dequeue_all(std::back_insert_iterator<std::vector<uint32_t> >(sorted_prims));
			std::sort(sorted_prims.begin(), sorted_prims.end());

			int y = i / num_tiles_x;
			int x = i - y * num_tiles_x;
			tile_vp.x = static_cast<float>(x * TILE_SIZE);
			tile_vp.y = static_cast<float>(y * TILE_SIZE);

			rasterize_func_(this, clipped_indices, clipped_verts_full, sorted_prims, tile_vp, pps);
		}

		local_working_package = working_package ++;
	}
}

void rasterizer::rasterize_line_func(const std::vector<uint32_t>& clipped_indices, const std::vector<vs_output>& clipped_verts_full, const std::vector<uint32_t>& sorted_prims, const viewport& tile_vp, const h_pixel_shader& pps){
	for (std::vector<uint32_t>::const_iterator iter = sorted_prims.begin(); iter != sorted_prims.end(); ++ iter){
		uint32_t iprim = *iter;
		this->rasterize_line(iprim, clipped_verts_full[clipped_indices[iprim * 2 + 0]], clipped_verts_full[clipped_indices[iprim * 2 + 1]], tile_vp, pps);
	}
}

void rasterizer::rasterize_triangle_func(const std::vector<uint32_t>& clipped_indices, const std::vector<vs_output>& clipped_verts_full, const std::vector<uint32_t>& sorted_prims, const viewport& tile_vp, const h_pixel_shader& pps){
	for (std::vector<uint32_t>::const_iterator iter = sorted_prims.begin(); iter != sorted_prims.end(); ++ iter){
		uint32_t iprim = *iter;
		this->rasterize_triangle(iprim, clipped_verts_full[clipped_indices[iprim * 3 + 0]], clipped_verts_full[clipped_indices[iprim * 3 + 1]], clipped_verts_full[clipped_indices[iprim * 3 + 2]], tile_vp, pps);
	}
}

void rasterizer::compact_clipped_verts_func(std::vector<uint32_t>& clipped_indicess, const std::vector<uint32_t>& clipped_indices_full,
		const std::vector<uint32_t>& addresses, const std::vector<uint32_t>& num_clipped_verts, int32_t prim_count,
		atomic<int32_t>& working_package, int32_t package_size){
	const int32_t num_packages = (prim_count + package_size - 1) / package_size;
	
	int32_t local_working_package = working_package ++;
	while (local_working_package < num_packages)
	{
		const int32_t start = local_working_package * package_size;
		const int32_t end = min(prim_count, start + package_size);
		for (int32_t i = start; i < end; ++ i)
		{
			const uint32_t addr = addresses[i];
			for (uint32_t j = 0; j < num_clipped_verts[i]; ++ j){
				clipped_indicess[addr + j] = clipped_indices_full[i * 12 + j];
			}
		}

		local_working_package = working_package ++;
	}
}

void rasterizer::draw(size_t prim_count){

	custom_assert(pparent_, "Renderer ָ��Ϊ�գ����ܸö���û�о�����ȷ�ĳ�ʼ����");
	if(!pparent_) return;

	const size_t num_samples = hfb_->get_num_samples();
	switch (num_samples){
	case 1:
		samples_pattern_[0] = vec2(0.5f, 0.5f);
		break;

	case 2:
		samples_pattern_[0] = vec2(0.25f, 0.25f);
		samples_pattern_[1] = vec2(0.75f, 0.75f);
		break;

	case 4:
		samples_pattern_[0] = vec2(0.375f, 0.125f);
		samples_pattern_[1] = vec2(0.875f, 0.375f);
		samples_pattern_[2] = vec2(0.125f, 0.625f);
		samples_pattern_[3] = vec2(0.625f, 0.875f);
		break;

	default:
		break;
	}

	primitive_topology primtopo = pparent_->get_primitive_topology();

	uint32_t prim_size = 0;
	switch(primtopo)
	{
	case primitive_line_list:
	case primitive_line_strip:
		prim_size = 2;
		rasterize_func_ = boost::mem_fn(&rasterizer::rasterize_line_func);
		break;
	case primitive_triangle_list:
	case primitive_triangle_strip:
		state_->triangle_rast_func(prim_size, rasterize_func_);
		break;
	default:
		custom_assert(false, "ö��ֵ��Ч����Ч��Primitive Topology");
		return;
	}

	const viewport& vp = pparent_->get_viewport();
	int num_tiles_x = static_cast<size_t>(vp.w + TILE_SIZE - 1) / TILE_SIZE;
	int num_tiles_y = static_cast<size_t>(vp.h + TILE_SIZE - 1) / TILE_SIZE;
	std::vector<lockfree_queue<uint32_t> > tiles(num_tiles_x * num_tiles_y);

	atomic<int32_t> working_package(0);
	size_t num_threads = num_available_threads();

	// Culling, Clipping, Geometry setup
	std::vector<uint32_t> num_clipped_verts(prim_count);
	std::vector<vs_output> clipped_verts_full(prim_count * 6);
	std::vector<uint32_t> clipped_indices_full(prim_count * 12);
	for (size_t i = 0; i < num_threads - 1; ++ i){
		global_thread_pool().schedule(boost::bind(&rasterizer::geometry_setup_func, this, boost::ref(num_clipped_verts),
			boost::ref(clipped_verts_full), boost::ref(clipped_indices_full), static_cast<int32_t>(prim_count), primtopo,
			boost::ref(working_package), GEOMETRY_SETUP_PACKAGE_SIZE));
	}
	geometry_setup_func(boost::ref(num_clipped_verts), boost::ref(clipped_verts_full), boost::ref(clipped_indices_full),
		static_cast<int32_t>(prim_count), primtopo, boost::ref(working_package), GEOMETRY_SETUP_PACKAGE_SIZE);
	global_thread_pool().wait();

	std::vector<uint32_t> addresses(prim_count);
	addresses[0] = 0;
	for (size_t i = 1; i < prim_count; ++ i){
		addresses[i] = addresses[i - 1] + num_clipped_verts[i - 1];
	}

	std::vector<uint32_t> clipped_indices(addresses.back() + num_clipped_verts.back());
	working_package = 0;
	for (size_t i = 0; i < num_threads - 1; ++ i){
		global_thread_pool().schedule(boost::bind(&rasterizer::compact_clipped_verts_func, this, boost::ref(clipped_indices),
			boost::ref(clipped_indices_full), boost::ref(addresses),
			boost::ref(num_clipped_verts), prim_count, boost::ref(working_package), COMPACT_CLIPPED_VERTS_PACKAGE_SIZE));
	}
	compact_clipped_verts_func(boost::ref(clipped_indices), boost::ref(clipped_indices_full), boost::ref(addresses),
			boost::ref(num_clipped_verts), static_cast<int32_t>(prim_count), boost::ref(working_package), COMPACT_CLIPPED_VERTS_PACKAGE_SIZE);
	global_thread_pool().wait();

	working_package = 0;
	edge_factors_.resize(clipped_indices.size() / prim_size * 3);
	for (size_t i = 0; i < num_threads - 1; ++ i){
		global_thread_pool().schedule(boost::bind(&rasterizer::dispatch_primitive_func, this, boost::ref(tiles),
			boost::ref(clipped_indices), boost::ref(clipped_verts_full), static_cast<int32_t>(clipped_indices.size() / prim_size),
			prim_size, boost::ref(working_package), DISPATCH_PRIMITIVE_PACKAGE_SIZE));
	}
	dispatch_primitive_func(boost::ref(tiles),
		boost::ref(clipped_indices), boost::ref(clipped_verts_full), static_cast<int32_t>(clipped_indices.size() / prim_size),
		prim_size, boost::ref(working_package), DISPATCH_PRIMITIVE_PACKAGE_SIZE);
	global_thread_pool().wait();

	working_package = 0;
	h_pixel_shader hps = pparent_->get_pixel_shader();
	std::vector<h_pixel_shader> ppps(num_threads - 1);
	for (size_t i = 0; i < num_threads - 1; ++ i){
		// create pixel_shader clone per thread from hps
		ppps[i] = hps->create_clone();
		global_thread_pool().schedule(boost::bind(&rasterizer::rasterize_primitive_func, this, boost::ref(tiles),
			num_tiles_x, boost::ref(clipped_indices), boost::ref(clipped_verts_full), ppps[i], boost::ref(working_package), RASTERIZE_PRIMITIVE_PACKAGE_SIZE));
	}
	rasterize_primitive_func(boost::ref(tiles), num_tiles_x, boost::ref(clipped_indices), boost::ref(clipped_verts_full), hps, boost::ref(working_package), RASTERIZE_PRIMITIVE_PACKAGE_SIZE);
	global_thread_pool().wait();
	// destroy all pixel_shader clone
	for (size_t i = 0; i < num_threads - 1; ++ i){
		hps->destroy_clone(ppps[i]);
	}
}

END_NS_SOFTART()
