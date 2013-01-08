#include <eflib/include/platform/disable_warnings.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Module.h>
#include <llvm/Support/CommandLine.h>
#include <eflib/include/platform/enable_warnings.h>

#include <sasl/include/codegen/cg_jit.h>
#include <sasl/include/codegen/module_vmcode_impl.h>

#include <eflib/include/platform/cpuinfo.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <eflib/include/platform/boost_end.h>

#include <vector>

#include <assert.h>

using std::vector;
using std::string;
using namespace eflib;

struct llvm_options
{
	llvm_options(){
		// Add Options
		char* options[] = {""/*, "-force-align-stack"*/};
		llvm::cl::ParseCommandLineOptions( sizeof(options)/sizeof(char*), options );
	}
};

llvm_options& initialize_llvm_options()
{
	static llvm_options opt;
	return opt;
};

BEGIN_NS_SASL_CODEGEN();

/*
boost::shared_ptr<cg_jit_engine> cg_jit_engine::create( boost::shared_ptr<module_vmcode> ctxt, fixed_string& error ){
	boost::shared_ptr<cg_jit_engine> ret = boost::shared_ptr<cg_jit_engine>( new cg_jit_engine( ctxt ) );
	if( !ret ){
		error.assign( "Unknown error occurred." );
	} else if ( !ret->is_valid() ){
		error = ret->error();
		ret.reset();
	}
	return ret;
}

void* cg_jit_engine::get_function( const fixed_string& func_name ){
	assert( global_ctxt );
	assert( engine );

	llvm::Function* func = global_ctxt->llvm_module()->getFunction( func_name.raw_string() );
	if (!func){
		return NULL;
	}

	void* native_fn = engine->getPointerToFunction( func );
	if( find( fns.begin(), fns.end(), func ) == fns.end() ){
		fns.push_back(func);
	}

	return native_fn;
}

cg_jit_engine::cg_jit_engine(boost::shared_ptr<module_vmcode> const& ctxt)
: jit_engine(), global_ctxt( ctxt )
{
	build();
}

void cg_jit_engine::build(){
	if ( !global_ctxt || !global_ctxt->llvm_module() ){
		engine.reset();
	}
	
	initialize_llvm_options();

	// Add Attrs
	vector<string> attrs;
	if( support_feature(cpu_sse2) ){
		attrs.push_back("+sse");
		attrs.push_back("+sse2");
	}
	
	llvm::TargetOptions opts;

	std::string err_str;
	engine.reset(
		llvm::EngineBuilder( global_ctxt->llvm_module() )
		.setTargetOptions(opts)
		.setMAttrs(attrs)
		.setErrorStr(&err_str)
		.create()
		);
	err = err_str;

	if ( engine ){
		if( !global_ctxt->take_ownership() ){
			engine.reset();
		}
	}
}

bool cg_jit_engine::is_valid(){
	return (bool)engine;
}

fixed_string cg_jit_engine::error(){
	return err;
}

cg_jit_engine::~cg_jit_engine()
{
	BOOST_FOREACH( llvm::Function* fn, fns ){
		engine->freeMachineCodeForFunction( fn );
	}
}

void cg_jit_engine::inject_function( void* fn, eflib::fixed_string const& name )
{
	llvm::Function* func = global_ctxt->llvm_module()->getFunction( name.raw_string() );
	if ( func ){
		engine->addGlobalMapping( func, fn );
	}
}
*/

END_NS_SASL_CODEGEN();
