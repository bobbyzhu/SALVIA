#include <sasl/include/code_generator/llvm/cgs.h>

#include <sasl/include/code_generator/llvm/utility.h>
#include <sasl/include/code_generator/llvm/ty_cache.h>
#include <sasl/include/code_generator/llvm/cgllvm_globalctxt.h>
#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/enums/enums_utility.h>

#include <salviar/include/shader_abi.h>

#include <eflib/include/math/math.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/CFG.h>
#include <llvm/Support/TypeBuilder.h>
#include <llvm/Intrinsics.h>
#include <llvm/Constants.h>
#include <eflib/include/platform/enable_warnings.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <eflib/include/platform/boost_end.h>

using llvm::Module;
using llvm::LLVMContext;
using llvm::DefaultIRBuilder;
using llvm::Value;
using llvm::Type;
using llvm::VectorType;
using llvm::TypeBuilder;
using llvm::BasicBlock;
using llvm::Instruction;

namespace Intrinsic = llvm::Intrinsic;

using boost::shared_ptr;

using namespace sasl::syntax_tree;
using namespace sasl::semantic;

using sasl::utility::is_integer;
using sasl::utility::is_real;
using sasl::utility::is_signed;
using sasl::utility::is_scalar;
using sasl::utility::is_vector;
using sasl::utility::is_matrix;
using sasl::utility::is_sampler;
using sasl::utility::scalar_of;
using sasl::utility::vector_of;
using sasl::utility::matrix_of;
using sasl::utility::vector_size;
using sasl::utility::vector_count;
using sasl::utility::storage_size;
using sasl::utility::replace_scalar;

using salviar::PACKAGE_ELEMENT_COUNT;
using salviar::SIMD_ELEMENT_COUNT;

using namespace eflib;

BEGIN_NS_SASL_CODE_GENERATOR();

/// @}

Function* cg_service::intrin_( int v )
{
	return intrins.get( llvm::Intrinsic::ID(v), module() );
}

template <typename FunctionT>
Function* cg_service::intrin_( int id )
{
	return intrins.get(id, module(), TypeBuilder<FunctionT, false>::get( context() ) );
}

bool cg_service::initialize( llvm_module_impl* mod, node_ctxt_fn const& fn )
{
	assert ( mod );

	mod_impl = mod;
	node_ctxt = fn;
	initialize_cache( context() );

	return true;
}

Module* cg_service::module() const{
	return mod_impl->module();
}

LLVMContext& cg_service::context() const{
	return mod_impl->context();
}

DefaultIRBuilder& cg_service::builder() const{
	return *( mod_impl->builder() );
}

function_t cg_service::fetch_function( shared_ptr<function_type> const& fn_node ){

	cgllvm_sctxt* fn_ctxt = node_ctxt( fn_node.get(), false );
	if( fn_ctxt->data().self_fn ){
		return fn_ctxt->data().self_fn;
	}

	function_t ret;
	ret.fnty = fn_node.get();
	ret.c_compatible = fn_node->si_ptr<storage_si>()->c_compatible();
	ret.external = fn_node->si_ptr<storage_si>()->external_compatible();
	ret.partial_execution = fn_node->si_ptr<storage_si>()->partial_execution();

	abis abi = param_abi( ret.c_compatible );

	vector<Type*> par_tys;

	Type* ret_ty = node_ctxt( fn_node->retval_type.get(), false )->get_typtr()->ty( abi );

	ret.ret_void = true;
	if( abi == abi_c || ret.external ){
		if( fn_node->retval_type->tycode != builtin_types::_void ){
			// If function need C compatible and return value is not void, The first parameter is set to point to return value, and parameters moves right.
			Type* ret_ptr = PointerType::getUnqual( ret_ty );
			par_tys.push_back( ret_ptr );
			ret.ret_void = false;
		}

		ret_ty = Type::getVoidTy( context() );
	}

	if( abi == abi_package && ret.partial_execution ){
		Type* mask_ty = Type::getInt16Ty( context() );
		par_tys.push_back(mask_ty);
	}

	// Create function type.
	BOOST_FOREACH( shared_ptr<parameter> const& par, fn_node->params )
	{
		cgllvm_sctxt* par_ctxt = node_ctxt( par.get(), false );
		value_tyinfo* par_ty = par_ctxt->get_typtr();
		assert( par_ty );

		Type* par_llty = par_ty->ty( abi );
		
		bool as_ref = false;
		builtin_types par_hint = par_ty->hint();
		if( ret.c_compatible || ret.external ){
			if( is_sampler( par_hint ) ){
				as_ref = false;
			} else if ( is_scalar(par_hint) && ( promote_abi( param_abi(false), abi_llvm ) == abi_llvm ) ){
				as_ref = false;
			} else {
				as_ref = true;
			}
		} else {
			as_ref = false;
		}

		if( as_ref )
		{
			par_tys.push_back( PointerType::getUnqual( par_llty ) );
		}
		else
		{
			par_tys.push_back( par_llty );
		}
	}

	FunctionType* fty = FunctionType::get( ret_ty, par_tys, false );

	// Create function
	ret.fn = Function::Create( fty, Function::ExternalLinkage, fn_node->symbol()->mangled_name(), module() );
	ret.cg = this;
	return ret;
}

value_t cg_service::null_value( value_tyinfo* tyinfo, abis abi )
{
	assert( tyinfo && abi != abi_unknown );
	Type* value_type = tyinfo->ty(abi);
	assert( value_type );
	return create_value( tyinfo, Constant::getNullValue(value_type), vkind_value, abi );
}

value_t cg_service::null_value( builtin_types bt, abis abi )
{
	assert( bt != builtin_types::none );
	Type* valty = type_( bt, abi );
	value_t val = create_value( bt, Constant::getNullValue( valty ), vkind_value, abi );
	return val;
}

value_t cg_service::create_value( value_tyinfo* tyinfo, Value* val, value_kinds k, abis abi ){
	return value_t( tyinfo, val, k, abi, this );
}

value_t cg_service::create_value( builtin_types hint, Value* val, value_kinds k, abis abi )
{
	return value_t( hint, val, k, abi, this );
}

value_t cg_service::create_value( value_tyinfo* tyinfo, builtin_types hint, Value* val, value_kinds k, abis abi )
{
	if( tyinfo ){
		return create_value( tyinfo, val, k, abi );
	} else {
		return create_value( hint, val, k ,abi );
	}
}

shared_ptr<value_tyinfo> cg_service::create_tyinfo( shared_ptr<tynode> const& tyn ){
	cgllvm_sctxt* ctxt = node_ctxt(tyn.get(), true);
	if( ctxt->get_tysp() ){
		return ctxt->get_tysp();
	}

	value_tyinfo* ret = new value_tyinfo();
	ret->tyn = tyn.get();
	ret->cls = value_tyinfo::unknown_type;

	if( tyn->is_builtin() ){
		ret->tys[abi_c] = type_( tyn->tycode, abi_c );
		ret->tys[abi_llvm] = type_( tyn->tycode, abi_llvm );
		ret->tys[abi_vectorize] = type_( tyn->tycode, abi_vectorize );
		ret->tys[abi_package] = type_( tyn->tycode, abi_package );
		ret->cls = value_tyinfo::builtin;
	} else {
		ret->cls = value_tyinfo::aggregated;

		if( tyn->is_struct() ){
			shared_ptr<struct_type> struct_tyn = tyn->as_handle<struct_type>();

			vector<Type*> c_member_types;
			vector<Type*> llvm_member_types;
			vector<Type*> vectorize_member_types;
			vector<Type*> package_member_types;

			BOOST_FOREACH( shared_ptr<declaration> const& decl, struct_tyn->decls){
				if( decl->node_class() == node_ids::variable_declaration ){
					shared_ptr<variable_declaration> decl_tyn = decl->as_handle<variable_declaration>();
					shared_ptr<value_tyinfo> decl_tyinfo = create_tyinfo( decl_tyn->type_info->si_ptr<type_info_si>()->type_info() );
					size_t declarator_count = decl_tyn->declarators.size();
					c_member_types.insert( c_member_types.end(), declarator_count, decl_tyinfo->ty(abi_c) );
					llvm_member_types.insert( llvm_member_types.end(), declarator_count, decl_tyinfo->ty(abi_llvm) );
					vectorize_member_types.insert( vectorize_member_types.end(), declarator_count, decl_tyinfo->ty(abi_vectorize) );
					package_member_types.insert( package_member_types.end(), declarator_count, decl_tyinfo->ty(abi_package) );
				}
			}

			StructType* ty_c	= StructType::create( c_member_types,			struct_tyn->name->str + ".abi.c" );
			StructType* ty_llvm	= StructType::create( llvm_member_types,		struct_tyn->name->str + ".abi.llvm" );
			StructType* ty_vec	= NULL;
			if( vectorize_member_types[0] != NULL ){
				ty_vec = StructType::create( vectorize_member_types,struct_tyn->name->str + ".abi.vec" );
			}
			StructType* ty_pkg	= NULL;
			if( package_member_types[0] != NULL ){
				ty_pkg	= StructType::create( package_member_types,		struct_tyn->name->str + ".abi.pkg" );
			}

			ret->tys[abi_c]			= ty_c;
			ret->tys[abi_llvm]		= ty_llvm;
			ret->tys[abi_vectorize]	= ty_vec;
			ret->tys[abi_package]	= ty_pkg;
		} else {
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	}

	ctxt->data().tyinfo = shared_ptr<value_tyinfo>(ret);
	return ctxt->data().tyinfo;
}

value_tyinfo* cg_service::member_tyinfo( value_tyinfo const* agg, size_t index ) const
{
	if( !agg ){
		return NULL;
	} else if ( agg->tyn_ptr()->is_struct() ){
		shared_ptr<struct_type> struct_sty = agg->tyn_ptr()->as_handle<struct_type>();

		size_t var_index = 0;
		BOOST_FOREACH( shared_ptr<declaration> const& child, struct_sty->decls ){
			if( child->node_class() == node_ids::variable_declaration ){
				shared_ptr<variable_declaration> vardecl = child->as_handle<variable_declaration>();
				var_index += vardecl->declarators.size();
				if( index < var_index ){
					return const_cast<cg_service*>(this)->node_ctxt( vardecl.get(), false )->get_typtr();
				}
			}
		}

		assert(!"Out of struct bound.");
	} else {
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	return NULL;
}

value_t cg_service::create_variable( builtin_types bt, abis abi, std::string const& name )
{
	Type* vty = type_( bt, abi );
	return create_value( bt, alloca_(vty, name), vkind_ref, abi );
}

value_t cg_service::create_variable( value_tyinfo const* ty, abis abi, std::string const& name )
{
	Type* vty = type_( ty, abi );
	return create_value( const_cast<value_tyinfo*>(ty), alloca_(vty, name), vkind_ref, abi );
}

insert_point_t cg_service::new_block( std::string const& hint, bool set_as_current )
{
	assert( in_function() );
	insert_point_t ret;
	ret.block = BasicBlock::Create( context(), hint, fn().fn );
	if( set_as_current ){ set_insert_point( ret ); }
	return ret;
}

void cg_service::clean_empty_blocks()
{
	assert( in_function() );

	typedef Function::BasicBlockListType::iterator block_iterator_t;
	block_iterator_t beg = fn().fn->getBasicBlockList().begin();
	block_iterator_t end = fn().fn->getBasicBlockList().end();

	// Remove useless blocks
	vector<BasicBlock*> useless_blocks;

	for( block_iterator_t it = beg; it != end; ++it )
	{
		// If block has terminator, that's a well-formed block.
		if( it->getTerminator() ) continue;

		// Add no-pred & empty block to remove list.
		if( llvm::pred_begin(it) == llvm::pred_end(it) && it->empty() ){
			useless_blocks.push_back( it );
			continue;
		}
	}

	BOOST_FOREACH( BasicBlock* bb, useless_blocks ){
		bb->removeFromParent();
	}

	// Relink unlinked blocks
	beg = fn().fn->getBasicBlockList().begin();
	end = fn().fn->getBasicBlockList().end();
	for( block_iterator_t it = beg; it != end; ++it ){
		if( it->getTerminator() ) continue;

		// Link block to next.
		block_iterator_t next_it = it;
		++next_it;
		builder().SetInsertPoint( it );
		if( next_it != fn().fn->getBasicBlockList().end() ){
			builder().CreateBr( next_it );
		} else {
			if( !fn().fn->getReturnType()->isVoidTy() ){
				builder().CreateRet( Constant::getNullValue( fn().fn->getReturnType() ) );
			} else {
				emit_return();
			}
		}
	}
}

bool cg_service::in_function() const{
	return !fn_ctxts.empty();
}

function_t& cg_service::fn(){
	return fn_ctxts.back();
}

void cg_service::push_fn( function_t const& fn ){
	if( !fn_ctxts.empty() )
	{
		assert( fn.fn != this->fn().fn );
	}
	fn_ctxts.push_back(fn);
}

void cg_service::pop_fn(){
	fn_ctxts.pop_back();
}

void cg_service::set_insert_point( insert_point_t const& ip ){
	builder().SetInsertPoint(ip.block);
}

insert_point_t cg_service::insert_point() const
{
	insert_point_t ret;
	ret.block = builder().GetInsertBlock();
	return ret;
}

Type* cg_service::type_( builtin_types bt, abis abi )
{
	assert( abi != abi_unknown );
	return get_llvm_type( context(), bt, abi );
}

Type* cg_service::type_( value_tyinfo const* ty, abis abi )
{
	assert( ty->ty(abi) );
	return ty->ty(abi);
}

llvm::Value* cg_service::load( value_t const& v )
{
	value_kinds kind = v.kind();
	Value* raw = v.raw();

	uint32_t masks = v.masks();

	assert( kind != vkind_unknown && kind != vkind_tyinfo_only );

	Value* ref_val = NULL;
	if( kind == vkind_ref || kind == vkind_value ){
		ref_val = raw;
	} else if( ( kind & (~vkind_ref) ) == vkind_swizzle ){
		// Decompose indexes.
		char indexes[4] = {-1, -1, -1, -1};
		mask_to_indexes(indexes, masks);
		vector<int> index_values;
		index_values.reserve(4);
		for( int i = 0; i < 4 && indexes[i] != -1; ++i ){
			index_values.push_back( indexes[i] );
		}
		assert( !index_values.empty() );

		// Swizzle and write mask
		if( index_values.size() == 1 && is_scalar(v.hint()) ){
			// Only one member we could extract reference.
			ref_val = emit_extract_val( v.parent()->to_rvalue(), index_values[0] ).load();
		} else {
			// Multi-members must be swizzle/writemask.
			assert( (kind & vkind_ref) == 0 );
			value_t ret_val = emit_extract_elem_mask( v.parent()->to_rvalue(), masks );
			return ret_val.load( v.abi() );
		}
	} else {
		assert(false);
	}

	if( kind & vkind_ref ){
		return builder().CreateLoad( ref_val );
	} else {
		return ref_val;
	}
}

llvm::Value* cg_service::load( value_t const& v, abis abi )
{
	return load_as( v, abi );
}

llvm::Value* cg_service::load_ref( value_t const& v )
{
	value_kinds kind = v.kind();

	if( kind == vkind_ref ){
		return v.raw();
	} else if( kind == (vkind_swizzle|vkind_ref) ){
		value_t non_ref( v );
		non_ref.kind( vkind_swizzle );
		return non_ref.load();
	} if( kind == vkind_swizzle ){
		assert( v.masks() );
		return emit_extract_elem_mask( *v.parent(), v.masks() ).load_ref();
	}
	return NULL;
}

Value* cg_service::load_ref( value_t const& v, abis abi )
{
	if( v.abi() == abi || v.hint() == builtin_types::_sampler ){
		return load_ref(v);
	} else {
		return NULL;
	}
}

Value* cg_service::load_as( value_t const& v, abis abi )
{
	assert( abi != abi_unknown );

	if( v.abi() == abi ){ return v.load(); }

	switch( v.abi() )
	{
	case abi_c:
		if( abi == abi_llvm ){
			return load_as_llvm_c(v, abi);
		} else if ( abi == abi_vectorize ){
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		} else if ( abi == abi_package ) {
			return load_c_as_package( v );
		} else {
			assert(false);
			return NULL;
		}
	case abi_llvm:
		if( abi == abi_c ){
			return load_as_llvm_c(v, abi);
		} else if ( abi == abi_vectorize ){
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		} else if ( abi == abi_package ) {
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		} else {
			assert(false);
			return NULL;
		}
	case abi_vectorize:
		if( abi == abi_c ){
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		} else if ( abi == abi_llvm ){
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		} else if ( abi == abi_package ) {
			return load_vec_as_package( v );
		} else {
			assert(false);
			return NULL;
		}
	case abi_package:
		if( abi == abi_c ){
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		} else if ( abi == abi_llvm ){
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		} else if ( abi == abi_vectorize ) {
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		} else {
			assert(false);
			return NULL;
		}
	}

	assert(false);
	return NULL;
}

Value* cg_service::load_as_llvm_c( value_t const& v, abis abi )
{
	builtin_types hint = v.hint();

	if( is_scalar( hint ) ){
		return v.load();
	} else if( is_vector( hint ) ){
		value_t ret_value = undef_value( hint, abi );

		size_t vec_size = vector_size( hint );
		for( size_t i = 0; i < vec_size; ++i ){
			ret_value = emit_insert_val( ret_value, (int)i, emit_extract_elem(v, i) );
		}

		return ret_value.load();
	} else if( is_matrix( hint ) ){
		value_t ret_value = null_value( hint, abi );
		size_t vec_count = vector_count( hint );
		for( size_t i = 0; i < vec_count; ++i ){
			value_t org_vec = emit_extract_val(v, (int)i);
			ret_value = emit_insert_val( ret_value, (int)i, org_vec );
		}

		return ret_value.load();
	} else {
		// NOTE: We assume that, if tyinfo is null and hint is none, it is only the entry of vs/ps. Otherwise, tyinfo must be not NULL.
		if( !v.tyinfo() && hint == builtin_types::none ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		} else {
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	}
	return NULL;
}

value_t cg_service::emit_insert_val( value_t const& lhs, value_t const& idx, value_t const& elem_value )
{
	Value* indexes[1] = { idx.load() };
	Value* agg = lhs.load();
	Value* new_value = NULL;
	if( agg->getType()->isStructTy() ){
		assert(false);
	} else if ( agg->getType()->isVectorTy() ){
		if( lhs.abi() == abi_vectorize || lhs.abi() == abi_package ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
		new_value = builder().CreateInsertElement( agg, elem_value.load(), indexes[0] );
	}
	assert(new_value);
	
	return create_value( lhs.tyinfo(), lhs.hint(), new_value, vkind_value, lhs.abi() );
}

value_t cg_service::emit_insert_val( value_t const& lhs, int index, value_t const& elem_value )
{
	Value* agg = lhs.load();
	Value* new_value = NULL;
	if( agg->getType()->isStructTy() ){
		if( lhs.abi() == abi_vectorize || lhs.abi() == abi_package ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
		new_value = builder().CreateInsertValue( agg, elem_value.load(lhs.abi()), (unsigned)index );
	} else if ( agg->getType()->isVectorTy() ){
		if( lhs.abi() == abi_vectorize || lhs.abi() == abi_package ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
		value_t index_value = create_value( builtin_types::_sint32, int_(index), vkind_value, abi_llvm );
		return emit_insert_val( lhs, index_value, elem_value );
	}
	assert(new_value);

	return create_value( lhs.tyinfo(), lhs.hint(), new_value, vkind_value, lhs.abi() );
}

Value* cg_service::load_vec_as_package( value_t const& v )
{
	builtin_types hint = v.hint();

	if( is_scalar(hint) || is_vector(hint) )
	{
		Value* vec_v = v.load();
		vector<Constant*> indexes;
		indexes.reserve( PACKAGE_ELEMENT_COUNT );
		for( size_t i = 0; i < PACKAGE_ELEMENT_COUNT; ++i ){
			indexes.push_back( int_( static_cast<int>(i) % SIMD_ELEMENT_COUNT() ) );
		}
		return builder().CreateShuffleVector( vec_v, vec_v, ConstantVector::get( indexes ) );
	} else {
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	return NULL;
}

Value* cg_service::load_c_as_package( value_t const& v )
{
	if( v.hint() == builtin_types::_sampler ){
		return v.load();
	} else {
		
		value_t llvm_v = create_value( v.tyinfo(), v.hint(), v.load(abi_llvm), vkind_value, abi_llvm );

		if( is_scalar( v.hint() ) || is_vector( v.hint() ) ){

			// Vectorize value if scalar.
			Value* vec_val = NULL;
			if( is_scalar(v.hint()) ){
				vec_val = cast_s2v( llvm_v ).load();
			} else {
				vec_val = v.load(abi_llvm);
			}

			// Shuffle llvm value to package value.
			size_t vec_size = vector_size(v.hint());
			size_t vec_stride = ceil_to_pow2(vec_size);

			int package_scalar_count = static_cast<int>( vec_stride * PACKAGE_ELEMENT_COUNT );
			vector<size_t> shuffle_indexes(package_scalar_count);
			
			for ( size_t i_elem = 0; i_elem < PACKAGE_ELEMENT_COUNT; ++i_elem ){
				for ( size_t i_scalar = 0 ; i_scalar < vec_stride; ++i_scalar ){
					shuffle_indexes[i_elem*vec_stride+i_scalar] = i_scalar;
				}
			}

			Value* shuffle_mask = vector_<int>( &(shuffle_indexes[0]), package_scalar_count );
			return builder().CreateShuffleVector( vec_val, UndefValue::get(vec_val->getType()), shuffle_mask );
		} else {
			EFLIB_ASSERT_UNIMPLEMENTED();
			return NULL;
		}
	}
}

abis cg_service::promote_abi( abis abi0, abis abi1 )
{
	if( abi0 == abi_c ){ return abi1; }
	if( abi1 == abi_c ){ return abi0; }
	if( abi0 == abi_llvm ){ return abi1; }
	if( abi1 == abi_llvm ){ return abi0; }
	if( abi0 == abi_vectorize ){ return abi1; }
	if( abi1 == abi_vectorize ){ return abi0; }
	return abi0;
}

abis cg_service::promote_abi( abis abi0, abis abi1, abis abi2 )
{
	return promote_abi( promote_abi( abi0, abi1 ), abi2 );
}

value_t cg_service::emit_mul_comp( value_t const& lhs, value_t const& rhs )
{
	builtin_types lhint = lhs.hint();
	builtin_types rhint = rhs.hint();

	assert( lhint != builtin_types::none );
	assert( rhint != builtin_types::none );

	value_t lv = lhs;
	value_t rv = rhs;

	if( lhint != rhint )
	{
		if( is_scalar(lhint) ) { lv = extend_to_vm( lhs, rhint ); }
		else if( is_scalar(rhint) ) { rv = extend_to_vm( rhs, lhint ); }
		else { assert(false); }
	}

	bin_fn_t f_mul = boost::bind( &DefaultIRBuilder::CreateFMul, builder(), _1, _2, "", (llvm::MDNode*)(NULL) );
	bin_fn_t i_mul = boost::bind( &DefaultIRBuilder::CreateMul,  builder(), _1, _2, "", false, false );
	return emit_bin_ps( lv, rv, i_mul, i_mul, f_mul );
}

bool xor(bool l, bool r)
{
	return (l && !r) || (!l && r);
}

value_t cg_service::emit_add( value_t const& lhs, value_t const& rhs )
{
	bin_fn_t f_add = boost::bind( &DefaultIRBuilder::CreateFAdd, builder(), _1, _2, "", (llvm::MDNode*)(NULL) );
	bin_fn_t i_add = boost::bind( &DefaultIRBuilder::CreateAdd,  builder(), _1, _2, "", false, false );

	return extend_scalar_and_emit_bin_ps( lhs, rhs, i_add, i_add, f_add );
}

value_t cg_service::emit_sub( value_t const& lhs, value_t const& rhs )
{
	bin_fn_t f_sub = boost::bind( &DefaultIRBuilder::CreateFSub, builder(), _1, _2, "", (llvm::MDNode*)(NULL) );
	bin_fn_t i_sub = boost::bind( &DefaultIRBuilder::CreateSub,  builder(), _1, _2, "", false, false );
	
	return extend_scalar_and_emit_bin_ps( lhs, rhs, i_sub, i_sub, f_sub );
}

value_t cg_service::emit_mul_intrin( value_t const& lhs, value_t const& rhs )
{
	builtin_types lhint = lhs.hint();
	builtin_types rhint = rhs.hint();

	if( is_scalar(lhint) ){
		if( is_scalar(rhint) ){
			return emit_mul_comp( lhs, rhs );
		} else if ( is_vector(rhint) ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		} else if ( is_matrix(rhint) ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	} else if ( is_vector(lhint) ){
		if( is_scalar(rhint) ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		} else if ( is_vector(rhint) ){
			return emit_dot_vv( lhs, rhs );
		} else if ( is_matrix(rhint) ){
			return emit_mul_vm( lhs, rhs );
		}
	} else if ( is_matrix(lhint) ){
		if( is_scalar(rhint) ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		} else if( is_vector(rhint) ){
			return emit_mul_mv( lhs, rhs );
		} else if( is_matrix(rhint) ){
			return emit_mul_mm( lhs, rhs );
		}
	}

	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cg_service::emit_div( value_t const& lhs, value_t const& rhs )
{
	bin_fn_t f_div = boost::bind( &DefaultIRBuilder::CreateFDiv, builder(), _1, _2, "", (llvm::MDNode*)(NULL) );
	bin_fn_t i_div = boost::bind( &DefaultIRBuilder::CreateSDiv, builder(), _1, _2, "", false );
	bin_fn_t u_div = boost::bind( &DefaultIRBuilder::CreateUDiv, builder(), _1, _2, "", false );
	bin_fn_t i_safe_div = boost::bind( &cg_service::safe_div_mod_, this, _1, _2, i_div );
	bin_fn_t u_safe_div = boost::bind( &cg_service::safe_div_mod_, this, _1, _2, u_div );

	return extend_scalar_and_emit_bin_ps( lhs, rhs, i_safe_div, u_safe_div, f_div );
}

value_t cg_service::emit_mod( value_t const& lhs, value_t const& rhs )
{	
	bin_fn_t i_mod = boost::bind( &DefaultIRBuilder::CreateSRem, builder(), _1, _2, "" );
	bin_fn_t u_mod = boost::bind( &DefaultIRBuilder::CreateURem, builder(), _1, _2, "" );
		
	bin_fn_t i_safe_mod = boost::bind( &cg_service::safe_div_mod_, this, _1, _2, i_mod );
	bin_fn_t u_safe_mod = boost::bind( &cg_service::safe_div_mod_, this, _1, _2, u_mod );

	bin_fn_t intrin_mod_f32 = bind_binary_external_( external_intrins[mod_f32] );
	bin_fn_t f_mod = boost::bind( &cg_service::bin_op_ps_, this, (Type*)NULL, _1, _2, intrin_mod_f32, bin_fn_t(), bin_fn_t(), bin_fn_t(), unary_fn_t() );

	return extend_scalar_and_emit_bin_ps( lhs, rhs, i_safe_mod, u_safe_mod, f_mod );
}

value_t cg_service::emit_lshift( value_t const& lhs, value_t const& rhs )
{
	bin_fn_t shl = boost::bind( &DefaultIRBuilder::CreateBinOp, builder(), Instruction::Shl, _1, _2, "" );
	return extend_scalar_and_emit_bin_ps( lhs, rhs, shl, shl, shl );
}

value_t cg_service::emit_rshift( value_t const& lhs, value_t const& rhs )
{
	bin_fn_t shr = boost::bind( &DefaultIRBuilder::CreateBinOp, builder(), Instruction::LShr, _1, _2, "" );
	return extend_scalar_and_emit_bin_ps( lhs, rhs, shr, shr, shr );
}

value_t cg_service::emit_bit_and( value_t const& lhs, value_t const& rhs )
{
	bin_fn_t bit_and = boost::bind( &DefaultIRBuilder::CreateBinOp, builder(), Instruction::And, _1, _2, "" );
	return extend_scalar_and_emit_bin_ps( lhs, rhs, bit_and, bit_and, bit_and );
}

value_t cg_service::emit_bit_or( value_t const& lhs, value_t const& rhs )
{
	bin_fn_t bit_or = boost::bind( &DefaultIRBuilder::CreateBinOp, builder(), Instruction::Or, _1, _2, "" );
	return extend_scalar_and_emit_bin_ps( lhs, rhs, bit_or, bit_or, bit_or );
}

value_t cg_service::emit_bit_xor( value_t const& lhs, value_t const& rhs )
{
	bin_fn_t bit_xor = boost::bind( &DefaultIRBuilder::CreateBinOp, builder(), Instruction::Xor, _1, _2, "" );
	return extend_scalar_and_emit_bin_ps( lhs, rhs, bit_xor, bit_xor, bit_xor );
}

value_t cg_service::emit_dot( value_t const& lhs, value_t const& rhs )
{
	return emit_dot_vv(lhs, rhs);
}

value_t cg_service::emit_cross( value_t const& lhs, value_t const& rhs )
{
	assert( lhs.hint() == vector_of( builtin_types::_float, 3 ) );
	assert( rhs.hint() == lhs.hint() );

	uint32_t swz_va = indexes_to_mask( 1, 2, 0, -1 );
	uint32_t swz_vb = indexes_to_mask( 2, 0, 1, -1 );

	value_t lvec_a = emit_extract_elem_mask( lhs, swz_va );
	value_t lvec_b = emit_extract_elem_mask( lhs, swz_vb );
	value_t rvec_a = emit_extract_elem_mask( rhs, swz_va );
	value_t rvec_b = emit_extract_elem_mask( rhs, swz_vb );

	return emit_sub( emit_mul_comp(lvec_a, rvec_b), emit_mul_comp(lvec_b, rvec_a) );
}

value_t cg_service::emit_extract_ref( value_t const& lhs, int idx )
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

value_t cg_service::emit_extract_ref( value_t const& /*lhs*/, value_t const& /*idx*/ )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cg_service::emit_extract_val( value_t const& lhs, int idx )
{
	builtin_types agg_hint = lhs.hint();

	Value* val = lhs.load();
	Value* elem_val = NULL;
	abis abi = abi_unknown;

	builtin_types elem_hint = builtin_types::none;
	value_tyinfo* elem_tyi = NULL;

	if( agg_hint == builtin_types::none ){
		elem_val = builder().CreateExtractValue(val, static_cast<unsigned>(idx));
		abi = lhs.abi();
		elem_tyi = member_tyinfo( lhs.tyinfo(), (size_t)idx );
	} else if( is_scalar(agg_hint) ){
		assert( idx == 0 );
		elem_val = val;
		elem_hint = agg_hint;
	} else if( is_vector(agg_hint) ){
		switch( lhs.abi() ){
		case abi_c:
			elem_val = builder().CreateExtractValue(val, static_cast<unsigned>(idx));
			break;
		case abi_llvm:
			elem_val = builder().CreateExtractElement(val, int_(idx) );
			break;
		case abi_vectorize:
			EFLIB_ASSERT_UNIMPLEMENTED();
			break;
		case abi_package:
			{
				char indexes[4] = { char(idx), -1, -1, -1};
				elem_val = emit_extract_elem_mask( lhs, indexes_to_mask(indexes) ).load();
				break;
			}
		default:
			assert(!"Unknown ABI");
			break;
		}
		abi = promote_abi( abi_llvm, lhs.abi() );
		elem_hint = scalar_of(agg_hint);
	} else if( is_matrix(agg_hint) ){
		assert( promote_abi(lhs.abi(), abi_llvm) == abi_llvm );
		elem_val = builder().CreateExtractValue(val, static_cast<unsigned>(idx));
		abi = lhs.abi();
		elem_hint = vector_of( scalar_of(agg_hint), vector_size(agg_hint) );
	}

	// assert( elem_tyi || elem_hint != builtin_types::none );

	return create_value( elem_tyi, elem_hint, elem_val, vkind_value, abi );
}

value_t cg_service::emit_extract_val( value_t const& /*lhs*/, value_t const& /*idx*/ )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cg_service::emit_extract_elem_mask( value_t const& vec, uint32_t mask )
{
	char indexes[4] = {-1, -1, -1, -1};
	mask_to_indexes( indexes, mask );
	uint32_t idx_len = indexes_length(indexes);

	assert( idx_len > 0 );
	if( vec.hint() == builtin_types::none && idx_len == 1 ){
		// Struct, array or not-package, return extract elem.
		// Else do extract mask.
		if( vec.abi() != abi_package || vec.hint() == builtin_types::none ){
			return emit_extract_elem( vec, indexes[0] );
		}
	}
	// Get the hint.
	assert( vec.hint() != builtin_types::none );

	builtin_types swz_hint = scalar_of( vec.hint() );
	if( is_vector(vec.hint()) ){
		swz_hint = vector_of( scalar_of(vec.hint()), idx_len );
	} else if ( is_matrix(vec.hint()) ){
		EFLIB_ASSERT_UNIMPLEMENTED();
	} else {
		assert(false);
	}

	if( vec.storable() ){
		value_t swz_proxy = create_value( NULL, swz_hint, NULL, vkind_swizzle, vec.abi() );
		swz_proxy.parent( vec );
		swz_proxy.masks( mask );
		return swz_proxy;
	} else {
		if( is_scalar( vec.hint() ) ) {
			EFLIB_ASSERT_UNIMPLEMENTED();
		} else if( is_vector( vec.hint() ) ) {
			Value* vec_v = vec.load( promote_abi(abi_llvm, vec.abi()) );
			switch( vec.abi() ){
			case abi_c:
			case abi_llvm:
				{
					Value* v = builder().CreateShuffleVector( vec_v, vec_v, vector_<int>(indexes, idx_len) );
					return create_value( NULL, swz_hint, v, vkind_value, abi_llvm );
				}
			case abi_vectorize:
				{
					vector<char> vectorize_idx( SIMD_ELEMENT_COUNT(), -1 );
					assert( idx_len < static_cast<uint32_t>(SIMD_ELEMENT_COUNT()) );
					copy( &indexes[0], &indexes[idx_len], vectorize_idx.begin() );
					fill( vectorize_idx.begin() + idx_len, vectorize_idx.end(), vector_size(vec.hint()) );

					Value* v = builder().CreateShuffleVector(
						vec_v, UndefValue::get(vec_v->getType()),
						vector_<int>( &(vectorize_idx[0]), vectorize_idx.size() )
						);
					return create_value( NULL, swz_hint, v, vkind_value, abi_vectorize );
				}
			case abi_package:
				{
					int src_element_pitch = ceil_to_pow2( static_cast<int>(vector_size(vec.hint())) );
					int swz_element_pitch = ceil_to_pow2( static_cast<int>(idx_len) );
						
					int src_scalar_count = src_element_pitch * PACKAGE_ELEMENT_COUNT;

					vector<char> indexes_per_value( src_scalar_count, src_scalar_count );
					copy( &indexes[0], &indexes[idx_len], indexes_per_value.begin() );

					vector<char> package_idx( PACKAGE_ELEMENT_COUNT * swz_element_pitch, -1 );
					assert( (int)idx_len <= SIMD_ELEMENT_COUNT() );

					for ( size_t i = 0; i < PACKAGE_ELEMENT_COUNT; ++i ){
						for( size_t j = 0; j < (size_t)swz_element_pitch; ++j ){
							package_idx[i*swz_element_pitch+j] = (char)(indexes_per_value[j]+i*src_element_pitch);
						}
					}

					Value* v = builder().CreateShuffleVector(
						vec_v, UndefValue::get(vec_v->getType()),
						vector_<int>( &(package_idx[0]), package_idx.size() )
						);
					return create_value( NULL, swz_hint, v, vkind_value, abi_package );
				}
			default:
				assert(false);
			}
		} else if( is_matrix(vec.hint()) ){
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	}

	return value_t();
}

value_t cg_service::emit_extract_col( value_t const& lhs, size_t index )
{
	assert( promote_abi(lhs.abi(), abi_llvm) == abi_llvm );

	value_t val = lhs.to_rvalue();
	builtin_types mat_hint( lhs.hint() );
	assert( is_matrix(mat_hint) );

	size_t row_count = vector_count( mat_hint );

	builtin_types out_hint = vector_of( scalar_of(mat_hint), row_count );

	value_t out_value = null_value( out_hint, lhs.abi() );
	for( size_t irow = 0; irow < row_count; ++irow ){
		value_t row = emit_extract_val( val, (int)irow );
		value_t cell = emit_extract_val( row, (int)index );
		out_value = emit_insert_val( out_value, (int)irow, cell );
	}

	return out_value;
}

value_t cg_service::emit_dot_vv( value_t const& lhs, value_t const& rhs )
{
	abis promoted_abi = promote_abi(lhs.abi(), rhs.abi(), abi_llvm);
	// assert( promoted_abi == abi_llvm );
	
	size_t vec_size = vector_size( lhs.hint() );
	value_t total = null_value( scalar_of( lhs.hint() ), promoted_abi );
	value_t prod = emit_mul_comp( lhs, rhs );
	for( size_t i = 0; i < vec_size; ++i ){
		value_t prod_elem = emit_extract_elem( prod, i );
		total.emplace( emit_add( total, prod_elem ).to_rvalue() );
	}

	return total;
}

value_t cg_service::emit_mul_mv( value_t const& lhs, value_t const& rhs )
{
	assert( promote_abi(lhs.abi(), rhs.abi(), abi_llvm) == abi_llvm );

	builtin_types mhint = lhs.hint();
	builtin_types vhint = rhs.hint();

	size_t row_count = vector_count(mhint);

	builtin_types ret_hint = vector_of( scalar_of(vhint), row_count );

	value_t ret_v = null_value( ret_hint, lhs.abi() );
	for( size_t irow = 0; irow < row_count; ++irow ){
		value_t row_vec = emit_extract_val( lhs, irow );
		ret_v = emit_insert_val( ret_v, irow, emit_dot_vv(row_vec, rhs) );
	}

	return ret_v;
}

value_t cg_service::emit_mul_vm( value_t const& lhs, value_t const& rhs )
{
	assert( promote_abi(lhs.abi(), rhs.abi(), abi_llvm) == abi_llvm );

	size_t out_v = vector_size( rhs.hint() );

	value_t lrv = lhs.to_rvalue();
	value_t rrv = rhs.to_rvalue();

	value_t ret = null_value( vector_of( scalar_of(lhs.hint()), out_v ), lhs.abi() );
	for( size_t idx = 0; idx < out_v; ++idx ){
		ret = emit_insert_val( ret, (int)idx, emit_dot_vv( lrv, emit_extract_col(rrv, idx) ) );
	}

	return ret;
}

value_t cg_service::emit_mul_mm( value_t const& lhs, value_t const& rhs )
{
	assert( promote_abi(lhs.abi(), rhs.abi(), abi_llvm) == abi_llvm );

	builtin_types lhint = lhs.hint();
	builtin_types rhint = rhs.hint();

	size_t out_v = vector_size( lhint );
	size_t out_r = vector_count( rhint );

	assert( vector_count(lhint) == vector_size(rhint) );

	builtin_types out_row_hint = vector_of( scalar_of(lhint), out_v );
	builtin_types out_hint = matrix_of( scalar_of(lhint), out_v, out_r );
	abis out_abi = lhs.abi();

	vector<value_t> out_cells(out_v*out_r);
	out_cells.resize( out_v*out_r );

	// Calculate matrix cells.
	for( size_t icol = 0; icol < out_v; ++icol){
		value_t col = emit_extract_col( rhs, icol );
		for( size_t irow = 0; irow < out_r; ++irow )
		{
			value_t row = emit_extract_col( rhs, icol );
			out_cells[irow*out_v+icol] = emit_dot_vv( col, row );
		}
	}

	// Compose cells to matrix
	value_t ret_value = null_value( out_hint, out_abi );
	for( size_t irow = 0; irow < out_r; ++irow ){
		value_t row_vec = null_value( out_row_hint, out_abi );
		for( size_t icol = 0; icol < out_v; ++icol ){
			row_vec = emit_insert_val( row_vec, (int)icol, out_cells[irow*out_v+icol] );
		}
		ret_value = emit_insert_val( ret_value, (int)irow, row_vec );
	}

	return ret_value;
}

Value* cg_service::shrink_( Value* vec, size_t vsize ){
	return extract_elements_( vec, 0, vsize );
}

Value* cg_service::extract_elements_( Value* src, size_t start_pos, size_t length ){
	VectorType* vty = cast<VectorType>(src->getType());
	uint32_t elem_count = vty->getNumElements();
	if( start_pos == 0 && length == elem_count ){
		return src;
	}

	vector<int> indexes;
	indexes.resize( length, 0 );
	for ( size_t i_elem = 0; i_elem < length; ++i_elem ){
		indexes[i_elem] = i_elem + start_pos;
	}
	Value* mask = vector_( &indexes[0], indexes.size() );
	return builder().CreateShuffleVector( src, UndefValue::get( src->getType() ), mask );
}

Value* cg_service::insert_elements_( Value* dst, Value* src, size_t start_pos ){
	if( src->getType() == dst->getType() ){
		return src;
	}

	VectorType* src_ty = cast<VectorType>(src->getType());
	uint32_t count = src_ty->getNumElements();

	// Expand source to dest size
	Value* ret = dst;
	for( size_t i_elem = 0; i_elem < count; ++i_elem ){
		Value* src_elem = builder().CreateExtractElement( src, int_<int>(i_elem) );
		ret = builder().CreateInsertElement( ret, src_elem, int_<int>(i_elem+start_pos) );
	}
	
	return ret;
}

value_t cg_service::emit_abs( value_t const& arg_value )
{
	builtin_types hint = arg_value.hint();
	builtin_types scalar_hint = scalar_of( arg_value.hint() );
	abis arg_abi = arg_value.abi();

	if ( scalar_hint == builtin_types::_double ){
		EFLIB_ASSERT_UNIMPLEMENTED();
	}
	if( arg_abi == abi_c || arg_abi == abi_llvm )
	{
		if( is_scalar(hint) || is_vector(hint) )
		{
			if( prefer_externals() ) {
				EFLIB_ASSERT_UNIMPLEMENTED();
			} else {
				Value* ret = NULL;
				uint64_t mask = (1ULL << ( (storage_size(scalar_hint)*8)-1 )) - 1;

				if( is_real(scalar_hint) )
				{
					Value* i = builder().CreateBitCast( arg_value.load(), type_( builtin_types::_sint32, arg_abi ) );
					i = builder().CreateAnd(i, mask);
					ret = builder().CreateBitCast( i, type_( builtin_types::_float, arg_abi ) );
				}
				else if( is_integer(scalar_hint) )
				{
					assert( is_signed(hint) );
					Value* v = arg_value.load();
					Value* sign = builder().CreateICmpSGT( v, Constant::getNullValue( v->getType() ) );
					Value* neg = builder().CreateNeg( v );
					ret = builder().CreateSelect(sign, v, neg);
				}
				else
				{
					EFLIB_ASSERT_UNIMPLEMENTED();
				}

				return create_value( arg_value.tyinfo(), hint, ret, vkind_value, arg_abi );
			}
		} 
		else 
		{
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	}

	return value_t();
}

value_t cg_service::emit_sqrt( value_t const& arg_value )
{
	builtin_types hint = arg_value.hint();
	builtin_types scalar_hint = scalar_of( arg_value.hint() );
	abis arg_abi = arg_value.abi();

	Value* v = arg_value.load(arg_abi);

	if( scalar_hint == builtin_types::_float )
	{
		Value* ret_v = unary_op_ps_(
			NULL,
			v,
			bind_unary_call_( intrin_<float(float)>(Intrinsic::sqrt) ),
			unary_fn_t(),
			bind_unary_call_( intrin_(Intrinsic::x86_sse_sqrt_ps) ),
			unary_fn_t()
			);

		return create_value( arg_value.tyinfo(), arg_value.hint(), ret_v, vkind_value, arg_abi );
	}
	else
	{
		EFLIB_ASSERT_UNIMPLEMENTED();
		return value_t();
	}
}

value_t cg_service::emit_exp( value_t const& arg_value )
{
	builtin_types hint = arg_value.hint();
	builtin_types scalar_hint = scalar_of( arg_value.hint() );
	abis arg_abi = arg_value.abi();

	Value* v = arg_value.load(arg_abi);

	if( scalar_hint == builtin_types::_float )
	{
		Value* ret_v = unary_op_ps_(
			NULL,
			v,
			bind_unary_external_( external_intrins[exp_f32] ), // bind_unary_call_( intrin_<float(float)>(Intrinsic::exp) ),
			unary_fn_t(),
			unary_fn_t(),
			unary_fn_t()
			);

		return create_value( arg_value.tyinfo(), arg_value.hint(), ret_v, vkind_value, arg_abi );
	}
	else
	{
		EFLIB_ASSERT_UNIMPLEMENTED();
		return value_t();
	}
}

value_t cg_service::undef_value( builtin_types bt, abis abi )
{
	assert( bt != builtin_types::none );
	Type* valty = type_( bt, abi );
	value_t val = create_value( bt, UndefValue::get(valty), vkind_value, abi );
	return val;
}


value_t cg_service::emit_call( function_t const& fn, vector<value_t> const& args )
{
	return emit_call( fn, args, value_t() );
}

value_t cg_service::emit_call( function_t const& fn, vector<value_t> const& args, value_t const& exec_mask )
{
	abis promoted_abi = abi_llvm;
	BOOST_FOREACH( value_t const& arg, args )
	{
		promoted_abi = promote_abi( arg.abi(), promoted_abi );
	}

	vector<Value*> arg_values;
	value_t var;

	if ( fn.first_arg_is_return_address() ){
		var = create_variable( fn.get_return_ty().get(), fn.abi(), ".tmp" );
		arg_values.push_back( var.load_ref() );
	}

	abis arg_abi = fn.c_compatible ? abi_c : promoted_abi;
	if( arg_abi == abi_package && fn.partial_execution ){
		if( exec_mask.abi() == abi_unknown ){
			arg_values.push_back( packed_mask().load( abi_llvm ) );
		} else {
			arg_values.push_back( exec_mask.load( abi_llvm ) );
		}
	}

	if( fn.c_compatible || fn.external ){
		BOOST_FOREACH( value_t const& arg, args ){
			builtin_types hint = arg.hint();
			if( ( is_scalar(hint) && (arg_abi == abi_c || arg_abi == abi_llvm) ) || is_sampler(hint) ){
				arg_values.push_back( arg.load( arg_abi ) );
			} else {
				Value* ref_arg = load_ref( arg, arg_abi );
				if( !ref_arg ){
					Value* arg_val = load( arg, arg_abi );
					ref_arg = builder().CreateAlloca( arg_val->getType() );
					builder().CreateStore( arg_val, ref_arg );
				}
				arg_values.push_back( ref_arg );
			}
		}
	} else {
		BOOST_FOREACH( value_t const& arg, args ){
			arg_values.push_back( arg.load( promoted_abi ) );
		}
	}

	Value* ret_val = builder().CreateCall( fn.fn, arg_values );

	if( fn.first_arg_is_return_address() ){
		return var;
	}

	abis ret_abi = fn.c_compatible ? abi_c : promoted_abi;
	return create_value( fn.get_return_ty().get(), ret_val, vkind_value, ret_abi );
}

value_t cg_service::cast_s2v( value_t const& v )
{
	builtin_types hint = v.hint();
	assert( is_scalar(hint) );
	builtin_types vhint = vector_of(hint, 1);

	// vector1 and scalar are same LLVM vector type when abi is Vectorize and Package 
	if( v.abi() == abi_vectorize || v.abi() == abi_package )
	{
		return create_value( NULL, vhint, v.load(), vkind_value, v.abi() );
	}

	// Otherwise return a new vector
	value_t ret = null_value( vhint, v.abi() );
	return emit_insert_val( ret, 0, v );
}

value_t cg_service::cast_v2s( value_t const& v )
{
	assert( is_vector(v.hint()) );

	// vector1 and scalar are same LLVM vector type when abi is Vectorize and Package 
	if( v.abi() == abi_vectorize || v.abi() == abi_package )
	{
		return create_value( NULL, scalar_of(v.hint()), v.load(), vkind_value, v.abi() );
	}
	return emit_extract_val( v, 0 );
}

value_t cg_service::cast_bits( value_t const& v, value_tyinfo* dest_tyi )
{
	abis abi = promote_abi(v.abi(), abi_llvm);

	Type* ty = dest_tyi->ty(abi);
	builtin_types dest_scalar_hint = scalar_of( dest_tyi->hint() );
	Type* dest_scalar_ty = type_( dest_scalar_hint, abi_llvm );
	unary_fn_t sv_cast_fn = boost::bind( &cg_service::bit_cast_, this, _1, dest_scalar_ty );
	Value* ret = unary_op_ps_( ty, v.load(abi), unary_fn_t(), unary_fn_t(), unary_fn_t(), sv_cast_fn );
	return create_value( dest_tyi, ret, vkind_value, abi );
}

void cg_service::jump_to( insert_point_t const& ip )
{
	assert( ip );
	if( !insert_point().block->getTerminator() ){
		builder().CreateBr( ip.block );
	}
}

void cg_service::jump_cond( value_t const& cond_v, insert_point_t const & true_ip, insert_point_t const& false_ip )
{
	Value* cond = cond_v.load_i1();
	builder().CreateCondBr( cond, true_ip.block, false_ip.block );
}

void cg_service::merge_swizzle( value_t const*& root, char indexes[], value_t const& v )
{
	root = &v;
	vector<uint32_t> masks;
	while( root->masks() != 0 ){
		masks.push_back(root->masks());
		root = root->parent();
	}

	for( vector<uint32_t>::reverse_iterator it = masks.rbegin();
		it != masks.rend(); ++it )
	{
		if( it == masks.rbegin() ){
			mask_to_indexes(indexes, *it);
			continue;
		}

		char current_indexes[]	= {-1, -1, -1, -1};
		char tmp_indexes[]		= {-1, -1, -1, -1};

		mask_to_indexes(current_indexes, *it);

		for(int i = 0; i < 4; ++i)
		{
			if( current_indexes[i] == -1 ) break;
			tmp_indexes[i] = indexes[ current_indexes[i] ];
		}

		std::copy(tmp_indexes, tmp_indexes+4, indexes);
	}
}

value_t cg_service::create_value_by_scalar( value_t const& scalar, value_tyinfo* tyinfo, builtin_types hint )
{
	builtin_types src_hint = scalar.hint();
	assert( is_scalar(src_hint) );
	builtin_types dst_hint = tyinfo ? tyinfo->hint() : hint;
	builtin_types dst_scalar = scalar_of(dst_hint);
	assert( dst_scalar == src_hint );

	if( is_scalar(dst_hint) )
	{
		return scalar;
	}
	else if( is_vector(dst_hint) )
	{
		size_t vsize = vector_size(dst_hint);
		vector<value_t> scalars;
		scalars.insert(scalars.end(), vsize, scalar);
		return create_vector(scalars, scalar.abi());
	}
	else
	{
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	return value_t();
}

value_t cg_service::emit_any( value_t const& v )
{
	builtin_types hint = v.hint();
	builtin_types scalar_hint = scalar_of(v.hint());
	if( is_scalar(hint) )
	{
		return emit_cmp_ne( v, null_value(hint, v.abi()) );
	}
	else if( is_vector(hint) )
	{
		value_t elem_v = emit_extract_val(v, 0);
		value_t ret = emit_cmp_ne( elem_v, null_value(scalar_hint, v.abi()) );
		for( size_t i = 1; i < vector_size(hint); ++i )
		{
			elem_v = emit_extract_val(v, i);
			ret = emit_or( ret, emit_cmp_ne( elem_v, null_value(scalar_hint, v.abi()) ) );
		}
		return ret;
	}
	else if( is_matrix(hint) )
	{
		EFLIB_ASSERT_UNIMPLEMENTED();	
	}
	else
	{
		assert(false);
	}
	return value_t();
}

value_t cg_service::emit_all( value_t const& v )
{
	builtin_types hint = v.hint();
	builtin_types scalar_hint = scalar_of(v.hint());
	if( is_scalar(hint) )
	{
		return emit_cmp_ne( v, null_value(hint, v.abi()) );
	}
	else if( is_vector(hint) )
	{
		value_t elem_v = emit_extract_val(v, 0);
		value_t ret = emit_cmp_ne( elem_v, null_value(scalar_hint, v.abi()) );
		for( size_t i = 1; i < vector_size(hint); ++i )
		{
			elem_v = emit_extract_val(v, i);
			ret = emit_and( ret, emit_cmp_ne( elem_v, null_value(scalar_hint, v.abi()) ) );
		}
		return ret;
	}
	else if( is_matrix(hint) )
	{
		EFLIB_ASSERT_UNIMPLEMENTED();	
	}
	else
	{
		assert(false);
	}
	return value_t();
}

Value* cg_service::i8toi1_( Value* v )
{
	Type* ty = IntegerType::get( v->getContext(), 1 );
	if( v->getType()->isVectorTy() )
	{
		ty = VectorType::get( ty, llvm::cast<VectorType>(v->getType())->getNumElements() );
	}

	return builder().CreateTruncOrBitCast(v, ty);
}

Value* cg_service::i1toi8_( Value* v )
{
	Type* ty = IntegerType::get( v->getContext(), 8 );
	if( v->getType()->isVectorTy() )
	{
		ty = VectorType::get( ty, llvm::cast<VectorType>(v->getType())->getNumElements() );
	}

	return builder().CreateZExtOrBitCast(v, ty);
}

value_t cg_service::emit_cmp( value_t const& lhs, value_t const& rhs, uint32_t pred_signed, uint32_t pred_unsigned, uint32_t pred_float )
{
	builtin_types hint = lhs.hint();
	builtin_types scalar_hint = scalar_of(hint);
	builtin_types ret_hint = replace_scalar(hint, builtin_types::_boolean);

	assert( hint == rhs.hint() );
	assert( is_scalar(scalar_hint) );

	abis promoted_abi = promote_abi(lhs.abi(), rhs.abi(), abi_llvm);

	Value* lhs_v = lhs.load(promoted_abi);
	Value* rhs_v = rhs.load(promoted_abi);
	Type* ret_ty = type_(ret_hint, promoted_abi);

	bin_fn_t cmp_fn;
	if( is_real(scalar_hint) )
	{
		cmp_fn = boost::bind( &DefaultIRBuilder::CreateFCmp, builder(), (CmpInst::Predicate)pred_float, _1, _2, "" );
	}
	else if ( is_integer(scalar_hint) )
	{
		int pred = is_signed(scalar_hint) ? pred_signed : pred_unsigned;
		cmp_fn = boost::bind( &DefaultIRBuilder::CreateICmp, builder(), (CmpInst::Predicate)pred, _1, _2, "" );
	}

	unary_fn_t cast_fn = boost::bind( &cg_service::i1toi8_, this, _1 );
	Value* ret = bin_op_ps_( ret_ty, lhs_v, rhs_v, bin_fn_t(), bin_fn_t(), bin_fn_t(), cmp_fn, cast_fn );

	return create_value( ret_hint, ret, vkind_value, promoted_abi );
}

value_t cg_service::emit_cmp_eq( value_t const& lhs, value_t const& rhs )
{
	return emit_cmp( lhs, rhs, CmpInst::ICMP_EQ, CmpInst::ICMP_EQ, CmpInst::FCMP_OEQ );
}

value_t cg_service::emit_cmp_lt( value_t const& lhs, value_t const& rhs )
{
	return emit_cmp( lhs, rhs, CmpInst::ICMP_SLT, CmpInst::ICMP_ULT, CmpInst::FCMP_ULT );
}

value_t cg_service::emit_cmp_le( value_t const& lhs, value_t const& rhs )
{
	return emit_cmp( lhs, rhs, CmpInst::ICMP_SLE, CmpInst::ICMP_ULE, CmpInst::FCMP_ULE );
}

value_t cg_service::emit_cmp_ne( value_t const& lhs, value_t const& rhs )
{
	return emit_cmp( lhs, rhs, CmpInst::ICMP_NE, CmpInst::ICMP_NE, CmpInst::FCMP_UNE );
}

value_t cg_service::emit_cmp_ge( value_t const& lhs, value_t const& rhs )
{
	return emit_cmp( lhs, rhs, CmpInst::ICMP_SGE, CmpInst::ICMP_UGE, CmpInst::FCMP_UGE );
}

value_t cg_service::emit_cmp_gt( value_t const& lhs, value_t const& rhs )
{
	return emit_cmp( lhs, rhs, CmpInst::ICMP_SGT, CmpInst::ICMP_UGT, CmpInst::FCMP_UGT );
}

value_t cg_service::emit_bin_ps( value_t const& lhs, value_t const& rhs, bin_fn_t signed_fn, bin_fn_t unsigned_fn, bin_fn_t float_fn )
{
	builtin_types hint( lhs.hint() );
	assert( hint == rhs.hint() );

	Value* ret = NULL;

	builtin_types scalar_hint = is_scalar(hint) ? hint : scalar_of(hint);
	abis promoted_abi = promote_abi( rhs.abi(), lhs.abi() );
	abis internal_abi = promote_abi( promoted_abi, abi_llvm );

	Value* lhs_v = lhs.load(internal_abi);
	Value* rhs_v = rhs.load(internal_abi);
	
	Type* ret_ty = lhs_v->getType();

	if( is_real(scalar_hint) )
	{
		ret = bin_op_ps_( ret_ty, lhs_v, rhs_v, bin_fn_t(), bin_fn_t(), bin_fn_t(), float_fn, unary_fn_t() );
	}
	else if( is_integer(scalar_hint) ) 
	{
		if( is_signed(scalar_hint) )
		{
			ret = bin_op_ps_( ret_ty, lhs_v, rhs_v, bin_fn_t(), bin_fn_t(), bin_fn_t(), signed_fn, unary_fn_t() );
		}
		else
		{
			ret = bin_op_ps_( ret_ty, lhs_v, rhs_v, bin_fn_t(), bin_fn_t(), bin_fn_t(), unsigned_fn, unary_fn_t() );
		}
	}
	else if( scalar_hint == builtin_types::_boolean )
	{
		ret = bin_op_ps_( ret_ty, lhs_v, rhs_v, bin_fn_t(), bin_fn_t(), bin_fn_t(), unsigned_fn, unary_fn_t() );
	}
	else
	{
		assert(false);
	}

	value_t retval = create_value( hint, ret, vkind_value, internal_abi );
	abis ret_abi = is_scalar(hint) ? internal_abi : promoted_abi;
	return create_value( hint, retval.load(ret_abi), vkind_value, ret_abi );
}

Value* cg_service::bin_op_ps_( Type* ret_ty, Value* lhs, llvm::Value* rhs, bin_fn_t sfn, bin_fn_t vfn, bin_fn_t simd_fn, bin_fn_t sv_fn, unary_fn_t cast_fn /*Must be sv fn*/ )
{
	Type* ty = lhs->getType();
	if( !ret_ty ) ret_ty = ty;

	if( !ty->isAggregateType() && !ty->isVectorTy() )
	{
		Value* ret_v = NULL;
		if( sv_fn ) {
			ret_v = sv_fn(lhs, rhs);
		} else {
			assert(sfn);
			ret_v = sfn(lhs, rhs);
		}
		if( cast_fn ){ ret_v = cast_fn(ret_v); }
		return ret_v;
	}

	if( ty->isVectorTy() )
	{
		Value* ret_v = NULL;

		if( sv_fn ) {
			ret_v = sv_fn(lhs, rhs);
		} else if( vfn ) {
			ret_v = vfn(lhs, rhs);
		}

		if( ret_v ) { return cast_fn ? cast_fn(ret_v) : ret_v; }

		unsigned elem_count = ty->getVectorNumElements();

		// SIMD
		if( simd_fn && elem_count % SIMD_ELEMENT_COUNT() == 0 )
		{
			int batch_count = elem_count / SIMD_ELEMENT_COUNT();

			ret_v = UndefValue::get(ret_ty);
			for( int i_batch = 0; i_batch < batch_count; ++i_batch ){
				Value* lhs_simd_elem = extract_elements_( lhs, i_batch*SIMD_ELEMENT_COUNT(), SIMD_ELEMENT_COUNT() );
				Value* rhs_simd_elem = extract_elements_( rhs, i_batch*SIMD_ELEMENT_COUNT(), SIMD_ELEMENT_COUNT() );
				Value* ret_simd_elem = simd_fn( lhs_simd_elem, rhs_simd_elem );
				if( cast_fn ) { ret_simd_elem = cast_fn(ret_simd_elem); }
				ret_v = insert_elements_( ret_v, ret_simd_elem, i_batch*SIMD_ELEMENT_COUNT() );
			}

			return ret_v;
		}

		// Scalar
		assert( sfn );
		ret_v = UndefValue::get( ret_ty );
		for( unsigned i = 0; i < elem_count; ++i )
		{
			Value* lhs_elem = builder().CreateExtractElement( lhs, int_(i) );
			Value* rhs_elem = builder().CreateExtractElement( rhs, int_(i) );
			Value* ret_elem = sfn(lhs_elem, rhs_elem);

			if( cast_fn ) { ret_elem = cast_fn(ret_elem); }

			ret_v = builder().CreateInsertElement( ret_v, ret_elem, int_(i) );
		}

		return ret_v;
	}

	if( ty->isStructTy() )
	{
		Value* ret = UndefValue::get(ret_ty);
		size_t elem_count = ty->getStructNumElements();
		unsigned int elem_index[1] = {0};
		for( unsigned int i = 0;i < elem_count; ++i )
		{
			elem_index[0] = i;
			Value* lhs_elem = builder().CreateExtractValue(lhs, elem_index);
			Value* rhs_elem = builder().CreateExtractValue(rhs, elem_index);
			Type* ret_elem_ty = ret_ty->getStructElementType(i);
			Value* ret_elem = bin_op_ps_(ret_elem_ty, lhs_elem, rhs_elem, sfn, vfn, simd_fn, sv_fn, cast_fn);
			ret = builder().CreateInsertValue( ret, ret_elem, elem_index );
		}
		return ret;
	}

	EFLIB_ASSERT_UNIMPLEMENTED();
	return NULL;
}

value_t cg_service::emit_and( value_t const& lhs, value_t const& rhs )
{
	assert( scalar_of( lhs.hint() ) == builtin_types::_boolean );
	assert( lhs.hint() == rhs.hint() );

	return emit_bit_and( lhs, rhs );
}

value_t cg_service::emit_or( value_t const& lhs, value_t const& rhs )
{
	assert( scalar_of( lhs.hint() ) == builtin_types::_boolean );
	assert( lhs.hint() == rhs.hint() );

	return emit_bit_or( lhs, rhs );
}

AllocaInst* cg_service::alloca_( Type* ty, string const& name )
{
	insert_point_t ip = insert_point();
	set_insert_point( fn().allocation_block() );
	AllocaInst* ret = builder().CreateAlloca( ty, NULL, name );
	set_insert_point( ip );
	return ret;
}

llvm::Value* cg_service::unary_op_ps_( llvm::Type* ret_ty, llvm::Value* v, unary_fn_t sfn, unary_fn_t vfn, unary_fn_t simd_fn, unary_fn_t sv_fn )
{
	Type* ty = v->getType();
	ret_ty = ret_ty ? ret_ty : ty;

	if( !ty->isAggregateType() && !ty->isVectorTy() )
	{
		if(sv_fn) { return sv_fn(v); }
		assert(sfn);
		return sfn(v);
	}

	if( ty->isVectorTy() )
	{
		if( sv_fn )	{ return sv_fn(v); }
		if( vfn )	{ return vfn(v); }

		unsigned elem_count = ty->getVectorNumElements();

		// SIMD
		if( simd_fn && elem_count % SIMD_ELEMENT_COUNT() == 0 )
		{
			int batch_count = elem_count / SIMD_ELEMENT_COUNT();

			Value* ret_v = UndefValue::get(ret_ty);
			for( int i_batch = 0; i_batch < batch_count; ++i_batch ){
				Value* src_simd_elem = extract_elements_( v, i_batch*SIMD_ELEMENT_COUNT(), SIMD_ELEMENT_COUNT() );
				Value* ret_simd_elem = simd_fn( src_simd_elem );
				ret_v = insert_elements_( ret_v, ret_simd_elem, i_batch*SIMD_ELEMENT_COUNT() );
			}

			return ret_v;
		}

		// Scalar
		assert( sfn );
		Value* ret_v = UndefValue::get( v->getType() );
		for( unsigned i = 0; i < elem_count; ++i )
		{
			Value* src_elem = builder().CreateExtractElement( v, int_(i) );
			Value* ret_elem = sfn(src_elem);
			ret_v = builder().CreateInsertElement( ret_v, ret_elem, int_(i) );
		}
		return ret_v;
	}

	if( ty->isStructTy() )
	{
		Value* ret = UndefValue::get(ret_ty);
		size_t elem_count = ty->getStructNumElements();
		unsigned int elem_index[1] = {0};
		for( unsigned int i = 0;i < elem_count; ++i )
		{
			elem_index[0] = i;
			Value* v_elem = builder().CreateExtractValue(v, elem_index);
			Value* ret_elem = unary_op_ps_(ret_ty->getStructElementType(i), v_elem, sfn, vfn, simd_fn, sv_fn);
			ret = builder().CreateInsertValue( ret, ret_elem, elem_index );
		}
		return ret;
	}

	assert(false);
	return NULL;
}

llvm::Value* cg_service::call_external1_( llvm::Function* f, llvm::Value* v )
{
	Argument* ret_arg = f->getArgumentList().begin();
	Type* ret_ty = ret_arg->getType()->getPointerElementType();
	Value* tmp = alloca_(ret_ty, "tmp");
	builder().CreateCall2( f, tmp, v );
	return builder().CreateLoad( tmp );
}

llvm::Value* cg_service::call_external2_( llvm::Function* f, llvm::Value* v0, llvm::Value* v1 )
{
	Argument* ret_arg = f->getArgumentList().begin();
	Type* ret_ty = ret_arg->getType()->getPointerElementType();
	Value* tmp = alloca_(ret_ty, "tmp");
	builder().CreateCall3( f, tmp, v0, v1 );
	return builder().CreateLoad( tmp );
}

cg_service::unary_fn_t cg_service::bind_unary_call_( llvm::Function* fn )
{
	typedef CallInst* (DefaultIRBuilder::*call1_ptr_t)(Value*, Value*, Twine const&) ;
	return boost::bind( static_cast<call1_ptr_t>(&DefaultIRBuilder::CreateCall), builder(), fn, _1, "" );
}

cg_service::bin_fn_t cg_service::bind_binary_call_( llvm::Function* fn )
{
	return boost::bind( &DefaultIRBuilder::CreateCall2, builder(), fn, _1, _2, "" );
}

cg_service::unary_fn_t cg_service::bind_unary_external_( llvm::Function* fn )
{
	return boost::bind( &cg_service::call_external1_, this, fn, _1 );
}

cg_service::bin_fn_t cg_service::bind_binary_external_( llvm::Function* fn )
{
	return boost::bind( &cg_service::call_external2_, this, fn, _1, _2 );
}

bool cg_service::register_external_intrinsic()
{
	builtin_types v4f32_hint = vector_of( builtin_types::_float, 4 );
	builtin_types v2f32_hint = vector_of( builtin_types::_float, 2 );

	Type* void_ty		= Type::getVoidTy( context() );
	Type* u16_ty		= Type::getInt16Ty( context() );
	Type* f32_ty		= Type::getFloatTy( context() );
	Type* samp_ty		= Type::getInt8PtrTy( context() );
	Type* f32ptr_ty		= Type::getFloatPtrTy( context() );

	Type* v4f32_ty		= type_(v4f32_hint, abi_llvm);
	Type* v4f32_pkg_ty	= type_(v4f32_hint, abi_package);
	Type* v2f32_pkg_ty	= type_(v2f32_hint, abi_package);

	FunctionType* f_f = NULL;
	{
		Type* arg_tys[2] = { f32ptr_ty, f32_ty };
		f_f = FunctionType::get( void_ty, arg_tys, false );
	}

	FunctionType* f_ff = NULL;
	{
		Type* arg_tys[3] = { f32ptr_ty, f32_ty, f32_ty };
		f_ff = FunctionType::get( void_ty, arg_tys, false );
	}

	FunctionType* vs_tex2dlod_ty = NULL;
	{
		Type* arg_tys[3] =
		{
			PointerType::getUnqual( v4f32_ty ),	/*Pixel*/
			samp_ty,							/*Sampler*/
			PointerType::getUnqual( v4f32_ty )	/*Coords(x, y, _, lod)*/
		};
		vs_tex2dlod_ty = FunctionType::get( void_ty, arg_tys, false );
	}

	FunctionType* ps_tex2dlod_ty = NULL;
	{
		Type* arg_tys[4] =
		{
			PointerType::getUnqual( v4f32_pkg_ty ),	/*Pixels*/
			u16_ty,									/*Mask*/
			samp_ty,								/*Sampler*/
			PointerType::getUnqual( v4f32_pkg_ty )	/*Coords(x, y, _, lod)*/
		};
		ps_tex2dlod_ty = FunctionType::get( void_ty, arg_tys, false );
	}

	FunctionType* ps_tex2dgrad_ty = NULL;
	{
		Type* arg_tys[6] =
		{
			PointerType::getUnqual( v4f32_pkg_ty ),	/*Pixels*/
			u16_ty,									/*Mask*/
			samp_ty,								/*Sampler*/
			PointerType::getUnqual( v2f32_pkg_ty ),	/*Coords(x, y)*/
			PointerType::getUnqual( v2f32_pkg_ty ),	/*ddx*/
			PointerType::getUnqual( v2f32_pkg_ty ),	/*ddy*/
		};
		ps_tex2dgrad_ty = FunctionType::get( void_ty, arg_tys, false );
	}

	FunctionType* ps_tex2dbias_ty = NULL;
	{
		Type* arg_tys[6] =
		{
			PointerType::getUnqual( v4f32_pkg_ty ),	/*Pixels*/
			u16_ty,									/*Mask*/
			samp_ty,								/*Sampler*/
			PointerType::getUnqual( v4f32_pkg_ty ),	/*Coords(x, y, _, bias)*/
			PointerType::getUnqual( v2f32_pkg_ty ),	/*ddx*/
			PointerType::getUnqual( v2f32_pkg_ty ),	/*ddy*/
		};
		ps_tex2dbias_ty = FunctionType::get( void_ty, arg_tys, false );
	}

	FunctionType* ps_tex2dproj_ty = NULL;
	{
		Type* arg_tys[6] =
		{
			PointerType::getUnqual( v4f32_pkg_ty ),	/*Pixels*/
			u16_ty,									/*Mask*/
			samp_ty,								/*Sampler*/
			PointerType::getUnqual( v4f32_pkg_ty ),	/*Coords(x, y, _, proj)*/
			PointerType::getUnqual( v4f32_pkg_ty ),	/*ddx*/
			PointerType::getUnqual( v4f32_pkg_ty ),	/*ddy*/
		};
		ps_tex2dproj_ty = FunctionType::get( void_ty, arg_tys, false );
	}

	external_intrins[exp_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.exp.f32", module() );
	external_intrins[exp2_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.exp2.f32", module() );
	external_intrins[sin_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.sin.f32", module() );
	external_intrins[cos_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.cos.f32", module() );
	external_intrins[tan_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.tan.f32", module() );
	external_intrins[asin_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.asin.f32", module() );
	external_intrins[acos_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.acos.f32", module() );
	external_intrins[atan_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.atan.f32", module() );
	external_intrins[ceil_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.ceil.f32", module() );
	external_intrins[floor_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.floor.f32", module() );
	external_intrins[log_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.log.f32", module() );
	external_intrins[log2_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.log2.f32", module() );
	external_intrins[log10_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.log10.f32", module() );
	external_intrins[rsqrt_f32]		= Function::Create(f_f , GlobalValue::ExternalLinkage, "sasl.rsqrt.f32", module() );

	external_intrins[mod_f32]		= Function::Create(f_ff, GlobalValue::ExternalLinkage, "sasl.mod.f32", module() );
	external_intrins[ldexp_f32]		= Function::Create(f_ff, GlobalValue::ExternalLinkage, "sasl.ldexp.f32", module() );

	external_intrins[tex2dlod_vs]	= Function::Create(vs_tex2dlod_ty , GlobalValue::ExternalLinkage, "sasl.vs.tex2d.lod", module() );
	external_intrins[tex2dlod_ps]	= Function::Create(ps_tex2dlod_ty , GlobalValue::ExternalLinkage, "sasl.ps.tex2d.lod", module() );
	external_intrins[tex2dgrad_ps]	= Function::Create(ps_tex2dgrad_ty, GlobalValue::ExternalLinkage, "sasl.ps.tex2d.grad", module() );
	external_intrins[tex2dbias_ps]	= Function::Create(ps_tex2dbias_ty, GlobalValue::ExternalLinkage, "sasl.ps.tex2d.bias", module() );
	external_intrins[tex2dproj_ps]	= Function::Create(ps_tex2dproj_ty, GlobalValue::ExternalLinkage, "sasl.ps.tex2d.proj", module() );

	return true;
}

value_t cg_service::emit_tex2Dlod( value_t const& samp, value_t const& coord )
{
	builtin_types v4f32_hint = vector_of( builtin_types::_float, 4 );
	abis abi = param_abi(false);
	assert( abi == abi_llvm || abi == abi_package );

	Type* ret_ty = type_( v4f32_hint, abi );
	Value* ret_ptr = alloca_( ret_ty, "ret.tmp" );

	Type* coord_ty = ret_ty;
	Value* coord_ptr = alloca_(coord_ty, "coord.tmp");
	builder().CreateStore( coord.load(abi), coord_ptr );

	if( abi == abi_llvm)
	{
		builder().CreateCall3( external_intrins[tex2dlod_vs], ret_ptr, samp.load(), coord_ptr );
	}
	else
	{
		builder().CreateCall4( external_intrins[tex2dlod_ps], ret_ptr, fn().packed_execution_mask().load(), samp.load(), coord_ptr );
	}

	return create_value( NULL, v4f32_hint, ret_ptr, vkind_ref, abi );
}

value_t cg_service::emit_tex2Dgrad( value_t const& samp, value_t const& coord, value_t const& ddx, value_t const& ddy )
{
	builtin_types v2f32_hint = vector_of( builtin_types::_float, 2 );
	builtin_types v4f32_hint = vector_of( builtin_types::_float, 4 );

	abis abi = param_abi(false);
	assert( abi == abi_package );

	Type* ret_ty = type_( v4f32_hint, abi );
	Value* ret_ptr = alloca_( ret_ty, "ret.tmp" );

	Type* v2f32_ty = type_( v2f32_hint, abi );

	Value* coord_ptr = alloca_(v2f32_ty, "coord.tmp");
	builder().CreateStore( coord.load(abi), coord_ptr );

	Value* ddx_ptr = alloca_(v2f32_ty, "ddx.tmp");
	builder().CreateStore( ddx.load(abi), ddx_ptr );

	Value* ddy_ptr = alloca_(v2f32_ty, "ddy.tmp");
	builder().CreateStore( ddy.load(abi), ddy_ptr );

	Value* args[] = 
	{
		ret_ptr, fn().packed_execution_mask().load(), samp.load(), coord_ptr, ddx_ptr, ddy_ptr
	};

	builder().CreateCall( external_intrins[tex2dgrad_ps], args );

	return create_value( NULL, v4f32_hint, ret_ptr, vkind_ref, abi );
}

value_t cg_service::emit_tex2Dbias( value_t const& samp, value_t const& coord )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return value_t();
}

value_t cg_service::emit_tex2Dproj( value_t const& samp, value_t const& coord )
{
	value_t ddx = emit_ddx( coord );
	value_t ddy = emit_ddy( coord );

	builtin_types v4f32_hint = vector_of( builtin_types::_float, 4 );

	abis abi = param_abi(false);
	assert( abi == abi_package );

	Type* ret_ty = type_( v4f32_hint, abi );
	Value* ret_ptr = alloca_( ret_ty, "ret.tmp" );

	Type* v4f32_ty = type_( v4f32_hint, abi );

	Value* coord_ptr = alloca_(v4f32_ty, "coord.tmp");
	builder().CreateStore( coord.load(abi), coord_ptr );

	Value* ddx_ptr = alloca_(v4f32_ty, "ddx.tmp");
	builder().CreateStore( ddx.load(abi), ddx_ptr );

	Value* ddy_ptr = alloca_(v4f32_ty, "ddy.tmp");
	builder().CreateStore( coord.load(abi), ddy_ptr );

	Value* args[] = 
	{
		ret_ptr, fn().packed_execution_mask().load(), samp.load(), coord_ptr, ddx_ptr, ddy_ptr
	};
	builder().CreateCall( external_intrins[tex2dgrad_ps], args );

	return create_value( NULL, v4f32_hint, ret_ptr, vkind_ref, abi );
}

value_t cg_service::create_constant_int( value_tyinfo* tyinfo, builtin_types bt, abis abi, uint64_t v )
{
	builtin_types hint = tyinfo ? tyinfo->hint() : bt;
	builtin_types scalar_hint = scalar_of(hint);
	assert( is_integer(scalar_hint) || scalar_hint == builtin_types::_boolean );
	uint32_t bits = static_cast<uint32_t>( storage_size(scalar_hint) ) << 3; 

	Type* ret_ty = type_( hint, abi );
	Value* ret = integer_value_( ret_ty, APInt( bits, v, is_signed(scalar_hint) ) );
	return create_value(tyinfo, bt, ret, vkind_value, abi);
}

Value* cg_service::integer_value_( Type* ty, llvm::APInt const& v )
{
	if( !ty->isAggregateType() )
	{
		return Constant::getIntegerValue(ty, v);
	}

	if( ty->isStructTy() )
	{
		Value* ret = UndefValue::get(ty);
		unsigned indexes[1] = {0};
		for( uint32_t i = 0; i < ty->getStructNumElements(); ++i )
		{
			indexes[0] = i;
			Value* elem_v = integer_value_( ty->getStructElementType(i), v );
			ret = builder().CreateInsertValue( ret, elem_v, indexes );
		}
		return ret;
	}

	EFLIB_ASSERT_UNIMPLEMENTED();
	return NULL;
}

Value* cg_service::bit_cast_( llvm::Value* v, llvm::Type* scalar_ty )
{
	assert( !v->getType()->isAggregateType() );
	Type* ret_ty 
		= v->getType()->isVectorTy()
		? VectorType::get( scalar_ty, v->getType()->getVectorNumElements() )
		: scalar_ty;
	return builder().CreateBitCast( v, ret_ty );
}

value_t cg_service::emit_unary_ps( std::string const& scalar_external_intrin_name, value_t const& v )
{
	Function* scalar_intrin = module()->getFunction( scalar_external_intrin_name );
	assert( scalar_intrin );

	Value* ret_v = unary_op_ps_(
		NULL,
		v.load(),
		bind_unary_external_( scalar_intrin ),
		unary_fn_t(),
		unary_fn_t(),
		unary_fn_t()
	);
	return create_value( v.tyinfo(), v.hint(), ret_v, vkind_value, v.abi() );
}

value_t cg_service::emit_bin_ps( std::string const& scalar_external_intrin_name, value_t const& v0, value_t const& v1 )
{
	Function* scalar_intrin = module()->getFunction( scalar_external_intrin_name );
	assert( scalar_intrin );

	builtin_types hint = v0.hint();
	assert( hint == v1.hint() );
	abis abi = promote_abi( v0.abi(), v1.abi() );

	Value* ret_v = bin_op_ps_(
		(Type*)NULL,
		v0.load(abi),
		v1.load(abi),
		bind_binary_external_( scalar_intrin ),
		bin_fn_t(),
		bin_fn_t(),
		bin_fn_t(),
		unary_fn_t()
		);

	return create_value( v0.tyinfo(), v0.hint(), ret_v, vkind_value, abi );
}

Value* cg_service::safe_div_mod_( Value* lhs, Value* rhs, bin_fn_t div_or_mod_fn )
{
	Type* rhs_ty = rhs->getType();
	Type* rhs_scalar_ty = rhs_ty->getScalarType();
	assert( rhs_scalar_ty->isIntegerTy() );

	Value* zero = Constant::getNullValue( rhs_ty );
	Value* is_zero = builder().CreateICmpEQ( rhs, zero );
	Value* one_value = Constant::getIntegerValue( rhs_ty, APInt(rhs_scalar_ty->getIntegerBitWidth(), 1) );
	Value* non_zero_rhs = builder().CreateSelect( is_zero, one_value, rhs );

	return div_or_mod_fn( lhs, non_zero_rhs );
}

value_t cg_service::extend_to_vm( value_t const& v, builtin_types complex_hint )
{
	builtin_types hint = v.hint();
	assert( is_scalar(hint) );

	if( is_vector(complex_hint) )
	{
		vector<value_t> values(vector_size(complex_hint), v);
		return create_vector( values, v.abi() );
	}

	if( is_matrix(complex_hint) )
	{
		vector<value_t> values(vector_size(complex_hint), v);
		value_t vec_v = create_vector( values, v.abi() );
		value_t ret_v = undef_value(complex_hint, v.abi());
		for( int i = 0; i < (int)vector_count(complex_hint); ++i )
		{
			ret_v = emit_insert_val( ret_v, i, vec_v );
		}
		return ret_v;
	}

	assert(false);
	return value_t();
}

value_t cg_service::extend_scalar_and_emit_bin_ps( std::string const& scalar_external_intrin_name, value_t const& lhs, value_t const& rhs )
{
	builtin_types lhint = lhs.hint();
	builtin_types rhint = rhs.hint();

	assert( lhint != builtin_types::none );
	assert( rhint != builtin_types::none );

	value_t lv = lhs;
	value_t rv = rhs;

	if( lhint != rhint )
	{
		if( is_scalar(lhint) ) { lv = extend_to_vm( lhs, rhint ); }
		else if( is_scalar(rhint) ) { rv = extend_to_vm( rhs, lhint ); }
		else { assert(false); }
	}

	return emit_bin_ps( scalar_external_intrin_name, lv, rv );
}

value_t cg_service::extend_scalar_and_emit_bin_ps( value_t const& lhs, value_t const& rhs, bin_fn_t signed_sv_fn, bin_fn_t unsigned_sv_fn, bin_fn_t float_sv_fn )
{
	builtin_types lhint = lhs.hint();
	builtin_types rhint = rhs.hint();

	assert( lhint != builtin_types::none );
	assert( rhint != builtin_types::none );

	value_t lv = lhs;
	value_t rv = rhs;

	if( lhint != rhint )
	{
		if( is_scalar(lhint) ) { lv = extend_to_vm( lhs, rhint ); }
		else if( is_scalar(rhint) ) { rv = extend_to_vm( rhs, lhint ); }
		else { assert(false); }
	}

	return emit_bin_ps( lv, rv, signed_sv_fn, unsigned_sv_fn, float_sv_fn );
}

END_NS_SASL_CODE_GENERATOR();