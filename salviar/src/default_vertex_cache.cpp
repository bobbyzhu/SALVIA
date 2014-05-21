#include <eflib/include/platform/config.h>

#include <salviar/include/vertex_cache.h>
#include <salviar/include/stream_assembler.h>
#include <salviar/include/host.h>
#include <salviar/include/shader.h>
#include <salviar/include/shader_regs.h>
#include <salviar/include/shader_regs_op.h>
#include <salviar/include/sync_renderer.h>
#include <salviar/include/render_state.h>
#include <salviar/include/stream_assembler.h>
#include <salviar/include/thread_context.h>
#include <salviar/include/async_object.h>
#include <salviar/include/shader_unit.h>

#include <eflib/include/platform/cpuinfo.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/ref.hpp>
#include <eflib/include/platform/boost_end.h>

#include <atomic>

#if defined(EFLIB_MSVC)
#	include <ppl.h>
#endif

using eflib::num_available_threads;

using boost::atomic;
using boost::shared_array;

using std::vector;

BEGIN_NS_SALVIAR();

const int GENERATE_INDICES_PACKAGE_SIZE = 8;
const int TRANSFORM_VERTEX_PACKAGE_SIZE = 8;

size_t const invalid_id = 0xffffffff;

#define USE_INDEX_RANGE 0

class default_vertex_cache : public vertex_cache
{
public:
	default_vertex_cache()
		: assembler_(nullptr)
		, verts_pool_( sizeof(vs_output) )
		, transformed_verts_capacity_ (0)
	{
	}

	void initialize(render_stages const* stages)
	{
		assembler_	= stages->assembler.get();
		host_		= stages->host.get();
	}

	void update(render_state const* state)
	{
		// transformed_verts_.reset();
		index_fetcher_.update(state);
		topology_	= state->prim_topo;
		viewport_	= &(state->vp);
		cpp_vs_		= state->cpp_vs.get();
		vs_proto_	= state->vs_proto;
        prim_count_ = state->prim_count;

        pipeline_stat_ = state->asyncs[static_cast<uint32_t>(async_object_ids::pipeline_statistics)].get();
		pipeline_prof_ = state->asyncs[static_cast<uint32_t>(async_object_ids::pipeline_profiles)].get();

        if(pipeline_stat_)
        {
            acc_ia_vertices_ = &async_pipeline_statistics::accumulate<pipeline_statistic_id::ia_vertices>;
            acc_vs_invocations_ = &async_pipeline_statistics::accumulate<pipeline_statistic_id::vs_invocations>;
        }
        else
        {
            acc_ia_vertices_ = &accumulate_fn<uint64_t>::null;
            acc_vs_invocations_ = &accumulate_fn<uint64_t>::null;
        }

		if(pipeline_prof_)
		{
			fetch_time_stamp_	= &async_pipeline_profiles::time_stamp;
			acc_gather_vtx_		= &async_pipeline_profiles::accumulate<pipeline_profile_id::gather_vtx>; 
			acc_vtx_proc_		= &async_pipeline_profiles::accumulate<pipeline_profile_id::vtx_proc>;
		}
		else
		{
			fetch_time_stamp_	= &time_stamp_fn::null;
			acc_gather_vtx_		= &accumulate_fn<uint64_t>::null;
			acc_vtx_proc_		= &accumulate_fn<uint64_t>::null;
		}
	}

	void prepare_vertices()
	{
		uint64_t gather_vtx_start_time = fetch_time_stamp_();

		uint32_t prim_size = 0;
		switch(topology_)
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

		indices_.resize(prim_count_ * prim_size);

		atomic<int32_t> working_package(0);
		size_t num_threads = num_available_threads( );

		// Generate indices
		std::atomic<uint32_t> min_index( std::numeric_limits<uint32_t>::max() );
		std::atomic<uint32_t> max_index(0);

		auto generate_indicies_ = [this, &min_index, &max_index, prim_size](thread_context* ctx)
		{
			this->generate_indices(this->indices_, min_index, max_index, prim_size, ctx);
		};
		execute_threads(generate_indicies_, prim_count_, GENERATE_INDICES_PACKAGE_SIZE);

		min_index_ = min_index;

#if !USE_INDEX_RANGE
		// Unique indices
		unique_indices_ = indices_;
#if defined(EFLIB_MSVC)
		concurrency::parallel_radixsort(unique_indices_.begin(), unique_indices_.end());
#else
		std::sort(unique_indices_.begin(), unique_indices_.end());
#endif
		unique_indices_.erase(std::unique(unique_indices_.begin(), unique_indices_.end()), unique_indices_.end());
		if( transformed_verts_capacity_ < unique_indices_.size() )
		{
			transformed_verts_.reset(new vs_output[unique_indices_.size()]);
			transformed_verts_capacity_ = unique_indices_.size();
		}
		used_verts_.resize( unique_indices_.back()+1 );

        // Accumulate query counters.
        acc_ia_vertices_( pipeline_stat_, static_cast<uint64_t>(prim_count_*prim_size) );
        acc_vs_invocations_( pipeline_stat_, static_cast<uint64_t>(unique_indices_.size()) );
		acc_gather_vtx_(pipeline_prof_, fetch_time_stamp_() - gather_vtx_start_time);

		// Transform vertexes
		if( cpp_vs_ )
		{
			assembler_->update_register_map( cpp_vs_->get_register_map() );
		}

		working_package = 0;
		boost::function<void()> task_transform_vertex;
		if(cpp_vs_)
		{
			task_transform_vertex = boost::bind(
				&default_vertex_cache::transform_vertex_cppvs,
				this, boost::ref(unique_indices_),
				static_cast<int32_t>(unique_indices_.size()), boost::ref(working_package), TRANSFORM_VERTEX_PACKAGE_SIZE
				);
		}
		else if (host_ != nullptr)
		{
			task_transform_vertex = boost::bind(
				&default_vertex_cache::transform_vertex_vs2,
				this, boost::ref(unique_indices_),
				static_cast<int32_t>(unique_indices_.size()), boost::ref(working_package), TRANSFORM_VERTEX_PACKAGE_SIZE
				);
		}
		else
		{
			task_transform_vertex = boost::bind(
				&default_vertex_cache::transform_vertex_vs,
				this, boost::ref(unique_indices_),
				static_cast<int32_t>(unique_indices_.size()), boost::ref(working_package), TRANSFORM_VERTEX_PACKAGE_SIZE
				);
		}

		volatile uint64_t vtx_proc_start_time = fetch_time_stamp_();
		for (size_t i = 0; i < num_threads - 1; ++ i)
		{
			global_thread_pool().schedule(task_transform_vertex);
		}
		task_transform_vertex();
		global_thread_pool().wait();
		acc_vtx_proc_(pipeline_prof_, fetch_time_stamp_() - vtx_proc_start_time);
#else
		uint32_t verts_count = max_index - min_index_ + 1;
		if(transformed_verts_capacity_ < verts_count)
		{
			transformed_verts_.reset(new vs_output[verts_count]);
			transformed_verts_capacity_ = verts_count;
		}

        // Accumulate query counters.
        acc_ia_vertices_( pipeline_stat_, static_cast<uint64_t>(prim_count_*prim_size) );
        acc_vs_invocations_( pipeline_stat_, static_cast<uint64_t>(verts_count) );
		acc_gather_vtx_(pipeline_prof_, fetch_time_stamp_() - gather_vtx_start_time);

		// Transform vertexes
		if( cpp_vs_ )
		{
			assembler_->update_register_map( cpp_vs_->get_register_map() );
		}

		uint64_t vtx_proc_start_time = fetch_time_stamp_();

		if(cpp_vs_)
		{
			auto execute_vert_shader = [this](thread_context* thread_ctx)
			{
				this->transform_vertex_cppvs(thread_ctx);
			};
			execute_threads(execute_vert_shader, verts_count, TRANSFORM_VERTEX_PACKAGE_SIZE);
		}
		else if (host_ != nullptr)
		{
			auto execute_vert_shader = [this](thread_context* thread_ctx)
			{
				this->transform_vertex_vs2(thread_ctx);
			};
			execute_threads(execute_vert_shader, verts_count, TRANSFORM_VERTEX_PACKAGE_SIZE);
		}
		else
		{
			auto execute_vert_shader = [this](thread_context* thread_ctx)
			{
				this->transform_vertex_vs(thread_ctx);
			};
			execute_threads(execute_vert_shader, verts_count, TRANSFORM_VERTEX_PACKAGE_SIZE);
		}

		acc_vtx_proc_(pipeline_prof_, fetch_time_stamp_() - vtx_proc_start_time);
#endif
	}

	void fetch3(vs_output** v, cache_entry_index id)
	{
		static vs_output null_obj;
		uint32_t id0 = indices_[id];
		uint32_t id1 = indices_[id+1];
		uint32_t id2 = indices_[id+2];
#if !USE_INDEX_RANGE
		
#if defined(EFLIB_DEBUG)
		if((id > used_verts_.size()) || (-1 == used_verts_[id]))
		{
			assert( !"The vertex could not be transformed. Maybe errors occurred on index statistics or vertex tranformation." );
			// return null_obj;
		}
#endif

		v[0] = &transformed_verts_[used_verts_[id0]];
		v[1] = &transformed_verts_[used_verts_[id1]];
		v[2] = &transformed_verts_[used_verts_[id2]];
#else
		return transformed_verts_[id - min_index_];
#endif
	}

private:
	void generate_indices(
		vector<uint32_t>& indices, std::atomic<uint32_t>& min_index, std::atomic<uint32_t>& max_index,
		uint32_t stride, thread_context const* thread_ctx)
	{
		// Fetch indexes and min/max of package
		uint32_t thread_min_index = std::numeric_limits<uint32_t>::max();
		uint32_t thread_max_index = 0;

		thread_context::package_cursor current_package = thread_ctx->next_package();
		while ( current_package.valid() )
		{
			auto prim_range = current_package.item_range();
			uint32_t pkg_min_index, pkg_max_index;
			index_fetcher_.fetch_indexes(&indices[prim_range.first*stride], &pkg_min_index, &pkg_max_index, prim_range.first, prim_range.second);
			thread_min_index = std::min(pkg_min_index, thread_min_index);
			thread_max_index = std::max(pkg_max_index, thread_max_index);
			current_package = thread_ctx->next_package();
		}

		// While thread is ended, merge min/max index to global.
		uint32_t old_min_index = 0;
		uint32_t new_min_index = 0;
		do
		{
			old_min_index = min_index;	
			new_min_index = std::min(old_min_index, thread_min_index);
		}
		while( !min_index.compare_exchange_weak(old_min_index, new_min_index) );

		uint32_t old_max_index = 0;
		uint32_t new_max_index = 0;
		do
		{
			old_max_index = max_index;
			new_max_index = std::max(old_max_index, thread_max_index);
		}
		while( !max_index.compare_exchange_weak(old_max_index, new_max_index) );
	}

#if !USE_INDEX_RANGE
	void transform_vertex_cppvs(
		vector<uint32_t> const& indices, int32_t index_count,
		atomic<int32_t>& working_package, int32_t package_size)
	{
		const int32_t num_packages = (index_count + package_size - 1) / package_size;

		int32_t local_working_package = working_package ++;
		while (local_working_package < num_packages)
		{
			const int32_t start = local_working_package * package_size;
			const int32_t end = std::min(index_count, start + package_size);
			for (int32_t i = start; i < end; ++ i){
				uint32_t id = indices[i];
				used_verts_[id] = i;

				vs_input vertex;
				assembler_->fetch_vertex(vertex, id);
				cpp_vs_->execute(vertex, transformed_verts_[i]);
			}

			local_working_package = working_package ++;
		}
	}

	void transform_vertex_vs(
		vector<uint32_t> const& indices, int32_t index_count,
		atomic<int32_t>& working_package, int32_t package_size )
	{
		vertex_shader_unit vsu	= *vs_proto_;

		vsu.bind_streams(assembler_);

		const int32_t num_packages = (index_count + package_size - 1) / package_size;

		int32_t local_working_package = working_package ++;
		while (local_working_package < num_packages)
		{
			const int32_t start = local_working_package * package_size;
			const int32_t end = std::min(index_count, start+package_size);
			for (int32_t i = start; i < end; ++ i)
			{
				uint32_t id = indices[i];
				used_verts_[id] = i;

				vsu.update( id );
				vsu.execute( transformed_verts_[i] );
			}

			local_working_package = working_package ++;
		}
	}

	void transform_vertex_vs2(
		vector<uint32_t> const& indices, int32_t index_count,
		atomic<int32_t>& working_package, int32_t package_size )
	{
		vx_shader_unit_ptr vsu	= host_->get_vx_shader_unit();

		const int32_t num_packages = (index_count + package_size - 1) / package_size;

		int32_t local_working_package = working_package ++;
		while (local_working_package < num_packages)
		{
			const int32_t start = local_working_package * package_size;
			const int32_t end = std::min(index_count, start+package_size);
			for (int32_t i = start; i < end; ++ i)
			{
				uint32_t vert_index = indices[i];
				used_verts_[vert_index] = i;

				vsu->execute(vert_index, transformed_verts_[i]);
			}

			local_working_package = working_package ++;
		}
	}
#else
	void transform_vertex_cppvs(thread_context* thread_ctx)
	{
		thread_context::package_cursor current_package = thread_ctx->next_package();
		while ( current_package.valid() )
		{
			auto vert_range = current_package.item_range();
			for(auto i = vert_range.first; i < vert_range.second; ++i)
			{
				vs_input vertex;
				assembler_->fetch_vertex(vertex, i + min_index_);
				cpp_vs_->execute(vertex, transformed_verts_[i]);
			}
			current_package = thread_ctx->next_package();
		}
	}

	void transform_vertex_vs(thread_context* thread_ctx)
	{
		vertex_shader_unit vsu = *vs_proto_;
		vsu.bind_streams(assembler_);

		thread_context::package_cursor current_package = thread_ctx->next_package();
		while ( current_package.valid() )
		{
			auto vert_range = current_package.item_range();
			for(auto i = vert_range.first; i < vert_range.second; ++i)
			{
				vsu.update(i + min_index_);
				vsu.execute( transformed_verts_[i] );
			}
			current_package = thread_ctx->next_package();
		}
	}

	void transform_vertex_vs2(thread_context* thread_ctx)
	{
		vx_shader_unit_ptr vsu = host_->get_vx_shader_unit();
		thread_context::package_cursor current_package = thread_ctx->next_package();
		while ( current_package.valid() )
		{
			auto vert_range = current_package.item_range();
			for(auto i = vert_range.first; i < vert_range.second; ++i)
			{
				vsu->execute(i + min_index_, transformed_verts_[i]);
			}
			current_package = thread_ctx->next_package();
		}
	}
#endif

private:
	stream_assembler*		assembler_;
	host*					host_;

    uint32_t                prim_count_;
	cpp_vertex_shader*		cpp_vs_;
	vertex_shader_unit_ptr	vs_proto_;
	viewport const*			viewport_;

	vector<uint32_t>		indices_;
	vector<uint32_t>		unique_indices_;
	primitive_topology		topology_;
	index_fetcher			index_fetcher_;

	shared_array<vs_output> transformed_verts_;
	size_t					transformed_verts_capacity_;

	vector<int32_t>			used_verts_;
	uint32_t				min_index_;

    async_object*           pipeline_stat_;
	async_object*			pipeline_prof_;

	time_stamp_fn::type		fetch_time_stamp_;

    accumulate_fn<uint64_t>::type
                            acc_ia_vertices_;
    accumulate_fn<uint64_t>::type
                            acc_vs_invocations_;
	accumulate_fn<uint64_t>::type
							acc_gather_vtx_;
	accumulate_fn<uint64_t>::type
							acc_vtx_proc_;

	boost::pool<>			verts_pool_;
};

vertex_cache_ptr create_default_vertex_cache()
{
	return vertex_cache_ptr( new default_vertex_cache() );
}

END_NS_SALVIAR();
