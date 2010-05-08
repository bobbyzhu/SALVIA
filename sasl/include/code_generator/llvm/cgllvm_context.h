#ifndef SASL_CODE_GENERATOR_LLVM_CGLLVM_CONTEXT_H
#define SASL_CODE_GENERATOR_LLVM_CGLLVM_CONTEXT_H

#include <sasl/include/code_generator/forward.h>
#include <sasl/include/code_generator/llvm/llvm_patch_begin.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Support/IRBuilder.h>
#include <sasl/include/code_generator/llvm/llvm_patch_end.h>
#include <boost/shared_ptr.hpp>
#include <string>

BEGIN_NS_SASL_CODE_GENERATOR();

class cgllvm_context{
public:
	cgllvm_context( const std::string& module_name );

	// attributes
	boost::shared_ptr<llvm::Module> module() const;

private:
	llvm::LLVMContext lctxt;
	boost::shared_ptr<llvm::Module> mod;
	boost::shared_ptr<llvm::IRBuilder<> > irbuilder;
};

END_NS_SASL_CODE_GENERATOR();

#endif