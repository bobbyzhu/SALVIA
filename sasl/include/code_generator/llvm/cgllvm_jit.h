#ifndef SASL_CODE_GENERATOR_LLVM_CGLLVM_JIT_H
#define SASL_CODE_GENERATOR_LLVM_CGLLVM_JIT_H

#include <sasl/include/code_generator/forward.h>
#include <sasl/include/code_generator/jit_api.h>
#include <string>

namespace llvm{
	class Function; 
	class ExecutionEngine;
}

BEGIN_NS_SASL_CODE_GENERATOR();

class cgllvm_global_context;
class cgllvm_sctxt;

class cgllvm_jit_engine:public jit_engine{
public:
	virtual void* get_function( const std::string& /*func_name*/ );
	static boost::shared_ptr<cgllvm_jit_engine> create( boost::shared_ptr<cgllvm_global_context>, std::string& error );
	virtual ~cgllvm_jit_engine(){}
protected:
	cgllvm_jit_engine( boost::shared_ptr<cgllvm_global_context> );
	void build();
	bool is_valid();
	std::string error();
	
private:
	cgllvm_jit_engine( const cgllvm_jit_engine& );
	cgllvm_jit_engine& operator = (const cgllvm_jit_engine& );

	boost::shared_ptr<cgllvm_global_context> global_ctxt;
	boost::shared_ptr<llvm::ExecutionEngine> engine;
	std::string err;
};

END_NS_SASL_CODE_GENERATOR();

#endif