#include <sasl/include/semantic/semantic_infos.h>

#include <sasl/enums/literal_constant_types.h>
#include <sasl/include/common/compiler_info_manager.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/semantic/type_checker.h>
#include <sasl/include/semantic/type_manager.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/node_creation.h>

#include <softart/include/enums.h>

#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/foreach.hpp>
#include <eflib/include/platform/boost_end.h>

#include <string>

using ::sasl::common::compiler_info_manager;
using ::sasl::common::token_t;
using ::sasl::syntax_tree::create_node;
using ::sasl::syntax_tree::builtin_type;

using softart::semantic;

using ::boost::addressof;
using ::boost::shared_ptr;
using ::boost::unordered_map;

using std::string;
using std::vector;

#define SASL_INIT_TYPE_INFO_PROXY( typemgr ) type_info_proxy(typemgr)

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
// type info semantic info implementation

type_info_si_impl::type_info_si_impl( boost::shared_ptr<type_manager> typemgr )
	: typemgr(typemgr), tid(-1)
{
}

type_entry::id_t type_info_si_impl::entry_id() const{
	return tid;
}

void type_info_si_impl::entry_id( type_entry::id_t id ){
	tid = id;
}

shared_ptr<type_specifier> type_info_si_impl::type_info() const{
	return typemgr.lock()->get( tid );
}

void type_info_si_impl::type_info( shared_ptr<type_specifier> typespec, shared_ptr<symbol> sym ){
	tid = typemgr.lock()->get( typespec, sym );
}

void type_info_si_impl::type_info( builtin_type_code btc )
{
	tid = typemgr.lock()->get( btc ); 
}

//////////////////////////////////////////////////////////////////////////
// constant value semantic info
const_value_si::const_value_si( shared_ptr<type_manager> typemgr )
	: SASL_INIT_TYPE_INFO_PROXY(typemgr)
{}

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
			type_info( is_long ? builtin_type_code::_uint64 : builtin_type_code::_uint32 );
		} else {
			val = boost::lexical_cast<int64_t>(nosuffix_litstr);
			type_info( is_long ? builtin_type_code::_sint64 : builtin_type_code::_sint32 );
		}
	} else if( lctype == literal_constant_types::real ){
		bool is_single(false);
		nosuffix_litstr = real_literal_suffix( litstr, is_single );
		val = boost::lexical_cast<double>(nosuffix_litstr);
		type_info( is_single ? builtin_type_code::_float : builtin_type_code::_double);
	} else if( lctype == literal_constant_types::boolean ){
		val = (litstr == "true");
		type_info( builtin_type_code::_boolean );
	} else if( lctype == literal_constant_types::character ){
		val = litstr[0];
		type_info( builtin_type_code::_sint8 );
	} else if( lctype == literal_constant_types::string ){
		val = litstr;
		type_info( builtin_type_code::none );
	}
}

builtin_type_code const_value_si::value_type() const{
	if( !type_info() ) return builtin_type_code::none;
	return type_info()->value_typecode;
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

shared_ptr<type_specifier> type_info_si::from_node( ::shared_ptr<node> n )
{
	shared_ptr<type_info_si> tisi = extract_semantic_info<type_info_si>(n);
	if ( tisi ){
		return tisi->type_info();
	}
	return shared_ptr<type_specifier>();
}

storage_si::storage_si( shared_ptr<type_manager> typemgr )
	: SASL_INIT_TYPE_INFO_PROXY(typemgr), sem(softart::SV_None), memidx(-1), swz(0)
{
}

softart::semantic storage_si::get_semantic() const{
	return sem;
}

void storage_si::set_semantic( softart::semantic v ){
	sem = v;
}

int storage_si::mem_index() const{
	return memidx;
}

void storage_si::mem_index( int i ){
	memidx = i;
}

int32_t storage_si::swizzle() const{
	return swz;
}

void storage_si::swizzle( int32_t v ){
	swz = v;
}


type_si::type_si( shared_ptr<type_manager> typemgr )
	: SASL_INIT_TYPE_INFO_PROXY(typemgr)
{
}

statement_si::statement_si():has_lp(false){}

const std::string& statement_si::exit_point() const{
	return exit_pt;
}

void statement_si::exit_point( const std::string& v ){
	exit_pt = v;
}

const std::string& statement_si::loop_point() const{
	return loop_pt;
}

void statement_si::loop_point( const std::string& v ){
	loop_pt = v;
}

bool statement_si::has_loop() const{
	return has_lp;
}
void statement_si::has_loop( bool v ){
	has_lp = v;
}

shared_ptr<node> statement_si::parent_block() const{
	return parent.lock();
}
void statement_si::parent_block( shared_ptr<node> v ){
	parent = v;
}


int32_t swizzle_field_name_to_id( char ch ){
	switch( ch ){
	case 'x': return 1;
	case 'y': return 2;
	case 'z': return 3;
	case 'w': return 4;
	}
	return 0;
}

int32_t encode_swizzle( char _1st, char _2nd, char _3rd, char _4th ){
	int32_t swz = 0;

	if( _1st == 0 ){
		return 0;
	} else {
		assert( swizzle_field_name_to_id(_1st) );
		swz = swizzle_field_name_to_id(_1st);
	}

	if( _2nd == 0){
		return swz;
	} else {
		assert( swizzle_field_name_to_id(_2nd) );
		swz &= ( _2nd << 8);
	}

	if( _3rd == 0){
		return swz;
	} else {
		assert( swizzle_field_name_to_id(_3rd) );
		swz &= ( _3rd << 16);
	}
	
	assert( swizzle_field_name_to_id(_4th) );
	swz &= ( _4th << 24);

	return swz;
}

int32_t encode_swizzle( int& dest_size, int& min_src_size, char const* masks ){
	min_src_size = 0;
	dest_size = 0;
	int32_t swz = 0;
	for( char const* p = &masks[0];;++p){
		if( *p ){
			int32_t field_swz = swizzle_field_name_to_id(*p);
			assert( field_swz );
			swz += ( field_swz << (dest_size * 8) );
			if( field_swz > min_src_size ){
				min_src_size = field_swz;
			}
			++dest_size;
		} else {
			break;
		}
	}

	return swz;
}

int32_t encode_sized_swizzle( int size )
{
	int32_t swz = 0;
	for( int32_t i = 1; i <= size; ++i ){
		swz &= ( i << (i-1) * 8 );
	}
	return swz;
}

END_NS_SASL_SEMANTIC();
