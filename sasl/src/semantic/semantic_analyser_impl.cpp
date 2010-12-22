#include <sasl/include/semantic/semantic_analyser_impl.h>

#include <sasl/enums/operators.h>

#include <sasl/include/common/compiler_info_manager.h>
#include <sasl/include/semantic/name_mangler.h>
#include <sasl/include/semantic/semantic_error.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/symbol_scope.h>
#include <sasl/include/semantic/type_checker.h>
#include <sasl/include/semantic/type_converter.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/syntax_tree/make_tree.h>
#include <sasl/include/syntax_tree/program.h>
#include <sasl/include/syntax_tree/statement.h>
#include <sasl/include/syntax_tree/utility.h>

#include <eflib/include/diagnostics/assert.h>
#include <eflib/include/metaprog/util.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/any.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/list_inserter.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/bind.hpp>
#include <boost/bind/apply.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

BEGIN_NS_SASL_SEMANTIC();

using ::sasl::common::compiler_info_manager;
using ::sasl::common::token_attr;

using ::sasl::syntax_tree::binary_expression;
using ::sasl::syntax_tree::buildin_type;
using ::sasl::syntax_tree::create_buildin_type;
using ::sasl::syntax_tree::create_node;
using ::sasl::syntax_tree::compound_statement;
using ::sasl::syntax_tree::constant_expression;
using ::sasl::syntax_tree::declaration;
using ::sasl::syntax_tree::expression;
using ::sasl::syntax_tree::function_type;
using ::sasl::syntax_tree::jump_statement;
using ::sasl::syntax_tree::node;
using ::sasl::syntax_tree::parameter;
using ::sasl::syntax_tree::program;
using ::sasl::syntax_tree::statement;
using ::sasl::syntax_tree::type_specifier;

using ::sasl::syntax_tree::dfunction_combinator;

using ::sasl::semantic::errors::semantic_error;
using ::sasl::semantic::extract_semantic_info;
using ::sasl::semantic::get_or_create_semantic_info;
using ::sasl::semantic::program_si;
using ::sasl::semantic::symbol;

using namespace boost::assign;

using boost::any;
using boost::any_cast;
using boost::format;
using boost::shared_ptr;
using boost::unordered_map;

using std::vector;

#define SASL_GET_OR_CREATE_SI( si_type, si_var, node ) shared_ptr<si_type> si_var = get_or_create_semantic_info<si_type>(node);
#define SASL_GET_OR_CREATE_SI_P( si_type, si_var, node, param ) shared_ptr<si_type> si_var = get_or_create_semantic_info<si_type>( node, param );

#define SASL_EXTRACT_SI( si_type, si_var, node ) shared_ptr<si_type> si_var = extract_semantic_info<si_type>(node);

// semantic analysis context
struct sacontext{
	shared_ptr<global_si> gsi;
	shared_ptr<symbol> parent_sym;

	shared_ptr<node> generated_node;
};


sacontext* any_to_ctxt_ptr( any& any_val ){
	return any_cast<sacontext>(&any_val);
}

sacontext const * any_to_ctxt_ptr( const any& any_val ){
	return any_cast<sacontext>(&any_val);
}

#define data_as_ctxt_ptr() ( any_to_ctxt_ptr(*data) )

// utility functions

shared_ptr<type_specifier> type_info_of( shared_ptr<node> n ){
	shared_ptr<type_info_si> typesi = extract_semantic_info<type_info_si>( n );
	if ( typesi ){
		return typesi->type_info();
	}
	return shared_ptr<type_specifier>();
}

semantic_analyser_impl::semantic_analyser_impl()
{
	typeconv.reset( new type_converter() );
}

#define SASL_VISITOR_TYPE_NAME semantic_analyser_impl

template <typename NodeT> any& semantic_analyser_impl::visit_child( any& child_ctxt, const any& init_data, shared_ptr<NodeT> child )
{
	child_ctxt = init_data;
	child->accept( this, &child_ctxt );
	return child_ctxt;
}

template <typename NodeT> any& semantic_analyser_impl::visit_child( any& child_ctxt, shared_ptr<NodeT> child )
{
	return visit_child( child_ctxt, child_ctxt, child );
}

template <typename NodeT> any& semantic_analyser_impl::visit_child(
	any& child_ctxt, const any& init_data,
	shared_ptr<NodeT> child, shared_ptr<NodeT>& generated_node )
{
	visit_child( child_ctxt, init_data, child );
	if( any_to_ctxt_ptr( child_ctxt )->generated_node ){
		generated_node = any_to_ctxt_ptr(child_ctxt)->generated_node->typed_handle<NodeT>();
	}
	return child_ctxt;
}

SASL_VISIT_NOIMPL( unary_expression );
SASL_VISIT_NOIMPL( cast_expression );
SASL_VISIT_DEF( binary_expression )
{
	any child_ctxt_init = *data;
	any_to_ctxt_ptr(child_ctxt_init)->generated_node.reset();

	shared_ptr<binary_expression> dup_expr = duplicate( v.handle() )->typed_handle<binary_expression>();

	any child_ctxt;
	visit_child( child_ctxt, child_ctxt_init, v.left_expr, dup_expr->left_expr );
	visit_child( child_ctxt, child_ctxt_init, v.right_expr, dup_expr->right_expr );

	// TODO: look up operator prototype.
	std::string opname = operator_name( v.op );
	vector< shared_ptr<expression> > exprs;
	exprs += dup_expr->left_expr, dup_expr->right_expr;
	vector< shared_ptr<symbol> > overloads = data_as_ctxt_ptr()->parent_sym->find_overloads( opname, typeconv, exprs );

	EFLIB_ASSERT_AND_IF( !overloads.empty(), "Need to report a compiler error. No overloading." ){
		return;
	}
	EFLIB_ASSERT_AND_IF( overloads.size() == 1,	( format(
		"Need to report a compiler error. Ambigous overloading. \r\n"
		"operator is %1%, left expression type is %2%, right expression type is %3%. \r\n" 
		) % v.op.name() % type_info_of( dup_expr->left_expr )->value_typecode.name()
		% type_info_of( dup_expr->right_expr )->value_typecode.name() ).str().c_str()
		)
	{
		return;
	}

	// update semantic information of binary expression
	type_entry::id_t result_tid = extract_semantic_info<type_info_si>( overloads[0]->node() )->entry_id();
	get_or_create_semantic_info<storage_si>( dup_expr, data_as_ctxt_ptr()->gsi->type_manager() )->entry_id( result_tid );

	data_as_ctxt_ptr()->generated_node = dup_expr->handle();
}

SASL_VISIT_NOIMPL( expression_list );
SASL_VISIT_NOIMPL( cond_expression );
SASL_VISIT_NOIMPL( index_expression );
SASL_VISIT_NOIMPL( call_expression );
SASL_VISIT_NOIMPL( member_expression );

SASL_VISIT_DEF( constant_expression )
{
	shared_ptr<constant_expression> dup_cexpr = duplicate( v.handle() )->typed_handle<constant_expression>();
	
	SASL_GET_OR_CREATE_SI_P( const_value_si, vsi, dup_cexpr, gctxt->type_manager() );
	vsi->set_literal( v.value_tok->str, v.ctype );

	data_as_ctxt_ptr()->generated_node = dup_cexpr->handle();	
}

SASL_VISIT_DEF( variable_expression ){
	UNREF_PARAM( v );
	UNREF_PARAM( data );

	// do nothing
}

// declaration & type specifier
SASL_VISIT_NOIMPL( initializer );
SASL_VISIT_DEF( expression_initializer )
{
	v.init_expr->accept(this, data);
}

SASL_VISIT_NOIMPL( member_initializer );
SASL_VISIT_NOIMPL( declaration );

SASL_VISIT_DEF( variable_declaration )
{
	//symbol_scope sc( v.name->str, v.handle(), cursym );

	//// process variable type
	//shared_ptr<type_specifier> vartype = v.type_info;
	//vartype->accept( this, data );
	//shared_ptr<type_si> tsi = extract_semantic_info<type_si>(v);

	//// check type.
	//if ( tsi->type_type() == type_types::buildin ){
	//	// TODO: ALLOCATE BUILD-IN TYPED VAR.
	//} else if ( tsi->type_type() == type_types::composited ){
	//	// TODO: ALLOCATE COMPOSITED TYPED VAR.
	//} else if ( tsi->type_type() == type_types::alias ){
	//	if ( typeseminfo->full_type() ){
	//		// TODO: ALLOCATE ACTUAL
	//	} else {
	//		infomgr->add_info( semantic_error::create( compiler_informations::uses_a_undef_type,
	//			v.handle(), list_of( typeseminfo->full_type() ) )
	//			);
	//		// remove created symbol
	//		cursym->remove();
	//		return;
	//	}
	//}

	// process initializer
	// v.init->accept( this, data );;
}

SASL_VISIT_DEF( type_definition ){
	//const std::string& alias_str = v.name->str;
	//shared_ptr<symbol> existed_sym = cursym->find( alias_str );
	//if ( existed_sym ){
	//	// if the symbol is used and is not a type node, it must be redifinition.
	//	// else compare the type.
	//	if ( !existed_sym->node()->node_class().included( syntax_node_types::type_specifier ) ){
	//		gctxt->compiler_infos()->add_info(
	//			semantic_error::create( compiler_informations::redef_cannot_overloaded,
	//			v.handle(),	list_of(existed_sym->node()) )
	//				);
	//		return;
	//	}
	//}

	//// process type node.
	//// remove old sym from symbol table.
	//cursym->remove_child( v.name->str );
	//{
	//	symbol_scope sc( v.name->str, v.handle(), cursym );

	//	v.type_info->accept(this, data);
	//	shared_ptr<type_si> new_tsi = extract_semantic_info<type_si>(v);

	//	// if this symbol is usable, process type node.
	//	if ( existed_sym ){
			//shared_ptr<type_semantic_info> existed_tsi = extract_semantic_info<type_semantic_info>( existed_sym->node() );
			//if ( !type_equal(existed_tsi->full_type(), new_tsi->full_type()) ){
			//	// if new symbol is different from the old, semantic error.
			//	// The final effect is that the new definition overwrites the old one.

			//	infomgr->add_info(
			//		semantic_error::create( compiler_informations::redef_diff_basic_type,
			//		v.handle(),	list_of(existed_sym->node()) )
			//		);
			//}
		//}
		// else if the same. do not updated.
		// NOTE:
		//   MAYBE IT NEEDS COMBINE OLD AND NEW SYMBOL INFOS UNDER SOME CONDITIONS.
		//   BUT I CAN NOT FIND OUT ANY EXAMPLE.
	//}
}

SASL_VISIT_NOIMPL( type_specifier );
SASL_VISIT_DEF( buildin_type ){
	// create type information on current symbol.
	// for e.g. create type info onto a variable node.
	SASL_GET_OR_CREATE_SI_P( type_si, tsi, v, gctxt->type_manager() );
	tsi->type_info( v.typed_handle<type_specifier>(), data_as_ctxt_ptr()->parent_sym );

	data_as_ctxt_ptr()->generated_node = tsi->type_info()->handle();
}

SASL_VISIT_NOIMPL( array_type );
SASL_VISIT_NOIMPL( struct_type );

SASL_VISIT_DEF( parameter )
{
	shared_ptr<parameter> dup_par = duplicate( v.handle() )->typed_handle<parameter>();
	data_as_ctxt_ptr()->parent_sym->add_child( v.name ? v.name->str : std::string(), dup_par );

	any child_ctxt;
	visit_child( child_ctxt, *data, v.param_type, dup_par->param_type );
	if ( v.init ){
		visit_child( child_ctxt, *data, v.init, dup_par->init );
	}

	type_entry::id_t tid = extract_semantic_info<type_info_si>(dup_par->param_type)->entry_id();
	get_or_create_semantic_info<storage_si>( dup_par, gctxt->type_manager() )->entry_id( tid );

	data_as_ctxt_ptr()->generated_node = dup_par->handle();
}

SASL_VISIT_DEF( function_type )
{
	// Copy node
	shared_ptr<node> dup_node = duplicate( v.handle() );
	EFLIB_ASSERT_AND_IF( dup_node, "Node swallow duplicated error !"){
		return;
	}
	shared_ptr<function_type> dup_fn = dup_node->typed_handle<function_type>();
	dup_fn->params.clear();

	shared_ptr<symbol> sym = data_as_ctxt_ptr()->parent_sym->add_function_begin( dup_fn );

	any child_ctxt_init = *data;
	any_to_ctxt_ptr(child_ctxt_init)->parent_sym = sym;

	any child_ctxt;

	dup_fn->retval_type
		= any_to_ctxt_ptr( visit_child( child_ctxt, child_ctxt_init, v.retval_type ) )->generated_node->typed_handle<type_specifier>();

	for( vector< shared_ptr<parameter> >::iterator it = v.params.begin();
		it != v.params.end(); ++it )
	{
		dup_fn->params.push_back( 
			any_to_ctxt_ptr( visit_child(child_ctxt, child_ctxt_init, *it) )->generated_node->typed_handle<parameter>()
			);
	}

	data_as_ctxt_ptr()->parent_sym->add_function_end( sym );
	
	type_entry::id_t ret_tid = extract_semantic_info<type_info_si>( dup_fn->retval_type )->entry_id();
	get_or_create_semantic_info<storage_si>( dup_fn, data_as_ctxt_ptr()->gsi->type_manager() )->entry_id( ret_tid );

	if ( v.body ){
		visit_child( child_ctxt, child_ctxt_init, v.body, dup_fn->body );
	}

	data_as_ctxt_ptr()->generated_node = dup_fn;
}

// statement
SASL_VISIT_NOIMPL( statement );

SASL_VISIT_DEF( declaration_statement )
{
	v.decl->accept(this, data);
}

SASL_VISIT_DEF( if_statement )
{
	v.cond->accept( this, data );;
	v.yes_stmt->accept( this, data );;
	v.no_stmt->accept(this, data);
}

SASL_VISIT_DEF( while_statement ){
	v.cond->accept( this, data );;
	v.body->accept( this, data );;
}

SASL_VISIT_NOIMPL( dowhile_statement );
SASL_VISIT_NOIMPL( case_label );
SASL_VISIT_NOIMPL( ident_label );
SASL_VISIT_NOIMPL( switch_statement );

SASL_VISIT_DEF( compound_statement )
{
	shared_ptr<compound_statement> dup_stmt = duplicate(v.handle())->typed_handle<compound_statement>();
	dup_stmt->stmts.clear();

	any child_ctxt_init = *data;
	any_to_ctxt_ptr(child_ctxt_init)->parent_sym = data_as_ctxt_ptr()->parent_sym->add_anonymous_child( dup_stmt );
	any_to_ctxt_ptr(child_ctxt_init)->generated_node.reset();

	any child_ctxt;
	for( vector< shared_ptr<statement> >::iterator it = v.stmts.begin();
		it != v.stmts.end(); ++it)
	{
		shared_ptr<statement> child_gen;
		visit_child( child_ctxt, child_ctxt_init, (*it), child_gen );
		dup_stmt->stmts.push_back(child_gen);
	}

	data_as_ctxt_ptr()->generated_node = dup_stmt->handle();
}

SASL_VISIT_DEF( expression_statement ){
	v.expr->accept( this, data );
}

SASL_VISIT_DEF( jump_statement )
{
	shared_ptr<jump_statement> dup_jump = duplicate(v.handle())->typed_handle<jump_statement>();

	any child_ctxt_init = *data;
	any_to_ctxt_ptr( child_ctxt_init )->generated_node.reset();

	if (v.code == jump_mode::_return){
		if( v.jump_expr ){
			any child_ctxt;
			visit_child( child_ctxt, child_ctxt_init, v.jump_expr, dup_jump->jump_expr );
		}
	}

	data_as_ctxt_ptr()->generated_node = dup_jump;
}

// program
SASL_VISIT_DEF( program ){
	// create semantic info
	gctxt.reset( new global_si() );

	any child_ctxt_init = sacontext();
	any_to_ctxt_ptr(child_ctxt_init)->gsi = gctxt;
	any_to_ctxt_ptr(child_ctxt_init)->parent_sym = gctxt->root();

	any child_ctxt = child_ctxt_init;

	register_buildin_types();
	register_type_converter( child_ctxt_init );
	register_buildin_functions( child_ctxt_init );

	shared_ptr<program> dup_prog = duplicate( v.handle() )->typed_handle<program>();
	dup_prog->decls.clear();

	// analysis decalarations.
	for( vector< shared_ptr<declaration> >::iterator it = v.decls.begin(); it != v.decls.end(); ++it ){
		shared_ptr<declaration> node_gen;
		visit_child( child_ctxt, child_ctxt_init, (*it), node_gen );
		dup_prog->decls.push_back( node_gen );
	}

	gctxt->root()->relink( dup_prog->handle() );
}

SASL_VISIT_NOIMPL( for_statement );

void semantic_analyser_impl::buildin_type_convert( shared_ptr<node> lhs, shared_ptr<node> rhs ){
	// do nothing
}

void semantic_analyser_impl::register_type_converter( const boost::any& ctxt ){
	// register default type converter
	type_manager* typemgr = any_to_ctxt_ptr(ctxt)->gsi->type_manager().get();

	type_entry::id_t sint8_ts = typemgr->get( buildin_type_code::_sint8 );
	type_entry::id_t sint16_ts = typemgr->get( buildin_type_code::_sint16 );
	type_entry::id_t sint32_ts = typemgr->get( buildin_type_code::_sint32 );
	type_entry::id_t sint64_ts = typemgr->get( buildin_type_code::_sint64 );

	type_entry::id_t uint8_ts = typemgr->get( buildin_type_code::_uint8 );
	type_entry::id_t uint16_ts = typemgr->get( buildin_type_code::_uint16 );
	type_entry::id_t uint32_ts = typemgr->get( buildin_type_code::_uint32 );
	type_entry::id_t uint64_ts = typemgr->get( buildin_type_code::_uint64 );

	type_entry::id_t float_ts = typemgr->get( buildin_type_code::_float );
	type_entry::id_t double_ts = typemgr->get( buildin_type_code::_double );

	type_entry::id_t bool_ts = typemgr->get( buildin_type_code::_boolean );

	// default conversation will do nothing.
	type_converter::converter_t default_conv = bind(&semantic_analyser_impl::buildin_type_convert, this, _1, _2);

	typeconv->register_converter( type_converter::implicit_conv, sint8_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint8_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint8_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint8_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint8_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint8_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint8_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint8_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint8_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint8_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, sint16_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint16_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint16_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint16_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint16_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint16_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint16_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint16_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint16_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint16_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, sint32_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint32_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint32_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint32_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint32_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint32_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint32_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, sint32_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint32_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint32_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, sint64_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint64_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint64_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint64_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint64_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint64_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, sint64_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, sint64_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, sint64_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, sint64_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, uint8_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint8_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, uint16_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint16_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint16_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint16_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint16_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint16_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint16_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint16_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint16_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint16_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, uint32_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint32_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint32_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::better_conv, uint32_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint32_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint32_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint32_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, uint32_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint32_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint32_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, uint64_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint64_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint64_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint64_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint64_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint64_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, uint64_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, uint64_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, uint64_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, uint64_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, float_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, float_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, float_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, float_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, float_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, float_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, float_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, float_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::implicit_conv, float_ts, double_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, float_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, double_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, double_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, double_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, double_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::warning_conv, double_ts, bool_ts, default_conv );

	typeconv->register_converter( type_converter::explicit_conv, bool_ts, sint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, sint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, sint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, sint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, uint8_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, uint16_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, uint32_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, uint64_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, float_ts, default_conv );
	typeconv->register_converter( type_converter::explicit_conv, bool_ts, double_ts, default_conv );

}

void semantic_analyser_impl::register_buildin_functions( const boost::any& child_ctxt_init ){
	any child_ctxt;

	typedef unordered_map<
		buildin_type_code, shared_ptr<buildin_type>, enum_hasher
		> bt_table_t;
	bt_table_t standard_bttbl;
	bt_table_t storage_bttbl;
	map_of_buildin_type( standard_bttbl, &sasl_ehelper::is_standard );
	map_of_buildin_type( storage_bttbl, &sasl_ehelper::is_storagable );

	shared_ptr<buildin_type> bt_bool = storage_bttbl[ buildin_type_code::_boolean ];
	shared_ptr<buildin_type> bt_i32 = storage_bttbl[ buildin_type_code::_sint32 ];

	shared_ptr<function_type> tmpft;

	// arithmetic operators
	vector<std::string> op_tbl;
	const vector<operators>& oplist = sasl_ehelper::list_of_operators();

	for( size_t i_op = 0; i_op < oplist.size(); ++i_op ){
		operators op = oplist[i_op];
		std::string op_name( operator_name(op) );

		if ( sasl_ehelper::is_arithmetic(op) ){
			for( bt_table_t::iterator it_type = standard_bttbl.begin(); it_type != standard_bttbl.end(); ++it_type ){
				dfunction_combinator(NULL).dname( op_name )
					.dreturntype().dnode( it_type->second ).end()
					.dparam().dtype().dnode( it_type->second ).end().end()
					.dparam().dtype().dnode( it_type->second ).end().end()
				.end( tmpft );

				if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
			}
		}

		if( sasl_ehelper::is_arith_assign(op) ){
			for( bt_table_t::iterator it_type = standard_bttbl.begin(); it_type != standard_bttbl.end(); ++it_type ){
				dfunction_combinator(NULL).dname( op_name )
					.dreturntype().dnode( it_type->second ).end()
					.dparam().dtype().dnode( it_type->second ).end().end()
					.dparam().dtype().dnode( it_type->second ).end().end()
					.end( tmpft );
				if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
			}
		}

		if( sasl_ehelper::is_relationship(op) ){

			for( bt_table_t::iterator it_type = standard_bttbl.begin(); it_type != standard_bttbl.end(); ++it_type ){
				dfunction_combinator(NULL).dname( op_name )
					.dreturntype().dnode( bt_bool ).end()
					.dparam().dtype().dnode( it_type->second ).end().end()
					.dparam().dtype().dnode( it_type->second ).end().end()
					.end( tmpft );
				if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
			}
		}

		if( sasl_ehelper::is_bit(op) || sasl_ehelper::is_bit_assign(op) ){
			for( bt_table_t::iterator it_type = standard_bttbl.begin(); it_type != standard_bttbl.end(); ++it_type ){
				if ( sasl_ehelper::is_integer(it_type->first) ){
					dfunction_combinator(NULL).dname( op_name )
						.dreturntype().dnode( it_type->second ).end()
						.dparam().dtype().dnode( it_type->second ).end().end()
						.dparam().dtype().dnode( it_type->second ).end().end()
					.end( tmpft );
					if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
				}
			}
		}

		if( sasl_ehelper::is_shift(op) || sasl_ehelper::is_shift_assign(op) ){
			for( bt_table_t::iterator it_type = standard_bttbl.begin(); it_type != standard_bttbl.end(); ++it_type ){
				if ( sasl_ehelper::is_integer(it_type->first) ){
					dfunction_combinator(NULL).dname( op_name )
						.dreturntype().dnode( it_type->second ).end()
						.dparam().dtype().dnode( it_type->second ).end().end()
						.dparam().dtype().dnode( bt_i32 ).end().end()
						.end( tmpft );
					if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
				}
			}
		}

		if( sasl_ehelper::is_bool_arith(op) ){
			dfunction_combinator(NULL).dname( op_name )
				.dreturntype().dnode( bt_bool ).end()
				.dparam().dtype().dnode( bt_bool ).end().end()
				.dparam().dtype().dnode( bt_bool ).end().end()
			.end( tmpft );
			if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
		}

		if( sasl_ehelper::is_prefix(op) || sasl_ehelper::is_postfix(op) || op == operators::positive ){
			for( bt_table_t::iterator it_type = standard_bttbl.begin(); it_type != standard_bttbl.end(); ++it_type ){
				if ( sasl_ehelper::is_integer(it_type->first) ){
					dfunction_combinator(NULL).dname( op_name )
						.dreturntype().dnode( it_type->second ).end()
						.dparam().dtype().dnode( it_type->second ).end().end()
						.end( tmpft );

					if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
				}
			}
		}

		if( op == operators::bit_not ){
			for( bt_table_t::iterator it_type = standard_bttbl.begin(); it_type != standard_bttbl.end(); ++it_type ){
				if ( sasl_ehelper::is_integer(it_type->first) ){
					dfunction_combinator(NULL).dname( op_name )
						.dreturntype().dnode( it_type->second ).end()
						.dparam().dtype().dnode( it_type->second ).end().end()
						.end( tmpft );

					if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
				}
			}
		}

		if( op == operators::logic_not ){
			dfunction_combinator(NULL).dname( op_name )
				.dreturntype().dnode( bt_bool ).end()
				.dparam().dtype().dnode( bt_bool ).end().end()
			.end( tmpft );

			if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
		}

		if( op == operators::negative ){
			for( bt_table_t::iterator it_type = standard_bttbl.begin(); it_type != standard_bttbl.end(); ++it_type ){
				if ( it_type->first != buildin_type_code::_uint64 ){
					dfunction_combinator(NULL).dname( op_name )
						.dreturntype().dnode( it_type->second ).end()
						.dparam().dtype().dnode( it_type->second ).end().end()
					.end( tmpft );

					if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
				}
			}
		}

		if ( op == operators::assign ){
			for( bt_table_t::iterator it_type = storage_bttbl.begin(); it_type != storage_bttbl.end(); ++it_type ){
				dfunction_combinator(NULL).dname( op_name )
					.dreturntype().dnode( it_type->second ).end()
					.dparam().dtype().dnode( it_type->second ).end().end()
					.dparam().dtype().dnode( it_type->second ).end().end()
				.end( tmpft );

				if ( tmpft ){ visit_child( child_ctxt, child_ctxt_init, tmpft ); }
			}
		}
	}
}

void semantic_analyser_impl::register_buildin_types(){
	BOOST_FOREACH( buildin_type_code const & btc, sasl_ehelper::list_of_buildin_type_codes() ){
		EFLIB_ASSERT( gctxt->type_manager()->get( btc ) > -1, "Register buildin type failed!" );
	}
}

boost::shared_ptr<global_si> semantic_analyser_impl::global_semantic_info() const{
	return gctxt;
}

END_NS_SASL_SEMANTIC();
