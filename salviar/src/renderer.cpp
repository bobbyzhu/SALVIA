#include <salviar/include/renderer.h>

#include <salviar/include/binary_modules.h>
#include <salviar/include/sync_renderer.h>
#include <salviar/include/sampler_api.h>
#include <salviar/include/shader_impl.h>
#include <salviar/include/shader_object.h>
#include <salviar/include/async_renderer.h>

#include <eflib/include/platform/dl_loader.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/shared_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

#include <vector>
#include <string>
#include <iostream>

using eflib::dynamic_lib;
using boost::shared_ptr;
using std::vector;
using std::string;
using std::cout;
using std::endl;

BEGIN_NS_SALVIAR();

#define USE_ASYNC_RENDERER
renderer_ptr create_software_renderer(renderer_parameters const* pparam, device_ptr const& hdev)
{
#if defined(USE_ASYNC_RENDERER)
	return create_async_renderer(pparam, hdev);
#else
	return create_sync_renderer(pparam, hdev);
#endif
}

renderer_ptr create_benchmark_renderer(renderer_parameters const* pparam, device_ptr const& hdev)
{
	return create_sync_renderer(pparam, hdev);
}

shader_object_ptr	compile(std::string const& code, shader_profile const& profile, shader_log_ptr& logs)
{
	vector<external_function_desc> external_funcs;
	external_funcs.push_back( external_function_desc(&tex2Dlod,		"sasl.vs.tex2d.lod",	true) );
	external_funcs.push_back( external_function_desc(&texCUBElod,	"sasl.vs.texCUBE.lod",	true) );
	external_funcs.push_back( external_function_desc(&tex2Dlod_ps,	"sasl.ps.tex2d.lod" ,	true) );
	external_funcs.push_back( external_function_desc(&tex2Dgrad_ps,	"sasl.ps.tex2d.grad",	true) );
	external_funcs.push_back( external_function_desc(&tex2Dbias_ps,	"sasl.ps.tex2d.bias",	true) );
	external_funcs.push_back( external_function_desc(&tex2Dproj_ps,	"sasl.ps.tex2d.proj",	true) );

	shader_object_ptr ret;
	modules::host::compile(ret, logs, code, profile, external_funcs);

	return ret;
}

shader_object_ptr	compile(std::string const& code, shader_profile const& profile)
{
	shader_log_ptr log;
	shader_object_ptr ret = compile(code, profile, log);

	if(!ret)
	{
		cout << "Shader was compiled failed!" << endl;
		for( size_t i = 0; i < log->count(); ++i )
		{
			cout << log->log_string(i) << endl;
		}
	}

	return ret;
}

shader_object_ptr	compile(std::string const& code, languages lang)
{
	shader_profile prof;
	prof.language = lang;
	return compile(code, prof);
}

END_NS_SALVIAR();