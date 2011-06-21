#ifndef SASL_SEMANTIC_TYPE_MANAGER_H
#define SASL_SEMANTIC_TYPE_MANAGER_H

#include <sasl/include/semantic/semantic_forward.h>
#include <eflib/include/platform/typedefs.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

#include <vector>

namespace sasl{
	namespace syntax_tree{
		struct type_specifier;
	}
}

struct builtin_types;

BEGIN_NS_SASL_SEMANTIC();

class symbol;

class type_entry{
public:
	typedef int id_t;
	typedef id_t type_entry::*id_ptr_t;

	type_entry();
	
	::boost::shared_ptr< ::sasl::syntax_tree::type_specifier > stored;
	
	id_t u_qual;
	/*
	id_t v_qual;
	id_t cv_qual;
	id_t a_qual;
	*/
};

class type_manager{
public:
	static boost::shared_ptr< type_manager > create();

	void root_symbol( boost::shared_ptr<symbol> const& sym );

	boost::shared_ptr<type_manager> handle() const;
	
	type_entry::id_t get(
		::boost::shared_ptr< ::sasl::syntax_tree::type_specifier > const& node,
		::boost::shared_ptr<symbol> const& parent
		);
	type_entry::id_t get( const builtin_types& btc );

	::boost::shared_ptr< ::sasl::syntax_tree::type_specifier > get( type_entry::id_t id );
	
private:
	type_entry::id_t allocate_and_assign_id( ::boost::shared_ptr< ::sasl::syntax_tree::type_specifier > const& node );
	::std::vector< type_entry > entries;
	boost::weak_ptr<type_manager> self_handle;
	boost::weak_ptr<symbol> rootsym;
};

END_NS_SASL_SEMANTIC();

#endif