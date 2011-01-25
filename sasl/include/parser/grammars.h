#ifndef SASL_PARSER_GRAMMARS_H
#define SASL_PARSER_GRAMMARS_H

#include <sasl/include/parser/grammars/declaration.h>
#include <sasl/include/parser/grammars/declaration_specifier.h>
#include <sasl/include/parser/grammars/expression.h>
#include <sasl/include/parser/grammars/initialized_declarator_list.h>
#include <sasl/include/parser/grammars/initializer.h>
#include <sasl/include/parser/grammars/literal_constant.h>
#include <sasl/include/parser/grammars/program.h>
#include <sasl/include/parser/grammars/statement.h>
#include <sasl/include/parser/grammars/token.h>

#include <boost/preprocessor.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#define SASL_SPECIALIZED_GRAMMAR_T( grammar_template_type ) grammar_template_type <IteratorT, LexerT>

#define SASL_GRAMMAR_INSTANCE_ACCESSOR( grammar_type, grammar_name ) \
	private: boost::shared_ptr< grammar_type > BOOST_PP_CAT(grammar_name,_); \
	public: grammar_type & grammar_name () { return get( BOOST_PP_CAT(grammar_name,_) ); } \
	public: void grammar_name ( grammar_type& g ) { set( g , BOOST_PP_CAT(grammar_name,_) ); }

#define SASL_GRAMMAR_INSTANCE_ACCESSOR_I( r, data, grammar_type_name_tuple ) \
	SASL_GRAMMAR_INSTANCE_ACCESSOR( \
		SASL_SPECIALIZED_GRAMMAR_T( BOOST_PP_TUPLE_ELEM(2, 0, grammar_type_name_tuple ) ), \
		BOOST_PP_TUPLE_ELEM(2, 1, grammar_type_name_tuple ) \
	)

#define SASL_GRAMMAR_INSTANCE_ACCESSORS( grammar_type_name_tuple_seq ) \
	BOOST_PP_SEQ_FOR_EACH( SASL_GRAMMAR_INSTANCE_ACCESSOR_I, 0, grammar_type_name_tuple_seq )

#define SASL_GRAMMAR_INSTANCE_INITIALIZATION_I( r, data, grammar_type_name_tuple ) \
	BOOST_PP_CAT(BOOST_PP_TUPLE_ELEM( 2, 1, grammar_type_name_tuple ), _).reset();

#define SASL_GRAMMAR_INSTANCE_INITIALIZATIONS( grammar_type_name_tuple_seq ) \
	BOOST_PP_SEQ_FOR_EACH( SASL_GRAMMAR_INSTANCE_INITIALIZATION_I, 0, grammar_type_name_tuple_seq )

#define SASL_GRAMMAR_TYPE_NAME_TUPLE_SEQ()					\
	((assignment_expression_grammar, assign_expr))			\
	((binary_expression_grammar, bin_expr))					\
	((cast_expression_grammar, cast_expr))					\
	((declaration_grammar, decl))							\
	((declaration_specifier_grammar, decl_spec))			\
	((expression_grammar, expr))							\
	((expression_list_grammar, expr_list))					\
	((initialized_declarator_list_grammar, initdecl_list))	\
	((initializer_grammar, init))							\
	((primary_expression_grammar, primary_expr))			\
	((literal_constant_grammar, lit_const))					\
	((program_grammar, prog))								\
	((statement_grammar, stmt))								\
	((struct_declaration_grammar, struct_decl))				\
	((variable_declaration_grammar, vardecl))				

template <typename IteratorT, typename LexerT, typename TokenDefT>
class sasl_grammar{
public:
	typedef sasl_grammar<IteratorT, LexerT, TokenDefT> this_type;
	
	sasl_grammar( const TokenDefT& tok ): tok(tok){
		SASL_GRAMMAR_INSTANCE_INITIALIZATIONS( SASL_GRAMMAR_TYPE_NAME_TUPLE_SEQ() );
	}
	
	SASL_GRAMMAR_INSTANCE_ACCESSORS( SASL_GRAMMAR_TYPE_NAME_TUPLE_SEQ() )

private:
	sasl_grammar( const sasl_grammar& );
	sasl_grammar& operator = ( const sasl_grammar& );

	template<typename GrammarT> GrammarT& get( boost::shared_ptr<GrammarT>& ptr){
		if( !ptr ){
			new GrammarT( tok, *this );
		}
		return *ptr;
	}

	template<typename GrammarT> void set(GrammarT& val, boost::shared_ptr<GrammarT>& ptr){
		ptr.reset( boost::addressof(val) );
	}

	TokenDefT const & tok;
};

#endif