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

template <typename FunctionT>
Function* cgs_sisd::intrin_( int id )
{
	return intrins.get(id, module(), TypeBuilder<FunctionT, false>::get( context() ) );
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

value_t cgs_sisd::emit_dot( value_t const& lhs, value_t const& rhs )
{
	return emit_dot_vv(lhs, rhs);
}

value_t cgs_sisd::emit_call( function_t const& fn, vector<value_t> const& args )
{
	vector<Value*> arg_values;
	value_t var;
	if( fn.c_compatible ){
		// 
		if ( fn.first_arg_is_return_address() ){
			var = create_variable( fn.get_return_ty().get(), abi_c, "ret" );
			arg_values.push_back( var.load_ref() );
		}

		BOOST_FOREACH( value_t const& arg, args ){
			builtin_types hint = arg.hint();
			if( is_scalar(hint) ){
				arg_values.push_back( arg.load(abi_llvm) );
			} else {
				EFLIB_ASSERT_UNIMPLEMENTED();
			}
			// arg_values.push_back( arg.load( abi_llvm ) );
		}
	} else {
		BOOST_FOREACH( value_t const& arg, args ){
			arg_values.push_back( arg.load( abi_llvm ) );
		}
	}

	Value* ret_val = builder().CreateCall( fn.fn, arg_values );

	if( fn.first_arg_is_return_address() ){
		return var;
	}

	abis ret_abi = fn.c_compatible ? abi_c : abi_llvm;
	return create_value( fn.get_return_ty().get(), ret_val, vkind_value, ret_abi );
}

void cgs_sisd::jump_to( insert_point_t const& ip )
{
	assert( ip );
	if( !insert_point().block->getTerminator() ){
		builder().CreateBr( ip.block );
	}
}

void cgs_sisd::jump_cond( value_t const& cond_v, insert_point_t const const & true_ip, insert_point_t const& false_ip )
{
	Value* cond = cond_v.load();
	builder().CreateCondBr( cond, true_ip.block, false_ip.block );
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

value_t cgs_sisd::emit_sqrt( value_t const& arg_value )
{
	builtin_types hint = arg_value.hint();
	builtin_types scalar_hint = scalar_of( arg_value.hint() );
	if( is_scalar(hint) ){
		if( hint == builtin_types::_float ){
			if( prefer_externals() ) {
				EFLIB_ASSERT_UNIMPLEMENTED();
				//	function_t fn = external_proto( &externals::sqrt_f );
				//	vector<value_t> args;
				//	args.push_back(lhs);
				//	return emit_call( fn, args );
			} else if( support_feature( cpu_sse2 ) && !prefer_scalar_code() ){
				// Extension to 4-elements vector.
				value_t v4 = undef_value( vector_of(scalar_hint, 4), abi_llvm );
				v4 = emit_insert_val( v4, 0, arg_value );
				Value* v = builder().CreateCall( intrin_( Intrinsic::x86_sse_sqrt_ss ), v4.load() );
				Value* ret = builder().CreateExtractElement( v, int_(0) );

				return create_value( arg_value.tyinfo(), hint, ret, vkind_value, abi_llvm );
			} else {
				// Emit LLVM intrinsics
				Value* v = builder().CreateCall( intrin_<float(float)>(Intrinsic::sqrt), arg_value.load() );
				return create_value( arg_value.tyinfo(), arg_value.hint(), v, vkind_value, abi_llvm );
			}
		} else if( hint == builtin_types::_double ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		} 
	} else if( is_vector(hint) ) {

		size_t vsize = vector_size(hint);

		if( scalar_hint == builtin_types::_float ){
			if( prefer_externals() ){
				EFLIB_ASSERT_UNIMPLEMENTED();
			} else if( support_feature(cpu_sse2) && !prefer_scalar_code() ){
				// TODO emit SSE2 instrinsic directly.

				// expanded to vector 4
				value_t v4;
				if( vsize == 4 ){	
					v4 = create_value( arg_value.tyinfo(), hint, arg_value.load(abi_llvm), vkind_value, abi_llvm );
				} else {
					v4 = null_value( vector_of( scalar_hint, 4 ), abi_llvm );
					for ( size_t i = 0; i < vsize; ++i ){
						v4 = emit_insert_val( v4, i, emit_extract_elem(arg_value, i) );
					}
				}

				// calculate
				Value* v = builder().CreateCall( intrin_( Intrinsic::x86_sse_sqrt_ps ), v4.load() );

				if( vsize < 4 ){
					// Shrink
					static int indexes[4] = {0, 1, 2, 3};
					Value* mask = vector_( &indexes[0], vsize );
					v = builder().CreateShuffleVector( v, UndefValue::get( v->getType() ), mask );
				}

				return create_value( NULL, hint, v, vkind_value, abi_llvm );
			} else {
				value_t ret = null_value( hint, arg_value.abi() );
				for( size_t i = 0; i < vsize; ++i ){
					value_t elem = emit_extract_elem( arg_value, i );
					ret = emit_insert_val( ret, i, emit_sqrt( arg_value ) );
				}
				return ret;
			}
		} else {
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	} else {
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	return value_t();
}

bool cgs_sisd::prefer_externals() const
{
	return false;
}

bool cgs_sisd::prefer_scalar_code() const
{
	return false;
}

Function* cgs_sisd::intrin_( int v )
{
	return intrins.get( llvm::Intrinsic::ID(v), module() );
}

value_t cgs_sisd::undef_value( builtin_types bt, abis abi )
{
	assert( bt != builtin_types::none );
	Type* valty = type_( bt, abi );
	value_t val = create_value( bt, UndefValue::get(valty), vkind_value, abi );
	return val;
}

value_t cgs_sisd::emit_cross( value_t const& lhs, value_t const& rhs )
{
	assert( lhs.hint() == vector_of( builtin_types::_float, 3 ) );
	assert( rhs.hint() == lhs.hint() );

	int swz_a[] = {1, 2, 0};
	int swz_b[] = {2, 0, 1};

	ConstantVector* swz_va = vector_( swz_a, 3 );
	ConstantVector* swz_vb = vector_( swz_b, 3 );

	Value* lvec_value = lhs.load(abi_llvm);
	Value* rvec_value = rhs.load(abi_llvm);

	Value* lvec_a = builder().CreateShuffleVector( lvec_value, UndefValue::get( lvec_value->getType() ), swz_va );
	Value* lvec_b = builder().CreateShuffleVector( lvec_value, UndefValue::get( lvec_value->getType() ), swz_vb );
	Value* rvec_a = builder().CreateShuffleVector( rvec_value, UndefValue::get( rvec_value->getType() ), swz_va );
	Value* rvec_b = builder().CreateShuffleVector( rvec_value, UndefValue::get( rvec_value->getType() ), swz_vb );

	Value* mul_first = builder().CreateFMul( lvec_a, rvec_b );
	Value* mul_second = builder().CreateFMul( lvec_b, rvec_a );

	Value* ret = builder().CreateFSub( mul_first, mul_second );

	return create_value( lhs.tyinfo(), lhs.hint(), ret, vkind_value, abi_llvm );
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

void function_t::arg_name( size_t index, std::string const& name ){
	size_t param_size = fn->arg_size();
	if( first_arg_is_return_address() ){
		--param_size;
	}
	assert( index < param_size );

	Function::arg_iterator arg_it = fn->arg_begin();
	if( first_arg_is_return_address() ){
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
	if( fn ){
		if( first_arg_is_return_address() ){ return fn->arg_size() - 1; }
		return fn->arg_size() - 1;
	}
	return 0;
}

value_t function_t::arg( size_t index ) const
{
	// If c_compatible and not void return, the first argument is address of return value.
	size_t arg_index = index;
	if( first_arg_is_return_address() ){ ++arg_index; }

	shared_ptr<parameter> par = fnty->params[index];
	value_tyinfo* par_typtr = cg->node_ctxt( par.get(), false )->get_typtr();

	Function::ArgumentListType::iterator it = fn->arg_begin();
	for( size_t idx_counter = 0; idx_counter < arg_index; ++idx_counter ){
		++it;
	}

	abis arg_abi = c_compatible ? abi_c: abi_llvm;
	return cg->create_value( par_typtr, &(*it), arg_is_ref(index) ? vkind_ref : vkind_value, arg_abi );
}

function_t::function_t(): fn(NULL), fnty(NULL), ret_void(true), c_compatible(false), cg(NULL)
{
}

bool function_t::arg_is_ref( size_t index ) const{
	assert( index < fnty->params.size() );

	builtin_types hint = fnty->params[index]->si_ptr<storage_si>()->type_info()->tycode;
	return c_compatible && !is_scalar(hint);
}

bool function_t::first_arg_is_return_address() const
{
	return c_compatible && !ret_void;
}

abis function_t::abi() const
{
	return c_compatible ? abi_c : abi_llvm;
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