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
#include <boost/bind.hpp>
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
using llvm::CmpInst;
using llvm::PHINode;
using llvm::DefaultIRBuilder;

namespace Intrinsic = llvm::Intrinsic;

using boost::any;
using boost::shared_ptr;
using boost::enable_if;
using boost::is_integral;
using boost::unordered_map;
using boost::lexical_cast;

using std::vector;
using std::string;

BEGIN_NS_SASL_CODE_GENERATOR();

namespace {
	template <typename T> APInt apint( T v )
	{
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
	Value* src = NULL;
	Value* address = NULL;
	value_kinds kind = lhs.kind();

	if( kind == vkind_ref ){	
		src = rhs.load( lhs.abi() );
		address = lhs.raw();
	} else if ( kind == vkind_swizzle ){
		char indexes[4] = {-1, -1, -1, -1};
		value_t const* root = NULL;
		merge_swizzle(root, indexes, lhs);

		if( is_vector( root->hint()) ){
			assert( lhs.parent()->storable() );
			
			value_t rhs_rvalue = rhs.to_rvalue();
			value_t ret_v = root->to_rvalue();
			for(size_t i = 0; i < vector_size(rhs.hint()); ++i)
			{
				ret_v = emit_insert_val( ret_v, indexes[i], emit_extract_val(rhs_rvalue, i) );
			}

			src = ret_v.load( lhs.abi() );
			address = root->load_ref();
		} else {
			src = rhs.load( lhs.abi() );
			address = lhs.load_ref();
		}
	}

	assert( src && address );
	builder().CreateStore( src, address );
}

value_t cgs_sisd::cast_ints( value_t const& v, value_tyinfo* dest_tyi )
{
	builtin_types hint_src = v.hint();
	builtin_types hint_dst = dest_tyi->hint();

	builtin_types scalar_hint_src = scalar_of(hint_src);

	Type* dest_ty = dest_tyi->ty(v.abi());
	Type* elem_ty = type_( scalar_of(hint_dst), abi_llvm );

	cast_ops op = is_signed(scalar_hint_src) ? cast_op_i2i_signed : cast_op_i2i_unsigned;
	unary_fn_t cast_sv_fn = bind_cast_sv_( elem_ty, op );
	
	Value* val = unary_op_ps_ts_sva_( dest_ty, v.load(), unary_fn_t(), unary_fn_t(), unary_fn_t(), cast_sv_fn );

	return create_value( dest_tyi, builtin_types::none, val, vkind_value, v.abi() );
}

value_t cgs_sisd::cast_i2f( value_t const& v, value_tyinfo* dest_tyi )
{
	builtin_types hint_i = v.hint();
	builtin_types hint_f = dest_tyi->hint();

	builtin_types scalar_hint_i = scalar_of(hint_i);

	Type* dest_ty = dest_tyi->ty(v.abi());
	Type* elem_ty = type_( scalar_of(hint_f), abi_llvm );

	cast_ops op = is_signed(hint_i) ? cast_op_i2f : cast_op_u2f;
	unary_fn_t cast_sv_fn = bind_cast_sv_( elem_ty, op );

	Value* val = unary_op_ps_ts_sva_( dest_ty, v.load(), unary_fn_t(), unary_fn_t(), unary_fn_t(), cast_sv_fn );

	return create_value( dest_tyi, builtin_types::none, val, vkind_value, v.abi() );
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
	builtin_types scalar_hint = scalars[0].hint();
	builtin_types hint = vector_of(scalar_hint, scalars.size());

	value_t ret = undef_value(hint, abi);
	for( size_t i = 0; i < scalars.size(); ++i )
	{
		ret = emit_insert_val( ret, (int)i, scalars[i] );
	}
	return ret;
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

value_t cgs_sisd::create_scalar( Value* val, value_tyinfo* tyinfo, builtin_types hint ){
	assert( is_scalar(hint) );
	return create_value( tyinfo, hint, val, vkind_value, abi_llvm );
}

Value* cgs_sisd::select_( Value* cond, Value* yes, Value* no )
{
	return builder().CreateSelect( cond, yes, no );
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
	EFLIB_UNREF_PARAM(lhs);
	EFLIB_UNREF_PARAM(mask);

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

Value* cgs_sisd::phi_( BasicBlock* b0, Value* v0, BasicBlock* b1, Value* v1 )
{
	PHINode* phi = builder().CreatePHI( v0->getType(), 2 );
	phi->addIncoming(v0, b0);
	phi->addIncoming(v1, b1);
	return phi;
}

END_NS_SASL_CODE_GENERATOR();