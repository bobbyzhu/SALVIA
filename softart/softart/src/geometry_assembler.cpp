#include "../include/geometry_assembler.h"

#include "eflib/include/eflib.h"

#include "../include/shaderregs_op.h"
#include "../include/shader.h"
#include "../include/renderer_impl.h"

#include "../include/stream.h"
#include "../include/buffer.h"
#include "../include/rasterizer.h"
#include "../include/stream_assembler.h"
#include "../include/vertex_cache.h"
#include "../include/cpuinfo.h"
#include "../include/thread_pool.h"

#include <iterator>

#include <boost/bind.hpp>
BEGIN_NS_SOFTART()


using namespace std;
using namespace efl;

const int TILE_SIZE = 64;
const int DISPATCH_PRIMITIVE_PACKAGE_SIZE = 1;
const int RASTERIZE_PRIMITIVE_PACKAGE_SIZE = 1;

void geometry_assembler::initialize(renderer_impl* pparent)
{
	custom_assert(pparent, "��ʼ�������쳣������ĳ�ʼ��ָ��Ϊ�գ�");

	pparent_ = pparent;
}

geometry_assembler::geometry_assembler()
{}

void geometry_assembler::dispatch_primitive_func(std::vector<lockfree_queue<uint32_t> >& tiles, int num_tiles_x, int num_tiles_y, int32_t prim_count, uint32_t stride, atomic<int32_t>& working_package, int32_t package_size){
	const h_vertex_cache& dvc = pparent_->get_vertex_cache();
	const h_rasterizer& hrast = pparent_->get_rasterizer();
	const cull_mode cm = hrast->get_cull_mode();

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
				const vs_output& v = dvc->fetch(i * stride + j);
				const float sign_w = sign(v.wpos.w);
				const float x = v.wpos.x * sign_w;
				const float y = v.wpos.y * sign_w;
				pv[j] = vec2(x, y);
			}

			bool culled = false;
			if ((3 == stride) && (cm != cull_none)){
				//�����޳�
				const float area = cross_prod2(pv[1] - pv[0], pv[2] - pv[0]);
				if (((cm == cull_front) && (area > 0))
						|| ((cm == cull_back) && (area < 0))){
					culled = true;
				}
			}

			if (!culled){
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
						lockfree_queue<uint32_t>& tile = tiles[y * num_tiles_x + x];
						tile.enqueue(i);
					}
				}
			}
		}

		local_working_package = working_package ++;
	}
}

void geometry_assembler::rasterize_primitive_func(std::vector<lockfree_queue<uint32_t> >& tiles, int num_tiles_x, const h_pixel_shader& pps, atomic<int32_t>& working_package, int32_t package_size)
{
	const h_vertex_cache& dvc = pparent_->get_vertex_cache();

	const int32_t num_tiles = static_cast<int32_t>(tiles.size());
	const int32_t num_packages = (num_tiles + package_size - 1) / package_size;

	const h_rasterizer& hrast = pparent_->get_rasterizer();
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

			rasterize_func_(this, dvc, hrast, sorted_prims, tile_vp, pps);
		}

		local_working_package = working_package ++;
	}
}

void geometry_assembler::rasterize_line_func(const h_vertex_cache& dvc, const h_rasterizer& hrast, const std::vector<uint32_t>& sorted_prims, const viewport& tile_vp, const h_pixel_shader& pps){
	for (std::vector<uint32_t>::const_iterator iter = sorted_prims.begin(); iter != sorted_prims.end(); ++ iter){
		uint32_t iprim = *iter;
		hrast->rasterize_line(dvc->fetch(iprim * 2 + 0), dvc->fetch(iprim * 2 + 1), tile_vp, pps);
	}
}

void geometry_assembler::rasterize_triangle_func(const h_vertex_cache& dvc, const h_rasterizer& hrast, const std::vector<uint32_t>& sorted_prims, const viewport& tile_vp, const h_pixel_shader& pps){
	for (std::vector<uint32_t>::const_iterator iter = sorted_prims.begin(); iter != sorted_prims.end(); ++ iter){
		uint32_t iprim = *iter;
		hrast->rasterize_triangle(dvc->fetch(iprim * 3 + 0), dvc->fetch(iprim * 3 + 1), dvc->fetch(iprim * 3 + 2), tile_vp, pps);
	}
}

void geometry_assembler::draw(size_t prim_count){

	custom_assert(pparent_, "Renderer ָ��Ϊ�գ����ܸö���û�о�����ȷ�ĳ�ʼ����");
	if(!pparent_) return;

	const h_rasterizer& hrast = pparent_->get_rasterizer();
	if(!hrast) return;

	primitive_topology primtopo = pparent_->get_primitive_topology();

	//������������
	size_t index_count = 0;
	switch(primtopo)
	{
	case primitive_line_list:
		index_count = prim_count * 2;
		break;
	case primitive_line_strip:
		index_count = prim_count + 1;
		break;
	case primitive_triangle_list:
		index_count = prim_count * 3;
		break;
	case primitive_triangle_strip:
		index_count = prim_count + 2;
		break;
	default:
		custom_assert(false, "ö��ֵ��Ч����Ч��Primitive Topology");
		return;
	}
	
	uint32_t prim_size = 0;
	switch(primtopo)
	{
	case primitive_line_list:
	case primitive_line_strip:
		prim_size = 2;
		rasterize_func_ = boost::mem_fn(&geometry_assembler::rasterize_line_func);
		break;
	case primitive_triangle_list:
	case primitive_triangle_strip:
		prim_size = 3;
		rasterize_func_ = boost::mem_fn(&geometry_assembler::rasterize_triangle_func);
		break;
	default:
		custom_assert(false, "ö��ֵ��Ч����Ч��Primitive Topology");
		return;
	}

	const viewport& vp = pparent_->get_viewport();
	int num_tiles_x = static_cast<size_t>(vp.w + TILE_SIZE - 1) / TILE_SIZE;
	int num_tiles_y = static_cast<size_t>(vp.h + TILE_SIZE - 1) / TILE_SIZE;
	std::vector<lockfree_queue<uint32_t> > tiles(num_tiles_x * num_tiles_y);

	//�任����
	pparent_->get_vertex_cache()->transform_vertices(static_cast<uint32_t>(prim_count));

	atomic<int32_t> working_package(0);
	size_t num_threads = num_available_threads();

	for (size_t i = 0; i < num_threads - 1; ++ i)
	{
		global_thread_pool().schedule(boost::bind(&geometry_assembler::dispatch_primitive_func, this, boost::ref(tiles),
			num_tiles_x, num_tiles_y, static_cast<int32_t>(prim_count),
			prim_size, boost::ref(working_package), DISPATCH_PRIMITIVE_PACKAGE_SIZE));
	}
	dispatch_primitive_func(boost::ref(tiles), num_tiles_x, num_tiles_y, static_cast<int32_t>(prim_count),
		prim_size, boost::ref(working_package), DISPATCH_PRIMITIVE_PACKAGE_SIZE);
	global_thread_pool().wait();

	working_package = 0;
	h_pixel_shader hps = pparent_->get_pixel_shader();
	std::vector<h_pixel_shader> ppps(num_threads - 1);
	for (size_t i = 0; i < num_threads - 1; ++ i)
	{
		// create pixel_shader clone per thread from hps
		ppps[i] = hps->create_clone();
		global_thread_pool().schedule(boost::bind(&geometry_assembler::rasterize_primitive_func, this, boost::ref(tiles),
			num_tiles_x, ppps[i], boost::ref(working_package), RASTERIZE_PRIMITIVE_PACKAGE_SIZE));
	}
	rasterize_primitive_func(boost::ref(tiles), num_tiles_x, hps, boost::ref(working_package), RASTERIZE_PRIMITIVE_PACKAGE_SIZE);
	global_thread_pool().wait();
	// destroy all pixel_shader clone
	for (size_t i = 0; i < num_threads - 1; ++ i)
	{
		hps->destroy_clone(ppps[i]);
	}
}

END_NS_SOFTART()
