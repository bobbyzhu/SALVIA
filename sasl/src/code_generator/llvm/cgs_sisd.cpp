#include <sasl/include/code_generator/llvm/cgs_sisd.h>

#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/enums/enums_utility.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Intrinsics.h>
#include <llvm/Support/TypeBuilder.h>
#include <llvm/Support/CFG.h>
#include <eflib/include/platform/enable_warnings.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>
#include <eflib/include/platform/boost_end.h>

#include <eflib/include/diagnostics/assert.h>
#include <eflib/include/platform/cpuinfo.h>

using sasl::syntax_tree::node;
using sasl::syntax_tree::function_type;
using sasl::syntax_tree::parameter;
using sasl::syntax_tree::tynode;
using sasl::syntax_tree::declaration;
using sasl::syntax_tree::variable_declaration;
using sasl::syntax_tree::struct_type;

using sasl::semantic::storage_si;
using sasl::semantic::type_info_si;

using eflib::support_feature;
using eflib::cpu_sse2;

using namespace sasl::utility;

using llvm::APInt;
using llvm::Argument;
using llvm::LLVMContext;
using llvm::Function;
using llvm::FunctionType;
using llvm::IntegerType;
using llvm::Type;
using llvm::PointerType;
using llvm::Value;
using llvm::BasicBlock;
using llvm::Constant;
using llvm::ConstantInt;
using llvm::ConstantVector;
using llvm::StructType;
using llvm::VectorType;
using llvm::UndefValue;
using llvm::StoreInst;
using llvm::TypeBuilder;
using llvm::AttrListPtr;
using llvm::SwitchInst;

namespace Intrinsic = llvm::Intrinsic;

using boost::any;
using boost::shared_ptr;
using boost::enable_if;
using boost::is_integral;
using boost::unordered_map;
using boost::lexical_cast;

using std::vector;
using std::string;

#define EMIT_CMP_EQ_NE_BODY( op_name ) \
	builtin_types hint = lhs.hint(); \
	assert( hint == rhs.hint() ); \
	assert( is_scalar(hint) || (hint == builtin_types::_boolean) ); \
	\
	Value* ret = NULL; \
	if( is_integer(hint) || (hint == builtin_types::_boolean) ){ \
	ret = builder().CreateICmp##op_name( lhs.load(), rhs.load() ); \
	} else if( is_real(hint) ) { \
	ret = builder().CreateFCmpU##op_name( lhs.load(), rhs.load() ); \
	} \
	\
	return create_value( builtin_types::_boolean, ret, vkind_value, abi_llvm );

#define EMIT_CMP_BODY( op_name ) \
	builtin_types hint = lhs.hint(); \
	assert( hint == rhs.hint() ); \
	assert( is_scalar(hint) ); \
	\
	Value* ret = NULL; \
	if( is_integer(hint) ){ \
	if( is_signed(hint) ){ \
	ret = builder().CreateICmpS##op_name( lhs.load(), rhs.load() ); \
	} else {\
	ret = builder().CreateICmpU##op_name( lhs.load(), rhs.load() ); \
	}\
	\
	} else if( is_real(hint) ) { \
	ret = builder().CreateFCmpU##op_name( lhs.load(), rhs.load() ); \
	} \
	\
	return create_value( builtin_types::_boolean, ret, vkind_value, abi_llvm );

BEGIN_NS_SASL_CODE_GENERATOR();

namespace {

	template <typename T>
	APInt apint( T v ){
		return APInt( sizeof(v) << 3, static_cast<uint64_t>(v), boost::is_signed<T>::value );
	}

	void mask_to_indexes( char indexes[4], uint32_t mask ){
		for( int i = 0; i < 4; ++i ){
			// XYZW is 1,2,3,4 but LLVM used 0,1,2,3
			char comp_index = static_cast<char>( (mask >> i*8) & 0xFF );
			if( comp_index == 0 ){
				indexes[i] = -1;
				break;
			}
			indexes[i] = comp_index - 1;
		}
	}

	uint32_t indexes_to_mask( char indexes[4] ){
		uint32_t mask = 0;
		for( int i = 0; i < 4; ++i ){
			mask += (uint32_t)( (indexes[i] + 1) << (i*8) );
		}
		return mask;
	}

	uint32_t indexes_to_mask( char idx0, char idx1, char idx2, char idx3 ){
		char indexes[4] = { idx0, idx1, idx2, idx3 };
		return indexes_to_mask( indexes );
	}

	void dbg_print_blocks( Function* fn ){
#ifdef _DEBUG
		/*printf( "Function: 0x%X\n", fn );
		for( Function::BasicBlockListType::iterator it = fn->getBasicBlockList().begin(); it != fn->getBasicBlockList().end(); ++it ){
		printf( "  Block: 0x%X\n", &(*it) );
		}*/
		fn = fn;
#else
		fn = fn;
#endif
	}
}

void cgs_sisd::store( value_t& lhs, value_t const& rhs ){
	Value* src = rhs.load( lhs.abi() );
	Value* address = NULL;
	value_kinds kind = lhs.kind();

	if( kind == vkind_ref ){	
		address = lhs.raw();
	} else if ( kind == vkind_swizzle ){
		if( is_vector( lhs.parent()->hint()) ){
			assert( lhs.parent()->storable() );
			EFLIB_ASSERT_UNIMPLEMENTED();
		} else {
			address = lhs.load_ref();
		}
	}

	StoreInst* inst = builder().CreateStore( src, address );
	inst->setAlignment(4);
}

value_t cgs_sisd::cast_ints( value_t const& v, value_tyinfo* dest_tyi )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_sisd::cast_i2f( value_t const& v, value_tyinfo* dest_tyi )
{
	builtin_types hint_i = v.hint();
	builtin_types hint_f = dest_tyi->hint();

	Value* val = NULL;
	if( is_signed(hint_i) ){
		val = builder().CreateSIToFP( v.load(), dest_tyi->ty(abi_llvm) );
	} else {
		val = builder().CreateUIToFP( v.load(), dest_tyi->ty(abi_llvm) );
	}

	return create_value( dest_tyi, builtin_types::none, val, vkind_value, abi_llvm );
}

value_t cgs_sisd::cast_f2i( value_t const& v, value_tyinfo* dest_tyi )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_sisd::cast_f2f( value_t const& v, value_tyinfo* dest_tyi )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_sisd::create_vector( std::vector<value_t> const& scalars, abis abi ){
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

void cgs_sisd::emit_return(){
	builder().CreateRetVoid();
}

void cgs_sisd::emit_return( value_t const& ret_v, abis abi ){
	if( abi == abi_unknown ){ abi = fn().abi(); }

	if( fn().first_arg_is_return_address() ){
		builder().CreateStore( ret_v.load(abi), fn().return_address() );
		builder().CreateRetVoid();
	} else {
		builder().CreateRet( ret_v.load(abi) );
	}
}

value_t cgs_sisd::create_scalar( Value* val, value_tyinfo* tyinfo ){
	return create_value( tyinfo, val, vkind_value, abi_llvm );
}

Value* cgs_sisd::select_( Value* cond, Value* yes, Value* no )
{
	return builder().CreateSelect( cond, yes, no );
}

value_t cgs_sisd::emit_cmp_eq( value_t const& lhs, value_t const& rhs )
{
	EMIT_CMP_EQ_NE_BODY( EQ );
}

value_t cgs_sisd::emit_cmp_lt( value_t const& lhs, value_t const& rhs )
{
	EMIT_CMP_BODY( LT );
}

value_t cgs_sisd::emit_cmp_le( value_t const& lhs, value_t const& rhs )
{
	EMIT_CMP_BODY( LE );
}

value_t cgs_sisd::emit_cmp_ne( value_t const& lhs, value_t const& rhs )
{
	EMIT_CMP_EQ_NE_BODY( NE );
}

value_t cgs_sisd::emit_cmp_ge( value_t const& lhs, value_t const& rhs )
{
	EMIT_CMP_BODY( GE );
}

value_t cgs_sisd::emit_cmp_gt( value_t const& lhs, value_t const& rhs )
{
	EMIT_CMP_BODY( GT );
}

bool cgs_sisd::prefer_externals() const
{
	return false;
}

bool cgs_sisd::prefer_scalar_code() const
{
	return false;
}

value_t cgs_sisd::emit_swizzle( value_t const& lhs, uint32_t mask )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_sisd::emit_write_mask( value_t const& vec, uint32_t mask )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

void cgs_sisd::switch_to( value_t const& cond, std::vector< std::pair<value_t, insert_point_t> > const& cases, insert_point_t const& default_branch )
{
	Value* v = cond.load();
	SwitchInst* inst = builder().CreateSwitch( v, default_branch.block, static_cast<unsigned>(cases.size()) );
	for( size_t i_case = 0; i_case < cases.size(); ++i_case ){
		inst->addCase( llvm::cast<ConstantInt>( cases[i_case].first.load() ), cases[i_case].second.block );
	}
}

value_t cgs_sisd::cast_i2b( value_t const& v )
{
	assert( is_integer(v.hint()) );
	return emit_cmp_ne( v, null_value( v.hint(), v.abi() ) );
}

value_t cgs_sisd::cast_f2b( value_t const& v )
{
	assert( is_real(v.hint()) );
	return emit_cmp_ne( v, null_value( v.hint(), v.abi() ) );
}

cgllvm_sctxt* cgs_sisd::node_ctxt( shared_ptr<node> const& node, bool create_if_need )
{
	return cg_service::node_ctxt( node.get(), create_if_need );
}

abis cgs_sisd::intrinsic_abi() const
{
	return abi_llvm;
}

abis cgs_sisd::param_abi( bool c_compatible ) const
{
	return c_compatible ? abi_c : abi_llvm;
}

value_t cgs_sisd::emit_ddx( value_t const& v )
{
	// It is not available in SISD mode.
	EFLIB_ASSERT_UNIMPLEMENTED();
	return v;
}

value_t cgs_sisd::emit_ddy( value_t const& v )
{
	// It is not available in SISD mode.
	EFLIB_ASSERT_UNIMPLEMENTED();
	return v;
}

value_t cgs_sisd::packed_mask()
{
	assert(false);
	return value_t();
}

void function_t::arg_name( size_t index, std::string const& name ){
	size_t param_size = fn->arg_size();

	assert( index < param_size );

	Function::arg_iterator arg_it = fn->arg_begin();
	if( first_arg_is_return_address() ){
		++arg_it;
	}
	if( partial_execution ){
		++arg_it;
	}

	for( size_t i = 0; i < index; ++i ){ ++arg_it; }
	arg_it->setName( name );
}

void function_t::args_name( vector<string> const& names )
{
	Function::arg_iterator arg_it = fn->arg_begin();
	vector<string>::const_iterator name_it = names.begin();

	if( first_arg_is_return_address() ){
		arg_it->setName(".ret");
		++arg_it;
	}
	if( partial_execution ){
		arg_it->setName(".exec_mask");
		++arg_it;
	}

	for( size_t i = 0; i < names.size(); ++i ){
		arg_it->setName( *name_it );
		++arg_it;
		++name_it;
	}
}

shared_ptr<value_tyinfo> function_t::get_return_ty() const{
	assert( fnty->is_function() );
	return shared_ptr<value_tyinfo>( cg->node_ctxt( fnty->retval_type.get(), false )->get_tysp() );
}

size_t function_t::arg_size() const{
	assert( fn );
	size_t arg_size = fn->arg_size();
	if( fn ){
		if( first_arg_is_return_address() ){ --arg_size; }
		if( partial_execution ) { --arg_size; }
		return arg_size;
	}
	return 0;
}

value_t function_t::arg( size_t index ) const
{
	// If c_compatible and not void return, the first argument is address of return value.
	size_t arg_index = index;
	if( first_arg_is_return_address() ){ ++arg_index; }
	if( partial_execution ){ ++arg_index; }

	shared_ptr<parameter> par = fnty->params[index];
	value_tyinfo* par_typtr = cg->node_ctxt( par.get(), false )->get_typtr();

	Function::ArgumentListType::iterator it = fn->arg_begin();
	for( size_t idx_counter = 0; idx_counter < arg_index; ++idx_counter ){
		++it;
	}

	abis arg_abi = cg->param_abi( c_compatible );
	return cg->create_value( par_typtr, &(*it), arg_is_ref(index) ? vkind_ref : vkind_value, arg_abi );
}

value_t function_t::packed_execution_mask() const
{
	if( !partial_execution ){ return value_t(); }

	Function::ArgumentListType::iterator it = fn->arg_begin();
	if( first_arg_is_return_address() ){ ++it; }

	return cg->create_value( builtin_types::_uint16, &(*it), vkind_value, abi_llvm );
}

function_t::function_t(): fn(NULL), fnty(NULL), ret_void(true), c_compatible(false), cg(NULL), external(false), partial_execution(false)
{
}

bool function_t::arg_is_ref( size_t index ) const{
	assert( index < fnty->params.size() );

	builtin_types hint = fnty->params[index]->si_ptr<storage_si>()->type_info()->tycode;
	return c_compatible && !is_scalar(hint) && !is_sampler(hint);
}

bool function_t::first_arg_is_return_address() const
{
	return ( c_compatible || external ) && !ret_void;
}

abis function_t::abi() const
{
	return cg->param_abi( c_compatible );
}

llvm::Value* function_t::return_address() const
{
	if( first_arg_is_return_address() ){
		return &(*fn->arg_begin());
	}
	return NULL;
}

void function_t::return_name( std::string const& s )
{
	if( first_arg_is_return_address() ){
		fn->arg_begin()->setName( s );
	}
}

void function_t::inline_hint()
{
	fn->addAttribute( 0, llvm::Attribute::InlineHint );
}

insert_point_t::insert_point_t(): block(NULL)
{
}

END_NS_SASL_CODE_GENERATOR();