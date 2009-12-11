#ifndef SASL_PARSER_TOKEN_H
#define SASL_PARSER_TOKEN_H

#include "../../enums/token_types.h"
#include <boost/spirit/include/lex.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/mpl/vector.hpp>
#include <string>

typedef boost::mpl::vector< token_attr > sasl_token_attr_type;
typedef boost::spirit::lex::lexertl::token< const char*, sasl_token_attr_type > sasl_token_type;
typedef boost::spirit::lex::lexertl::actor_lexer< sasl_token_type > sasl_lexer_base;

struct lex_context{
	lex_context(): line(0), column(0) {}
	size_t line;
	size_t column;
};

struct token_attribute_setter{
	token_attribute_setter( lex_context& ctxt ): lex_ctxt( ctxt ){
	}
	token_attribute_setter( const token_attribute_setter& rhs ): lex_ctxt( rhs.lex_ctxt ){
	}

	template <typename IteratorT, typename PassFlagT, typename IdtypeT, typename ContextT>
	void operator () (IteratorT& start, IteratorT& end, PassFlagT& matched, IdtypeT& id, ContextT& ctx){
		string lit;
		lit.assign( start, end );
		ctx.set_value( token_attr( lit, lex_ctxt.line, lex_ctxt.column ) );
		lex_ctxt.column += lit.length();
	}

	lex_context& lex_ctxt;
};

template <typename BaseLexerT>
struct sasl_tokens : public boost::spirit::lex::lexer< BaseLexerT > {
	sasl_tokens(): attr_setter(ctxt){
		using boost::spirit::qi::_val;
		using boost::spirit::qi::_1;
		using boost::phoenix::ref;

		(littok_int = "[0-9]+").id( token_types::_constant.to_value() );
		(optok_add = "[\\+]").id( token_types::_operator.to_value() );

		this->self = 
			littok_int	[attr_setter]
			| optok_add	[attr_setter];
	}

	boost::spirit::lex::token_def<token_attr> littok_int, optok_add;
	lex_context ctxt;
	token_attribute_setter attr_setter;
};

typedef sasl_tokens<sasl_lexer_base> sasl_tokenizer;
typedef sasl_tokenizer::iterator_type sasl_token_iterator;

#endif //SASL_PARSER_TOKEN_H