#ifndef SASL_SEMANTIC_SEMANTIC_ANALYSER_IMPL_H
#define SASL_SEMANTIC_SEMANTIC_ANALYSER_IMPL_H

#include <sasl/include/semantic/semantic_forward.h>
#include <sasl/include/syntax_tree/visitor.h>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace sasl{
	namespace common{
		class compiler_info_manager;
	}
	namespace syntax_tree{
		struct node;
		struct function_type;
	}
}
BEGIN_NS_SASL_SEMANTIC();

class symbol;
class type_converter;

class semantic_analyser_impl: public ::sasl::syntax_tree::syntax_tree_visitor{
public:
	semantic_analyser_impl( boost::shared_ptr<::sasl::common::compiler_info_manager> infomgr );

	// expressio n
	virtual void visit( ::sasl::syntax_tree::unary_expression& v );
	virtual void visit( ::sasl::syntax_tree::cast_expression& v);
	virtual void visit( ::sasl::syntax_tree::binary_expression& v );
	virtual void visit( ::sasl::syntax_tree::expression_list& v );
	virtual void visit( ::sasl::syntax_tree::cond_expression& v );
	virtual void visit( ::sasl::syntax_tree::index_expression& v );
	virtual void visit( ::sasl::syntax_tree::call_expression& v );
	virtual void visit( ::sasl::syntax_tree::member_expression& v );

	virtual void visit( ::sasl::syntax_tree::constant_expression& v );
	virtual void visit( ::sasl::syntax_tree::variable_expression& v );

	// declaration & type specifier
	virtual void visit( ::sasl::syntax_tree::initializer& v );
	virtual void visit( ::sasl::syntax_tree::expression_initializer& v );
	virtual void visit( ::sasl::syntax_tree::member_initializer& v );
	virtual void visit( ::sasl::syntax_tree::declaration& v );
	virtual void visit( ::sasl::syntax_tree::variable_declaration& v );
	virtual void visit( ::sasl::syntax_tree::type_definition& v );
	virtual void visit( ::sasl::syntax_tree::type_specifier& v );
	virtual void visit( ::sasl::syntax_tree::buildin_type& v );
	virtual void visit( ::sasl::syntax_tree::array_type& v );
	virtual void visit( ::sasl::syntax_tree::struct_type& v );
	virtual void visit( ::sasl::syntax_tree::parameter& v );
	virtual void visit( ::sasl::syntax_tree::function_type& v );

	// statement
	virtual void visit( ::sasl::syntax_tree::statement& v );
	virtual void visit( ::sasl::syntax_tree::declaration_statement& v );
	virtual void visit( ::sasl::syntax_tree::if_statement& v );
	virtual void visit( ::sasl::syntax_tree::while_statement& v );
	virtual void visit( ::sasl::syntax_tree::dowhile_statement& v );
	virtual void visit( ::sasl::syntax_tree::case_label& v );
	virtual void visit( ::sasl::syntax_tree::ident_label& v );
	virtual void visit( ::sasl::syntax_tree::switch_statement& v );
	virtual void visit( ::sasl::syntax_tree::compound_statement& v );
	virtual void visit( ::sasl::syntax_tree::expression_statement& v );
	virtual void visit( ::sasl::syntax_tree::jump_statement& v );

	// program
	virtual void visit( ::sasl::syntax_tree::program& v );

	void buildin_type_convert(
		boost::shared_ptr<::sasl::syntax_tree::node>,
		boost::shared_ptr<::sasl::syntax_tree::node>
		);

	void register_type_converter();
	void register_buildin_function();
private:
	boost::shared_ptr<type_converter> typeconv;
	boost::shared_ptr<::sasl::common::compiler_info_manager> infomgr;
	boost::shared_ptr<symbol> cursym;
	bool is_local;

	//////////////////////////////////////////////////////////////////////////
	// buildin functions for build-in types.
	std::vector< boost::shared_ptr<::sasl::syntax_tree::function_type> > buildin_functions;
};

END_NS_SASL_SEMANTIC();

#endif