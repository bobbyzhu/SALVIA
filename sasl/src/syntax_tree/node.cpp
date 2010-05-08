#include <sasl/include/syntax_tree/node.h>

using namespace boost;

BEGIN_NS_SASL_SYNTAX_TREE();

node::node(syntax_node_types tid, shared_ptr<token_attr> tok )
: type_id(tid), tok(tok)
{
	// DO NOTHING
}

boost::shared_ptr<class ::sasl::semantic::symbol> node::symbol() const{
	return sym;
}

boost::shared_ptr<token_attr> node::token() const{
	return tok;
}

syntax_node_types node::node_class() const{
	return type_id;
}

node::~node(){
	// DO NOTHING
}


END_NS_SASL_SYNTAX_TREE();