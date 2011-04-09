#ifndef SASL_CODE_GENERATOR_LLVM_CGLLVM_TYPE_CONVERTER_H
#define SASL_CODE_GENERATOR_LLVM_CGLLVM_TYPE_CONVERTER_H

#include <sasl/include/code_generator/forward.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

namespace sasl{
	namespace semantic{
		class type_converter;
		class type_manager;
		class symbol;
	}
	namespace syntax_tree{
		struct node;
	}
}

namespace llvm{
	class IRBuilderBase;
	class Value;
}

BEGIN_NS_SASL_CODE_GENERATOR();

class cgllvm_sctxt;

boost::shared_ptr< ::sasl::semantic::type_converter> create_type_converter(
		boost::shared_ptr<llvm::IRBuilderBase> const& builder,
		boost::function<
			cgllvm_sctxt* ( boost::shared_ptr<sasl::syntax_tree::node> const& )
		> const& ctxt_lookup,
		boost::function< llvm::Value* (cgllvm_sctxt*) > const& loader,
		boost::function< void (llvm::Value*, cgllvm_sctxt*) > const& storer
		);

void register_builtin_typeconv(
	boost::shared_ptr< ::sasl::semantic::type_converter> typeconv,
	boost::shared_ptr< sasl::semantic::type_manager> typemgr
	);

END_NS_SASL_CODE_GENERATOR();

#endif
