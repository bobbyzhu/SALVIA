#include <sasl/include/code_generator/llvm/cgllvm_module_impl.h>

#include <sasl/include/semantic/abi_analyser.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_os_ostream.h>
#include <eflib/include/platform/enable_warnings.h>

using sasl::semantic::module_semantic;
using salviar::sv_usage;
using salviar::su_buffer_in;
using salviar::su_buffer_out;
using salviar::su_stream_in;
using salviar::su_stream_out;
using salviar::storage_usage_count;
using boost::shared_ptr;

BEGIN_NS_SASL_CODE_GENERATOR();

cgllvm_module_impl::cgllvm_module_impl()
: llvm_mod_(NULL), have_mod_(true)
{
	llvm_ctxt_.reset( new llvm::LLVMContext() );
	irbuilder_.reset( new llvm::IRBuilder<>( *llvm_ctxt_ ) );
}

void cgllvm_module_impl::create_llvm_module( const std::string& modname ){
	llvm_mod_ = new llvm::Module( modname, *llvm_ctxt_ );
	have_mod_ = true;
}

llvm::Module* cgllvm_module_impl::llvm_module() const{
	return llvm_mod_;
}

llvm::LLVMContext& cgllvm_module_impl::llvm_context(){
	return *llvm_ctxt_;
}

cgllvm_module_impl::~cgllvm_module_impl(){
	if( have_mod_ && llvm_mod_ ){
		delete llvm_mod_;
		llvm_mod_ = NULL;
		have_mod_ = false;
	}
}

llvm::Module* cgllvm_module_impl::take_ownership() const{
	if ( have_mod_ ){
		have_mod_ = false;
		return llvm_mod_;
	}
	return NULL;
}

llvm::DefaultIRBuilder* cgllvm_module_impl::builder() const{
	return irbuilder_.get();
}

void cgllvm_module_impl::dump_ir() const
{
	llvm_mod_->dump();
}

void cgllvm_module_impl::dump_ir( std::ostream& ostr ) const
{
	llvm::raw_os_ostream raw_os(ostr);
	llvm_mod_->print( raw_os, NULL );
	raw_os.flush();
}

module_semantic* cgllvm_module_impl::get_semantic() const
{
	return sem_.get();
}

void cgllvm_module_impl::set_semantic( shared_ptr<module_semantic> const& v )
{
	sem_ = v;
}

module_context* cgllvm_module_impl::get_context() const
{
	return ctxt_.get();
}

void cgllvm_module_impl::set_context( shared_ptr<module_context> const& v )
{
	ctxt_ = v;
}

END_NS_SASL_CODE_GENERATOR();