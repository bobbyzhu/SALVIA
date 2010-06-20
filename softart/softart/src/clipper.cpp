#include "../include/clipper.h"

#include "../include/shaderregs_op.h"
#include "../include/shader.h"

#include <algorithm>
BEGIN_NS_SOFTART()


using namespace efl;
using namespace std;

clipper::clipper(){
	planes_[0] = vec4(1.0f, 0.0f, 0.0f, 1.0f);
	planes_[1] = vec4(0.0f, 1.0f, 0.0f, 1.0f);
	planes_[2] = vec4(0.0f, 0.0f, 1.0f, 0.0f);
	planes_[3] = vec4(-1.0f, 0.0f, 0.0f, 1.0f);
	planes_[4] = vec4(0.0f, -1.0f, 0.0f, 1.0f);
	planes_[5] = vec4(0.0f, 0.0f, -1.0f, 1.0f);

	std::fill(planes_enable_.begin(), planes_enable_.end(), true);
}

void clipper::set_clip_plane_enable(bool enable, size_t idx)
{
	if(idx >= plane_num){
		custom_assert(false, "");
	}

	planes_enable_[idx] = enable;
}

void clipper::clip(vs_output* out_clipped_verts, uint32_t& num_out_clipped_verts, const viewport& vp, const vs_output& v0, const vs_output& v1) const
{
	efl::pool::stack_pool< vs_output, 12 > pool;
	const vs_output* clipped_verts[2][2];
	uint32_t num_clipped_verts[2];

	//��ʼclip, Ping-Pong idioms
	clipped_verts[0][0] = &v0;
	clipped_verts[0][1] = &v1;
	num_clipped_verts[0] = 2;

	float di, dj;
	size_t src_stage = 0;
	size_t dest_stage = 1;

	for(size_t i_plane = 0; i_plane < planes_.size(); ++i_plane)
	{
		if( ! planes_enable_[i_plane] ){
			continue;
		}

		num_clipped_verts[dest_stage] = 0;

		if (num_clipped_verts[src_stage] != 0)
		{
			di = dot_prod4(planes_[i_plane], clipped_verts[src_stage][0]->position);
			dj = dot_prod4(planes_[i_plane], clipped_verts[src_stage][1]->position);

			if(di >= 0.0f){
				num_clipped_verts[dest_stage] = 2;

				clipped_verts[dest_stage][0] = clipped_verts[src_stage][0];

				if(dj < 0.0f){
					vs_output* pclipped = (vs_output*)(pool.malloc());

					//LERP
					*pclipped = *clipped_verts[src_stage][0] + (*clipped_verts[src_stage][1] - *clipped_verts[src_stage][0]) * (di / (di - dj));
					clipped_verts[dest_stage][1] = pclipped;
				}
				else {
					clipped_verts[dest_stage][1] = clipped_verts[src_stage][1];
				}
			}
			else {
				if(dj >= 0.0f){
					num_clipped_verts[dest_stage] = 2;

					vs_output* pclipped = (vs_output*)(pool.malloc());

					//LERP
					*pclipped = *clipped_verts[src_stage][1] + (*clipped_verts[src_stage][0] - *clipped_verts[src_stage][1]) * (dj / (dj - di));

					clipped_verts[dest_stage][0] = pclipped;

					clipped_verts[dest_stage][1] = clipped_verts[src_stage][1];
				}
			}
		}

		//swap src and dest pool
		++src_stage;
		++dest_stage;
		src_stage &= 1;
		dest_stage &= 1;
	}

	const vs_output** clipped_verts_ptrs = clipped_verts[src_stage];
	num_out_clipped_verts = num_clipped_verts[src_stage];
	for(size_t i = 0; i < num_out_clipped_verts; ++i){
		out_clipped_verts[i] = *clipped_verts_ptrs[i];
		viewport_transform(out_clipped_verts[i].position, vp);
	}
}

void clipper::clip(vs_output* out_clipped_verts, uint32_t& num_out_clipped_verts, const viewport& vp, const vs_output& v0, const vs_output& v1, const vs_output& v2) const
{
	efl::pool::stack_pool< vs_output, 12 > pool;
	const vs_output* clipped_verts[2][6];
	uint32_t num_clipped_verts[2];

	//��ʼclip, Ping-Pong idioms
	clipped_verts[0][0] = &v0;
	clipped_verts[0][1] = &v1;
	clipped_verts[0][2] = &v2;
	num_clipped_verts[0] = 3;

	float d[2];
	size_t src_stage = 0;
	size_t dest_stage = 1;

	for(size_t i_plane = 0; i_plane < planes_.size(); ++i_plane)
	{
		if( ! planes_enable_[i_plane] ){
			continue;
		}

		num_clipped_verts[dest_stage] = 0;

		if (num_clipped_verts[src_stage] != 0){
			d[0] = dot_prod4(planes_[i_plane], clipped_verts[src_stage][0]->position);
		}
		for(size_t i = 0, j = 1; i < num_clipped_verts[src_stage]; ++i, ++j)
		{
			//wrap
			j %= num_clipped_verts[src_stage];

			d[1] = dot_prod4(planes_[i_plane], clipped_verts[src_stage][j]->position);

			if(d[0] >= 0.0f){
				clipped_verts[dest_stage][num_clipped_verts[dest_stage]] = clipped_verts[src_stage][i];
				++ num_clipped_verts[dest_stage];

				if(d[1] < 0.0f){
					vs_output* pclipped = (vs_output*)(pool.malloc());

					//LERP
					*pclipped = *clipped_verts[src_stage][i] + (*clipped_verts[src_stage][j] - *clipped_verts[src_stage][i]) * (d[0] / (d[0] - d[1]));

					clipped_verts[dest_stage][num_clipped_verts[dest_stage]] = pclipped;
					++ num_clipped_verts[dest_stage];
				}
			}
			else {
				if(d[1] >= 0.0f){
					vs_output* pclipped = (vs_output*)(pool.malloc());

					//LERP
					*pclipped = *clipped_verts[src_stage][j] + (*clipped_verts[src_stage][i] - *clipped_verts[src_stage][j]) * (d[1] / (d[1] - d[0]));

					clipped_verts[dest_stage][num_clipped_verts[dest_stage]] = pclipped;
					++ num_clipped_verts[dest_stage];
				}
			}

			d[0] = d[1];
		}

		//swap src and dest pool
		++src_stage;
		++dest_stage;
		src_stage &= 1;
		dest_stage &= 1;
	}

	const vs_output** clipped_verts_ptrs = clipped_verts[src_stage];
	num_out_clipped_verts = num_clipped_verts[src_stage];
	for(size_t i = 0; i < num_out_clipped_verts; ++i){
		out_clipped_verts[i] = *clipped_verts_ptrs[i];
		viewport_transform(out_clipped_verts[i].position, vp);
	}
}
END_NS_SOFTART()
