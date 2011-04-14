#include <sasl/include/code_generator/llvm/cgllvm_vs.h>
#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/include/code_generator/llvm/cgllvm_globalctxt.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/syntax_tree/statement.h>
#include <sasl/include/syntax_tree/program.h>

#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Support/IRBuilder.h>
#include <eflib/include/platform/enable_warnings.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/foreach.hpp>
#include <eflib/include/platform/boost_end.h>

#define SASL_VISITOR_TYPE_NAME cgllvm_vs

using sasl::semantic::buffer_in;
using sasl::semantic::buffer_out;
using sasl::semantic::stream_in;
using sasl::semantic::stream_out;
using sasl::semantic::storage_info;
using sasl::semantic::storage_si;
using sasl::semantic::storage_types;
using sasl::semantic::symbol;

using namespace sasl::syntax_tree;
using namespace llvm;

using boost::any;
using boost::shared_ptr;
using std::vector;

BEGIN_NS_SASL_CODE_GENERATOR();

void cgllvm_vs::fill_llvm_type_from_si( storage_types st ){
	vector<storage_info*> sis = abii->storage_infos( st );
	BOOST_FOREACH( storage_info* si, sis ){
		bool sign(false);
		Type const* storage_llvm_type = llvm_type(si->sv_type, sign );
		assert(storage_llvm_type);
		if( stream_in == st || stream_out == st ){
			entry_params_types[st].push_back( PointerType::getUnqual( storage_llvm_type ) );
		} else {
			entry_params_types[st].push_back( storage_llvm_type );
		}
	}
	
	// Here we create packed data.
	// It is easy to compute struct layout.
	// TODO support aligned and unpacked layout in future.
	entry_params_structs[st].data() = StructType::get( mod_ptr()->context(), entry_params_types[st], true );

	char const* struct_name = NULL;
	switch( st ){
	case stream_in:
		struct_name = ".s.stri";
		break;
	case buffer_in:
		struct_name = ".s.bufi";
		break;
	case stream_out:
		struct_name = ".s.stro";
		break;
	case buffer_out:
		struct_name = ".s.bufo";
		break;
	}
	assert( struct_name );

	llmodule()->addTypeName( struct_name, entry_params_structs[st].data() );
}

void cgllvm_vs::create_entry_params(){
	fill_llvm_type_from_si ( buffer_in );
	fill_llvm_type_from_si ( buffer_out );
	fill_llvm_type_from_si ( stream_in );
	fill_llvm_type_from_si ( stream_out );
}

void cgllvm_vs::add_entry_param_type( boost::any* data, storage_types st, vector<Type const*>& par_types ){
	StructType* par_type = entry_params_structs[st].data();
	PointerType* parref_type = PointerType::getUnqual( par_type );

	cgllvm_sctxt* ctxt = new cgllvm_sctxt();
	ctxt->copy( sc_ptr(data) );

	ctxt->data().val_type = par_type;
	ctxt->data().ref_type = parref_type;
	ctxt->data().is_ref = true;

	param_ctxts[st].reset(ctxt);

	par_types.push_back(parref_type);
}

void cgllvm_vs::copy_to_result( boost::shared_ptr<sasl::syntax_tree::expression> const& v ){
	cgllvm_sctxt* expr_ctxt = node_ctxt(v);
	cgllvm_sctxt* ret_ctxt = node_ctxt( entry_sym->node(), false );

	store( load(expr_ctxt), ret_ctxt );
}

// expressions
SASL_VISIT_DEF_UNIMPL( unary_expression );
SASL_VISIT_DEF_UNIMPL( cast_expression );
SASL_VISIT_DEF_UNIMPL( binary_expression );
SASL_VISIT_DEF_UNIMPL( expression_list );
SASL_VISIT_DEF_UNIMPL( cond_expression );
SASL_VISIT_DEF_UNIMPL( index_expression );
SASL_VISIT_DEF_UNIMPL( call_expression );
SASL_VISIT_DEF_UNIMPL( member_expression );

SASL_VISIT_DEF_UNIMPL( constant_expression );

SASL_VISIT_DEF( variable_expression ){
	// TODO Referenced symbol must be evaluated in semantic analysis stages.
	shared_ptr<symbol> sym = find_symbol( sc_ptr(data), v.var_name->str );
	assert(sym);

	cgllvm_sctxt* varctxt = node_ctxt( sym->node() );
	storage_si* var_ssi = dynamic_cast<storage_si*>( v.semantic_info().get() );

	if ( is_entry( sc_data_ptr(data)->self_fn ) ){
		storage_info* var_si = abii->input_storage( sym );

		if( var_ssi->get_semantic() == softart::SV_None && !var_si ){
			// If non semantic and not have abii, it must be local variable.
			// Use normal data loader.
			sc_data_ptr(data)->val = load( varctxt );
			sc_ptr(data)->type( varctxt );
		} else {
			// Else the expression is stored as an offsetted space in argument.
			if( !var_si ){
				// If it is not global. It must be parameter of entry function.
				var_si = abii->input_storage( var_ssi->get_semantic() );
			}
			assert(var_si);

			sc_ptr(data)->storage_and_type( varctxt );
			sc_data_ptr(data)->agg.parent = param_ctxts[var_si->storage].get();
			sc_data_ptr(data)->val = load( sc_ptr(data) );
		}

		node_ctxt(v, true)->copy( sc_ptr(data) );

	} else {
		parent_class::visit( v, data );
	}
}

SASL_VISIT_DEF_UNIMPL( identifier );

// declaration & type specifier
SASL_VISIT_DEF_UNIMPL( initializer );
SASL_VISIT_DEF_UNIMPL( expression_initializer );
SASL_VISIT_DEF_UNIMPL( member_initializer );
SASL_VISIT_DEF_UNIMPL( declaration );
SASL_VISIT_DEF( declarator ){
	sc_env_ptr(data)->sym = v.symbol();

	storage_si* pssi = dynamic_cast<storage_si*>( v.semantic_info().get() );

	// Local variable will call parent version.
	if( sc_env_ptr(data)->parent_fn ){
		parent_class::visit( v, data );
	}

	// Global is filled by offset value with null parent.
	// The parent is filled when it is referred.
	storage_info* psi = NULL;
	if( pssi->get_semantic() == softart::SV_None ){
		psi = abii->input_storage( v.symbol() );
	} else {
		psi = abii->input_storage( pssi->get_semantic() );
	}

	sc_ptr(data)->data().val_type = llvm_type( psi->sv_type, sc_ptr(data)->data().is_signed );
	sc_ptr(data)->data().agg.index = psi->index;
	if( psi->storage == stream_in || psi->storage == stream_out ){
		sc_ptr(data)->data().is_ref = true;
	} else {
		sc_ptr(data)->data().is_ref = false;
	}

	if (v.init){
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	node_ctxt(v, true)->copy( sc_ptr(data) );
}

SASL_VISIT_DEF_UNIMPL( type_definition );
SASL_VISIT_DEF_UNIMPL( type_specifier );
SASL_VISIT_DEF_UNIMPL( builtin_type );
SASL_VISIT_DEF_UNIMPL( array_type );
SASL_VISIT_DEF_UNIMPL( struct_type );
SASL_VISIT_DEF_UNIMPL( parameter );

// statement
SASL_VISIT_DEF_UNIMPL( statement );
SASL_VISIT_DEF_UNIMPL( declaration_statement );
SASL_VISIT_DEF_UNIMPL( if_statement );
SASL_VISIT_DEF_UNIMPL( while_statement );
SASL_VISIT_DEF_UNIMPL( dowhile_statement );
SASL_VISIT_DEF_UNIMPL( for_statement );
SASL_VISIT_DEF_UNIMPL( case_label );
SASL_VISIT_DEF_UNIMPL( ident_label );
SASL_VISIT_DEF_UNIMPL( switch_statement );
SASL_VISIT_DEF_UNIMPL( expression_statement );

// In cgllvm_vs, you would initialize entry function before call
SASL_SPECIFIC_VISIT_DEF( before_decls_visit, program ){
	// Call parent for initialization
	parent_class::before_decls_visit( v, data );

	// Create entry function
	create_entry_params();
}

SASL_SPECIFIC_VISIT_DEF( create_fnsig, function_type ){
	if( !entry_fn && abii->is_entry( v.symbol() ) ){

		boost::any child_ctxt;

		vector<Type const*> param_types;
		add_entry_param_type( &( child_ctxt = *data ), stream_in, param_types );
		add_entry_param_type( &( child_ctxt = *data ), buffer_in, param_types );
		add_entry_param_type( &( child_ctxt = *data ), stream_out, param_types );
		add_entry_param_type( &( child_ctxt = *data ), buffer_out, param_types );

		FunctionType* fntype = FunctionType::get( Type::getVoidTy(llcontext()), param_types, false );
		Function* fn = Function::Create( fntype, Function::ExternalLinkage, v.name->str, llmodule() );
		entry_fn = fn;
		entry_sym = v.symbol().get();

		sc_data_ptr(data)->val_type = fntype;
		sc_data_ptr(data)->self_fn = fn;

	} else {
		parent_class::create_fnsig(v, data);
	}
}

SASL_SPECIFIC_VISIT_DEF( create_fnargs, function_type ){
	Function* fn = sc_data_ptr(data)->self_fn;

	if( abii->is_entry( v.symbol() ) ){
		// Create entry arguments.
		Function::arg_iterator arg_it = fn->arg_begin();

		cgllvm_sctxt* psctxt = new cgllvm_sctxt();
		param_ctxts[stream_in].reset( psctxt );
		arg_it->setName( ".arg.stri" );
		psctxt->data().val = arg_it++;
		psctxt->data().is_ref = true;
		psctxt->data().val_type = entry_params_structs[stream_in].data();

		psctxt = new cgllvm_sctxt();
		param_ctxts[buffer_in].reset( psctxt );
		arg_it->setName( ".arg.bufi" );
		psctxt->data().val = arg_it++;
		psctxt->data().is_ref = true;
		psctxt->data().val_type = entry_params_structs[buffer_in].data();

		psctxt = new cgllvm_sctxt();
		param_ctxts[stream_out].reset( psctxt );
		arg_it->setName( ".arg.stro" );
		psctxt->data().val = arg_it++;
		psctxt->data().is_ref = true;
		psctxt->data().val_type = entry_params_structs[stream_out].data();

		psctxt = new cgllvm_sctxt();
		param_ctxts[buffer_out].reset( psctxt );
		arg_it->setName( ".arg.bufo" );
		psctxt->data().val = arg_it++;
		psctxt->data().is_ref = true;
		psctxt->data().val_type = entry_params_structs[buffer_out].data();

		// Create return type
		psctxt = node_ctxt(v.symbol()->node(), true );
		storage_si* fn_ssi = dynamic_cast<storage_si*>( v.semantic_info().get() );
		if( fn_ssi->get_semantic() != softart::SV_None ){
			// Return an built-in value.
			storage_info* si = abii->output_storage( fn_ssi->get_semantic() );
			if( si->storage == stream_out ){
				psctxt->data().is_ref = true;
			} else {
				psctxt->data().is_ref = false;
			}
			psctxt->data().agg.index = si->index;
			psctxt->data().agg.parent = param_ctxts[si->storage].get();
		} else {
			// Return an aggregated value.
			EFLIB_ASSERT_UNIMPLEMENTED();
		}

		// Create virutal arguments
		create_virtual_args(v, data);

	} else {
		parent_class::create_fnargs(v, data);
	}
}

SASL_SPECIFIC_VISIT_DEF( create_virtual_args, function_type ){
	any child_ctxt_init = *data;
	sc_ptr(child_ctxt_init)->clear_data();

	any child_ctxt;

	BOOST_FOREACH( shared_ptr<parameter> const& par, v.params ){
		visit_child( child_ctxt, child_ctxt_init, par->param_type );
		storage_si* par_ssi = dynamic_cast<storage_si*>( par->semantic_info().get() );

		cgllvm_sctxt* tctxt = sc_ptr(child_ctxt);
		cgllvm_sctxt* pctxt = node_ctxt( par, true );
		pctxt->type(tctxt);

		if( par_ssi->type_info()->is_builtin() ){
			// Virtual args for built in typed argument.

			// Get Value from semantic.
			// Create local variable for 'virtual argument'.
			// Store value to local variable.
			softart::semantic par_sem = par_ssi->get_semantic();
			assert( par_sem != softart::SV_None );
			pctxt->env( sc_ptr(data) );
			pctxt->data().local = builder()->CreateAlloca( pctxt->data().val_type, 0, v.name->str );
			storage_info* psi = abii->input_storage( par_sem );
			
			cgllvm_sctxt tmpctxt;
			tmpctxt.data().agg.parent = param_ctxts[psi->storage].get();
			tmpctxt.data().agg.index = psi->index;

			store( load(&tmpctxt), pctxt );

		} else {
			// Virtual args for aggregated argument
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	}
}

SASL_SPECIFIC_VISIT_DEF( return_statement, jump_statement ){
	assert( sc_env_ptr(data)->parent_fn );
	if( is_entry( sc_env_ptr(data)->parent_fn ) ){
		assert( v.jump_expr );
		copy_to_result( v.jump_expr );
		sc_data_ptr(data)->return_inst = builder()->CreateRetVoid();
	} else {
		parent_class::return_statement(v, data);
	}
}

cgllvm_vs::cgllvm_vs(): entry_fn(NULL), entry_sym(NULL){}

bool cgllvm_vs::is_entry( llvm::Function* fn ) const{
	assert(fn && entry_fn);
	return fn && fn == entry_fn;
}

cgllvm_modvs* cgllvm_vs::mod_ptr(){
	assert( dynamic_cast<cgllvm_modvs*>( mod.get() ) );
	return static_cast<cgllvm_modvs*>( mod.get() );
}

bool cgllvm_vs::create_mod( sasl::syntax_tree::program& v )
{
	if ( mod ){ return false; }
	mod = create_codegen_context<cgllvm_modvs>( v.handle() );
	return true;
}

boost::shared_ptr<sasl::semantic::symbol> cgllvm_vs::find_symbol( cgllvm_sctxt* data, std::string const& str ){
	return data->env().sym.lock()->find( str );
}

END_NS_SASL_CODE_GENERATOR();