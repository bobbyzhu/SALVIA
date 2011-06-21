#ifndef SASL_SEMANTIC_ABI_ANALYSER_H
#define SASL_SEMANTIC_ABI_ANALYSER_H

#include <sasl/include/semantic/semantic_forward.h>

#include <salviar/include/shader.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/shared_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

namespace sasl{
	namespace syntax_tree{
		struct node;
	}
}
BEGIN_NS_SASL_SEMANTIC();

class symbol;
class module_si;

class abi_info;

// If entry of VS and PS was set, match the ABIs to generate interpolating code.
class abi_analyser{
public:
	bool entry( boost::shared_ptr<module_si> const& mod, std::string const& name, salviar::languages lang );
	bool auto_entry( boost::shared_ptr<module_si> const& mod, salviar::languages lang );

	boost::shared_ptr<symbol> const& entry( salviar::languages lang ) const;

	void reset( salviar::languages lang );
	void reset_all();

	bool update_abiis();
	bool verify_abiis();

	abi_info const* abii( salviar::languages lang ) const;
	abi_info* abii( salviar::languages lang );

	boost::shared_ptr<abi_info> shared_abii( salviar::languages lang ) const;

private:
	bool entry(
		boost::shared_ptr<module_si> const& mod, boost::shared_ptr<symbol> const& fnsym,
		salviar::languages lang );

	bool add_semantic(
		boost::shared_ptr<sasl::syntax_tree::node> const& v,
		bool is_member, bool enable_nested,
		salviar::languages lang, bool is_output_semantic
		);

	bool update( salviar::languages lang );

	bool verify_vs_ps();
	bool verify_ps_bs();

	boost::shared_ptr<module_si> mods[salviar::lang_count];
	boost::shared_ptr<symbol> entries[salviar::lang_count];
	boost::shared_ptr<abi_info> abiis[salviar::lang_count];
};

END_NS_SASL_SEMANTIC();

#endif