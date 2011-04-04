#include <sasl/include/semantic/abi_analyser.h>

#include <sasl/include/semantic/abi_info.h>

#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/syntax_tree/declaration.h>

#include <sasl/enums/builtin_type_code.h>
#include <sasl/enums/enums_helper.h>

#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/foreach.hpp>
#include <boost/utility/addressof.hpp>
#include <eflib/include/platform/boost_end.h>

#include <algorithm>

using namespace sasl::syntax_tree;

using boost::addressof;
using boost::make_shared;
using boost::shared_ptr;

using std::lower_bound;
using std::string;
using std::vector;


BEGIN_NS_SASL_SEMANTIC();

bool verify_semantic_type( builtin_type_code btc, softart::semantic sem ){
	switch( semantic_base(sem) ){

	case softart::SV_None:
		return false;

	case softart::SV_Position:
	case softart::SV_Texcoord:
		return 
			( sasl_ehelper::is_scalar(btc) || sasl_ehelper::is_vector(btc) )
			&& sasl_ehelper::scalar_of(btc) == builtin_type_code::_float;
	}

	return false;
}

storage_types vsinput_semantic_storage( softart::semantic sem ){
	switch( semantic_base(sem) ){
	case softart::SV_Position:
		return stream_in;
	case softart::SV_Texcoord:
		return stream_in;
	}
	return storage_none;
}

storage_types vsoutput_semantic_storage( softart::semantic sem ){
	switch( semantic_base(sem) ){
	case softart::SV_Position:
		return buffer_out;
	case softart::SV_Texcoord:
		return buffer_out;
	}
	return storage_none;
}

storage_types semantic_storage( softart::languages lang, bool is_output, softart::semantic sem ){
	switch ( lang ){
	case softart::lang_vertex_sl:
		if( is_output ){
			return vsoutput_semantic_storage(sem);
		} else {
			return vsinput_semantic_storage(sem);
		}
	}
	return storage_none;
}

void abi_analyser::reset( softart::languages lang ){
	assert( lang < softart::lang_count );
	
	mods[lang].reset();
	entries[lang].reset();
	abiis[lang].reset();
}

void abi_analyser::reset_all(){
	for( int i = 0; i < softart::lang_count; ++i ){
		reset( static_cast<softart::languages>(i) );
	}
}

bool abi_analyser::entry( shared_ptr<module_si> const& mod, string const& name, softart::languages lang ){
	vector< shared_ptr<symbol> > const& overloads = mod->root()->find_overloads( name );
	if ( overloads.size() != 1 ){
		return false;
	}

	return entry( mod, overloads[0], lang );
}

bool abi_analyser::entry( shared_ptr<module_si> const& mod, shared_ptr<symbol> const& fnsym, softart::languages lang ){
	assert( lang < softart::lang_count );

	mods[lang] = mod;
	entries[lang] = fnsym;
	abiis[lang].reset();

	return update(lang);
}

bool abi_analyser::auto_entry( shared_ptr<module_si> const& mod, softart::languages lang ){
	shared_ptr<symbol> candidate;
	shared_ptr<abi_info> candidate_abii;
	BOOST_FOREACH( shared_ptr<symbol> const& fnsym, mod->functions() ){
		if( entry(mod, fnsym, lang) ){
			if( candidate ){
				// TODO More than one mactched. conflict error.
				reset(lang);
				return false;
			}
			candidate = fnsym;
			candidate_abii = abiis[lang];
		}
	}

	if( candidate ){
		mods[lang] = mod;
		entries[lang] = candidate;
		abiis[lang] = candidate_abii;
		return true;
	}

	return false;
}

boost::shared_ptr<symbol> const& abi_analyser::entry( softart::languages lang ) const{
	return entries[lang];
}

bool abi_analyser::update_abiis(){
	return update( softart::lang_vertex_sl )
		&& update( softart::lang_pixel_sl )
		&& update( softart::lang_blend_sl )
		;
}

bool abi_analyser::verify_abiis(){
	return verify_vs_ps() && verify_ps_bs();
}

bool abi_analyser::update( softart::languages lang ){
	if ( abiis[lang] ){
		return true;
	}

	if( ! (mods[lang] && entries[lang]) ){
		return false;
	}

	// Initialize language ABI information.
	abiis[lang] = make_shared<abi_info>();
	abiis[lang]->lang = lang;
	abiis[lang]->mod = mods[lang].get();
	abiis[lang]->entry_point = entries[lang].get();

	if( lang == softart::lang_vertex_sl
		|| lang == softart::lang_pixel_sl
		|| lang == softart::lang_blend_sl
		)
	{
		// Process entry function.
		shared_ptr<function_type> entry_fn = entries[lang]->node()->typed_handle<function_type>();
		assert( entry_fn );

		if( !add_semantic( entry_fn, false, false, softart::lang_vertex_sl, true ) ){
			reset(lang);
			return false;
		}

		BOOST_FOREACH( shared_ptr<parameter> const& param, entry_fn->params )
		{
			if( !add_semantic( param, false, false, lang, false ) ){
				reset(lang);
				return false;
			}
		}

		// Process global variables.
		BOOST_FOREACH( shared_ptr<symbol> const& gvar_sym, mods[lang]->globals() ){
			shared_ptr<declarator> gvar = gvar_sym->node()->typed_handle<declarator>();
			assert(gvar);

			// is_member is set to true for preventing aggregated variable.
			// And global variable only be treated as input.
			if( !add_semantic( gvar, true, false, lang, false ) ){
				// If it is not attached to an valid semantic, it should be uniform variable.

				// Check the data type of global. Now global variables only support built-in types.
				storage_si* psi = dynamic_cast<storage_si*>( gvar->semantic_info().get() );
				if( !psi->type_info()->is_builtin() ){
					//TODO It an semantic error need to be reported.
					return false;
				}

				abiis[lang]->add_global_var(gvar_sym, psi->type_info()->value_typecode );
			}
		}
	}
	

	// Compute ABI memory layout.
	abiis[lang]->compute_layout();

	return true;
}

abi_info const* abi_analyser::abii( softart::languages lang ) const{
	return abiis[lang].get();
}

abi_info* abi_analyser::abii( softart::languages lang ){
	return abiis[lang].get();
}

bool abi_analyser::add_semantic(
	shared_ptr<node> const& v,
	bool is_member, bool enable_nested,
	softart::languages lang, bool is_output)
{
	abi_info* ai = abii(lang);
	assert( ai );
	storage_si* pssi = dynamic_cast<storage_si*>( v->semantic_info().get() );
	assert(pssi); // TODO Here are semantic analysis error.
	type_specifier* ptspec = pssi->type_info().get();
	assert(ptspec); // TODO Here are semantic analysis error.

	softart::semantic node_sem = pssi->get_semantic();

	if( ptspec->is_builtin() ){
		builtin_type_code btc = ptspec->value_typecode;
		if ( verify_semantic_type( btc, node_sem ) ) {
			storage_types sem_s = semantic_storage( lang, is_output, node_sem );
			switch( sem_s ){

			case stream_in:
			case buffer_in:
				return ai->add_input_semantic( node_sem, btc );

			case buffer_out:
			case stream_out:
				return ai->add_output_semantic( node_sem, btc );
			}

			return false;
		}
	} else if( ptspec->node_class() == syntax_node_types::struct_type ){
		if( is_member && !enable_nested ){
			return false;
		}

		// TODO do not support nested aggregated variable. 
		struct_type* pstructspec = dynamic_cast<struct_type*>( ptspec );
		assert( pstructspec );
		BOOST_FOREACH( shared_ptr<declaration> const& decl, pstructspec->decls )
		{
			if ( decl->node_class() == syntax_node_types::variable_declaration ){
				shared_ptr<variable_declaration> vardecl = decl->typed_handle<variable_declaration>();
				BOOST_FOREACH( shared_ptr<declarator> const& dclr, vardecl->declarators ){
					if ( !add_semantic( dclr, true, enable_nested, lang, is_output ) ){
						return false;
					}
				}
			}
		}

		return true;
	}

	return false;
}

bool abi_analyser::verify_vs_ps(){
	EFLIB_ASSERT_UNIMPLEMENTED();
	return false;
}

bool abi_analyser::verify_ps_bs(){
	EFLIB_ASSERT_UNIMPLEMENTED();
	return false;
}

END_NS_SASL_SEMANTIC();