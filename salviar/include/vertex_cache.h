#ifndef SALVIAR_VERTEX_CACHE_H
#define SALVIAR_VERTEX_CACHE_H

#include "decl.h"

#include "renderer.h"
#include "render_stage.h"
#include "index_fetcher.h"
#include <eflib/include/memory/atomic.h>

#include <boost/pool/pool.hpp>

#include <vector>
#include <utility>

#include <salviar/include/salviar_forward.h>

BEGIN_NS_SALVIAR();

class stream_assembler;

//������ֱ��������input vertexʱ����ֻ��low��������ã��������Ϊ���к�Ķ��㣬��hi idҲ��������
typedef size_t cache_entry_index;

class vertex_cache : public render_stage
{
public:
	virtual void reset(const h_buffer& hbuf, index_type idxtype, primitive_topology primtopo, uint32_t startpos, uint32_t basevert) = 0;

	virtual result set_input_layout(const input_layout_decl& layout) = 0;
	virtual result set_stream(stream_index sidx, h_buffer hbuf) = 0;

	virtual void transform_vertices(uint32_t prim_count) = 0;

	virtual vs_output& fetch(cache_entry_index id) = 0;
	virtual vs_output& fetch_for_write(cache_entry_index id) = 0;

	virtual vs_output* new_vertex() = 0;
	virtual void delete_vertex(vs_output* const pvert) = 0;
};

class default_vertex_cache : public vertex_cache
{
public:
	default_vertex_cache();

	void initialize(renderer_impl* psr);

	void reset(const h_buffer& hbuf, index_type idxtype, primitive_topology primtopo, uint32_t startpos, uint32_t basevert);

	result set_input_layout(const input_layout_decl& layout);
	result set_stream(stream_index sidx, h_buffer hbuf);

	void transform_vertices(uint32_t prim_count);

	vs_output& fetch(cache_entry_index id);
	vs_output& fetch_for_write(cache_entry_index id);

	vs_output* new_vertex();
	void delete_vertex(vs_output* const pvert);

private:
	void generate_indices_func(std::vector<uint32_t>& indices, int32_t prim_count, uint32_t stride, eflib::atomic<int32_t>& working_package, int32_t package_size);
	void transform_vertex_func(const std::vector<uint32_t>& indices, int32_t index_count, eflib::atomic<int32_t>& working_package, int32_t package_size);
	void transform_vertex_by_shader( const std::vector<uint32_t>& indices, int32_t index_count, eflib::atomic<int32_t>& working_package, int32_t package_size );
private:
	vertex_shader* pvs_;
	h_stream_assembler hsa_;

	primitive_topology primtopo_;
	index_fetcher ind_fetcher_;
	std::vector<uint32_t> indices_;

	boost::shared_array<vs_output> verts_;
	std::vector<int32_t> used_verts_;

	boost::pool<> verts_pool_;
	const viewport* pvp_;
};


END_NS_SALVIAR()

#endif