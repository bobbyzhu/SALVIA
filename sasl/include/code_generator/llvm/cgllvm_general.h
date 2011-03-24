#ifndef SASL_CODE_GENERATOR_LLVM_CGLLVM_GENERAL_H
#define SASL_CODE_GENERATOR_LLVM_CGLLVM_GENERAL_H

#include <sasl/include/code_generator/forward.h>

#include <sasl/include/code_generator/llvm/cgllvm_impl.h>
#include <sasl/include/syntax_tree/visitor.h>
#include <sasl/include/semantic/abi_analyser.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Support/IRBuilder.h>
#include <eflib/include/platform/enable_warnings.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/any.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <eflib/include/platform/boost_end.h>

#include <string>

namespace sasl{
	namespace semantic{
		class type_converter;
		class module_si;
	}
	namespace syntax_tree{
		struct expression;
		struct type_specifier;
		struct node;
	}
}

namespace llvm{
	class Constant;
}

struct builtin_type_code;

BEGIN_NS_SASL_CODE_GENERATOR();

class cgllvm_common_context;
class cgllvm_global_context;
class llvm_code;

class cgllvm_general: public cgllvm{
public:
	cgllvm_general();

	bool generate(
		sasl::semantic::module_si* mod,
		sasl::semantic::abi_info const* abii
		);

	SASL_VISIT_DCL( unary_expression );
	SASL_VISIT_DCL( cast_expression );
	SASL_VISIT_DCL( binary_expression );
	SASL_VISIT_DCL( expression_list );
	SASL_VISIT_DCL( cond_expression );
	SASL_VISIT_DCL( index_expression );
	SASL_VISIT_DCL( call_expression );
	SASL_VISIT_DCL( member_expression );

	SASL_VISIT_DCL( constant_expression );
	SASL_VISIT_DCL( variable_expression );
	SASL_VISIT_DCL( identifier );

	// declaration & type specifier
	SASL_VISIT_DCL( initializer );
	SASL_VISIT_DCL( expression_initializer );
	SASL_VISIT_DCL( member_initializer );
	SASL_VISIT_DCL( declaration );
	SASL_VISIT_DCL( declarator );
	SASL_VISIT_DCL( variable_declaration );
	SASL_VISIT_DCL( type_definition );
	SASL_VISIT_DCL( type_specifier );
	SASL_VISIT_DCL( builtin_type );
	SASL_VISIT_DCL( array_type );
	SASL_VISIT_DCL( struct_type );
	SASL_VISIT_DCL( parameter );
	SASL_VISIT_DCL( function_type );

	// statement
	SASL_VISIT_DCL( statement );
	SASL_VISIT_DCL( declaration_statement );
	SASL_VISIT_DCL( if_statement );
	SASL_VISIT_DCL( while_statement );
	SASL_VISIT_DCL( dowhile_statement );
	SASL_VISIT_DCL( for_statement );
	SASL_VISIT_DCL( case_label );
	SASL_VISIT_DCL( ident_label );
	SASL_VISIT_DCL( switch_statement );
	SASL_VISIT_DCL( compound_statement );
	SASL_VISIT_DCL( expression_statement );
	SASL_VISIT_DCL( jump_statement );

	// program
	SASL_VISIT_DCL( program );

	boost::shared_ptr<llvm_code> module();

	template <typename NodeT> boost::any& visit_child( boost::any& child_ctxt, const boost::any& child_ctxt_init, boost::shared_ptr<NodeT> child );
	template <typename NodeT> boost::any& visit_child( boost::any& child_ctxt, boost::shared_ptr<NodeT> child );

private:
	template <typename NodeT>
	cgllvm_common_context* node_ctxt( boost::shared_ptr<NodeT> const&, bool create_if_need = false );
	cgllvm_common_context* node_ctxt( sasl::syntax_tree::node&, bool create_if_need = false );
	
	boost::function<cgllvm_common_context*( boost::shared_ptr<sasl::syntax_tree::node> const& )> ctxt_getter;

	void do_assign(
		boost::any* data,
		boost::shared_ptr<sasl::syntax_tree::expression> lexpr,
		boost::shared_ptr<sasl::syntax_tree::expression> rexpr
		);

	llvm::Constant* get_zero_filled_constant( boost::shared_ptr<sasl::syntax_tree::type_specifier> );
	llvm::Type const* create_builtin_type( builtin_type_code const& btc, bool& sign );
	llvm::Type const* get_llvm_type( boost::shared_ptr<sasl::syntax_tree::type_specifier> const& );
	
	void restart_block( boost::any* data );

	sasl::semantic::module_si* msi;
	sasl::semantic::abi_info const* abii;
	boost::shared_ptr<cgllvm_global_context> mctxt;
	boost::shared_ptr< ::sasl::semantic::type_converter > typeconv;

	typedef boost::unordered_map< sasl::syntax_tree::node*, boost::shared_ptr<cgllvm_common_context> > ctxts_t;
	ctxts_t ctxts ;
};

END_NS_SASL_CODE_GENERATOR()

#endif