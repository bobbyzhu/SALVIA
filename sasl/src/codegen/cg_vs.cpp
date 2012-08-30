#include <sasl/include/codegen/cg_vs.h>

#include <sasl/include/codegen/cg_contexts.h>
#include <sasl/include/codegen/cg_module_impl.h>
#include <sasl/include/codegen/cg_impl.imp.h>
#include <sasl/include/semantic/pety.h>
#include <sasl/include/semantic/semantics.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/syntax_tree/statement.h>
#include <sasl/include/syntax_tree/program.h>
#include <sasl/include/host/utility.h>
#include <sasl/enums/enums_utility.h>
#include <eflib/include/diagnostics/assert.h>
#include <eflib/include/utility/unref_declarator.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Target/TargetData.h>
#include <eflib/include/platform/enable_warnings.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <eflib/include/platform/boost_end.h>

#define SASL_VISITOR_TYPE_NAME cg_vs

using salviar::sv_usage;
using salviar::su_buffer_in;
using salviar::su_buffer_out;
using salviar::su_stream_in;
using salviar::su_stream_out;
using salviar::storage_usage_count;

using salviar::sv_layout;

using sasl::semantic::node_semantic;
using sasl::semantic::symbol;
using sasl::semantic::node_semantic;
using sasl::semantic::abi_info;

using namespace sasl::syntax_tree;
using namespace llvm;
using namespace sasl::utility;

using boost::any;
using boost::bind;
using boost::shared_ptr;

using std::vector;
using std::sort;
using std::pair;
using std::transform;
using std::make_pair;

#define FUNCTION_SCOPE( fn ) \
	push_fn( (fn) );	\
	scope_guard<void> pop_fn_on_exit##__LINE__( bind( &cgs_sisd::pop_fn, this ) );

BEGIN_NS_SASL_CODEGEN();

bool layout_type_pairs_cmp( pair<sv_layout*, Type*> const& lhs, pair<sv_layout*, Type*> const& rhs ){
	return lhs.first->element_size < rhs.first->element_size;
}

/// Re-arrange layouts will Sort up struct members by storage size.
/// For e.g.
///   struct { int i; byte b; float f; ushort us; }
/// Will be arranged to
///   struct { byte b; ushort us; int i; float f; };
/// It will minimize the structure size.
void rearrange_layouts( vector<sv_layout*>& sorted_layouts, vector<Type*>& sorted_tys, vector<sv_layout*> const& layouts, vector<Type*> const& tys, TargetData const* target )
{
	size_t layouts_count = layouts.size();
	vector<size_t> elems_size;
	elems_size.reserve( layouts_count );

	vector< pair<sv_layout*, Type*> > layout_ty_pairs;
	layout_ty_pairs.reserve( layouts_count );

	vector<sv_layout*>::const_iterator layout_it = layouts.begin();
	BOOST_FOREACH( Type* ty, tys ){
		size_t sz = (size_t)target->getTypeStoreSize(ty);
		(*layout_it)->element_size = sz;
		layout_ty_pairs.push_back( make_pair(*layout_it, ty) );
		++layout_it;
	}

	sort( layout_ty_pairs.begin(), layout_ty_pairs.end(), layout_type_pairs_cmp );

	sorted_layouts.clear();
	std::transform( layout_ty_pairs.begin(), layout_ty_pairs.end(), back_inserter(sorted_layouts), boost::bind(&pair<sv_layout*, Type*>::first, _1) );

	sorted_tys.clear();
	std::transform( layout_ty_pairs.begin(), layout_ty_pairs.end(), back_inserter(sorted_tys), boost::bind(&pair<sv_layout*, Type*>::second, _1) );
}

void cg_vs::fill_llvm_type_from_si( sv_usage su ){
	vector<sv_layout*> svls = abii->layouts( su );
	vector<Type*>& tys = entry_params_types[su];

	BOOST_FOREACH( sv_layout* si, svls ){
		builtin_types storage_bt = to_builtin_types(si->value_type);
		entry_param_tys[su].push_back( storage_bt );
		Type* storage_ty = service()->type_( storage_bt, abi_c );

		if( su_stream_in == su || su_stream_out == su || si->agg_type == salviar::aggt_array ){
			tys.push_back( PointerType::getUnqual( storage_ty ) );
		} else {
			tys.push_back( storage_ty );
		}
	}
	
	if( su_buffer_in == su || su_buffer_out == su ){
		rearrange_layouts( svls, tys, svls, tys, target_data );
	}

	char const* struct_name = NULL;
	switch( su ){
	case su_stream_in:
		struct_name = ".s.stri";
		break;
	case su_buffer_in:
		struct_name = ".s.bufi";
		break;
	case su_stream_out:
		struct_name = ".s.stro";
		break;
	case su_buffer_out:
		struct_name = ".s.bufo";
		break;
	}
	assert( struct_name );

	// Tys must not be empty. So placeholder (int8) will be inserted if tys is empty.
	StructType* out_struct = tys.empty() ? StructType::create( struct_name, service()->type_(builtin_types::_sint8, abi_llvm), NULL ) : StructType::create( tys, struct_name );
	entry_params_structs[su].data() = out_struct;

	// Update Layout physical informations.
	if( su_buffer_in == su || su_buffer_out == su ){
		StructLayout const* struct_layout = target_data->getStructLayout( out_struct );

		size_t next_offset = 0;
		for( size_t i_elem = 0; i_elem < svls.size(); ++i_elem ){
			size_t offset = next_offset;
			svls[i_elem]->offset = offset;
			svls[i_elem]->physical_index = i_elem;

			size_t next_i_elem = i_elem + 1;
			if( next_i_elem < tys.size() ){
				next_offset = (size_t)struct_layout->getElementOffset( static_cast<unsigned>(next_i_elem) );
			} else {
				next_offset = (size_t)struct_layout->getSizeInBytes();
				const_cast<abi_info*>(abii)->update_size( next_offset, su );
			}
		
			svls[i_elem]->element_padding = (next_offset - offset) - svls[i_elem]->element_size;
		}
	}
}

void cg_vs::create_entry_params(){
	fill_llvm_type_from_si ( su_buffer_in );
	fill_llvm_type_from_si ( su_buffer_out );
	fill_llvm_type_from_si ( su_stream_in );
	fill_llvm_type_from_si ( su_stream_out );
}

void cg_vs::add_entry_param_type( sv_usage st, vector<Type*>& par_types ){
	StructType* par_type = entry_params_structs[st].data();
	PointerType* parref_type = PointerType::getUnqual( par_type );

	par_types.push_back(parref_type);
}

// expressions
SASL_VISIT_DEF_UNIMPL( cast_expression );
SASL_VISIT_DEF_UNIMPL( expression_list );
SASL_VISIT_DEF_UNIMPL( cond_expression );

SASL_VISIT_DEF( member_expression ){
	EFLIB_UNREF_DECLARATOR(data);

	visit_child(v.expr);
	node_context* agg_ctxt = node_ctxt( v.expr );
	assert(agg_ctxt);
	
	// Aggregated value
	node_semantic* tisi = sem_->get_semantic( v.expr.get() );
	node_context* ctxt = node_ctxt(v, true);

	if( tisi->ty_proto()->is_builtin() ){
		// Swizzle or write mask
		uint32_t masks = sem_->get_semantic(&v)->swizzle();
		cg_value agg_value = agg_ctxt->node_value;
		if( is_scalar(tisi->ty_proto()->tycode) ){
			agg_value = service()->cast_s2v(agg_value);
		}
		ctxt->node_value = service()->emit_extract_elem_mask( agg_value, masks );
	} else {
		// Member
		symbol* struct_sym = sem_->get_symbol( tisi->ty_proto() );
		symbol* mem_sym = struct_sym->find_this( v.member->str );
		assert( mem_sym );

		if( agg_ctxt->is_semantic_mode ){
			node_semantic* par_mem_ssi = sem_->get_semantic( mem_sym->associated_node() );
			assert( par_mem_ssi && par_mem_ssi->ty_proto()->is_builtin() );

			salviar::semantic_value const& sem = par_mem_ssi->semantic_value_ref();
			sv_layout* psvl = abii->input_sv_layout( sem );
			layout_to_sc(ctxt, psvl, false);
		} else {
			// If it is not semantic mode, use general code
			node_context* mem_ctxt = node_ctxt( mem_sym->associated_node(), true );
			assert( mem_ctxt );
			ctxt->node_value = mem_ctxt->node_value;
			ctxt->node_value.parent( agg_ctxt->node_value );
			ctxt->node_value.abi( agg_ctxt->node_value.abi() );
		}
	}
}

SASL_VISIT_DEF( variable_expression ){
	// T ODO Referenced symbol must be evaluated in semantic analysis stages.
	symbol* sym = find_symbol(v.var_name->str);
	assert(sym);
	
	// var_si is not null if sym is global value( sv_none is available )
	sv_layout* var_si = abii->input_sv_layout(sym);

	node_context* varctxt = node_ctxt( sym->associated_node() );
	node_context* ctxt = node_ctxt(v, true);
	if( var_si ){
		// TODO global only available in entry function.
		assert( is_entry( service()->fn().fn ) );
		ctxt->node_value = varctxt->node_value;
		return;
	}

	// Argument("virtual args") or local variable or in non-entry
	cg_impl::visit( v, data );
}

SASL_VISIT_DEF_UNIMPL( identifier );

// declaration & type specifier
SASL_VISIT_DEF_UNIMPL( initializer );
SASL_VISIT_DEF_UNIMPL( member_initializer );
SASL_VISIT_DEF_UNIMPL( declaration );

SASL_VISIT_DEF_UNIMPL( type_definition );
SASL_VISIT_DEF_UNIMPL( tynode );

SASL_VISIT_DEF_UNIMPL( alias_type );

// In cg_vs, you would initialize entry function before call
SASL_SPECIFIC_VISIT_DEF( before_decls_visit, program ){
	// Call parent for initialization
	parent_class::before_decls_visit( v, data );
	// Create entry function
	create_entry_params();
}

SASL_SPECIFIC_VISIT_DEF( bin_logic, binary_expression ){
	EFLIB_UNREF_DECLARATOR(data);
	cg_value ret_value = emit_logic_op(v.op, v.left_expr, v.right_expr);
	node_ctxt(v, true)->node_value = ret_value.to_rvalue();
}

SASL_SPECIFIC_VISIT_DEF( create_fnsig, function_type ){
	
	if( !entry_fn && abii->is_entry( sem_->get_symbol(&v) ) ){

		node_context* ctxt = node_ctxt(v, true);

		vector<Type*> param_types;
		add_entry_param_type( su_stream_in, param_types );
		add_entry_param_type( su_buffer_in, param_types );
		add_entry_param_type( su_stream_out, param_types );
		add_entry_param_type( su_buffer_out, param_types );

		FunctionType* fntype = FunctionType::get( Type::getVoidTy( cg_impl::context() ), param_types, false );
		Function* fn = Function::Create( fntype, Function::ExternalLinkage, sem_->get_symbol(&v)->mangled_name(), cg_impl::module() );
		fn->addFnAttr( Attribute::constructStackAlignmentFromInt(16) );
		entry_fn = fn;
		entry_sym = sem_->get_symbol(&v);

		ctxt->function_scope = ctxt_->create_cg_function();
		ctxt->function_scope->fn = fn;
		ctxt->function_scope->fnty = &v;
		ctxt->function_scope->cg = service();
	} else {
		parent_class::create_fnsig(v, data);
	}
}

SASL_SPECIFIC_VISIT_DEF( create_fnargs, function_type ){
	Function* fn = node_ctxt(v)->function_scope->fn;

	if( abii->is_entry( sem_->get_symbol(&v) ) ){
		// Create entry arguments.
		Function::arg_iterator arg_it = fn->arg_begin();

		arg_it->setName( ".arg.stri" );
		param_values[su_stream_in] = service()->create_value( builtin_types::none, arg_it, vkind_ref, abi_c );
		++arg_it;

		arg_it->setName( ".arg.bufi" );
		param_values[su_buffer_in] = service()->create_value( builtin_types::none, arg_it, vkind_ref, abi_c );
		++arg_it;

		arg_it->setName( ".arg.stro" );
		param_values[su_stream_out] = service()->create_value( builtin_types::none, arg_it, vkind_ref, abi_c );
		++arg_it;

		arg_it->setName( ".arg.bufo" );
		param_values[su_buffer_out] = service()->create_value( builtin_types::none, arg_it, vkind_ref, abi_c );
		++arg_it;

		// Create virtual arguments
		create_virtual_args(v, data);

	} else {
		parent_class::create_fnargs(v, data);
	}
}

SASL_SPECIFIC_VISIT_DEF( create_virtual_args, function_type ){
	EFLIB_UNREF_DECLARATOR(data);
	
	service()->new_block( ".init.vargs", true );

	BOOST_FOREACH( shared_ptr<parameter> const& par, v.params ){
		visit_child( par->param_type );
		node_semantic* par_ssi = sem_->get_semantic(par);

		node_context* pctxt = node_ctxt( par, true );

		// Create local variable for 'virtual argument' and 'virtual result'.
		if( par_ssi->ty_proto()->is_builtin() ){
			// Virtual args for built in typed argument.

			// Get Value from semantic.
			// Store value to local variable.
			salviar::semantic_value const& par_sem = par_ssi->semantic_value_ref();
			assert( par_sem != salviar::sv_none );
			sv_layout* psi = abii->input_sv_layout( par_sem );

			builtin_types hint = par_ssi->ty_proto()->tycode;
			pctxt->node_value = service()->create_variable( hint, abi_c, par->name->str );
			layout_to_sc(pctxt, psi, true);
		} else {
			// Virtual args for aggregated argument
			pctxt->is_semantic_mode = true;
		}
	}
	
	// Update globals
	BOOST_FOREACH( symbol* gsym, sem_->global_vars() ){
		node_semantic* pssi = sem_->get_semantic( gsym->associated_node() );

		// Global is filled by offset value with null parent.
		// The parent is filled when it is referred.
		sv_layout* svl = NULL;
		if( pssi->semantic_value_ref() == salviar::sv_none ){
			svl = abii->input_sv_layout( gsym );
		} else {
			svl = abii->input_sv_layout( pssi->semantic_value_ref() );
		}

		layout_to_sc(node_ctxt(gsym->associated_node(), true), svl, false);

		//if (v.init){
		//	EFLIB_ASSERT_UNIMPLEMENTED();
		//}
	}
}

SASL_SPECIFIC_VISIT_DEF( visit_return, jump_statement ){
	EFLIB_UNREF_DECLARATOR(data);

	if( is_entry( service()->fn().fn ) ){
		visit_child( v.jump_expr );

		// Copy result.
		cg_value ret_value = node_ctxt( v.jump_expr )->node_value;

		if( ret_value.hint() != builtin_types::none ){
			node_semantic* ret_ssi = sem_->get_semantic(service()->fn().fnty);
			sv_layout* ret_si = abii->input_sv_layout( ret_ssi->semantic_value_ref() );
			assert( ret_si );
			layout_to_value(ret_si).store( ret_value );
		} else {
			shared_ptr<struct_type> ret_struct = service()->fn().fnty->retval_type->as_handle<struct_type>();
			size_t member_index = 0;
			BOOST_FOREACH( shared_ptr<declaration> const& child, ret_struct->decls ){
				if( child->node_class() == node_ids::variable_declaration ){
					shared_ptr<variable_declaration> vardecl = child->as_handle<variable_declaration>();
					BOOST_FOREACH( shared_ptr<declarator> const& decl, vardecl->declarators ){
						node_semantic* decl_ssi = sem_->get_semantic(decl);
						sv_layout* decl_si = abii->output_sv_layout( decl_ssi->semantic_value_ref() );
						assert( decl_si );
						layout_to_value(decl_si).store( service()->emit_extract_val(ret_value, (int)member_index) );
						++member_index;
					}
				}
			}
		}
		
		// Emit entry return.
		service()->emit_return();
	} else {
		parent_class::visit_return(v, data);
	}
}

cg_vs::cg_vs()
	: entry_fn(NULL), entry_sym(NULL)
{
	service_ = new cgs_sisd();
}

bool cg_vs::is_entry( llvm::Function* fn ) const{
	assert(fn && entry_fn);
	return fn && fn == entry_fn;
}

cg_module_impl* cg_vs::mod_ptr(){
	return llvm_mod_.get();
}


cg_value cg_vs::layout_to_value(sv_layout* svl)
{
	cg_value ret;

	// TODO need to emit_extract_ref
	if( svl->usage == su_stream_in || svl->usage == su_stream_out || svl->agg_type == salviar::aggt_array ){
		ret = service()->emit_extract_val( param_values[svl->usage], svl->physical_index );
		ret = ret.as_ref();
	} else {
		ret = service()->emit_extract_ref( param_values[svl->usage], svl->physical_index );
	}

	assert(svl->value_type != salviar::lvt_none);
	ret.hint( to_builtin_types(svl->value_type) );

	return ret;
}

void cg_vs::layout_to_sc(node_context* psc, salviar::sv_layout* svl, bool store_to_existed_value)
{
	builtin_types bt = to_builtin_types(svl->value_type);

	cg_value ret;
	// TODO need to emit_extract_ref
	if( svl->usage == su_stream_in || svl->usage == su_stream_out || svl->agg_type == salviar::aggt_array ){
		ret = service()->emit_extract_val( param_values[svl->usage], svl->physical_index );
		ret = ret.as_ref();
	} else {
		ret = service()->emit_extract_ref( param_values[svl->usage], svl->physical_index );
	}

	if(svl->internal_type == -1)
	{
		ret.hint( to_builtin_types(svl->value_type) );
	}
	else
	{
		psc->ty = service()->create_ty( sem_->pety()->get_proto(svl->internal_type) );
		ret.tyinfo( psc->ty );
	}

	if( store_to_existed_value && psc->node_value.storable() )
	{
		psc->node_value.store(ret);
	}
	else
	{
		psc->node_value = ret;
	}
}

cg_vs::~cg_vs(){}

END_NS_SASL_CODEGEN();
