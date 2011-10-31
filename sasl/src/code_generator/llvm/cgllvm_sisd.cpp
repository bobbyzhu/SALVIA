#include <sasl/include/code_generator/llvm/cgllvm_sisd.h>

#include <sasl/include/code_generator/llvm/cgllvm_impl.imp.h>
#include <sasl/include/code_generator/llvm/cgllvm_globalctxt.h>
#include <sasl/include/code_generator/llvm/cgllvm_type_converters.h>
#include <sasl/include/code_generator/llvm/cgllvm_service.h>

#include <sasl/include/semantic/name_mangler.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/semantic_infos.imp.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/semantic/tecov.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/syntax_tree/program.h>
#include <sasl/include/syntax_tree/statement.h>

#include <sasl/enums/enums_utility.h>

#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/BasicBlock.h>
#include <llvm/Constants.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Support/IRBuilder.h>
#include <eflib/include/platform/enable_warnings.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/scope_exit.hpp>
#include <boost/bind.hpp>
#include <eflib/include/platform/boost_end.h>

using namespace llvm;
using namespace sasl::syntax_tree;
using namespace boost::assign;
using namespace sasl::utility;

using sasl::semantic::extract_semantic_info;
using sasl::semantic::symbol;
using sasl::semantic::type_info_si;
using sasl::semantic::storage_si;
using sasl::semantic::const_value_si;
using sasl::semantic::call_si;
using sasl::semantic::fnvar_si;
using sasl::semantic::operator_name;

using boost::addressof;
using boost::any_cast;
using boost::bind;

using std::vector;
using std::string;

#define SASL_VISITOR_TYPE_NAME cgllvm_sisd
#define FUNCTION_SCOPE( fn ) \
	push_fn( (fn) );	\
	scope_guard<void> pop_fn_on_exit##__LINE__( bind( &cg_service::pop_fn, this ) );

BEGIN_NS_SASL_CODE_GENERATOR();

cgllvm_sisd::~cgllvm_sisd(){
}

void cgllvm_sisd::mask_to_indexes( char indexes[4], uint32_t mask ){
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

bool cgllvm_sisd::generate( sasl::semantic::module_si* mod, sasl::semantic::abi_info const* abii ){
	this->msi = mod;
	this->abii = abii;

	if ( msi ){
		assert( msi->root() );
		assert( msi->root()->node() );

		msi->root()->node()->accept( this, NULL );
		return true;
	}

	return false;
}

SASL_VISIT_DEF( program ){
	// Create module.
	if( !create_mod( v ) ){
		return;
	}

	// Initialization.
	before_decls_visit( v, data );

	// visit declarations
	any child_ctxt = cgllvm_sctxt();
	for( vector< shared_ptr<declaration> >::iterator
		it = v.decls.begin(); it != v.decls.end(); ++it )
	{
		visit_child( child_ctxt, (*it) );
	}
}

SASL_VISIT_DEF( binary_expression ){
	any child_ctxt_init = *data;
	sc_ptr(child_ctxt_init)->clear_data();
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.left_expr );
	visit_child( child_ctxt, child_ctxt_init, v.right_expr );

	if( v.op == operators::assign ){
		bin_assign( v, data );
	} else {
		shared_ptr<type_info_si> larg_tsi = extract_semantic_info<type_info_si>(v.left_expr);
		shared_ptr<type_info_si> rarg_tsi = extract_semantic_info<type_info_si>(v.right_expr);

		//////////////////////////////////////////////////////////////////////////
		// type conversation for matching the operator prototype

		// get an overloadable prototype.
		std::vector< shared_ptr<expression> > args;
		args += v.left_expr, v.right_expr;

		symbol::overloads_t overloads
			= sc_env_ptr(data)->sym.lock()->find_overloads( operator_name( v.op ), typeconv, args );

		EFLIB_ASSERT_AND_IF( !overloads.empty(), "Error report: no prototype could match the expression." ){
			return;
		}
		EFLIB_ASSERT_AND_IF( overloads.size() == 1, "Error report: prototype was ambigous." ){
			return;
		}

		boost::shared_ptr<function_type> op_proto = overloads[0]->node()->as_handle<function_type>();

		shared_ptr<type_info_si> p0_tsi = extract_semantic_info<type_info_si>( op_proto->params[0] );
		shared_ptr<type_info_si> p1_tsi = extract_semantic_info<type_info_si>( op_proto->params[1] );

		// convert value type to match proto type.
		if( p0_tsi->entry_id() != larg_tsi->entry_id() ){
			if( ! node_ctxt( p0_tsi->type_info() ) ){
				visit_child( child_ctxt, child_ctxt_init, op_proto->params[0]->param_type );
			}

			node_ctxt(v.left_expr)->data().tyinfo
				= node_ctxt( p0_tsi->type_info() )->data().tyinfo;

			typeconv->convert( p0_tsi->type_info(), v.left_expr );
		}
		if( p1_tsi->entry_id() != rarg_tsi->entry_id() ){
			if( ! node_ctxt( p1_tsi->type_info() ) ){
				visit_child( child_ctxt, child_ctxt_init, op_proto->params[1]->param_type );
			}

			node_ctxt(v.left_expr)->data().tyinfo
				= node_ctxt( p0_tsi->type_info() )->data().tyinfo;

			typeconv->convert( p1_tsi->type_info(), v.right_expr );
		}

		// use type-converted value to generate code.

		value_t lval = node_ctxt(v.left_expr)->get_rvalue();
		value_t rval = node_ctxt(v.right_expr)->get_rvalue();

		value_t retval = null_value( NULL, abi_llvm );

		if( lval.raw() && rval.raw() ){

			builtin_types lbtc = p0_tsi->type_info()->tycode;
			builtin_types rbtc = p1_tsi->type_info()->tycode;

			assert( lbtc == rbtc );

			if( is_scalar(lbtc) ){
				if( v.op == operators::add ){
					retval = emit_add(lval, rval);
				} else {
					EFLIB_ASSERT_UNIMPLEMENTED();
				}
			} else {
				EFLIB_ASSERT_UNIMPLEMENTED();
			}
		}

		sc_ptr(data)->get_value() = retval;
		node_ctxt(v, true)->copy( sc_ptr(data) );
	}
}

SASL_VISIT_DEF( member_expression ){

	any child_ctxt = *data;
	sc_ptr(child_ctxt)->clear_data();
	visit_child( child_ctxt, v.expr );

	cgllvm_sctxt* agg_ctxt = node_ctxt( v.expr );
	assert( agg_ctxt );

	// Aggregated value
	type_info_si* tisi = dynamic_cast<type_info_si*>( v.expr->semantic_info().get() );

	if( tisi->type_info()->is_builtin() ){
		// Swizzle or write mask
		storage_si* mem_ssi = v.si_ptr<storage_si>();
		value_t vec_value = agg_ctxt->get_value();
		// mem_ctxt->get_value() = create_extract_elem();
		EFLIB_ASSERT_UNIMPLEMENTED();
	} else {
		// Member
		shared_ptr<symbol> struct_sym = tisi->type_info()->symbol();
		shared_ptr<symbol> mem_sym = struct_sym->find_this( v.member->str );

		assert( mem_sym );
		cgllvm_sctxt* mem_ctxt = node_ctxt( mem_sym->node(), true );
		sc_ptr(data)->get_value() = mem_ctxt->get_value();
		sc_ptr(data)->get_value().set_parent( agg_ctxt->get_value() );
	}

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( constant_expression ){

	any child_ctxt_init = *data;
	any child_ctxt;

	boost::shared_ptr<const_value_si> c_si = extract_semantic_info<const_value_si>(v);
	if( ! node_ctxt( c_si->type_info() ) ){
		visit_child( child_ctxt, child_ctxt_init, c_si->type_info() );
	}

	Value* retval = NULL;

	cgllvm_sctxt* const_ctxt = node_ctxt( c_si->type_info() );

	value_tyinfo* tyinfo = const_ctxt->get_typtr();
	assert( tyinfo );

	value_t val;
	if( c_si->value_type() == builtin_types::_sint32 ){
		val = create_constant_scalar( c_si->value<int32_t>(), tyinfo );
	} else if ( c_si->value_type() == builtin_types::_uint32 ) {
		val = create_constant_scalar( c_si->value<uint32_t>(), tyinfo );
	} else if ( c_si->value_type() == builtin_types::_float ) {
		val = create_constant_scalar( c_si->value<double>(), tyinfo );
	} else {
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	sc_ptr(data)->get_value().emplace( val );

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( call_expression ){
	any child_ctxt_init = *data;
	sc_ptr(&child_ctxt_init)->clear_data();

	any child_ctxt;

	Value* ret = NULL;

	call_si* csi = v.si_ptr<call_si>();
	if( csi->is_function_pointer() ){
		visit_child( child_ctxt, child_ctxt_init, v.expr );
		EFLIB_ASSERT_UNIMPLEMENTED();
	} else {
		// Get LLVM Function
		symbol* fn_sym = csi->overloaded_function();
		function_t fn = fetch_function( fn_sym->node()->as_handle<function_type>() );

		// TODO implicit type conversations.
		vector<value_t> args;
		BOOST_FOREACH( shared_ptr<expression> const& arg_expr, v.args ){
			visit_child( child_ctxt, child_ctxt_init, arg_expr );
			cgllvm_sctxt* arg_ctxt = node_ctxt( arg_expr, false );
			args.push_back( arg_ctxt->get_value() );
		}

		value_t rslt = emit_call( fn, args );
		
		cgllvm_sctxt* expr_ctxt = node_ctxt( v, true );
		expr_ctxt->data().val = rslt;
		expr_ctxt->data().tyinfo = fn.get_return_ty();

		sc_ptr(data)->copy( expr_ctxt );
	}
}

SASL_VISIT_DEF( variable_expression ){
	shared_ptr<symbol> declsym = sc_env_ptr(data)->sym.lock()->find( v.var_name->str );
	assert( declsym && declsym->node() );

	sc_ptr(data)->get_value() = node_ctxt( declsym->node(), false )->get_value();
	sc_ptr(data)->get_tysp() = node_ctxt( declsym->node(), false )->get_tysp();
	sc_ptr(data)->data().semantic_mode = node_ctxt( declsym->node(), false )->data().semantic_mode;

	// sc_data_ptr(data)->hint_name = v.var_name->str.c_str();
	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( builtin_type ){

	shared_ptr<type_info_si> tisi = extract_semantic_info<type_info_si>( v );

	cgllvm_sctxt* pctxt = node_ctxt( tisi->type_info(), true );

	if( !pctxt->get_typtr() ){
		shared_ptr<value_tyinfo> bt_tyinfo = create_tyinfo( v.as_handle<tynode>() );
		assert( bt_tyinfo );
		pctxt->data().tyinfo = bt_tyinfo;

		std::string tips = v.tycode.name() + std::string(" was not supported yet.");
		EFLIB_ASSERT_AND_IF( pctxt->data().tyinfo, tips.c_str() ){
			return;
		}
	}

	sc_ptr( data )->data( pctxt );
	return;
}

// Generate normal function code.
SASL_VISIT_DEF( function_type ){
	sc_env_ptr(data)->sym = v.symbol();

	cgllvm_sctxt* fnctxt = node_ctxt(v.symbol()->node(), true);
	if( !fnctxt->data().self_fn ){
		create_fnsig( v, data );
	}

	if ( v.body ){
		// TODO
		// sc_env_ptr(data)->parent_fn = sc_data_ptr(data)->self_fn;
		create_fnargs( v, data );
		create_fnbody( v, data );
	}

	// Here use the definition node.
	node_ctxt(v.symbol()->node(), true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( struct_type ){
	// Create context.
	// Declarator visiting need parent information.
	cgllvm_sctxt* ctxt = node_ctxt(v, true);

	// A struct is visited at definition type.
	// If the visited again, it must be as an alias_type.
	// So return environment directly.
	if( ctxt->data().tyinfo ){
		sc_ptr(data)->data(ctxt);
		return;
	}

	std::string name = v.symbol()->mangled_name();

	// Init data.
	any child_ctxt_init = *data;
	sc_ptr(child_ctxt_init)->clear_data();
	sc_env_ptr(&child_ctxt_init)->parent_struct = ctxt;

	any child_ctxt;
	BOOST_FOREACH( shared_ptr<declaration> const& decl, v.decls ){
		visit_child( child_ctxt, child_ctxt_init, decl );
	}
	sc_data_ptr(data)->tyinfo = create_tyinfo( v.si_ptr<type_info_si>()->type_info() );

	ctxt->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( declarator ){

	// local *OR* member.
	// TODO TBD: Support member function and nested structure ?

	// TODO
	// assert( !(sc_env_ptr(data)->parent_fn && sc_env_ptr(data)->parent_struct) );

	if( in_function() ){
		visit_local_declarator( v, data );
	} else if( sc_env_ptr(data)->parent_struct ){
		visit_member_declarator( v, data );
	} else {
		visit_global_declarator(v, data);
	}
}

SASL_VISIT_DEF( variable_declaration ){
	// Visit type info
	any child_ctxt_init = *data;
	sc_ptr(child_ctxt_init)->clear_data();
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.type_info );

	// bool as_vector = sc_data_ptr(&child_ctxt)->as_vector;
	// bool is_mat = sc_data_ptr(&child_ctxt)->is_matrix;

	sc_env_ptr(&child_ctxt_init)->tyinfo = sc_data_ptr(&child_ctxt)->tyinfo;
	// sc_env_ptr(&child_ctxt_init)->is_mat = is_mat;
	// sc_env_ptr(&child_ctxt_init)->as_vec = as_vector;

	BOOST_FOREACH( shared_ptr<declarator> const& dclr, v.declarators ){
		visit_child( child_ctxt, child_ctxt_init, dclr );
	}

	sc_data_ptr(data)->declarator_count = static_cast<int>( v.declarators.size() );

	sc_data_ptr(data)->tyinfo = sc_data_ptr(&child_ctxt)->tyinfo;
	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( parameter ){
	sc_ptr(data)->clear_data();

	any child_ctxt_init = *data;
	sc_ptr(child_ctxt_init)->clear_data();

	any child_ctxt;
	visit_child( child_ctxt, child_ctxt_init, v.param_type );

	if( v.init ){
		visit_child( child_ctxt, child_ctxt_init, v.init );
	}

	sc_data_ptr(data)->val = sc_data_ptr(&child_ctxt)->val;
	sc_data_ptr(data)->tyinfo = sc_data_ptr(&child_ctxt)->tyinfo;
	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( declaration_statement ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.decl );

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( compound_statement ){
	sc_env_ptr(data)->sym = v.symbol();

	any child_ctxt_init = *data;
	any child_ctxt;

	for ( std::vector< boost::shared_ptr<statement> >::iterator it = v.stmts.begin();
		it != v.stmts.end(); ++it)
	{
		visit_child( child_ctxt, child_ctxt_init, *it );
	}

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF( jump_statement ){

	any child_ctxt_init = *data;
	any child_ctxt;

	if (v.jump_expr){
		visit_child( child_ctxt, child_ctxt_init, v.jump_expr );
	}

	if ( v.code == jump_mode::_return ){
		return_statement(v, data);
	} else if ( v.code == jump_mode::_continue ){
		EFLIB_ASSERT_UNIMPLEMENTED();
		//assert( sc_env_ptr(data)->continue_to );
		//builder()->CreateBr( sc_env_ptr(data)->continue_to );

	} else if ( v.code == jump_mode::_break ){
		EFLIB_ASSERT_UNIMPLEMENTED();

		//assert( sc_env_ptr(data)->break_to );
		//builder()->CreateBr( sc_env_ptr(data)->break_to );
	}

	// Restart a new block for sealing the old block.
	new_block("", true);

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_SPECIFIC_VISIT_DEF( return_statement, jump_statement ){
	if ( !v.jump_expr ){
		emit_return();
	} else {
		emit_return( node_ctxt(v.jump_expr)->get_value() );
		// builder()->CreateRet( load( node_ctxt(v.jump_expr) ) );
	}
}

SASL_VISIT_DEF( expression_statement ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.expr );

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_SPECIFIC_VISIT_DEF( before_decls_visit, program ){
	mod_ptr()->create_module( v.name );

	ctxt_getter = boost::bind( &cgllvm_sisd::node_ctxt<node>, this, _1, false );

	typeconv = create_type_converter( ctxt_getter, this );
	register_builtin_typeconv( typeconv, msi->pety() );

	// Instrinsics will be generated before code was 
	process_intrinsics( v, data );
}

SASL_SPECIFIC_VISIT_DEF( create_fnsig, function_type ){
	any child_ctxt_init = *data;
	sc_ptr(child_ctxt_init)->clear_data();

	any child_ctxt;

	// Generate return type node.
	visit_child( child_ctxt, child_ctxt_init, v.retval_type );
	shared_ptr<value_tyinfo> ret_ty = sc_data_ptr(&child_ctxt)->tyinfo;
	assert( ret_ty );

	// Generate parameters.
	BOOST_FOREACH( shared_ptr<parameter> const& par, v.params ){
		visit_child( child_ctxt, child_ctxt_init, par );
	}

	sc_data_ptr(data)->self_fn = fetch_function( v.as_handle<function_type>() );
}

SASL_SPECIFIC_VISIT_DEF( create_fnargs, function_type ){
	push_fn( sc_data_ptr(data)->self_fn );
	scope_guard<void> pop_fn_on_exit( bind( &cg_service::pop_fn, this ) );

	// Register arguments names.
	assert( fn().arg_size() == v.params.size() );

	size_t i_arg = 0;
	BOOST_FOREACH( shared_ptr<parameter> const& par, v.params )
	{
		sctxt_handle par_ctxt = node_ctxt( par );
		fn().arg_name( i_arg, par->symbol()->unmangled_name() );
		par_ctxt->get_value() = fn().arg( i_arg++ );
	}
}

SASL_SPECIFIC_VISIT_DEF( create_fnbody, function_type ){

	FUNCTION_SCOPE( sc_data_ptr(data)->self_fn );

	any child_ctxt_init = *data;
	any child_ctxt;

	new_block(".body", true);
	visit_child( child_ctxt, child_ctxt_init, v.body );

	clean_empty_blocks();
}

SASL_SPECIFIC_VISIT_DEF( visit_member_declarator, declarator ){
	
	shared_ptr<value_tyinfo> decl_ty = sc_env_ptr(data)->tyinfo;
	assert(decl_ty);

	// Needn't process init expression now.
	storage_si* si = v.si_ptr<storage_si>();
	sc_data_ptr(data)->tyinfo = decl_ty;
	sc_data_ptr(data)->val = create_value(decl_ty.get(), NULL, value_t::kind_swizzle, abi_unknown );
	sc_data_ptr(data)->val.set_index( si->mem_index() );

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_SPECIFIC_VISIT_DEF( visit_global_declarator, declarator ){
	EFLIB_ASSERT_UNIMPLEMENTED();
}

SASL_SPECIFIC_VISIT_DEF( visit_local_declarator, declarator ){
	any child_ctxt_init = *data;
	sc_ptr(child_ctxt_init)->clear_data();

	any child_ctxt;

	sc_data_ptr(data)->tyinfo = sc_env_ptr(data)->tyinfo;
	sc_data_ptr(data)->val = create_variable( sc_data_ptr(data)->tyinfo.get(), v.si_ptr<storage_si>()->c_compatible() ? abi_c : abi_llvm, v.name->str );

	if ( v.init ){
		sc_env_ptr(&child_ctxt_init)->variable_to_fill = v.as_handle();
		visit_child( child_ctxt, child_ctxt_init, v.init );
		sc_data_ptr(data)->val.store( sc_ptr(&child_ctxt)->get_value() );
	}

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

/* Make binary assignment code.
*    Note: Right argument is assignee, and left argument is value.
*/
SASL_SPECIFIC_VISIT_DEF( bin_assign, binary_expression ){

	shared_ptr<type_info_si> larg_tsi = extract_semantic_info<type_info_si>(v.left_expr);
	shared_ptr<type_info_si> rarg_tsi = extract_semantic_info<type_info_si>(v.right_expr);

	if ( larg_tsi->entry_id() != rarg_tsi->entry_id() ){
		EFLIB_ASSERT_UNIMPLEMENTED();
		/*if( typeconv->implicit_convertible( rarg_tsi->entry_id(), larg_tsi->entry_id() ) ){
			typeconv->convert( rarg_tsi->type_info(), v.left_expr );
		} else {
			assert( !"Expression could not converted to storage type." );
		}*/
	}

	// Evaluated by visit(binary_expression)
	cgllvm_sctxt* lctxt = node_ctxt( v.left_expr );
	cgllvm_sctxt* rctxt = node_ctxt( v.right_expr );

	rctxt->get_value().store( lctxt->get_value() );

	cgllvm_sctxt* pctxt = node_ctxt(v, true);
	pctxt->data( rctxt->data() );
	pctxt->env( sc_ptr(data) );
}

cgllvm_sctxt const * sc_ptr( const boost::any& any_val ){
	return any_cast<cgllvm_sctxt>(&any_val);
}

cgllvm_sctxt* sc_ptr( boost::any& any_val ){
	return any_cast<cgllvm_sctxt>(&any_val);
}

cgllvm_sctxt const * sc_ptr( const boost::any* any_val )
{
	return any_cast<cgllvm_sctxt>(any_val);
}

cgllvm_sctxt* sc_ptr( boost::any* any_val )
{
	return any_cast<cgllvm_sctxt>(any_val);
}

cgllvm_sctxt_data* sc_data_ptr( boost::any* any_val ){
	return addressof( sc_ptr(any_val)->data() );
}

cgllvm_sctxt_data const* sc_data_ptr( boost::any const* any_val ){
	return addressof( sc_ptr(any_val)->data() );
}

cgllvm_sctxt_env* sc_env_ptr( boost::any* any_val ){
	return addressof( sc_ptr(any_val)->env() );
}

cgllvm_sctxt_env const* sc_env_ptr( boost::any const* any_val ){
	return addressof( sc_ptr(any_val)->env() );
}

Constant* cgllvm_sisd::zero_value( boost::shared_ptr<tynode> typespec )
{
	//if( typespec->is_builtin() ){
	//	builtin_types btc = typespec->tycode;
	//	Type const* valtype = node_ctxt(typespec)->data().val_type;
	//	if( is_integer( btc ) ){
	//		return ConstantInt::get( valtype, 0, is_signed(btc) );
	//	}
	//	if( is_real( btc ) ){
	//		return ConstantFP::get( valtype, 0.0 );
	//	}
	//}

	EFLIB_ASSERT_UNIMPLEMENTED();
	return NULL;
}

cgllvm_sctxt* cgllvm_sisd::node_ctxt( sasl::syntax_tree::node& v, bool create_if_need /*= false */ )
{
	return cgllvm_impl::node_ctxt<cgllvm_sctxt>(v, create_if_need);
}

cgllvm_modimpl* cgllvm_sisd::mod_ptr(){
	assert( dynamic_cast<cgllvm_modimpl*>( mod.get() ) );
	return static_cast<cgllvm_modimpl*>( mod.get() );
}

//llvm::Value* cgllvm_sisd::load( boost::any* data ){
//	return load( sc_ptr(data) );
//}
//
//llvm::Value* cgllvm_sisd::load( cgllvm_sctxt* data ){
//	assert(data);
//	Value* val = data->data().val;
//
//	const char* name = data->data().hint_name;
//	name = ( name == NULL ? "" : name );
//
//	do{
//		if( val ){ break; }
//		if( data->data().local ){
//			val = builder()->CreateLoad( data->data().local, name );
//			break;
//		}
//		if( data->data().global ){
//			val = builder()->CreateLoad( data->data().global, name );
//			break;
//		}
//		if( data->data().agg.parent ){
//			val = load( data->data().agg.parent );
//			if( data->data().agg.is_swizzle ){
//				// Swizzle with shuffle instruction.
//				char indices[4] = {-1, -1, -1, -1};
//				mask_to_indexes(indices, data->data().agg.swizzle);
//
//				cgv_vector v( NULL, ext.get() );
//				if( val->getType()->isVectorTy() ){
//					v = cgv_vector( val, ext.get() );
//				} else {
//					v = cgv_vector( llval( val, ext.get() ), 1 );
//				}
//
//				v = v.swizzle( indices, 4 );
//
//			} else {
//				llagg agg_val( val, ext.get() );
//				val = agg_val[data->data().agg.index].val;
//			}
//			break;
//		}
//
//		return NULL;
//	} while(0);
//
//	if( data->data().is_ref ){
//		val = builder()->CreateLoad( val, name );
//	}
//
//	if( data->data().as_vector && val->getType()->isStructTy() ){
//		return cgv_vector( val, ext.get() ).val;
//	}
//	return val;
//}

llvm::Value* cgllvm_sisd::to_abi( builtin_types hint, llvm::Value* v )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return NULL;
	//if( is_vector(hint) ){
	//	return cgv_vector( llagg( v, ext.get() ) ).val;
	//} else if( is_matrix( hint ) ) {
	//	bool as_vec(false), as_mat(false);
	//	llvm::Type const* mat_abity = llvm_type( hint, as_vec, as_mat );
	//	llagg ret = llagg( ext->null_value(mat_abity).val, ext.get() ) ;
	//	llagg matv = llagg( v, ext.get() );
	//	for( size_t i = 0; i < matv.size(); ++i ){
	//		ret.set( i, llagg( cgv_vector( matv[i].val, ext.get() ) ) );
	//	}
	//	return ret.val;
	//} else {
	//	return v;
	//}
}

llvm::Value* cgllvm_sisd::from_abi( builtin_types hint, llvm::Value* v )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
	return NULL;
	/*if( is_vector(hint) ){
		return llagg( cgv_vector( v, ext.get() ) ).val;
	} else if( is_matrix( hint ) ) {
		bool as_vec(false), as_mat(false);
		llvm::Type const* mat_abity = llvm_type( hint, as_vec, as_mat );
		llagg matv = llagg( ext->null_value(mat_abity).val, ext.get() ) ;
		llagg ret = llagg( v, ext.get() );
		for( size_t i = 0; i < matv.size(); ++i ){
			ret.set( i, llagg( cgv_vector( matv[i].val, ext.get() ) ) );
		}
		return ret.val;
	} else {
		return v;
	}*/
}

//llvm::Value* cgllvm_sisd::load_ptr( cgllvm_sctxt* data ){
//	cgllvm_sctxt_data* inner_data = &data->data();
//
//	Value* addr = NULL;
//	if( inner_data->val ){ addr = NULL; }
//	if( inner_data->local ){
//		addr = inner_data->local;
//	}
//	if( inner_data->global ){
//		addr = inner_data->global;
//	}
//	if( inner_data->agg.parent ){
//		Value* indexes[2];
//		indexes[0] = ConstantInt::get( Type::getInt32Ty(llcontext()), 0 );
//		indexes[1] = ConstantInt::get( Type::getInt32Ty(llcontext()), inner_data->agg.index );
//		addr = builder()->CreateGEP(
//			load_ptr( inner_data->agg.parent ), indexes, indexes + 2
//			);
//	}
//
//	if( inner_data->is_ref ){
//		if( !addr ){
//			addr = inner_data->val;
//		} else {
//			const char* name = data->data().hint_name;
//			addr = builder()->CreateLoad( addr, Twine( name ? name : "" ) );
//		}
//	}
//
//	return addr;
//}
//
//void cgllvm_sisd::store( llvm::Value* v, boost::any* data ){
//	store(v, sc_ptr(data) );
//}
//
//// Store will be enabled in following cases:
////	value to address of same type.
////	value to null data.
////	scalar to vector value.
//void cgllvm_sisd::store( llvm::Value* v, cgllvm_sctxt* data ){
//	if( !data->data().agg.is_swizzle ){
//		Value* addr = load_ptr( data );
//
//		if( addr ){
//			if( data->data().as_vector && !data->data().is_matrix ){
//				v = llagg( v, ext.get() ).val;
//			}
//
//			builder()->CreateStore( v, addr );
//		} else {
//			assert( data->data().agg.parent == NULL );
//			assert( data->data().val == NULL );
//			assert( data->data().local == NULL );
//			assert( data->data().global == NULL );
//
//			data->data().val = v;
//		}
//	} else {
//		// Write mask
//		// Steps:
//		//  Load or create vector value
//		//  Shuffle or insert value to vector.
//		//  Save new vector to parent.
//		char mask_indexes[4] = {-1, -1, -1, -1};
//		mask_to_indexes( mask_indexes, data->data().agg.swizzle );
//
//		cgv_vector vec( load(data->data().agg.parent), ext.get() );
//
//		if( v->getType()->isIntegerTy() || v->getType()->isFloatingPointTy() ){
//			// Scalar, insert directly
//			if( !vec.val ){
//				vec = cgv_vector( llval(v, ext.get()), 1 );
//			} else {
//				vec.set( 0, llval(v, ext.get()) );
//			}
//		} else {
//			// Vector, insert per element.
//			assert( v->getType()->isVectorTy() );
//			for( int i = 0; i < 4 && mask_indexes[i] != -1; ++i ){
//				cgv_vector src_vec( v, ext.get() );
//				vec.set( mask_indexes[i], src_vec[i] );
//			}
//		}
//
//		store( vec.val, data->data().agg.parent );
//	}
//}
//
//void cgllvm_sisd::create_alloca( cgllvm_sctxt* ctxt, std::string const& name ){
//	assert( ctxt );
//	assert( ctxt->data().val_type );
//
//	Function* parent_fn = ctxt->env().parent_fn;
//	if( parent_fn ){
//		ctxt->data().local
//			= builder()->CreateAlloca( ctxt->data().val_type, 0, name.c_str() );
//	} else {
//		ctxt->data().global
//			= cast<GlobalVariable>( llmodule()->getOrInsertGlobal( name, ctxt->data().val_type ) );
//	}
//}
//
//void cgllvm_sisd::clear_empty_blocks( llvm::Function* fn )
//{
	// Inner empty block, insert an br instruction for jumping to next block.
	// And the tail empty block we add an virtual return instruction.

	//EFLIB_ASSERT_UNIMPLEMENTED();

	/*typedef Function::BasicBlockListType::iterator block_iterator_t;
	block_iterator_t beg = fn->getBasicBlockList().begin();
	block_iterator_t end = fn->getBasicBlockList().end();

	for(  block_iterator_t it = beg; it != end; ++it )
	{
		if( !it->getTerminator() ){
			block_iterator_t next_it = it;
			++next_it;

			builder()->SetInsertPoint( &(*it) );

			if( next_it != fn->getBasicBlockList().end() ){	
				mod_ptr()->builder()->CreateBr( &(*next_it) );
			} else {
				if( !fn->getReturnType()->isVoidTy() ){
					ext->return_( ext->null_value( fn->getReturnType() ) );
				} else {
					ext->return_();
				}
			}
		}
	}*/
//}

template <typename ElementT> llvector<ElementT> cgllvm_sisd::mul_vm(
	llvm::Value* v, llvm::Value* m,
	size_t vec_size, size_t mat_vec_size,
	Type const* ret_type
	)
{
	llvector< ElementT > lval = llvector<ElementT>::create( ext.get(), v );
	llarray< llvector< ElementT > > rval( m, ext.get() );

	llvector<ElementT> ret_val = ext->null_value< llvector<ElementT> >(ret_type);

	for(size_t i = 0; i < mat_vec_size; ++i){
		ElementT agg_value = ret_val[i];
		for( size_t j = 0; j < vec_size; ++j ){
			agg_value = agg_value + lval[j] * rval[j][i];
		}
		ret_val.set( i, agg_value );
	}

	return ret_val;
}

template <typename ElementT> llvector<ElementT> cgllvm_sisd::mul_mv(
	llvm::Value* m, llvm::Value* v,
	size_t vec_size, size_t n_vec,
	llvm::Type const* ret_type )
{
	typedef llvector< ElementT > vector_t;
	typedef llarray< llvector< ElementT > > matrix_t;

	matrix_t lval( m, ext.get() );
	vector_t rval( v, ext.get() );

	vector_t ret_val = ext->null_value< vector_t >(ret_type);

	for(size_t i = 0; i < n_vec; ++i){
		ElementT agg_value = ret_val[i];
		for( size_t j = 0; j < vec_size; ++j ){
			agg_value = agg_value + lval[i][j] * rval[j];
		}
		ret_val.set( i, agg_value );
	}

	return ret_val;
}

template <typename ElementT> ElementT cgllvm_sisd::dot_prod(
	llvm::Value* lhs, llvm::Value* rhs,
	size_t vec_size,
	llvm::Type const* ret_type
	)
{
	typedef llvector<ElementT> vector_t;

	vector_t lval( lhs, ext.get() );
	vector_t rval( rhs, ext.get() );

	ElementT ret_val = ext->null_value<ElementT>(ret_type);
	for( size_t i = 0; i < vec_size; ++i ){
		ret_val = ret_val + lval[i] * rval[i];
	}
	
	return ret_val;
}

SASL_SPECIFIC_VISIT_DEF( process_intrinsics, program )
{
	vector< shared_ptr<symbol> > const& intrinsics = msi->intrinsics();

	BOOST_FOREACH( shared_ptr<symbol> const& intr, intrinsics ){
		shared_ptr<function_type> intr_fn = intr->node()->as_handle<function_type>();

		// If intrinsic is not invoked, we don't generate code for it.
		if( ! intr_fn->si_ptr<storage_si>()->is_invoked() ){
			continue;
		}

		any child_ctxt = cgllvm_sctxt();

		visit_child( child_ctxt, intr_fn );

		cgllvm_sctxt* intrinsic_ctxt = node_ctxt( intr_fn, false );
		assert( intrinsic_ctxt );

		push_fn( intrinsic_ctxt->data().self_fn );
		scope_guard<void> pop_fn_on_exit( bind( &cg_service::pop_fn, this ) );

		insert_point_t ip_body = new_block( ".body", true );

		// Parse Parameter Informations
		vector< shared_ptr<tynode> > par_tys;
		vector<builtin_types> par_tycodes;
		vector<cgllvm_sctxt*> par_ctxts;

		BOOST_FOREACH( shared_ptr<parameter> const& par, intr_fn->params )
		{
			par_tys.push_back( par->si_ptr<type_info_si>()->type_info() );
			assert( par_tys.back() );
			par_tycodes.push_back( par_tys.back()->tycode );
			par_ctxts.push_back( node_ctxt(par, false) );
			assert( par_ctxts.back() );
		}

		shared_ptr<value_tyinfo> result_ty = fn().get_return_ty();
		
		// Process Intrinsic
		if( intr->unmangled_name() == "mul" ){
			
			assert( par_tys.size() == 2 );

			// Set Argument name
			fn().arg_name( 0, ".lhs" );
			fn().arg_name( 1, ".rhs" );

			value_t ret_val = emit_mul( fn().arg(0), fn().arg(1) );
			emit_return( ret_val );

		} else if( intr->unmangled_name() == "dot" ) {
			
			assert( par_tys.size() == 2 );

			// Set Argument name
			fn().arg_name( 0, ".lhs" );
			fn().arg_name( 1, ".rhs" );

			value_t ret_val = emit_dot( fn().arg(0), fn().arg(1) );
			emit_return( ret_val );

		}
		else
		{
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	}
}


END_NS_SASL_CODE_GENERATOR();