#include "../include/shaderregs_op.h"
#include "../include/shader.h"
#include "../include/renderer.h"
BEGIN_NS_SOFTART()


using namespace boost;
using namespace eflib;

template <int N>
vs_output& construct_n(vs_output& out,
		const eflib::vec4& position, bool front_face,
		const vs_output::attrib_array_type& attribs,
		const vs_output::attrib_modifier_array_type& modifiers)
{
	out.position = position;
	out.front_face = front_face;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		out.attributes[i_attr] = attribs[i_attr];
		out.attribute_modifiers[i_attr] = modifiers[i_attr];
	}
	return out;
}
template <int N>
vs_output& copy_n(vs_output& out, const vs_output& in)
{
	out.position = in.position;
	out.front_face = in.front_face;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		out.attributes[i_attr] = in.attributes[i_attr];
		out.attribute_modifiers[i_attr] = in.attribute_modifiers[i_attr];
	}
	return out;
}

template <int N>
vs_output& project_n(vs_output& out, const vs_output& in)
{
	if (&out != &in){
		for(size_t i_attr = 0; i_attr < N; ++i_attr){
			out.attributes[i_attr] = in.attributes[i_attr];
			out.attribute_modifiers[i_attr] = in.attribute_modifiers[i_attr];
			if (!(in.attribute_modifiers[i_attr] & vs_output::am_noperspective)){
				out.attributes[i_attr] *= in.position.w;
			}
		}
		out.position = in.position;
		out.front_face = in.front_face;
	}
	else{
		for(size_t i_attr = 0; i_attr < N; ++i_attr){
			if (!(in.attribute_modifiers[i_attr] & vs_output::am_noperspective)){
				out.attributes[i_attr] *= in.position.w;
			}
		}
	}
	return out;
}

template <int N>
vs_output& unproject_n(vs_output& out, const vs_output& in)
{
	const float inv_w = 1.0f / in.position.w;
	if (&out != &in){
		for(size_t i_attr = 0; i_attr < N; ++i_attr){
			out.attributes[i_attr] = in.attributes[i_attr];
			out.attribute_modifiers[i_attr] = in.attribute_modifiers[i_attr];
			if (!(in.attribute_modifiers[i_attr] & vs_output::am_noperspective)){
				out.attributes[i_attr] *= inv_w;
			}
		}
		out.position = in.position;
		out.front_face = in.front_face;
	}
	else{
		for(size_t i_attr = 0; i_attr < N; ++i_attr){
			if (!(in.attribute_modifiers[i_attr] & vs_output::am_noperspective)){
				out.attributes[i_attr] *= inv_w;
			}
		}
	}
	return out;
}

template <int N>
vs_output& lerp_n(vs_output& out, const vs_output& start, const vs_output& end, float step)
{
	out.position = start.position + (end.position - start.position) * step;
	out.front_face = start.front_face;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(start.attribute_modifiers[i_attr] == end.attribute_modifiers[i_attr], "");

		out.attributes[i_attr] = start.attributes[i_attr];
		out.attribute_modifiers[i_attr] = start.attribute_modifiers[i_attr];
		if (!(start.attribute_modifiers[i_attr] & vs_output::am_nointerpolation)){
			out.attributes[i_attr] += (end.attributes[i_attr] - start.attributes[i_attr]) * step;
		}
	}
	return out;
}

template <int N>
vs_output& integral1_n(vs_output& out, const vs_output& in, const vs_output& derivation)
{
	out.position = in.position + derivation.position;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(in.attribute_modifiers[i_attr] == derivation.attribute_modifiers[i_attr], "");
		out.attribute_modifiers[i_attr] = in.attribute_modifiers[i_attr];
		if (!(in.attribute_modifiers[i_attr] & vs_output::am_nointerpolation)){
			out.attributes[i_attr] = in.attributes[i_attr] + derivation.attributes[i_attr];
		}
	}
	return out;
}
template <int N>
vs_output& integral2_n(vs_output& out, const vs_output& in, float step, const vs_output& derivation)
{
	out.position = in.position + (derivation.position * step);
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(in.attribute_modifiers[i_attr] == derivation.attribute_modifiers[i_attr], "");
		out.attribute_modifiers[i_attr] = in.attribute_modifiers[i_attr];
		if (!(in.attribute_modifiers[i_attr] & vs_output::am_nointerpolation)){
			out.attributes[i_attr] = in.attributes[i_attr] + (derivation.attributes[i_attr] * step);
		}
	}
	return out;
}
template <int N>
vs_output& selfintegral1_n(vs_output& inout, const vs_output& derivation)
{
	inout.position += derivation.position;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(inout.attribute_modifiers[i_attr] == derivation.attribute_modifiers[i_attr], "");
		if (!(inout.attribute_modifiers[i_attr] & vs_output::am_nointerpolation)){
			inout.attributes[i_attr] += derivation.attributes[i_attr];
		}
	}
	return inout;
}
template <int N>
vs_output& selfintegral2_n(vs_output& inout, float step, const vs_output& derivation)
{
	inout.position += (derivation.position * step);
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(inout.attribute_modifiers[i_attr] == derivation.attribute_modifiers[i_attr], "");
		if (!(inout.attribute_modifiers[i_attr] & vs_output::am_nointerpolation)){
			inout.attributes[i_attr] += (derivation.attributes[i_attr] * step);
		}
	}
	return inout;
}

template <int N>
vs_output& operator_selfadd_n(vs_output& lhs, const vs_output& rhs)
{
	lhs.position += rhs.position;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(lhs.attribute_modifiers[i_attr] == rhs.attribute_modifiers[i_attr], "");
		lhs.attributes[i_attr] += rhs.attributes[i_attr];
	}
	return lhs;
}
template <int N>
vs_output& operator_selfsub_n(vs_output& lhs, const vs_output& rhs)
{
	lhs.position -= rhs.position;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(lhs.attribute_modifiers[i_attr] == rhs.attribute_modifiers[i_attr], "");
		lhs.attributes[i_attr] -= rhs.attributes[i_attr];
	}
	return lhs;
}
template <int N>
vs_output& operator_selfmul_n(vs_output& lhs, float f)
{
	lhs.position *= f;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		lhs.attributes[i_attr] *= f;
	}
	return lhs;
}
template <int N>
vs_output& operator_selfdiv_n(vs_output& lhs, float f)
{
	EFLIB_ASSERT(!eflib::equal<float>(f, 0.0f), "");
	return operator_selfmul_n<N>(lhs, 1 / f);
}

template <int N>
vs_output& operator_add_n(vs_output& out, const vs_output& vso0, const vs_output& vso1)
{
	out.position = vso0.position + vso1.position;
	out.front_face = vso0.front_face;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(vso0.attribute_modifiers[i_attr] == vso1.attribute_modifiers[i_attr], "");
		out.attributes[i_attr] = vso0.attributes[i_attr] + vso1.attributes[i_attr];
		out.attribute_modifiers[i_attr] = vso0.attribute_modifiers[i_attr];
	}
	return out;
}
template <int N>
vs_output& operator_sub_n(vs_output& out, const vs_output& vso0, const vs_output& vso1)
{
	out.position = vso0.position - vso1.position;
	out.front_face = vso0.front_face;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		EFLIB_ASSERT(vso0.attribute_modifiers[i_attr] == vso1.attribute_modifiers[i_attr], "");
		out.attributes[i_attr] = vso0.attributes[i_attr] - vso1.attributes[i_attr];
		out.attribute_modifiers[i_attr] = vso0.attribute_modifiers[i_attr];
	}
	return out;
}
template <int N>
vs_output& operator_mul_n(vs_output& out, const vs_output& vso0, float f)
{
	out.position = vso0.position * f;
	out.front_face = vso0.front_face;
	for(size_t i_attr = 0; i_attr < N; ++i_attr){
		out.attributes[i_attr] = vso0.attributes[i_attr] * f;
		out.attribute_modifiers[i_attr] = vso0.attribute_modifiers[i_attr];
	}
	return out;
}
template <int N>
vs_output& operator_div_n(vs_output& out, const vs_output& vso0, float f)
{
	return operator_mul_n<N>(out, vso0, 1 / f);
}

template <int N>
vs_output_op gen_vs_output_op_n()
{
	vs_output_op ret;

	ret.construct = construct_n<N>;
	ret.copy = copy_n<N>;
	
	ret.project = project_n<N>;
	ret.unproject = unproject_n<N>;

	ret.operator_selfadd = operator_selfadd_n<N>;
	ret.operator_selfsub = operator_selfsub_n<N>;
	ret.operator_selfmul = operator_selfmul_n<N>;
	ret.operator_selfdiv = operator_selfdiv_n<N>;
	
	ret.operator_add = operator_add_n<N>;
	ret.operator_sub = operator_sub_n<N>;
	ret.operator_mul = operator_mul_n<N>;
	ret.operator_div = operator_div_n<N>;

	ret.lerp = lerp_n<N>;
	
	ret.integral1 = integral1_n<N>;
	ret.integral2 = integral2_n<N>;
	ret.selfintegral1 = selfintegral1_n<N>;
	ret.selfintegral2 = selfintegral2_n<N>;

	return ret;
}

vs_output_op vs_output_ops[vso_attrib_regcnt] = {
	gen_vs_output_op_n<0>(),
	gen_vs_output_op_n<1>(),
	gen_vs_output_op_n<2>(),
	gen_vs_output_op_n<3>(),
	gen_vs_output_op_n<4>(),
	gen_vs_output_op_n<5>(),
	gen_vs_output_op_n<6>(),
	gen_vs_output_op_n<7>(),
	gen_vs_output_op_n<8>(),
	gen_vs_output_op_n<9>(),
	gen_vs_output_op_n<10>(),
	gen_vs_output_op_n<11>(),
	gen_vs_output_op_n<12>(),
	gen_vs_output_op_n<13>(),
	gen_vs_output_op_n<14>(),
	gen_vs_output_op_n<15>(),
	gen_vs_output_op_n<16>(),
	gen_vs_output_op_n<17>(),
	gen_vs_output_op_n<18>(),
	gen_vs_output_op_n<19>(),
	gen_vs_output_op_n<20>(),
	gen_vs_output_op_n<21>(),
	gen_vs_output_op_n<22>(),
	gen_vs_output_op_n<23>(),
	gen_vs_output_op_n<24>(),
	gen_vs_output_op_n<25>(),
	gen_vs_output_op_n<26>(),
	gen_vs_output_op_n<27>(),
	gen_vs_output_op_n<28>(),
	gen_vs_output_op_n<29>(),
	gen_vs_output_op_n<30>(),
	gen_vs_output_op_n<31>()
};

const vs_output_op& get_vs_output_op(uint32_t n)
{
	return vs_output_ops[n];
}

const eflib::vec4& vs_input::operator [](size_t i) const
{
	return attributes_[i];
}

eflib::vec4& vs_input::operator[](size_t i)
{
	if (num_used_attribute_ < i + 1)
	{
		num_used_attribute_ = static_cast<uint32_t>(i + 1);
	}

	return attributes_[i];
}

void viewport_transform(vec4& position, const viewport& vp)
{
	float invw = (eflib::equal<float>(position.w, 0.0f)) ? 1.0f : 1.0f / position.w;
	vec4 pos = position * invw;

	//viewport �任
	float ox = (vp.x + vp.w) * 0.5f;
	float oy = (vp.y + vp.h) * 0.5f;

	position.x = (float(vp.w) * 0.5f) * pos.x + ox;
	position.y = (float(vp.h) * 0.5f) * -pos.y + oy;
	position.z = (vp.maxz - vp.minz) * 0.5f * pos.z + vp.minz;
	position.w = invw;
}

float compute_area(const vs_output& v0, const vs_output& v1, const vs_output& v2)
{
	return cross_prod2( (v1.position - v0.position).xy(), (v2.position - v0.position).xy() );
}

/*****************************************
 *  Vertex Shader
 ****************************************/
void vertex_shader::execute(const vs_input& in, vs_output& out){
	shader_prog(in, out);
}

void blend_shader::execute(size_t sample, backbuffer_pixel_out& out, const ps_output& in){
	shader_prog(sample, out, in);
}

END_NS_SOFTART()
