#include <sasl/include/parser/parse_api.h>

#include <sasl/include/common/lex_context.h>
#include <sasl/include/parser/lexer.h>
#include <sasl/include/parser/grammars.h>

#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/format.hpp>
#include <eflib/include/platform/boost_end.h>

#include <iostream>

namespace sasl{
	namespace common{
		class diag_chat;
	}
}
using sasl::common::lex_context;
using sasl::common::code_source;
using sasl::common::diag_chat;
using boost::shared_ptr;
using std::cout;
using std::endl;

void sasl::parser::parse( 
	shared_ptr<attribute>& pt_root,
	const std::string& code,
	shared_ptr<lex_context> ctxt,
	lexer& l, grammars& g, diag_chat* diags
	)
{
	sasl::parser::token_seq toks;
	bool tok_result = l.tokenize_with_end(code, ctxt, toks);
	EFLIB_ASSERT( tok_result, "Tokenizing is failed." );	
	token_iterator it = toks.begin();
	g.prog.parse( it, toks.end()-1, pt_root, diags );
}

void sasl::parser::parse( 
	shared_ptr<attribute>& pt_root,
	code_source* src,
	shared_ptr<lex_context > ctxt,
	lexer& l, grammars& g, diag_chat* diags
	)
{
	sasl::parser::token_seq toks;

	l.begin_incremental();
	while( !src->eof() ){
		bool tok_result = l.incremental_tokenize( src->next(), ctxt, toks );
		if( !tok_result ){
			boost::format fmt( "%s(%d): fatal error: unrecognized token: '%s' " );
			std::string etok = src->error();
			if( etok.empty() ){ etok = "<Unrecognized>"; }
			fmt % ctxt->file_name() % ctxt->line() % etok;
			cout << ( boost::str(fmt).c_str() ) << endl;
			assert( !"Tokenize failed!" );
			return;
		}
	}
	l.end_incremental( ctxt, toks );

	token_iterator it = toks.begin();
	g.prog.parse( it, toks.end()-1, pt_root, diags );
}
