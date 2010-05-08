#ifndef SASL_SYNTAX_TREE_NODE_H
#define SASL_SYNTAX_TREE_NODE_H

#include <sasl/include/syntax_tree/syntax_tree_fwd.h>
#include <sasl/include/common/token_attr.h>
#include <sasl/enums/syntax_node_types.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>


namespace sasl{ 
	namespace common{ 
		struct token_attr; 
	}

	namespace semantic{
		class symbol;
	}
}
BEGIN_NS_SASL_SYNTAX_TREE();

class syntax_tree_visitor;

using ::sasl::common::token_attr;

struct node{
	boost::shared_ptr<node> handle() const;
	boost::shared_ptr<class ::sasl::semantic::symbol> symbol() const;
	void symbol( boost::shared_ptr<class ::sasl::semantic::symbol> sym );
	boost::shared_ptr<token_attr> token() const;
	syntax_node_types node_class() const;

	virtual void accept( syntax_tree_visitor* visitor ) = 0;

protected:
	node(syntax_node_types tid, boost::shared_ptr<token_attr> tok);
	node& operator = ( const node& );
	node( const node& );
	
	syntax_node_types				type_id;
	boost::shared_ptr<token_attr>	tok;
	boost::shared_ptr<class ::sasl::semantic::symbol>	sym;
	boost::weak_ptr<node> selfptr;
	virtual ~node();
};

template <typename NodeT> boost::shared_ptr<NodeT> create_node();
template <typename NodeT, typename ParamT> boost::shared_ptr<NodeT> create_node(ParamT);

END_NS_SASL_SYNTAX_TREE();

#define SASL_SYNTAX_NODE_CREATORS() \
	template <typename R> friend boost::shared_ptr<R> create_node(); \
	template <typename R, typename P0> friend boost::shared_ptr<R> create_node( P0 );

#endif //SASL_SYNTAX_TREE_NODE_H