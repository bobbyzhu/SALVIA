#include <sasl/include/semantic/ssa_context.h>
#include <sasl/include/semantic/deps_graph.h>

using sasl::syntax_tree::node;
using boost::unordered_map;

BEGIN_NS_SASL_SEMANTIC();

variable_t* ssa_context::create_variable( node* decl )
{
	variable_t* ret = new variable_t();
	ret->decl = decl;
	ret->value = NULL;
	vars.push_back( ret );
	return ret;
}

ssa_attribute& ssa_context::attr( node* n )
{
	ssa_attr_iter_t it = ssa_attrs.find(n);
	if( it == ssa_attrs.end() ){
		ssa_attribute* pattr = new ssa_attribute();
		ssa_attrs[n] = pattr;
		return *pattr;
	} else {
		return *(it->second);
	}
}

function_t* ssa_context::create_function()
{
	function_t* ret = new function_t();
	ret->entry = NULL;
	ret->exit = NULL;
	ret->fn = NULL;
	fns.push_back( ret );
	return ret;
}

value_t* ssa_context::create_value()
{
	value_t* ret = new value_t();
	ret->expr_node = NULL;
	ret->parent = NULL;
	values.push_back(ret);
	return ret;
}

block_t* ssa_context::create_block()
{
	block_t* ret = new block_t();
	blocks.push_back(ret);
	return ret;
}

value_t* ssa_context::load( variable_t* var )
{
	// EFLIB_ASSERT_UNIMPLEMENTED();
	return NULL;
}

value_t* ssa_context::load( sasl::syntax_tree::node* n )
{
	if( attr(n).var )
	{
		return load( attr(n).var );
	}
	return attr(n).val;
}

void ssa_context::store( variable_t* var, value_t* val )
{
	EFLIB_ASSERT_UNIMPLEMENTED();
}

instruction_t* ssa_context::emit( block_t* parent, int id )
{
	instruction_t* ret = new instruction_t();
	ret->id = (instruction_t::IDs)id;
	if( !parent->ins.empty() )
	{
		ret->prev = parent->ins.empty() ? NULL : parent->ins.back();
		parent->ins.back()->next = ret;
		
	}
	ret->parent = parent;
	parent->ins.push_back(ret);

	return ret;
}

END_NS_SASL_SEMANTIC();

