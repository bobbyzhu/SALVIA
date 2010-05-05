#include "../include/rasterizer.h"

#include "../include/shaderregs_op.h"
#include "../include/clipper.h"
#include "../include/framebuffer.h"
#include "../include/renderer_impl.h"
#include "../include/cpuinfo.h"

#include "eflib/include/slog.h"

#include <algorithm>
#include <boost/format.hpp>
BEGIN_NS_SOFTART()


using namespace std;
using namespace efl;
using namespace boost;

struct scanline_info
{
	size_t scanline_width;

	vs_output ddx;

	vs_output base_vert;
	size_t base_x;
	size_t base_y;

	scanline_info()
	{}

	scanline_info(const scanline_info& rhs)
		:ddx(rhs.ddx), base_vert(rhs.base_vert), scanline_width(scanline_width),
		base_x(rhs.base_x), base_y(rhs.base_y)
	{}

	scanline_info& operator = (const scanline_info& rhs){
		ddx = rhs.ddx;
		base_vert = rhs.base_vert;
		base_x = rhs.base_x;
		base_y = rhs.base_y;
	}
};

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
void rasterizer::rasterize_line_impl(const vs_output& v0, const vs_output& v1, const viewport& vp, const h_pixel_shader& pps)
{

	vs_output diff = project(v1) - project(v0);
	const efl::vec4& dir = diff.wpos;
	float diff_dir = abs(dir.x) > abs(dir.y) ? dir.x : dir.y;

	h_blend_shader hbs = pparent_->get_blend_shader();

	//������
	vs_output derivation = diff;

	vs_output ddx = diff * (diff.wpos.x / (diff.wpos.xy().length_sqr()));
	vs_output ddy = diff * (diff.wpos.y / (diff.wpos.xy().length_sqr()));

	int vpleft = fast_floori(max(0.0f, vp.x));
	int vpbottom = fast_floori(max(0.0f, vp.y));
	int vpright = fast_floori(min(vp.x+vp.w, (float)(hfb_->get_width())));
	int vptop = fast_floori(min(vp.y+vp.h, (float)(hfb_->get_height())));

	ps_output px_out;

	//��Ϊx major��y majorʹ��DDA������
	if( abs(dir.x) > abs(dir.y))
	{

		//�������յ㣬ʹ�������
		const vs_output *start, *end;
		if(dir.x < 0){
			start = &v1;
			end = &v0;
		} else {
			start = &v0;
			end = &v1;
		}

		triangle_info info;
		info.set(start->wpos, ddx, ddy);
		pps->ptriangleinfo_ = &info;

		float fsx = fast_floor(start->wpos.x + 0.5f);

		int sx = fast_floori(fsx);
		int ex = fast_floori(end->wpos.x - 0.5f);

		//��ȡ����Ļ��
		sx = efl::clamp<int>(sx, vpleft, int(vpright - 1));
		ex = efl::clamp<int>(ex, vpleft, int(vpright));

		//��������vs_output
		vs_output px_start(project(*start));
		vs_output px_end(project(*end));
		float step = fsx + 0.5f - start->wpos.x;
		vs_output px_in = lerp(px_start, px_end, step / diff_dir);

		//x-major ���߻���
		vs_output unprojed;
		for(int iPixel = sx; iPixel < ex; ++iPixel)
		{
			//���Բ���vp��Χ�ڵ�����
			if(px_in.wpos.y >= vpbottom){
				if(dir.y > 0) break;
				continue;
			}
			if(px_in.wpos.y < 0){
				if(dir.y < 0) break;
				continue;
			}

			//����������Ⱦ
			unproject(unprojed, px_in);
			if(pps->execute(unprojed, px_out)){
				hfb_->render_pixel(hbs, iPixel, fast_floori(px_in.wpos.y), px_out);
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
		} else {
			start = &v0;
			end = &v1;
		}

		triangle_info info;
		info.set(start->wpos, ddx, ddy);
		pps->ptriangleinfo_ = &info;

		float fsy = fast_floor(start->wpos.y + 0.5f);

		int sy = fast_floori(fsy);
		int ey = fast_floori(end->wpos.y - 0.5f);

		//��ȡ����Ļ��
		sy = efl::clamp<int>(sy, vptop, int(vpbottom - 1));
		ey = efl::clamp<int>(ey, vptop, int(vpbottom));

		//��������vs_output
		vs_output px_start(project(*start));
		vs_output px_end(project(*end));
		float step = fsy + 0.5f - start->wpos.y;
		vs_output px_in = lerp(px_start, px_end, (fsy + 0.5f - start->wpos.y) / diff_dir);

		//x-major ���߻���
		vs_output unprojed;
		for(int iPixel = sy; iPixel < ey; ++iPixel)
		{
			//���Բ���vp��Χ�ڵ�����
			if(px_in.wpos.x >= vpright){
				if(dir.x > 0) break;
				continue;
			}
			if(px_in.wpos.x < 0){
				if(dir.x < 0) break;
				continue;
			}

			//����������Ⱦ
			unproject(unprojed, px_in);
			if(pps->execute(unprojed, px_out)){
				hfb_->render_pixel(hbs, fast_floori(px_in.wpos.x), iPixel, px_out);
			}

			//��ֵ���
			++ step;
			px_in = lerp(px_start, px_end, step / diff_dir);
		}
	}
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
void rasterizer::rasterize_triangle_impl(const vs_output& v0, const vs_output& v1, const vs_output& v2, const viewport& vp, const h_pixel_shader& pps)
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

	//��������
	if(pvert[0]->wpos.y > pvert[1]->wpos.y){
		swap(pvert[1], pvert[0]);
	}
	if(pvert[1]->wpos.y > pvert[2]->wpos.y){
		swap(pvert[2], pvert[1]);
		if(pvert[0]->wpos.y > pvert[1]->wpos.y) 
			swap(pvert[1], pvert[0]);
	}

	vs_output projed_vert0 = project(*(pvert[0]));

	//��ʼ���߼��������ԵĲ�
	vs_output e01 = project(*(pvert[1])) - projed_vert0;
	//float watch_x = e01.attributes[2].x;
	
	vs_output e02 = project(*(pvert[2])) - projed_vert0;
	vs_output e12;



	//��ʼ�����ϵĸ���������ֵ����ֻҪ�������߾Ϳ����ˡ���
	e12.wpos = pvert[2]->wpos - pvert[1]->wpos;

	//��ʼ��dxdy
	float dxdy_01 = efl::equal<float>(e01.wpos.y, 0.0f) ? 0.0f: e01.wpos.x / e01.wpos.y;
	float dxdy_02 = efl::equal<float>(e02.wpos.y, 0.0f) ? 0.0f: e02.wpos.x / e02.wpos.y;
	float dxdy_12 = efl::equal<float>(e12.wpos.y, 0.0f) ? 0.0f: e12.wpos.x / e12.wpos.y;

	//�������
	float area = cross_prod2(e02.wpos.xy(), e01.wpos.xy());
	float inv_area = 1.0f / area;
	if(equal<float>(area, 0.0f)) return;

	/**********************************************************
	*  ���������ԵĲ��ʽ
	*********************************************************/
	vs_output ddx((e02 * e01.wpos.y - e02.wpos.y * e01)*inv_area);
	vs_output ddy((e01 * e02.wpos.x - e01.wpos.x * e02)*inv_area);

	triangle_info info;
	info.set(pvert[0]->wpos, ddx, ddy);
	pps->ptriangleinfo_ = &info;

	/*************************************
	*   ���û�����scanline���ԡ�
	*   ��Щ���Խ��ڶ��ɨ�����б�����ͬ
	************************************/
	scanline_info base_scanline;
	base_scanline.ddx = ddx;

	/*************************************************
	*   ��ʼ���ƶ���Ρ��������-�·ָ�����㷨
	*   ��ɨ���߹�դ����֤���������ҵġ�
	*   ���Բ���Ҫ����major edge����������ұߡ�
	*************************************************/

	const int bot_part = 0;
	//const int top_part = 1;

	int vpleft = fast_floori(max(0.0f, vp.x));
	int vpbottom = fast_floori(max(0.0f, vp.y));
	int vpright = fast_floori(min(vp.x+vp.w, (float)(hfb_->get_width())));
	int vptop = fast_floori(min(vp.y+vp.h, (float)(hfb_->get_height())));

	for(int iPart = 0; iPart < 2; ++iPart){

		//�����ߵ�dxdy
		float dxdy0 = 0.0f;
		float dxdy1 = 0.0f;

		//��ʼ/��ֹ��x��y����; 
		//�������λ�׼��λ��,���ڼ��㶥������
		float
			fsx0(0.0f), fsx1(0.0f), // ��׼�����ֹx����
			fsy(0.0f), fey(0.0f),	// Part����ֹɨ����y����
			fcx0(0.0f), fcx1(0.0f), // ����ɨ���ߵ���ֹ������
			offsety(0.0f);

		int isy(0), iey(0);	//Part����ֹɨ���ߺ�

		//ɨ���ߵ���ֹ����
		const vs_output* s_vert = NULL;
		const vs_output* e_vert = NULL;

		//����Ƭ��������ʼ����
		if(iPart == bot_part){
			s_vert = pvert[0];
			e_vert = pvert[1];

			dxdy0 = dxdy_01;
		} else {
			s_vert = pvert[1];
			e_vert = pvert[2];

			dxdy0 = dxdy_12;
		}
		dxdy1 = dxdy_02;

		if(equal<float>(s_vert->wpos.y, e_vert->wpos.y)){
			continue; // next part
		}

		fsy = fast_ceil(s_vert->wpos.y + 0.5f) - 1;
		fey = fast_ceil(e_vert->wpos.y - 0.5f) - 1;

		isy = fast_floori(fsy);
		iey = fast_floori(fey);

		offsety = fsy + 0.5f - pvert[0]->wpos.y;

		//����x�������������εĲ�ͬ��������ͬ
		if(iPart == bot_part){
			fsx0 = pvert[0]->wpos.x + dxdy_01*(fsy + 0.5f - pvert[0]->wpos.y);
		} else {
			fsx0 = pvert[1]->wpos.x + dxdy_12*(fsy + 0.5f - pvert[1]->wpos.y);
		}
		fsx1 = pvert[0]->wpos.x + dxdy_02*(fsy + 0.5f - pvert[0]->wpos.y);

		//���û�׼ɨ���ߵ�����
		base_scanline.base_vert = projed_vert0;
		integral(base_scanline.base_vert, offsety, ddy);

		//��ǰ�Ļ�׼ɨ���ߣ������(base_vert.x, scanline.y)����
		//�ڴ��ݵ�rasterize_scanline֮ǰ��Ҫ�������������ɨ���ߵ�����ˡ�
		scanline_info current_base_scanline(base_scanline);

		for(int iy = isy; iy <= iey; ++iy)
		{	
			//���ɨ������view port��������������	
			if( iy >= vptop ){
				break;
			}

			if( iy >= vpbottom ){
				//ɨ�������ӿ��ڵľ���ɨ����
				int icx_s = 0;
				int icx_e = 0;

				fcx0 = dxdy0 * (iy - isy) + fsx0;
				fcx1 = dxdy1 * (iy - isy) + fsx1;

				//LOG: ��¼ɨ���ߵ���ֹ�㡣�汾
				//if (fcx0 > 256.0 && iy == 222)
				//{
				//	log_serializer_indent_scope<log_system<slog_type>::slog_type> scope(&log_system<slog_type>::instance());
				//	log_system<slog_type>::instance().write(
				//		to_tstring(str(format("%1%") % iy)), 
				//		to_tstring(str(format("%1$8.5f, %2$8.5f") % fcx0 % fcx1)), LOGLEVEL_MESSAGE
				//		);
				//}

				if(fcx0 < fcx1){
					icx_s = fast_ceili(fcx0 + 0.5f) - 1;
					icx_e = fast_ceili(fcx1 - 0.5f) - 1;
				} else {
					icx_s = fast_ceili(fcx1 + 0.5f) - 1;
					icx_e = fast_ceili(fcx0 - 0.5f) - 1;
				}

				//����������յ�˵��scanline�в������κ��������ģ�ֱ��������
				if ((icx_s <= icx_e) && (icx_s < vpright) && (icx_e >= vpleft)) {
					icx_s = efl::clamp(icx_s, vpleft, vpright - 1);
					icx_e = efl::clamp(icx_e, vpleft, vpright - 1);

					float offsetx = icx_s + 0.5f - pvert[0]->wpos.x;

					//����ɨ������Ϣ
					scanline_info scanline(current_base_scanline);
					integral(scanline.base_vert, offsetx, ddx);

					scanline.base_x = icx_s;
					scanline.base_y = iy;
					scanline.scanline_width = icx_e - icx_s + 1;

					//��դ��
					rasterize_scanline_impl(scanline, pps);
				}
			}

			//��ֵ���
			integral(current_base_scanline.base_vert, 1.0f, ddy);
		}
	}
}

//ɨ���߹�դ�����򣬽���ɨ�������ݲ����Ϣ���й�դ��������դ����Ƭ�δ��ݵ�������ɫ����.
//Note:��������ؽ�w�˻ص�attribute��.
void rasterizer::rasterize_scanline_impl(const scanline_info& sl, const h_pixel_shader& pps)
{

	h_blend_shader hbs = pparent_->get_blend_shader();

	vs_output px_in(sl.base_vert);
	ps_output px_out;
	vs_output unprojed;
	for(size_t i_pixel = 0; i_pixel < sl.scanline_width; ++i_pixel)
	{
		//if(px_in.wpos.z <= 0.0f)
			//continue;
 
		unproject(unprojed, px_in);
		if(pps->execute(unprojed, px_out)){
			hfb_->render_pixel(hbs, sl.base_x + i_pixel, sl.base_y, px_out);
		}

		integral(px_in, 1.0f, sl.ddx);
	}
}

rasterizer::rasterizer()
{
	cm_ = cull_back;
	fm_ = fill_solid;
}
rasterizer::~rasterizer()
{
}
void rasterizer::rasterize_line(const vs_output& v0, const vs_output& v1, const viewport& vp, const h_pixel_shader& pps)
{
	//�����ȫ�����߽磬���޳�

	if(v0.wpos.x < 0 && v1.wpos.x < 0) return;
	if(v0.wpos.y < 0 && v1.wpos.y < 0) return;
	if(v0.wpos.z < vp.minz && v1.wpos.z < vp.minz) return;

	if(v0.wpos.x >= vp.w && v1.wpos.x >= vp.w) return;
	if(v0.wpos.y >= vp.w && v1.wpos.y >= vp.w) return;
	if(v0.wpos.z >= vp.maxz && v1.wpos.z >= vp.maxz) return;

	//render
	rasterize_line_impl(v0, v1, vp, pps);
}

void rasterizer::rasterize_triangle(const vs_output& v0, const vs_output& v1, const vs_output& v2, const viewport& vp, const h_pixel_shader& pps)
{
	//�߽��޳�
	
	//�����޳�
	if(cm_ != cull_none) {
		const vec2 pv0 = v0.position.xy() * abs(v0.wpos.w);
		const vec2 pv1 = v1.position.xy() * abs(v1.wpos.w);
		const vec2 pv2 = v2.position.xy() * abs(v2.wpos.w);
		const float area = cross_prod2(pv1 - pv0, pv2 - pv0);
		if( (cm_ == cull_front) && (area > 0) ) {
			return;
		}
		if( (cm_ == cull_back) && (area < 0) ) {
			return;
		}
	}

	//��Ⱦ
	if(fm_ == fill_wireframe)
	{
		rasterize_line(v0, v1, vp, pps);
		rasterize_line(v1, v2, vp, pps);
		rasterize_line(v0, v2, vp, pps);
	} else
	{
		vector<vs_output> clipped_verts;

		h_clipper clipper = pparent_->get_clipper();
		clipper->clip(clipped_verts , pparent_->get_viewport() , v0, v1, v2);

		for(int i_tri = 1; i_tri < int(clipped_verts.size()) - 1; ++i_tri)
		{
			rasterize_triangle_impl(clipped_verts[0], clipped_verts[i_tri], clipped_verts[i_tri+1], vp, pps);
		}
	}
}

void rasterizer::set_cull_mode(cull_mode cm)
{
	cm_ = cm;
}

void rasterizer::set_fill_mode(fill_mode fm)
{
	fm_ = fm;
}
END_NS_SOFTART()
