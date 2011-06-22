#include <salviar/include/shader_unit.h>

#include <salviar/include/shader_code.h>
#include <salviar/include/shaderregs.h>
#include <salviar/include/renderer.h>
#include <salviar/include/buffer.h>

#include <sasl/include/semantic/abi_info.h>

#include <eflib/include/diagnostics/assert.h>
#include <eflib/include/math/math.h>
#include <eflib/include/platform/boost_begin.h>
#include <boost/foreach.hpp>
#include <eflib/include/platform/boost_end.h>

using namespace sasl::semantic;
using namespace eflib;
using std::vector;

BEGIN_NS_SALVIAR();

void vertex_shader_unit::initialize( shader_code const* code ){
	this->code = code;
	this->stream_data.resize( code->abii()->storage_size( sc_stream_in), 0 );
	this->buffer_data.resize( code->abii()->storage_size(sc_buffer_in), 0 );
	this->stream_odata.resize( code->abii()->storage_size(sc_stream_out), 0 );
	this->buffer_odata.resize( code->abii()->storage_size(sc_buffer_out), 0 );
}

void vertex_shader_unit::bind_streams( vector<input_element_decl> const& layout, vector<h_buffer> const& streams ){
	this->layout = layout;
	this->streams = streams;
}

void vertex_shader_unit::update( size_t ivert )
{
	abi_info const* abii = code->abii();
	BOOST_FOREACH( input_element_decl const& elem_decl, layout ){
		if( elem_decl.usage == input_register_usage_position ){
			void* psrc = streams[elem_decl.stream_idx]->raw_data( elem_decl.stride*ivert + elem_decl.offset );
			storage_info* pos_si = abii->input_storage( SV_Position );
			void* pdest = &(stream_data[pos_si->offset]);
			*static_cast<intptr_t*>(pdest) = reinterpret_cast<intptr_t>(psrc);
		} else {
			// TODO Ignore others yet.
		}
	}
}

void vertex_shader_unit::execute( vs_output& out )
{
	void (*p)(void*, void*, void*, void*)
		= static_cast<void (*)(void*, void*, void*, void*)>( code->function_pointer() );

	void* psi = stream_data.empty() ? NULL : &(stream_data[0]);
	void* pbi = buffer_data.empty() ? NULL : &(buffer_data[0]);
	void* pso = stream_odata.empty() ? NULL : &(stream_odata[0]);
	void* pbo = buffer_odata.empty() ? NULL : &(buffer_odata[0]);

	p( psi, pbi, pso, pbo );

#if 0
	/**(mat44*)pbi = mat44::identity();
	(*(mat44*)pbi).f[0][0] = 2.0f;
	(*(mat44*)pbi).f[0][1] = 0.7f;*/

	vec4 src_vec4(*((float**)psi), 4);
	vec4 ref_result;
	transform( ref_result, src_vec4, *(mat44*)pbi );
#endif

	// Copy output position to vs_output.
	memset( &out.position, 0, sizeof(out.position) );
	storage_info* out_info = code->abii()->output_storage(SV_Position);
	memcpy( &out.position, &(buffer_odata[out_info->offset]), out_info->size );
}

void vertex_shader_unit::set_variable( std::string const& name, void* data )
{
	storage_info* vsi = code->abii()->input_storage( name );
	memcpy( &buffer_data[vsi->offset], data, vsi->size );
}

vertex_shader_unit::vertex_shader_unit()
: code(NULL)
{
}

vertex_shader_unit::vertex_shader_unit( vertex_shader_unit const& rhs )
: code(rhs.code),
layout(rhs.layout), streams(rhs.streams),
stream_data(rhs.stream_data), buffer_data(rhs.buffer_data),
stream_odata(rhs.stream_odata), buffer_odata(rhs.buffer_odata)
{
}

vertex_shader_unit& vertex_shader_unit::operator=( vertex_shader_unit const& rhs ){
	code = rhs.code;
	layout = rhs.layout;
	streams = rhs.streams;
	stream_data = rhs.stream_data;
	buffer_data = rhs.buffer_data;
	stream_odata = rhs.stream_odata;
	buffer_odata = rhs.buffer_odata;
	return *this;
}

vertex_shader_unit::~vertex_shader_unit()
{
}


END_NS_SALVIAR();