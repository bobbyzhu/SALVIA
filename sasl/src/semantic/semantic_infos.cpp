#include <sasl/include/semantic/semantic_infos.h>

#include <sasl/enums/literal_constant_types.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/semantic/type_checker.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/node_creation.h>

#include <eflib/include/diagnostics/assert.h>
#include <string>

using ::sasl::common::token_attr;
using ::sasl::syntax_tree::create_node;
using ::sasl::syntax_tree::buildin_type;

using ::boost::shared_ptr;

BEGIN_NS_SASL_SEMANTIC();

// some free functions.

std::string integer_literal_suffix( const std::string& str, bool& is_unsigned, bool& is_long ){
	is_unsigned = false;
	is_long = false;

	std::string::const_reverse_iterator ch_it = str.rbegin();
	char ch[2] = {'\0', '\0'};
	ch[0] = *ch_it;
	++ch_it;
	ch[1] = (ch_it == str.rend() ? '\0' : *ch_it);

	int tail_count = 0;
	for ( int i = 0; i < 2; ++i ){
		switch (ch[i]){
			case 'u':
			case 'U':
				is_unsigned = true;
				++tail_count;
				break;
			case 'l':
			case 'L':
				is_long = true;
				++tail_count;
				break;
			default:
				// do nothing
				break;
		}
	}

	// remove suffix for lexical casting.
	return ::std::string( str.begin(), str.end()-tail_count );
}
std::string real_literal_suffix( const std::string& str, bool& is_single){
	is_single = false;

	std::string::const_reverse_iterator ch_it = str.rbegin();
	if ( *ch_it == 'F' || *ch_it == 'f' ){
		is_single = true;
	}

	// remove suffix for lexical casting.
	if( is_single ){
		return std::string( str.begin(), str.end()-1 );
	} else {
		return str;
	}
}
////////////////////////////////

//////////////////////
// program semantics

program_si::program_si(): prog_name("######undefined######"){}
const std::string& program_si::name() const {
	return prog_name;
}
void program_si::name( const std::string& str ){
	prog_name = str;
}

//////////////////////////////////////////////////////////////////////////
// type info semantic info impl

type_entry::id_t type_info_si_impl::entry_id() const{
	return tid;
}

void type_info_si_impl::entry_id( type_entry::id_t id ){
	tid = id;
}

shared_ptr<type_specifier> type_info_si_impl::type_info() const{
	return typemgr.lock()->get( tid );
}

void type_info_si_impl::type_info( shared_ptr<type_specifier> typespec){
	tid = typemgr.lock()->get( typespec, typespec->symbol() );
}

void type_info_si_impl::type_manager( boost::shared_ptr< class type_manager > typemgr ){
	this->typemgr = typemgr;
}

//////////////////////////////////////////////////////////////////////////
// constant value semantic info
const_value_si::const_value_si(){}

void const_value_si::set_literal(
	const std::string& litstr,
	literal_constant_types lctype)
{
	std::string nosuffix_litstr;
	if (lctype == literal_constant_types::integer ){
		bool is_unsigned(false);
		bool is_long(false);
		nosuffix_litstr = integer_literal_suffix( litstr, is_unsigned, is_long );
		if ( is_unsigned ){
			val = boost::lexical_cast<uint64_t>(nosuffix_litstr);
			type_info()->value_typecode = ( is_long ? buildin_type_code::_uint64 : buildin_type_code::_uint32 );
		} else {
			val = boost::lexical_cast<int64_t>(nosuffix_litstr);
			type_info()->value_typecode = ( is_long ? buildin_type_code::_sint64 : buildin_type_code::_sint32 );
		}
	} else if( lctype == literal_constant_types::real ){
		bool is_single(false);
		nosuffix_litstr = real_literal_suffix( litstr, is_single );
		val = boost::lexical_cast<double>(nosuffix_litstr);
		type_info()->value_typecode = (is_single ? buildin_type_code::_float : buildin_type_code::_double);
	} else if( lctype == literal_constant_types::boolean ){
		val = (litstr == "true");
		type_info()->value_typecode = buildin_type_code::_boolean;
	} else if( lctype == literal_constant_types::character ){
		val = litstr[0];
		type_info()->value_typecode = buildin_type_code::_sint8;
	} else if( lctype == literal_constant_types::string ){
		val = litstr;
		type_info()->value_typecode = buildin_type_code::none;
	}
}

buildin_type_code const_value_si::value_type() const{
	if( !type_info() ) return buildin_type_code::none;
	return type_info()->value_typecode;
}

type_semantic_info::type_semantic_info(): ttype(type_types::none) { }

shared_ptr<type_specifier> type_semantic_info::full_type() const{
	shared_ptr<type_specifier> ret_type = type_node.lock();
	return /*(ttype == type_types::alias) ? actual_type( ret_type ) :*/ ret_type;
}

void type_semantic_info::full_type( shared_ptr<type_specifier> ftnode ){
	type_node = ftnode;
}
type_types type_semantic_info::type_type() const{
	return ttype;
}

void type_semantic_info::type_type( type_types ttype ){
	this->ttype = ttype;
}

variable_semantic_info::variable_semantic_info()
	: isloc(false)
{
}
bool variable_semantic_info::is_local() const{
	return isloc;
}

void variable_semantic_info::is_local( bool isloc ){
	this->isloc = isloc;
}

execution_block_semantic_info::execution_block_semantic_info()
{
}


shared_ptr<type_specifier> type_info_si::from_node( ::shared_ptr<node> n )
{
	shared_ptr<type_info_si> tisi = extract_semantic_info<type_info_si>(n);
	if ( tisi ){
		return tisi->type_info();
	}
	return shared_ptr<type_specifier>();
}

storage_si::storage_si(){
}

type_si::type_si(){
}

END_NS_SASL_SEMANTIC();
