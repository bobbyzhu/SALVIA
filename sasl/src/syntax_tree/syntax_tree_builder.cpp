//#include <sasl/include/syntax_tree/syntax_tree_builder.h>
//
//#include <sasl/include/syntax_tree/make_tree.h>
//
//#include <eflib/include/diagnostics/assert.h>
//
//#include <eflib/include/platform/boost_begin.h>
//#include <boost/foreach.hpp>
//#include <eflib/include/platform/boost_end.h>
//
//using boost::shared_ptr;
//using boost::unordered_map;
//using sasl::common::token;
//
//BEGIN_NS_SASL_SYNTAX_TREE();
//
//expression_variant_visitor::expression_variant_visitor( syntax_tree_builder* builder, ast_builder_context& ctxt )
//: base_visitor(builder, ctxt){
//}
//
//shared_ptr<expression> expression_variant_visitor::operator()( const int& /*v*/ )
//{
//	assert(false);
//	return shared_ptr<expression>();
//}
//
//shared_ptr<expression> expression_variant_visitor::operator()( const constant_t& /*v*/ )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<expression>();
//}
//
//template <typename ExpressionT>
//shared_ptr<expression> expression_variant_visitor::operator()( const ExpressionT& v )
//{
//	return builder->build( v, ctxt );
//}
//
//postfix_qualified_expression_visitor::postfix_qualified_expression_visitor(
//	syntax_tree_builder* builder, ast_builder_context& ctxt )
//	:base_visitor(builder, ctxt)
//{
//}
//
//shared_ptr<expression> postfix_qualified_expression_visitor::operator()( const sasl::parser_tree::idx_expression& v )
//{
//	return composite<index_expression>(v);
//}
//
//shared_ptr<expression> postfix_qualified_expression_visitor::operator()( const sasl::parser_tree::call_expression& v )
//{
//	return composite<call_expression>(v);
//}
//
//shared_ptr<expression> postfix_qualified_expression_visitor::operator()( const sasl::parser_tree::mem_expression& v )
//{
//	return composite<member_expression>(v);
//}
//
//shared_ptr<expression> postfix_qualified_expression_visitor::operator()( const token& v )
//{
//	return 
//		dexpr_combinator(NULL)
//			.dunary( builder->build_operator(v, operators::none) )
//			.dexpr().dnode(expr).end()
//		.end().typed_node2<expression>();
//}
//
//shared_ptr<statement> statement_visitor::operator()( const ::sasl::parser_tree::declaration_statement& v )
//{
//	shared_ptr<declaration_statement> ret;
//	shared_ptr<declaration> decl = builder->build(v, ctxt);
//	if( decl ){
//		ret = create_node<declaration_statement>( token::null() );
//		ret->decl = decl;
//	}
//	return ret;
//}
//
//#define INSERT_INTO_TYPEMAP( litname, enum_code ) \
//	{	\
//		shared_ptr<buildin_type> bt = create_node<buildin_type>( token::null() );	\
//		bt->value_typecode = buildin_type_code::enum_code;	\
//		typemap.insert( make_pair( std::string( #litname ), bt ) );	\
//	}
//
//unordered_map< std::string, shared_ptr<buildin_type> > type_visitor::typemap;
//shared_ptr<type_specifier> type_visitor::operator()( token const& v )
//{
//	// Initialize code
//	if( typemap.empty() ){
//		INSERT_INTO_TYPEMAP( sbyte,    _sint8   );
//		INSERT_INTO_TYPEMAP( int8_t,   _sint8   );
//		INSERT_INTO_TYPEMAP( ubyte,    _uint8   );
//		INSERT_INTO_TYPEMAP( uint8_t,  _uint8   );
//		INSERT_INTO_TYPEMAP( short,    _sint16  );
//		INSERT_INTO_TYPEMAP( int16_t,  _sint16  );
//		INSERT_INTO_TYPEMAP( ushort,   _uint16  );
//		INSERT_INTO_TYPEMAP( uint16_t, _uint16  );
//		INSERT_INTO_TYPEMAP( int,      _sint32  );
//		INSERT_INTO_TYPEMAP( int32_t,  _sint32  );
//		INSERT_INTO_TYPEMAP( uint,     _uint32  );
//		INSERT_INTO_TYPEMAP( uint32_t, _uint32  );
//		INSERT_INTO_TYPEMAP( long,     _uint32  );
//		INSERT_INTO_TYPEMAP( int64_t,  _sint64  );
//		INSERT_INTO_TYPEMAP( ulong,    _sint64  );
//		INSERT_INTO_TYPEMAP( uint64_t, _uint64  );
//
//		INSERT_INTO_TYPEMAP( float,    _float   );
//		INSERT_INTO_TYPEMAP( double,   _double  );
//		INSERT_INTO_TYPEMAP( bool,     _boolean );
//		INSERT_INTO_TYPEMAP( void,     _void    );
//	}
//
//	// Type identifier.
//	assert( !v.empty() );
//	if ( typemap.count(v.str) > 0 ){
//		return typemap[v.str];
//	}
//
//	shared_ptr<alias_type> type_ident = create_node<alias_type>( token::null() );
//	type_ident->alias = v.make_copy();
//
//	return type_ident;
//}
//
//template< typename STPostExpressionT, typename ExpressionPostT  >
//shared_ptr<expression> postfix_qualified_expression_visitor::composite( const ExpressionPostT& v )
//{
//	shared_ptr<expression> parent_node = builder->build(v, ctxt);
//	parent_node->typed_handle<STPostExpressionT>()->expr = expr;
//	return parent_node;
//}
//
//operators syntax_tree_builder::build_operator( const sasl::parser_tree::token& /*v*/, operators /*modifier*/ )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return operators::none;
//}
//
//shared_ptr<constant_expression> syntax_tree_builder::build_constant( const token& /*v*/ )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<constant_expression>();
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::unary_expression& v, ast_builder_context& ctxt )
//{
//	return build_variant_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::cast_expression& v, ast_builder_context& ctxt )
//{
//	return build_variant_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::mul_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::add_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::shf_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::rel_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::eql_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::band_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::bxor_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::bor_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::land_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::lor_expression& v, ast_builder_context& ctxt )
//{
//	return build_binary_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::rhs_expression& v, ast_builder_context& ctxt )
//{
//	return build_variant_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::assign_expression& v, ast_builder_context& ctxt )
//{
//	return build_right_combine_binary_expression( v, ctxt );
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::expression& v, ast_builder_context& ctxt )
//{
//	if( v.follow_exprs.empty() ){
//		return build( v.first_expr, ctxt );
//	}
//
//	shared_ptr<expression_list> ret = create_node<expression_list>( token::null() );
//	ret->exprs.push_back( build(v.first_expr, ctxt) );
//
//	for( sasl::parser_tree::expression_lst::expr_list_t::const_iterator it = v.follow_exprs.begin(); it != v.follow_exprs.end(); ++it ){
//		shared_ptr<expression> expr = build( boost::fusion::at_c<1>(*it), ctxt );
//		ret->exprs.push_back( expr );
//	}
//	return ret;
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::expression_post& v, ast_builder_context& ctxt )
//{
//	return build_variant_expression(v, ctxt);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::pm_expression& v, ast_builder_context& ctxt )
//{
//	expression_variant_visitor visitor( this, ctxt );
//	return boost::apply_visitor( visitor, v );
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::typecast_expression& /*v*/, ast_builder_context& /*ctxt*/ )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<expression>();
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::post_expression& /*v*/, ast_builder_context& /*ctxt*/ )
//{
//	//shared_ptr<expression> expr = build(v.expr, ctxt);
//	//for(size_t i = 0; i < v.post.size(); ++i){
//	//	expr = append_expression_post( expr, v.post[i] );
//	//}
//	return shared_ptr<expression>(); //expr;
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::cond_expression& v, ast_builder_context& ctxt )
//{
//	shared_ptr<cond_expression> ret = create_node<cond_expression>( token::null() );
//	ret->cond_expr = build( v.condexpr, ctxt );
//	ret->yes_expr = build( boost::fusion::at_c<1>(v.branchexprs), ctxt );
//	ret->no_expr = build( boost::fusion::at_c<3>(v.branchexprs), ctxt );
//	return ret;
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::idx_expression& v, ast_builder_context& ctxt )
//{
//	shared_ptr<index_expression> ret = create_node<index_expression>( token::null() );
//	ret->index_expr = build(v.expr, ctxt);
//	return shared_ptr<expression>( ret );
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::call_expression& v, ast_builder_context& ctxt )
//{
//	shared_ptr<call_expression> ret = create_node<call_expression>( token::null() );
//	if( v.args ){
//		shared_ptr<expression> arg_exprs = build( *(v.args), ctxt );
//		ret->args = arg_exprs->typed_handle<expression_list>()->exprs;
//	}
//	return ret;
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::mem_expression& v, ast_builder_context& /*ctxt*/ )
//{
//	shared_ptr<member_expression> ret = create_node<member_expression>( token::null() );
//	ret->member = v.ident.make_copy();
//	return ret;
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::unaried_expression& v, ast_builder_context& ctxt )
//{
//	shared_ptr<unary_expression> ret = create_node<unary_expression>( token::null() );
//	ret->expr = build( v.expr, ctxt );
//	ret->op = build_operator( v.preop, operators::none );
//	return shared_ptr<expression>(ret);
//}
//
//shared_ptr<expression> syntax_tree_builder::build( const sasl::parser_tree::paren_expression& v, ast_builder_context& ctxt )
//{
//	return build( v.expr, ctxt );
//}
//
//shared_ptr<statement> syntax_tree_builder::build( const sasl::parser_tree::statement& v, ast_builder_context& ctxt )
//{
//	statement_visitor visitor(this, ctxt);
//	return boost::apply_visitor(visitor, v);
//}
//
//shared_ptr<statement> syntax_tree_builder::build( const sasl::parser_tree::if_statement& v, ast_builder_context& ctxt )
//{
//	shared_ptr<if_statement> ret = create_node<if_statement>(v.if_keyword.make_copy());
//	ret->cond = build(v.cond, ctxt);
//	ret->yes_stmt = build(v.stmt, ctxt);
//	if (v.else_part){
//		ret->no_stmt = build( boost::fusion::at_c<1>(*(v.else_part)), ctxt );
//	}
//	return ret;
//}
//
//shared_ptr<program> syntax_tree_builder::build( const sasl::parser_tree::program& v )
//{
//	
//	shared_ptr<program> prog = create_node<program>( std::string("test") );
//	
//	ast_builder_context ctxt;
//	ctxt.parent = prog;
//
//	BOOST_FOREACH( const sasl::parser_tree::declaration& pt_decl, v ){
//		shared_ptr<declaration> gen_decl = build(pt_decl, ctxt);
//		if( gen_decl ){
//			// if variable declaration with multiple declarator,
//			// it will push itself into parent container and return null.
//			prog->decls.push_back( gen_decl );
//		}
//	}
//	return prog;
//}
//
//shared_ptr<declaration> syntax_tree_builder::build( const sasl::parser_tree::declaration& v, ast_builder_context& ctxt )
//{
//	dispatch_visitor< shared_ptr<declaration> > visitor(this, ctxt);
//	return boost::apply_visitor( visitor, v );
//}
//
//shared_ptr<declaration> syntax_tree_builder::build( const sasl::parser_tree::function_definition& /*v*/, ast_builder_context& /*ctxt*/ )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<declaration>();
//}
//
//shared_ptr<declaration> syntax_tree_builder::build( const sasl::parser_tree::basic_declaration& v, ast_builder_context& ctxt )
//{
//	if (v.decl_body){
//		dispatch_visitor< shared_ptr<declaration> > visitor(this, ctxt);
//		return boost::apply_visitor( visitor, *v.decl_body );
//	}
//	return shared_ptr<declaration>();
//}
//
//shared_ptr<declaration> syntax_tree_builder::build( const sasl::parser_tree::variable_declaration& v, ast_builder_context& ctxt )
//{
//	shared_ptr<variable_declaration> ret = create_node<variable_declaration>( token::null() );
//	ret->type_info = build( v.declspec, ctxt );
//	
//	// build declarators
//	ret->declarators.push_back( build( v.decllist.first, ctxt ) );
//	typedef boost::fusion::vector< token, sasl::parser_tree::initialized_declarator > following_pair_t;
//	BOOST_FOREACH( following_pair_t const & follow_pair, v.decllist.follows	)
//	{
//		ret->declarators.push_back( build( boost::fusion::at_c<1>( follow_pair), ctxt ) );
//	}
//
//	return ret;
//}
//
//shared_ptr<declaration> syntax_tree_builder::build( const sasl::parser_tree::function_declaration& /*v*/, ast_builder_context& /*ctxt*/ )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<declaration>();
//}
//
//shared_ptr<declaration> syntax_tree_builder::build( const sasl::parser_tree::typedef_declaration& /*v*/, ast_builder_context& /*ctxt*/ )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<declaration>();
//}
//
//shared_ptr<declaration> syntax_tree_builder::build( const sasl::parser_tree::struct_declaration& /*v*/, ast_builder_context& /*ctxt*/ )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<declaration>();
//}
//
//shared_ptr<type_specifier> syntax_tree_builder::build( const sasl::parser_tree::declaration_specifier& v, ast_builder_context& ctxt )
//{
//	shared_ptr<type_specifier> inner = build( v.unqual_type, ctxt );
//	return qualify( v.quals, inner );
//}
//
//shared_ptr<declarator> syntax_tree_builder::build( const sasl::parser_tree::initialized_declarator& v, ast_builder_context& ctxt )
//{
//	shared_ptr<declarator> ret = create_node<declarator>( token::null() );
//	if( !v.ident.empty() ){
//		ret->name = v.ident.make_copy();
//	}
//	if( v.init ){
//		ret->init = build( *(v.init), ctxt );
//	}
//	return ret;
//}
//
//shared_ptr<initializer> syntax_tree_builder::build( sasl::parser_tree::initializer const & v, ast_builder_context& ctxt ){
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<initializer>();
//}
//
//boost::shared_ptr<type_specifier> syntax_tree_builder::build( sasl::parser_tree::prefix_qualified_type const & v, ast_builder_context& ctxt )
//{
//	shared_ptr<type_specifier> inner = build( v.unqual_type, ctxt );
//	return qualify( v.quals, inner );
//}
//
//boost::shared_ptr<type_specifier> syntax_tree_builder::build( sasl::parser_tree::unqualified_type const & v, ast_builder_context& ctxt )
//{
//	type_visitor visitor( this, ctxt );
//	return boost::apply_visitor( visitor, v );
//}
//
//boost::shared_ptr<type_specifier> syntax_tree_builder::build( sasl::parser_tree::paren_post_qualified_type const & v, ast_builder_context& ctxt )
//{
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<type_specifier>();
//}
//
//boost::shared_ptr<type_specifier> syntax_tree_builder::qualify( sasl::parser_tree::postfix_qualified_type::qualifiers_t const & quals, boost::shared_ptr<type_specifier> inner )
//{
//	if( quals.empty() ) return inner;
//
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<type_specifier>();
//}
//
//boost::shared_ptr<type_specifier> syntax_tree_builder::qualify( sasl::parser_tree::prefix_qualified_type::qualifiers_t const & quals, boost::shared_ptr<type_specifier> inner )
//{
//	if( quals.empty() ) return inner;
//
//	EFLIB_ASSERT_UNIMPLEMENTED();
//	return shared_ptr<type_specifier>();
//}
//
//END_NS_SASL_SYNTAX_TREE()