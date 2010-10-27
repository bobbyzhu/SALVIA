#ifndef SASL_TEST_TEST_CASES_CGLLVM_CASES_H
#define SASL_TEST_TEST_CASES_CGLLVM_CASES_H

#include <eflib/include/config.h>

#include <sasl/test/test_cases/utility.h>
#include <sasl/include/common/compiler_info_manager.h>
#include <sasl/include/syntax_tree/program.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/code_generator/llvm/cgllvm_api.h>
#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/include/code_generator/llvm/cgllvm_jit.h>
#include <boost/shared_ptr.hpp>
#include <eflib/include/disable_warnings.h>
#include <boost/thread.hpp>
#include <eflib/include/enable_warnings.h>

class cgllvm_cases{
public:
	static cgllvm_cases& instance();
	static bool is_avaliable();
	static void release();

	TEST_CASE_SP_VARIABLE( CODEGEN_(cgllvm_jit_engine), jit );

	TEST_CASE_SP_VARIABLE( CODEGEN_(llvm_code), root );
	TEST_CASE_SP_VARIABLE( CODEGEN_(llvm_code), null_root );
	TEST_CASE_SP_VARIABLE( CODEGEN_(llvm_code), jit_prog );

	TEST_CASE_SP_VARIABLE( CODEGEN_(cgllvm_common_context), type_void );
	TEST_CASE_SP_VARIABLE( CODEGEN_(cgllvm_common_context), type_float );
	TEST_CASE_SP_VARIABLE( CODEGEN_(cgllvm_common_context), func_flt_2p_n_gen );
	TEST_CASE_SP_VARIABLE( CODEGEN_(cgllvm_common_context), func_flt_2p_n_gen_p0 );
	TEST_CASE_SP_VARIABLE( CODEGEN_(cgllvm_common_context), func_flt_2p_n_gen_p1 );

private:
	cgllvm_cases();
	void initialize();

	static boost::shared_ptr<cgllvm_cases> tcase;
	static boost::mutex mtx;
};

#endif // SASL_TEST_TEST_CASES_cgllvm_cases_H