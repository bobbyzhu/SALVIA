#include <sasl/include/syntax_tree/make_tree.h>

#include <sasl/enums/buildin_type_code.h>
#include <sasl/include/common/token_attr.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/syntax_tree/program.h>
#include <sasl/include/syntax_tree/statement.h>

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_base_of.hpp>
#include <boost/type_traits/is_same.hpp>

#define DEFAULT_STATE_SCOPE() state_scope ss(this, e_other);

BEGIN_NS_SASL_SYNTAX_TREE();

using ::sasl::common::token_attr;

literal_constant_types typecode_map::type_codes[] =
{
	literal_constant_types::boolean,
	literal_constant_types::integer,
	literal_constant_types::integer,
	literal_constant_types::integer,
	literal_constant_types::integer,
	literal_constant_types::integer,
	literal_constant_types::integer,
	literal_constant_types::integer,
	literal_constant_types::integer,
	literal_constant_types::real,
	literal_constant_types::real
};

SASL_TYPED_NODE_ACCESSORS_IMPL( tree_combinator, node );

tree_combinator& tree_combinator::do_end()
{
	// parent may release it, so reserve what we need for using later.
	tree_combinator* ret_p = parent;

	if( ret_p ){
		ret_p->child_ended();
		return *ret_p;
	}

	return *this;
}

tree_combinator::~tree_combinator()
{
	assert( is_state(e_none) );
}

void tree_combinator::syntax_error()
{
	assert( !"Fuck!" );
}

void tree_combinator::enter( state_t s )
{
	assert( s != e_none );
	assert( e_state == e_none );
	e_state = s;
}

/////////////////////////////////
// program combinator
SASL_TYPED_NODE_ACCESSORS_IMPL( dprog_combinator, program );

dprog_combinator::dprog_combinator( const std::string& prog_name ):
	tree_combinator(NULL)
{
	typed_node( create_node<program>( prog_name ) );
}

tree_combinator& dprog_combinator::dvar( const std::string& var_name )
{
	enter_child( e_vardecl, var_comb );
	var_comb->typed_node()->name = token_attr::from_string(var_name);
	return *var_comb;
}

tree_combinator& dprog_combinator::dstruct( const std::string& struct_name )
{
	enter_child( e_struct, struct_comb );
	struct_comb->typed_node()->name = token_attr::from_string( struct_name );
	return *struct_comb;
}

void dprog_combinator::child_ended()
{
	switch ( leave() ){
		case e_vardecl:
			typed_node()->decls.push_back( move_node2<declaration>(var_comb) );
			return;
		case e_struct:
			typed_node()->decls.push_back( move_node2<declaration>(struct_comb) );
			return;
		default:
			assert(!"invalid state.");
			return;
	}
}

/////////////////////////////////
// type combinator
SASL_TYPED_NODE_ACCESSORS_IMPL( dtype_combinator, type_specifier );

dtype_combinator::dtype_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
}

tree_combinator& dtype_combinator::dbuildin( buildin_type_code btc )
{
	DEFAULT_STATE_SCOPE();

	if( cur_node ){
		return default_proc();
	}
	
	typed_node( create_node<buildin_type>(token_attr::null()) );
	typed_node()->value_typecode = btc;
	return *this;
}

tree_combinator& dtype_combinator::dvec( buildin_type_code comp_btc, size_t size )
{
	DEFAULT_STATE_SCOPE();

	if ( cur_node ){
		return default_proc();
	}

	typed_node( create_node<buildin_type>(token_attr::null()) );
	typed_node()->value_typecode = btc_helper::vector_of( comp_btc, size );
	return *this;
}

tree_combinator& dtype_combinator::dmat( buildin_type_code comp_btc, size_t s0, size_t s1 )
{
	DEFAULT_STATE_SCOPE();

	if( cur_node ){
		return default_proc();
	}
	typed_node( create_node<buildin_type>(token_attr::null() ) );
	typed_node()->value_typecode = btc_helper::matrix_of(comp_btc, s0, s1);
	return *this;
}

tree_combinator& dtype_combinator::dalias( const std::string& alias )
{
	DEFAULT_STATE_SCOPE();

	if( cur_node ){
		return default_proc();
	}
	typed_node( create_node<struct_type>( token_attr::null() ) );
	typed_node2<struct_type>()->name = token_attr::from_string(alias);
	return *this;
}

tree_combinator& dtype_combinator::dtypequal( type_qualifiers qual )
{
	DEFAULT_STATE_SCOPE();

	if( !cur_node || typed_node()->qual != type_qualifiers::none )
	{
		return default_proc();
	}
	typed_node()->qual = qual;
	return *this;
}

tree_combinator& dtype_combinator::darray()
{
	if ( !cur_node ) { return default_proc(); }
	return enter_child( e_array, expr_comb );
}

void dtype_combinator::child_ended()
{
	boost::shared_ptr<array_type> outter_type;
	switch ( leave() ){
		case e_array:
			assert ( typed_node() );
			if ( typed_node()->node_class() != syntax_node_types::array_type ){
				outter_type = create_node<array_type>( token_attr::null() );
				outter_type->elem_type = typed_node();
				typed_node( outter_type );
			} else {
				outter_type = typed_node2<array_type>();
			}
			outter_type->array_lens.push_back( move_node(expr_comb) );
			return;
		default:
			assert( !"invalid state." );
			return;
	}
}

/////////////////////////////////////
// variable combinator
SASL_TYPED_NODE_ACCESSORS_IMPL( dvar_combinator, variable_declaration );

dvar_combinator::dvar_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
	typed_node( create_node<variable_declaration>( token_attr::null() ) );
}

tree_combinator& dvar_combinator::dname( const std::string& name )
{
	DEFAULT_STATE_SCOPE();

	typed_node()->name = token_attr::from_string(name);
	return *this;
}

tree_combinator& dvar_combinator::dtype()
{
	return enter_child( e_type, type_comb );
}

void dvar_combinator::child_ended()
{
	switch( leave() )
	{
	case e_type:
		typed_node()->type_info = move_node( type_comb );
		break;
	default:
		default_proc();
		break;
	}
}

/////////////////////////////////////////////////////////////////
// expression combinator

SASL_TYPED_NODE_ACCESSORS_IMPL( dexpr_combinator, expression );

dexpr_combinator::dexpr_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
}

tree_combinator& dexpr_combinator::dconstant( literal_constant_types lct, const std::string& v )
{
	DEFAULT_STATE_SCOPE();

	boost::shared_ptr< constant_expression > ret
		= create_node<constant_expression>( token_attr::null() );
	ret->value_tok = token_attr::from_string( v );
	ret->ctype = lct;

	typed_node( ret );
	return *this;
}

tree_combinator& dexpr_combinator::dvarexpr( const std::string& v)
{
	DEFAULT_STATE_SCOPE();

	boost::shared_ptr< variable_expression > ret = create_node<variable_expression>( token_attr::null() );
	ret->var_name = token_attr::from_string( v );
	
	typed_node( ret );
	return *this;
}

tree_combinator& dexpr_combinator::dunary( operators op )
{
	assert( operators_helper::instance().is_unary(op) );
	assert( !typed_node() );

	enter(e_unary);

	boost::shared_ptr< unary_expression > ret = create_node<unary_expression>( token_attr::null() );
	ret->op = op;
	typed_node( ret );
	expr_comb = boost::make_shared<dexpr_combinator>( this );

	return *expr_comb;
}

tree_combinator& dexpr_combinator::dcast()
{
	return enter_child( e_cast, cast_comb );
}

tree_combinator& dexpr_combinator::dbinary()
{
	return enter_child( e_binexpr, binexpr_comb );
}

tree_combinator& dexpr_combinator::dbranchexpr()
{
	return enter_child( e_branchexpr, branch_comb );
}

tree_combinator& dexpr_combinator::dmember( const std::string& m )
{
	DEFAULT_STATE_SCOPE();

	assert( typed_node() );
	boost::shared_ptr<member_expression> mexpr = create_node<member_expression>( token_attr::null() );
	mexpr->expr = typed_node();
	mexpr->member = token_attr::from_string(m);
	typed_node(mexpr);

	return *this;
}

tree_combinator& dexpr_combinator::dcall()
{
	return enter_child( e_callexpr, call_comb );
}

tree_combinator& dexpr_combinator::dindex()
{
	assert ( typed_node() );
	boost::shared_ptr<index_expression> indexexpr = create_node<index_expression>( token_attr::null() );
	indexexpr->expr = typed_node();
	typed_node( indexexpr );
	enter( e_indexexpr );
	expr_comb = boost::make_shared<dexpr_combinator>(this);
	return *expr_comb;
}

void dexpr_combinator::child_ended()
{
	switch( leave() ){
		case e_unary:
			typed_node2<unary_expression>()->expr = move_node( expr_comb );
			return;

		case e_cast:
			typed_node( move_node( cast_comb ) );
			return;

		case e_binexpr:
			typed_node( move_node( binexpr_comb ) );
			return;

		case e_branchexpr:
			typed_node( move_node( branch_comb ) );
			return;

		case e_callexpr:
			typed_node( move_node( call_comb ) );
			return;

		case e_indexexpr:
			typed_node2<index_expression>()->index_expr = move_node( expr_comb );
			return;

		default:
			assert( !"unknown state." );
			return;
	}
}

//////////////////////////////////////////////////////////////////////////
// cast combinator

SASL_TYPED_NODE_ACCESSORS_IMPL( dcast_combinator, cast_expression );

dcast_combinator::dcast_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
	typed_node( create_node<cast_expression>( token_attr::null() ) );
}

tree_combinator& dcast_combinator::dtype()
{
	return enter_child( e_type, type_comb );
}

tree_combinator& dcast_combinator::dexpr()
{
	return enter_child( e_expr, expr_comb );
}

void dcast_combinator::child_ended()
{
	switch ( leave() ){
		case e_type:
			typed_node()->casted_type = move_node( type_comb );
			return;
		case e_expr:
			typed_node()->expr = move_node( expr_comb );
			return;
		default:
			assert( !"invalid state." );
			return;
	}
}

//////////////////////////////////////////////////////////////////////////
// binary expression combinator
SASL_TYPED_NODE_ACCESSORS_IMPL( dbinexpr_combinator, binary_expression );

dbinexpr_combinator::dbinexpr_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
	typed_node( create_node<binary_expression>( token_attr::null() ) );
}

tree_combinator& dbinexpr_combinator::dlexpr()
{
	return enter_child( e_lexpr, lexpr_comb );
}

tree_combinator& dbinexpr_combinator::dop( operators op )
{
	DEFAULT_STATE_SCOPE();

	assert( typed_node()->op == operators::none );
	assert( operators_helper::instance().is_binary(op) );
	typed_node()->op = op;

	return *this;
}

tree_combinator& dbinexpr_combinator::drexpr()
{
	return enter_child( e_rexpr, rexpr_comb );
}

void dbinexpr_combinator::child_ended()
{
	switch( leave() ){
		case e_lexpr:
			typed_node()->left_expr = move_node( lexpr_comb );
			return;
		case e_rexpr:
			assert( rexpr_comb->typed_node() );
			typed_node()->right_expr = move_node( rexpr_comb );
			return;
		default:
			assert( !"invalid state" );
			return;
	}
}

//////////////////////////////////////////////////////////////////////////
//  conditional expression

SASL_TYPED_NODE_ACCESSORS_IMPL( dbranchexpr_combinator, cond_expression );

dbranchexpr_combinator::dbranchexpr_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
	typed_node( create_node<cond_expression>( token_attr::null() ) );
}

tree_combinator& dbranchexpr_combinator::dcond()
{
	return enter_child( e_cond, cond_comb );
}

tree_combinator& dbranchexpr_combinator::dyes()
{
	return enter_child( e_yes, yes_comb );
}

tree_combinator& dbranchexpr_combinator::dno()
{
	return enter_child( e_no, no_comb );
}

void dbranchexpr_combinator::child_ended()
{
	switch( leave() ){
		case e_cond:
			typed_node()->cond_expr = move_node( cond_comb );
			return;
		case e_yes:
			assert( yes_comb->typed_node() );
			typed_node()->yes_expr = yes_comb->typed_node();
			return;
		case e_no:
			assert( no_comb->typed_node() );
			typed_node()->no_expr = no_comb->typed_node();
			return;
		default:
			assert(!"invalid state.");
			return;
	}
}

//////////////////////////////////////////////////////////////////////////
// call expression combinator

SASL_TYPED_NODE_ACCESSORS_IMPL( dcallexpr_combinator, call_expression );

dcallexpr_combinator::dcallexpr_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
	typed_node( create_node<call_expression>( token_attr::null() ) );
	
	assert( parent->typed_node() );
	parent->get_node( typed_node()->expr );
}

tree_combinator& dcallexpr_combinator::dargument()
{
	return enter_child( e_argument, argexpr );
}

void dcallexpr_combinator::child_ended()
{
	switch ( leave() ){
		case e_argument:
			typed_node()->args.push_back( move_node2<expression>(argexpr) );
			return;
		default:
			assert( !"invalid state." );
			return;
	}
}

//////////////////////////////////////////////////////////////////////////
//  struct combinator

SASL_TYPED_NODE_ACCESSORS_IMPL( dstruct_combinator, struct_type );

dstruct_combinator::dstruct_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
	typed_node( create_node<struct_type>( token_attr::null() ) );
}

tree_combinator& dstruct_combinator::dname( const std::string& name )
{
	assert( !typed_node()->name );
	typed_node()->name = token_attr::from_string( name );
	return *this;
}

tree_combinator& dstruct_combinator::dmember( const std::string& var_name )
{
	enter_child(e_vardecl, var_comb);
	var_comb->typed_node()->name = token_attr::from_string(var_name);
	return *var_comb;
}

void dstruct_combinator::child_ended()
{
	switch( leave() )
	{
	case e_vardecl:
		typed_node()->decls.push_back( move_node2<declaration>(var_comb) );
		return;
	default:
		assert("invalid state.");
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
// compound statements

SASL_TYPED_NODE_ACCESSORS_IMPL( dstatements_combinator, compound_statement );

dstatements_combinator::dstatements_combinator( tree_combinator* parent )
: tree_combinator( parent )
{
	typed_node( create_node<compound_statement>( token_attr::null() ) );
}

tree_combinator& dstatements_combinator::dvarstmt()
{
	return enter_child( e_varstmt, var_comb );
}

tree_combinator& dstatements_combinator::dexprstmt()
{
	return enter_child( e_exprstmt, expr_comb );
}

void dstatements_combinator::child_ended()
{
	boost::shared_ptr<declaration_statement> declstmt;
	switch( leave() ){
		case e_varstmt:
			typed_node()->stmts.push_back(
				move_node2<declaration_statement>( var_comb ) );
			break;
		case e_exprstmt:
			typed_node()->stmts.push_back(
				move_node2<expression_statement>( expr_comb ) );
			return;
		default:
			assert(!"invalid state.");
	}
}

//////////////////////////////////////////////////////////////////////////
// declaration statement combinator

dvarstmt_combinator::dvarstmt_combinator( tree_combinator* parent )
: dvar_combinator( parent ){
}

void dvarstmt_combinator::before_end()
{
	assert( typed_node() );
	boost::shared_ptr<declaration_statement> instead_node = create_node<declaration_statement>( token_attr::null() );
	instead_node->decl = typed_node();
	typed_node( instead_node );
}

//////////////////////////////////////////////////////////////////////////
// expression statement combinator

dexprstmt_combinator::dexprstmt_combinator( tree_combinator* parent )
: dexpr_combinator( parent )
{
}

void dexprstmt_combinator::before_end()
{
	assert( typed_node() );
	boost::shared_ptr<expression_statement> instead_node = create_node<expression_statement>( token_attr::null() );
	instead_node->expr = typed_node();
	typed_node( instead_node );
}
END_NS_SASL_SYNTAX_TREE();