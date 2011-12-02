#include <sasl/include/semantic/abi_info.h>

#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/host/utility.h>

#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/foreach.hpp>
#include <eflib/include/platform/boost_end.h>

using namespace sasl::utility::operators;
using namespace sasl::utility;

using salviar::sv_usage;
using salviar::su_none;
using salviar::su_stream_in;
using salviar::su_stream_out;
using salviar::su_buffer_in;
using salviar::su_buffer_out;
using salviar::storage_usage_count;

using salviar::sv_layout;

using boost::addressof;
using boost::shared_ptr;

using std::vector;

BEGIN_NS_SASL_SEMANTIC();

abi_info::abi_info()
	: mod(NULL), entry_point(NULL), lang(salviar::lang_none)
{
	memset( counts, 0, sizeof(counts) );
	memset( offsets, 0, sizeof(offsets) );
}

void abi_info::module( shared_ptr<module_si> const& v ){
	mod = v.get();
}

bool abi_info::is_module( shared_ptr<module_si> const& v ) const{
	return mod == v.get();
}

void abi_info::entry( shared_ptr<symbol> const& v ){
	entry_point = v.get();
	entry_point_name = entry_point->mangled_name();
}

bool abi_info::is_entry( shared_ptr<symbol> const& v ) const{
	return entry_point == v.get();
}

std::string abi_info::entry_name() const{
	return entry_point_name;
}
bool abi_info::add_input_semantic( salviar::semantic_value const& sem, builtin_types btc, bool is_stream )
{
	vector<salviar::semantic_value>::iterator it = std::lower_bound( sems_in.begin(), sems_in.end(), sem );
	if( it != sems_in.end() ){
		if( *it == sem ){
			sv_layout* si = input_sv_layout( sem );
			assert(si);
			if( si->value_type == btc || si->value_type == builtin_types::none ){
				si->value_type = to_lvt(btc);
				return true;
			}
			return false;
		}
	}

	sv_layout* si = alloc_input_storage( sem );
	si->value_type = to_lvt( btc );
	si->usage = is_stream ? su_stream_in : su_buffer_in;
	si->sv = sem;
	sems_in.insert( it, sem );
	return true;
}

bool abi_info::add_output_semantic( salviar::semantic_value const& sem, builtin_types btc ){
	vector<salviar::semantic_value>::iterator it = std::lower_bound( sems_out.begin(), sems_out.end(), sem );
	if( it != sems_out.end() ){
		if( *it == sem ){
			sv_layout* si = alloc_output_storage( sem );
			if( si->value_type != btc && si->value_type == builtin_types::none ){
				si->value_type = to_lvt( btc );
				return true;
			}
			return false;
		}
	}

	sv_layout* si = alloc_output_storage( sem );
	si->value_type = to_lvt( btc );
	si->usage = su_buffer_out;
	si->sv = sem;
	sems_out.insert( it, sem );
	return true;
}

void abi_info::add_global_var( boost::shared_ptr<symbol> const& v, builtin_types btc )
{
	syms_in.push_back( v.get() );
	sv_layout* si = alloc_input_storage( v );
	si->value_type = to_lvt( btc );
	si->usage = su_buffer_in;

	name_storages.insert( make_pair(v->unmangled_name(), si) );
}

sv_layout* abi_info::input_sv_layout( salviar::semantic_value const& sem ) const {
	sem_storages_t::const_iterator it = semin_storages.find( sem );
	if ( it == semin_storages.end() ){
		return NULL;
	}
	return const_cast<sv_layout*>( addressof( it->second ) );
}

sv_layout* abi_info::alloc_input_storage( salviar::semantic_value const& sem ){
	return addressof( semin_storages[sem] );
}

sv_layout* abi_info::input_sv_layout( boost::shared_ptr<symbol> const& v ) const {
	sym_storages_t::const_iterator it = symin_storages.find( v.get() );
	if ( it == symin_storages.end() ){
		return NULL;
	}
	return const_cast<sv_layout*>( addressof( it->second ) );
}

sv_layout* abi_info::input_sv_layout( std::string const& name ) const
{
	name_storages_t::const_iterator it = name_storages.find( name );
	if( it == name_storages.end() ){
		return NULL;
	}
	return it->second;
}
sv_layout* abi_info::alloc_input_storage( boost::shared_ptr<symbol> const& v ){
	return addressof( symin_storages[v.get()] );
}

sv_layout* abi_info::output_sv_layout( salviar::semantic_value const& sem ) const{
	sem_storages_t::const_iterator it = semout_storages.find( sem );
	if ( it == semout_storages.end() ){
		return NULL;
	}
	return const_cast<sv_layout*>( addressof( it->second ) );
}

size_t abi_info::total_size( sv_usage st ) const{
	return offsets[st];
}

sv_layout* abi_info::alloc_output_storage( salviar::semantic_value const& sem ){
	return addressof( semout_storages[sem] );
}

// Update ABI Information
void abi_info::compute_layout(){
	if ( !mod || !entry_point ) return;

	if ( lang == salviar::lang_general ){
		return;
	}

	compute_input_semantics_layout();
	compute_output_buffer_layout();
	compute_output_stream_layout();
	compute_input_constant_layout();
}

std::vector<sv_layout*> abi_info::layouts( sv_usage st ) const{
	std::vector<sv_layout*> ret;

	// Process output
	if( st == su_buffer_out ){
		BOOST_FOREACH( salviar::semantic_value const& sem, sems_out ){
			ret.push_back( output_sv_layout(sem) );
		}
		return ret;
	}

	// Process input
	for ( size_t index = 0; index < sems_in.size(); ++index ){
		sv_layout* svl = input_sv_layout( sems_in[index] );
		if ( svl->usage == st ){
			ret.push_back( svl );
		}
	}

	if ( st == su_buffer_in ){
		BOOST_FOREACH( symbol* sym, syms_in ){
			ret.push_back( const_cast<sv_layout*>( addressof( symin_storages.find(sym)->second ) ) );
		}
	}

	return ret;
}

void abi_info::compute_input_semantics_layout(){
	for ( size_t index = 0; index < sems_in.size(); ++index ){

		sv_layout* svl = input_sv_layout( sems_in[index] );
		assert( svl );

		svl->physical_index =  counts[svl->usage];
		svl->logical_index  =  counts[svl->usage];
		svl->offset = offsets[svl->usage];
		svl->element_size = 
			svl->usage == su_buffer_in ?
			static_cast<int>( storage_size( to_builtin_types( svl->value_type ) ) )
			: static_cast<int> ( sizeof(void*) )
			;
		svl->element_count = 1;

		counts[svl->usage]++;
		offsets[svl->usage] += svl->total_size();

	}
}

void abi_info::compute_output_buffer_layout(){
	for ( size_t index = 0; index < sems_out.size(); ++index ){
		sv_layout* svl = output_sv_layout( sems_out[index] );
		assert(svl);

		svl->usage = su_buffer_out;
		svl->physical_index =  counts[svl->usage];
		svl->logical_index  =  counts[svl->usage];
		svl->offset = offsets[svl->usage];
		svl->element_size = static_cast<int>( storage_size( to_builtin_types( svl->value_type ) ) );
		svl->element_count = 1;
		
		counts[svl->usage]++;
		offsets[svl->usage] += svl->total_size();
	}
}

void abi_info::compute_output_stream_layout()
{
	// Do nothing.
}

void abi_info::compute_input_constant_layout(){
	for ( size_t index = 0; index < syms_in.size(); ++index ){
		sv_layout* svl = addressof( symin_storages[ syms_in[index] ] );
		svl->usage = su_buffer_in;
		svl->physical_index =  counts[svl->usage];
		svl->logical_index  =  counts[svl->usage];
		svl->offset = offsets[su_buffer_in];
		
		int size = static_cast<int>( storage_size( to_builtin_types( svl->value_type ) ) );
		svl->element_size = size;
		svl->element_count = 1;

		counts[su_buffer_in]++;
		offsets[su_buffer_in] += size;
	}
}

END_NS_SASL_SEMANTIC();