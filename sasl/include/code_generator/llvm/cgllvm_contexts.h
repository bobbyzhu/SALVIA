#ifndef SASL_CODE_GENERATOR_LLVM_CGLLVM_CONTEXTS_H
#define SASL_CODE_GENERATOR_LLVM_CGLLVM_CONTEXTS_H

#include <sasl/include/code_generator/forward.h>
#include <sasl/include/code_generator/codegen_context.h>

namespace llvm{
	// Node
	class AllocaInst;
	class Argument;
	class Function;
	class GlobalVariable;
	class Value;
	class BasicBlock;
	
	// Type
	class FunctionType;
	class Type;

	// Instructions
	class ReturnInst;
}

namespace sasl{
	namespace semantic{
		class symbol;
	}
}

BEGIN_NS_SASL_CODE_GENERATOR();

//////////////////////////////////////////////////////////
// used by a lot of node for common information storage

class cgllvm_common_context: public codegen_context{
public:
	typedef codegen_context base_type;
	cgllvm_common_context();

	boost::weak_ptr< sasl::semantic::symbol > sym;
	boost::weak_ptr< sasl::syntax_tree::node> variable_to_fill;

	llvm::Value* val;
	llvm::Function* func;
	llvm::Function* parent_func; // For inserting statement code into function .
	llvm::Argument* arg;
	llvm::BasicBlock* block;

	llvm::BasicBlock* continue_to;
	llvm::BasicBlock* break_to;

	const llvm::Type* type;
	bool is_signed;
	const llvm::FunctionType* func_type;

	llvm::Value* addr;

	// Instructions
	llvm::ReturnInst* return_inst;
};

class cgllvm_type_context: public codegen_context{
public:
	typedef codegen_context base_type;
	cgllvm_type_context();

	const llvm::Type* basic_type;
};
END_NS_SASL_CODE_GENERATOR();

#endif