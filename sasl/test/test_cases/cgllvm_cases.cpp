#include <sasl/test/test_cases/cgllvm_cases.h>
#include <sasl/test/test_cases/syntax_cases.h>
#include <sasl/include/code_generator/llvm/cgllvm_api.h>
#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/include/code_generator/llvm/cgllvm_globalctxt.h>
#include <sasl/include/semantic/abi_analyser.h>
#include <sasl/include/semantic/semantic_api.h>
#include <sasl/include/syntax_tree/utility.h>

#include <salviar/include/enums.h>

#include <eflib/include/diagnostics/assert.h>
#include <eflib/include/diagnostics/logrout.h>
#include <eflib/include/memory/lifetime_manager.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/any.hpp>
#include <eflib/include/platform/boost_end.h>

using boost::shared_ptr;

#define SYNCASE_(case_name) syntax_cases::instance().case_name ()
#define SYNCASENAME_( case_name ) syntax_cases::instance(). case_name##_name()

#define SEMCASE_(case_name) semantic_cases::instance().case_name ()
#define SEMCASENAME_( case_name ) semantic_cases::instance(). case_name##_name()

boost::mutex cgllvm_cases::mtx;
boost::shared_ptr<cgllvm_cases> cgllvm_cases::tcase;

cgllvm_cases& cgllvm_cases::instance(){
	boost::mutex::scoped_lock lg(mtx);
	if ( !tcase ) {
		eflib::lifetime_manager::at_main_exit( cgllvm_cases::release );
		tcase.reset( new cgllvm_cases() );
		tcase->initialize();
	}
	return *tcase;
}

bool cgllvm_cases::is_avaliable()
{
	boost::mutex::scoped_lock lg(mtx);
	return tcase;
}

void cgllvm_cases::release(){
	boost::mutex::scoped_lock lg(mtx);
	if ( tcase ){
		tcase->LOCVAR_(jit).reset();
		tcase.reset();
	}
}

cgllvm_cases::cgllvm_cases(){
}

void cgllvm_cases::initialize(){

	shared_ptr< SEMANTIC_(module_semantic) > si_jit_root = SEMANTIC_(analysis_semantic)( SYNCASE_(prog_for_jit_test), NULL, 1 );
	SEMANTIC_(abi_analyser) aa;
	aa.auto_entry( si_jit_root, salviar::lang_general );

	LOCVAR_(root) = CODEGEN_(generate_llvm_code)( si_jit_root.get(), aa.abii(salviar::lang_general) );

	fputs("\n======================================================\r\n", stderr);
	fputs("Verify generated code: \r\n", stderr);

	std::vector<CODEGEN_(optimization_options)> ops;
	ops.push_back( CODEGEN_(opt_verify) );
	CODEGEN_(optimize) ( root(), ops );

	eflib::logrout::write_state( eflib::logrout::screen(), eflib::logrout::off() );

	fputs("\n======================================================\n", stderr);
	fputs("Generated LLVM IR (before optimized): \r\n", stderr);
	root()->dump();
	fputs("======================================================\n", stderr);

	ops.clear();
	ops.push_back( CODEGEN_(opt_preset_std_for_function) );
	CODEGEN_(optimize) ( root(), ops );

	fputs("\n======================================================\n", stderr);
	fputs("Generated LLVM IR (after optimized): \r\n", stderr);
	root()->dump();
	fputs("======================================================\n", stderr);
	
	eflib::logrout::write_state( eflib::logrout::screen(), eflib::logrout::on() );

	std::string err;
	LOCVAR_(jit) = CODEGEN_(cgllvm_jit_engine::create)( boost::shared_polymorphic_cast<CODEGEN_(llvm_module_impl)>( LOCVAR_(root) ), err);
	EFLIB_ASSERT( LOCVAR_(jit), err.c_str() );
}
