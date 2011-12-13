#include <sasl/include/code_generator/llvm/cgllvm_simd.h>

#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/include/code_generator/llvm/utility.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/semantic/semantic_infos.h>

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

int const SIMD_WIDTH_IN_BYTES = 16;
int const PACKAGE_SIZE = 16;

BEGIN_NS_SASL_CODE_GENERATOR();

void cgs_simd::store( value_t& lhs, value_t const& rhs )
{
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

value_t cgs_simd::cast_ints( value_t const& v, value_tyinfo* dest_tyi )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::cast_i2f( value_t const& v, value_tyinfo* dest_tyi )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::cast_f2i( value_t const& v, value_tyinfo* dest_tyi )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::cast_f2f( value_t const& v, value_tyinfo* dest_tyi )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::cast_i2b( value_t const& v )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::cast_f2b( value_t const& v )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::create_vector( vector<value_t> const& scalars, abis abi )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

abis cgs_simd::intrinsic_abi() const
{
	return abi_vectorize;
}

value_t cgs_simd::emit_add( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_sub( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_mul( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

void cgs_simd::emit_return()
{
	builder().CreateRetVoid();
}

void cgs_simd::emit_return( value_t const& ret_v, abis abi )
{
	if( abi == abi_unknown ){ abi = fn().abi(); }

	if( fn().first_arg_is_return_address() ){
		builder().CreateStore( ret_v.load(abi), fn().return_address() );
		builder().CreateRetVoid();
	} else {
		builder().CreateRet( ret_v.load(abi) );
	}
}

value_t cgs_simd::emit_dot( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_sqrt( value_t const& lhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_cross( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

abis cgs_simd::param_abi( bool /*c_compatible*/ ) const
{
	return abi_package;
}

value_t cgs_simd::emit_extract_val( value_t const& lhs, int idx )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_extract_val( value_t const& lhs, value_t const& idx )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_extract_ref( value_t const& lhs, int idx )
{
	assert( lhs.storable() );

	builtin_types agg_hint = lhs.hint();

	if( is_vector(agg_hint) ){
		char indexes[4] = { (char)idx, -1, -1, -1 };
		uint32_t mask = indexes_to_mask( indexes );
		return value_t::slice( lhs, mask );
	} else if( is_matrix(agg_hint) ){
		EFLIB_ASSERT_UNIMPLEMENTED();
		return value_t();
	} else if ( agg_hint == builtin_types::none ){
		Value* agg_address = lhs.load_ref();
		Value* elem_address = builder().CreateStructGEP( agg_address, (unsigned)idx );
		value_tyinfo* tyinfo = NULL;
		if( lhs.tyinfo() ){
			tyinfo = member_tyinfo( lhs.tyinfo(), (size_t)idx );
		}
		return create_value( tyinfo, elem_address, vkind_ref, lhs.abi() );
	}
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_extract_ref( value_t const& lhs, value_t const& idx )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_extract_elem_mask( value_t const& vec, uint32_t mask )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_cmp_lt( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_cmp_le( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_cmp_eq( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_cmp_ne( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_cmp_ge( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::emit_cmp_gt( value_t const& lhs, value_t const& rhs )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cgs_simd::create_scalar( llvm::Value* v, value_tyinfo* tyi )
{
	Type* ty = tyi->ty( abi_vectorize );
	Value* vectorize_v = Constant::getNullValue( ty );

	for( size_t i_elem = 0; i_elem < llvm::cast<VectorType>(ty)->getNumElements(); ++i_elem ){
		vectorize_v = builder().CreateInsertElement( vectorize_v, v, int_( static_cast<int>(i_elem) ) );
	}

	return create_value( tyi, vectorize_v, vkind_value, abi_vectorize );
}

END_NS_SASL_CODE_GENERATOR();
