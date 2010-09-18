#ifndef SASL_TEST_TEST_CASES_SYNTAX_CASES_H
#define SASL_TEST_TEST_CASES_SYNTAX_CASES_H

#include <sasl/test/test_cases/utility.h>
#include <sasl/include/syntax_tree/program.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

class syntax_cases{
public:
	static syntax_cases& instance();
	static void release();
	
	TEST_CASE_SP_VARIABLE( SYNTAX_(program), empty_prog );
	TEST_CASE_CREF_VARIABLE( std::string, prog_name );
	TEST_CASE_CREF_VARIABLE( float, val_3p25f );
	TEST_CASE_CREF_VARIABLE( float, val_3p08f );
	TEST_CASE_CREF_VARIABLE( float, val_17ushort );

	// buildin type codes.
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_sint8 );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_uint64 );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_float );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_double );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_boolean );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_void );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_short2 );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_float3 );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_double2x4 );
	TEST_CASE_CREF_VARIABLE( buildin_type_code, btc_ulong3x2 );

	TEST_CASE_SP_VARIABLE( SYNTAX_(constant_expression), cexpr_3p25f );
	TEST_CASE_SP_VARIABLE( SYNTAX_(constant_expression), cexpr_17ushort );
	TEST_CASE_SP_VARIABLE( SYNTAX_(expression_initializer), exprinit_cexpr_3p25f );

	// buildin types
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_sint8 );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_uint64 );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_float );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_double );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_boolean );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_void );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_short2 );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_float3 );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_double2x4 );
	TEST_CASE_SP_VARIABLE( SYNTAX_(buildin_type), type_ulong3x2 );

	// variables
	TEST_CASE_SP_VARIABLE( SYNTAX_(variable_declaration), var_int8 );
	TEST_CASE_SP_VARIABLE( SYNTAX_(variable_declaration), var_float_3p25f );

	// functions
	TEST_CASE_SP_VARIABLE( SYNTAX_(function_type), func_nnn );

private:
	syntax_cases();
	void initialize();
	static boost::shared_ptr<syntax_cases> tcase;
	static boost::mutex mtx;
};

#endif // SASL_TEST_TEST_CASES_PROGRAM_CASES_H