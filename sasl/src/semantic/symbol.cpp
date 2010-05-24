#include <sasl/include/semantic/symbol_info.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/syntax_tree/node.h>

using namespace std;

BEGIN_NS_SASL_SEMANTIC();

boost::shared_ptr<symbol> symbol::create_root( boost::shared_ptr<struct node> root_node ){
	return create( boost::shared_ptr<symbol>(), root_node, std::string("") );
}

boost::shared_ptr<symbol> symbol::create( boost::shared_ptr<symbol> parent, boost::shared_ptr<struct node> correspond_node, const std::string& name ){
	boost::shared_ptr<symbol> ret( new symbol( parent, correspond_node, name ) );
	ret->selfptr = ret;
	correspond_node->symbol( ret );
	return ret;
}

symbol::symbol( boost::shared_ptr<symbol> parent,
			   boost::shared_ptr<struct node> correspond_node,
			   const string& name
			   )
			   :this_parent(parent),
			   correspond_node(correspond_node),
			   symname( name )
{
}

boost::shared_ptr<symbol> symbol::find_this( const std::string& s )
{
	children_iterator_t ret_it = children.find(s);
	if (ret_it == children.end()){
		return boost::shared_ptr<symbol>();
	} else {
		return ret_it->second;
	}
}

boost::shared_ptr<symbol> symbol::find_all( const std::string& s )
{
	boost::shared_ptr<symbol> this_ret = find_this(s);
	if (this_ret) {	return this_ret; }
	if ( !parent() ) { return boost::shared_ptr<symbol>();	}
	return parent()->find_all(s);
}

boost::shared_ptr<symbol> symbol::add_child( const std::string& s, boost::shared_ptr<struct node> child_node )
{
	children_iterator_t ret_it = children.find(s);
	if ( ret_it != children.end() ){
		return boost::shared_ptr<symbol>();
	}
	boost::shared_ptr<symbol> ret = create( selfptr.lock(), child_node, s );
	children.insert( std::make_pair( s, ret ) );
	return ret;
}

void symbol::remove_child( const std::string& s ){
	children_iterator_t ret_it = children.find(s);
	if ( ret_it == children.end() ){
		return;
	}
	// remove symbol from corresponding node.
	boost::shared_ptr<symbol> rmsym = ret_it->second;
	if( rmsym->node() ){
		rmsym->node()->symbol( boost::shared_ptr<symbol>() );
	}
	// remove itself.
	children.erase( ret_it );
}

void symbol::remove_from_tree(){
	if ( parent() ){
		parent()->remove_child( name() );
	}
}

boost::shared_ptr<symbol> symbol::parent() const{
	return this_parent.lock();
}

boost::shared_ptr<class symbol_info> symbol::symbol_info( const std::string& clsname ) const{
	for ( symbol_infos_t::const_iterator it = syminfos.begin(); it != syminfos.end(); ++it ){
		if ( (*it)->class_name() == clsname ){
			return *it;
		}
	}
	return boost::shared_ptr<class symbol_info>();
}

void symbol::symbol_info( boost::shared_ptr<class symbol_info> syminfo ){
	if ( symbol_info(syminfo->class_name()) ){
		return;
	}
	syminfos.push_back( syminfo );
}

boost::shared_ptr<node> symbol::node() const
{
	return correspond_node.lock();
}

void symbol::relink( boost::shared_ptr<struct node> n ){
	correspond_node = n;
}

const std::string& symbol::name() const{
	return symname;
}

END_NS_SASL_SEMANTIC();