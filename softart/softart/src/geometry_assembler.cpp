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

#include <boost/bind.hpp>
BEGIN_NS_SOFTART()


using namespace std;
using namespace efl;

const int TILE_SIZE = 256;

void geometry_assembler::initialize(renderer_impl* pparent)
{
	custom_assert(pparent, "��ʼ�������쳣������ĳ�ʼ��ָ��Ϊ�գ�");

	pparent_ = pparent;
	dvc_.initialize(pparent);
}

geometry_assembler::geometry_assembler()
:indexbuf_((buffer*)NULL),
primtopo_(primitive_triangle_list), idxtype_(index_int16)
{}

void geometry_assembler::set_primitive_topology(primitive_topology prim_topo){
	switch (prim_topo)
	{
	case primitive_line_list:
	case primitive_line_strip:
	case primitive_triangle_list:
	case primitive_triangle_strip:
		break;
	default:
		custom_assert(false, "ö��ֵ��Ч����Ч��ͼԪ����ö�١�");
		return;
	}

	primtopo_ = prim_topo;
}

void geometry_assembler::set_stream(size_t index, h_buffer hbuf){
	sa_.set_stream(stream_index(index), hbuf);
}

void geometry_assembler::set_index_buffer(h_buffer hbuf, index_type idxtype){
	switch (idxtype)
	{
	case index_int16:
	case index_int32:
		break;
	default:
		custom_assert(false, "ö��ֵ��Ч����Ч����������");
		return;
	}

	indexbuf_ = hbuf;
	idxtype_ = idxtype;
}

void geometry_assembler::set_input_layout(const vector<input_element_decl>& elem_decl){
	//layout_ ֻ�ܵ������ڼ����...
	sa_.set_input_layout(elem_decl);
}

void geometry_assembler::dispatch_primitive_impl(std::vector<lockfree_queue<uint32_t> >& tiles, const std::vector<uint32_t>& indices, atomic<int32_t>& working_prim, int32_t prim_count, uint32_t stride){
	int32_t local_working_prim = working_prim ++;

	float x_min;
	float x_max;
	float y_min;
	float y_max;
	while (local_working_prim < prim_count)
	{
		const vs_output& v0 = dvc_.fetch(indices[local_working_prim * stride + 0]);
		x_min = v0.wpos.x;
		x_max = v0.wpos.x;
		y_min = v0.wpos.y;
		y_max = v0.wpos.y;

		for (size_t i = 1; i < stride; ++ i)
		{
			const vs_output& v = dvc_.fetch(indices[local_working_prim * stride + i]);
			x_min = min(x_min, v.wpos.x);
			x_max = max(x_max, v.wpos.x);
			y_min = min(y_min, v.wpos.y);
			y_max = max(y_max, v.wpos.y);
		}

		int sx = efl::clamp(static_cast<int>(floor(x_min / TILE_SIZE)), 0, num_tiles_x_);
		int sy = efl::clamp(static_cast<int>(floor(y_min / TILE_SIZE)), 0, num_tiles_y_);
		int ex = efl::clamp(static_cast<int>(ceil(x_max / TILE_SIZE)), 0, num_tiles_x_);
		int ey = efl::clamp(static_cast<int>(ceil(y_max / TILE_SIZE)), 0, num_tiles_y_);

		for (int y = sy; y < ey; ++ y){
			for (int x = sx; x < ex; ++ x){
				lockfree_queue<uint32_t>& tile = tiles[y * num_tiles_x_ + x];
				tile.enqueue(local_working_prim);
			}
		}

		local_working_prim = working_prim ++;
	}
}

void geometry_assembler::rasterize_primitive_func(std::vector<lockfree_queue<uint32_t> >& tiles, const std::vector<uint32_t>& indices, atomic<int32_t>& working_tile , const h_pixel_shader& pps)
{
	const h_rasterizer& hrast = pparent_->get_rasterizer();
	const viewport& vp = pparent_->get_viewport();

	viewport tile_vp;
	tile_vp.w = TILE_SIZE;
	tile_vp.h = TILE_SIZE;
	tile_vp.minz = vp.minz;
	tile_vp.maxz = vp.maxz;

	int32_t local_working_tile = working_tile ++;

	while (local_working_tile < tiles.size()){
		lockfree_queue<uint32_t>& prims = tiles[local_working_tile];

		std::vector<uint32_t> sorted_prims;
		uint32_t iprim;
		while (!prims.empty()){
			prims.dequeue(iprim);
			sorted_prims.push_back(iprim);
		}
		std::sort(sorted_prims.begin(), sorted_prims.end());

		int y = local_working_tile / num_tiles_x_;
		int x = local_working_tile - y * num_tiles_x_;
		tile_vp.x = x * TILE_SIZE;
		tile_vp.y = y * TILE_SIZE;

		switch(primtopo_){
		case primitive_line_list:
		case primitive_line_strip:
			for (size_t i = 0; i < sorted_prims.size(); ++ i){
				iprim = sorted_prims[i];
				hrast->rasterize_line(dvc_.fetch(indices[iprim * 2 + 0]), dvc_.fetch(indices[iprim * 2 + 1]), tile_vp, pps);
			}
			break;
		case primitive_triangle_list:
		case primitive_triangle_strip:
			for (size_t i = 0; i < sorted_prims.size(); ++ i){
				iprim = sorted_prims[i];
				hrast->rasterize_triangle(dvc_.fetch(indices[iprim * 3 + 0]), dvc_.fetch(indices[iprim * 3 + 1]), dvc_.fetch(indices[iprim * 3 + 2]), tile_vp, pps);
			}
			break;
		}

		local_working_tile = working_tile ++;
	}
}

void geometry_assembler::generate_indices_impl(std::vector<uint32_t>& indices, atomic<int32_t>& working_prim, int32_t prim_count, uint32_t stride)
{
	int32_t local_working_prim = working_prim ++;

	while (local_working_prim < prim_count){
		ind_fetcher_.fetch_indices(&indices[local_working_prim * stride], local_working_prim);

		local_working_prim = working_prim ++;
	}
}

void geometry_assembler::draw_index_impl(size_t startpos, size_t prim_count, int basevert){

	custom_assert(pparent_, "Renderer ָ��Ϊ�գ����ܸö���û�о�����ȷ�ĳ�ʼ����");
	if(!pparent_) return;

	//������������
	size_t index_count = 0;
	switch(primtopo_)
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

	//��֯��������դ��
	const h_rasterizer& hrast = pparent_->get_rasterizer();
	if(!hrast) return;

	const viewport& vp = pparent_->get_viewport();
	num_tiles_x_ = static_cast<size_t>(vp.w + TILE_SIZE - 1) / TILE_SIZE;
	num_tiles_y_ = static_cast<size_t>(vp.h + TILE_SIZE - 1) / TILE_SIZE;
	std::vector<lockfree_queue<uint32_t> > tiles(num_tiles_x_ * num_tiles_y_);

	size_t prim_size = 0;
	switch(primtopo_)
	{
	case primitive_line_list:
	case primitive_line_strip:
		prim_size = 2;
		break;

	case primitive_triangle_list:
	case primitive_triangle_strip:
		prim_size = 3;
		break;
	}

	std::vector<uint32_t> indices(prim_count * prim_size);

	atomic<int32_t> working_prim(0);
#ifdef SOFTART_MULTITHEADING_ENABLED
	size_t num_threads = num_cpu_cores();

	for (size_t i = 0; i < num_threads; ++ i){
		global_thread_pool().schedule(boost::bind(&geometry_assembler::generate_indices_impl, this, boost::ref(indices), boost::ref(working_prim), prim_count, prim_size));
	}
	global_thread_pool().wait();
#else
	geometry_assembler::generate_indices_impl(boost::ref(indices), boost::ref(working_prim), prim_count, prim_size);
#endif

	//�任����
	dvc_.transform_vertices(indices);

	working_prim = 0;
#ifdef SOFTART_MULTITHEADING_ENABLED
	for (size_t i = 0; i < num_threads; ++ i)
	{
		global_thread_pool().schedule(boost::bind(&geometry_assembler::dispatch_primitive_impl, this, boost::ref(tiles), boost::ref(indices), boost::ref(working_prim), prim_count, prim_size));
	}
	global_thread_pool().wait();
#else
	geometry_assembler::dispatch_primitive_impl(boost::ref(tiles), boost::ref(indices), boost::ref(working_prim), prim_count, prim_size);
#endif

	atomic<int32_t> working_tile(0);
	h_pixel_shader hps = pparent_->get_pixel_shader();
#ifdef SOFTART_MULTITHEADING_ENABLED
	std::vector<h_pixel_shader> ppps(num_threads);
	for (size_t i = 0; i < num_threads; ++ i)
	{
		// create pixel_shader clone per thread from hps
		ppps[i] = hps->create_clone();
		global_thread_pool().schedule(boost::bind(&geometry_assembler::rasterize_primitive_func, this, boost::ref(tiles), boost::ref(indices), boost::ref(working_tile), ppps[i]));
	}
	global_thread_pool().wait();
	// destroy all pixel_shader clone
	for (size_t i = 0; i < num_threads; ++ i)
	{
		hps->destroy_clone(ppps[i]);
	}
#else
	geometry_assembler::rasterize_primitive_func(boost::ref(tiles), boost::ref(indices), boost::ref(working_tile), hps.get());
#endif
}

void geometry_assembler::draw(size_t startpos, size_t prim_count){
	dvc_.reset();
	ind_fetcher_.initialize(h_buffer(), idxtype_, primtopo_, startpos, 0);
	draw_index_impl(startpos, prim_count, 0);
}

void geometry_assembler::draw_index(size_t startpos, size_t prim_count, int basevert){
	dvc_.reset();
	ind_fetcher_.initialize(indexbuf_, idxtype_, primtopo_, startpos, basevert);
	draw_index_impl(startpos, prim_count, basevert);
}

END_NS_SOFTART()
