/*********************
Name Mangling Grammar:
Look at the documentation in sasl/docs/Name Mangling Syntax.docx
*********************/

#include <sasl/include/semantic/name_mangler.h>

#include <sasl/enums/enums_helper.h>
#include <sasl/enums/operators.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/semantic/type_checker.h>

#include <eflib/include/disable_warnings.h>
#include <boost/assign/list_inserter.hpp>
#include <boost/thread.hpp>
#include <eflib/include/enable_warnings.h>

#include <cassert>

using ::sasl::syntax_tree::array_type;
using ::sasl::syntax_tree::buildin_type;
using ::sasl::syntax_tree::expression;
using ::sasl::syntax_tree::function_type;
using ::sasl::syntax_tree::node;
using ::sasl::syntax_tree::struct_type;
using ::sasl::syntax_tree::type_specifier;
using ::sasl::syntax_tree::variable_declaration;

//////////////////////////////////////////////////////////////////////////
// lookup table for translating enumerations to string.
static boost::mutex lookup_table_mtx;

static std::string mangling_tag("M");
static boost::unordered_map< buildin_type_code, std::string > btc_decorators;
static bool is_initialized(false);

static void initialize_lookup_table(){
	boost::mutex::scoped_lock locker(lookup_table_mtx);

	if ( is_initialized ){ return; }

	boost::assign::insert( btc_decorators )
		( buildin_type_code::_void, "O" )
		( buildin_type_code::_boolean, "B" )
		( buildin_type_code::_sint8, "S1" )
		( buildin_type_code::_sint16, "S2" )
		( buildin_type_code::_sint32, "S4" )
		( buildin_type_code::_sint64, "S8" )
		( buildin_type_code::_uint8, "U1" )
		( buildin_type_code::_uint16, "U2" )
		( buildin_type_code::_uint32, "U4" )
		( buildin_type_code::_uint64, "U8" )
		( buildin_type_code::_float, "F" )
		( buildin_type_code::_double, "D" )
		;

	is_initialized = true;
}

//////////////////////////////////////////////////////////////////////////
// some free function for manging
static void append( std::string& str, boost::shared_ptr<type_specifier> typespec );

static void append( std::string& str, buildin_type_code btc, bool is_component = false ){
	if ( sasl_ehelper::is_scalar( btc ) ) {
		if ( !is_component ){
			// if it is not a component of a vector or matrix,
			// add a lead char 'B' since is a BaseTypeName of a buildin scalar type.
			str.append("B");
		}
		str.append( btc_decorators[btc] );
	} else if( sasl_ehelper::is_vector( btc ) ) {
		char vector_len_buf[2];
		str.append("V");
		str.append( _ui64toa( sasl_ehelper::len_0( btc ), vector_len_buf, 10 ) );
		append( str, sasl_ehelper::scalar_of(btc), true );
	} else if ( sasl_ehelper::is_matrix(btc) ) {
		char matrix_len_buf[2];
		str.append("M");
		str.append( _ui64toa( sasl_ehelper::len_0( btc ), matrix_len_buf, 10 ) );
		str.append( _ui64toa( sasl_ehelper::len_1( btc ), matrix_len_buf, 10 ) );
		append( str, sasl_ehelper::scalar_of(btc), true );
	}
}

static void append( std::string& str, type_qualifiers qual ){
	if ( qual.included( type_qualifiers::_uniform ) ){
		str.append("U");
	}
	str.append("Q");
}

static void append( std::string& str, boost::shared_ptr<struct_type> stype ){
	str.append("S");
	str.append( stype->name->str );
}

static void append( std::string& str, boost::shared_ptr<array_type> atype ){
	for ( size_t i_dim = 0; i_dim < atype->array_lens.size(); ++i_dim ){
		str.append("A");
		// str.append( atype->array_lens[i_dim] );
		// if ( i_dim < atype->array_lens.size() - 1 ){
		//	str.append("Q");
		// }
	}
	append( str, atype->elem_type );
}

static void append( std::string& str, boost::shared_ptr<type_specifier> typespec ){
	append(str, typespec->qual);
	// append (str, scope_qualifier(typespec) );
	if ( typespec->node_class() == syntax_node_types::buildin_type ){
		append( str, typespec->value_typecode );
	} else if ( typespec->node_class() == syntax_node_types::struct_type ) {
		append( str, boost::shared_polymorphic_cast<struct_type>( typespec ) );
	} else if( typespec->node_class() == syntax_node_types::array_type ){
		append( str, boost::shared_polymorphic_cast<array_type>(typespec) );
	} else if ( typespec->node_class() == syntax_node_types::function_type ){
		// append( str, boost::shared_polymorphic_cast<function_type>(typespec) );
	}
}


BEGIN_NS_SASL_SEMANTIC();

std::string mangle( boost::shared_ptr<function_type> mangling_function ){
	initialize_lookup_table();

	// start char
	std::string mangled_name = "M";

	// qualified name
	// append( str, scope_qualifier( mangling_function ) );
	mangled_name += mangling_function->name->str;

	// splitter
	mangled_name.append("@@");

	// parameter types
	for (size_t i_param = 0; i_param < mangling_function->params.size(); ++i_param){
		boost::shared_ptr<type_specifier> par_type
			= type_info_si::from_node( mangling_function->params[i_param] );
		append( mangled_name, par_type );
		mangled_name.append( "@@" );
	}

	// calling convention
	// append( str, callingconv( mangling_function ) );

	return mangled_name;
}

std::string mangle( boost::shared_ptr<expression> expr ){

}
END_NS_SASL_SEMANTIC();