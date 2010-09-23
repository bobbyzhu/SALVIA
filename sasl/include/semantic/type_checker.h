#ifndef SASL_SEMANTIC_TYPE_CHECKER_H
#define SASL_SEMANTIC_TYPE_CHECKER_H

#include <sasl/include/semantic/semantic_forward.h>
#include <boost/shared_ptr.hpp>
#include <string>

namespace sasl{
	namespace syntax_tree{
		struct type_specifier;
		struct function_type;
		struct buildin_type;
	}
}

BEGIN_NS_SASL_SEMANTIC();

std::string mangle_function_name( boost::shared_ptr<::sasl::syntax_tree::function_type> v );

bool type_equal(
	boost::shared_ptr<::sasl::syntax_tree::type_specifier> lhs,
	boost::shared_ptr<::sasl::syntax_tree::type_specifier> rhs
);

bool type_equal(
	boost::shared_ptr<::sasl::syntax_tree::buildin_type> lhs,
	boost::shared_ptr<::sasl::syntax_tree::buildin_type> rhs
);

// boost::shared_ptr<::sasl::syntax_tree::type_specifier> actual_type( boost::shared_ptr<::sasl::syntax_tree::type_specifier> );
END_NS_SASL_SEMANTIC();

#endif