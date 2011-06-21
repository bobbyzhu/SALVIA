#ifndef SASL_ENUMS_ENUMS_HELPER_H
#define SASL_ENUMS_ENUMS_HELPER_H

#include <eflib/include/platform/disable_warnings.h>
#include <boost/thread/mutex.hpp>
#include <eflib/include/platform/enable_warnings.h>

#include <string>
#include <vector>

struct builtin_types;
struct operators;

namespace sasl{
	namespace utility{
	
		// builtin type code
		bool is_none( const builtin_types& /*btc*/ );
		bool is_void( const builtin_types& /*btc*/ );
		bool is_integer( const builtin_types& /*btc*/ );
		bool is_real( const builtin_types& /*btc*/ );
		bool is_signed( const builtin_types& /*btc*/ );
		bool is_unsigned( const builtin_types& /*btc*/ );
		bool is_scalar( const builtin_types& /*btc*/ );
		bool is_vector( const builtin_types& /*btc*/ );
		bool is_matrix( const builtin_types& /*btc*/ );

		bool is_storagable( const builtin_types& /*btc*/ );
		bool is_standard( const builtin_types& /*btc*/ );

		builtin_types scalar_of( const builtin_types& /*btc*/ );
		builtin_types vector_of( const builtin_types& /*btc*/, size_t len );
		builtin_types matrix_of( const builtin_types& /*btc*/, size_t len_0, size_t len_1 );

		size_t len_0( const builtin_types& /*btc*/);
		size_t len_1( const builtin_types& /*btc*/);

		size_t storage_size( const builtin_types& /*btc*/ );

		const std::vector<builtin_types>& list_of_builtin_types();

		//////////////////////////////////////////////////////////////////////////
		// operators
		// types
		bool is_arithmetic( const operators& );
		bool is_relationship( const operators& );
		bool is_bit( const operators& );
		bool is_shift( const operators& );
		bool is_bool_arith( const operators& );

		// operand count
		bool is_prefix( const operators& );
		bool is_postfix( const operators& );
		bool is_unary_arith( const operators& );

		// is assign?
		bool is_arith_assign( const operators& );
		bool is_bit_assign( const operators& );
		bool is_shift_assign( const operators& );
		bool is_assign( const operators& );

		const std::vector<operators>& list_of_operators();
	}
}

#endif