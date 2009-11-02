#ifndef SOFTART_SHADERREGS_OP_H
#define SOFTART_SHADERREGS_OP_H

#include "shaderregs.h"

#include <boost/static_assert.hpp>

struct viewport;

class vs_input_impl : public vs_input
{
public:
	vs_input_impl(){}
	
	vs_input_impl(vsinput_attributes_t& attrs)
		:vs_input(attrs)
	{}

	//default copy and assignment functions.
};

class vs_output_impl : public vs_output
{
	friend vs_output_impl operator + (const vs_output_impl& vso0, const vs_output_impl& vso1);
	friend vs_output_impl operator - (const vs_output_impl& vso0, const vs_output_impl& vso1);
	friend vs_output_impl operator * (const vs_output_impl& vso0, float f);
	friend vs_output_impl operator * (float f, const vs_output_impl& vso0);
	friend vs_output_impl operator / (const vs_output_impl& vso0, float f);

	friend vs_output_impl project(const vs_output_impl& in);
	friend vs_output_impl& project(vs_output_impl& out, const vs_output_impl& in);
	friend vs_output_impl unproject(const vs_output_impl& in);
	friend vs_output_impl& unproject(vs_output_impl& out, const vs_output_impl& in);

	friend vs_output_impl lerp(const vs_output_impl& start, const vs_output_impl& end, float step);
	friend vs_output_impl& integral(vs_output_impl& inout, float step, const vs_output_impl& derivation);

	friend void update_wpos(vs_output_impl& vso, const viewport& vp);

	friend float compute_area(const vs_output_impl& v0, const vs_output_impl& v1, const vs_output_impl& v2);

public:
	//Ĭ�Ϲ��캯����ִ���κγ�ʼ������
	vs_output_impl(){}
	vs_output_impl(
		const efl::vec4& position, 
		const efl::vec4& wpos,
		const color_array_type& colors,
		const attrib_array_type& attribs)
		:vs_output(position, wpos, colors, attribs)
	{}

	//���������븳ֵ
	vs_output_impl(const vs_output_impl& rhs)
		:vs_output(rhs){}

	vs_output_impl& operator = (const vs_output_impl& rhs){
		if(&rhs == this) return *this;
		position = rhs.position;
		wpos = rhs.wpos;
		colors = rhs.colors;
		attributes = rhs.attributes;
		return *this;
	}
};

BOOST_STATIC_ASSERT(sizeof(vs_output_impl) == sizeof(vs_output));
BOOST_STATIC_ASSERT(sizeof(vs_input_impl) == sizeof(vs_input));

class triangle_info
{
	friend class pixel_shader;

	const efl::vec4* pbase_vert;
	const vs_output* pddx;
	const vs_output* pddy;

	const efl::vec4& base_vert() const;
	const vs_output& ddx() const;
	const vs_output& ddy() const;

public:
	void set(const efl::vec4& base_vert, const vs_output& ddx, const vs_output& ddy);
};

#endif