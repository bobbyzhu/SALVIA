#include <sasl/include/code_generator/llvm/cgllvm_vs.h>
#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/include/code_generator/llvm/cgllvm_globalctxt.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/program.h>

#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Function.h>
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

using namespace llvm;

using std::vector;

BEGIN_NS_SASL_CODE_GENERATOR();

void cgllvm_vs::fill_llvm_type_from_si( storage_types st ){
	vector<storage_info*> sis = abii->storage_infos( st );
	BOOST_FOREACH( storage_info* si, sis ){
		bool sign(false);
		Type const* storage_llvm_type = llvm_type(si->sv_type, sign );
		assert(storage_llvm_type);
		if( stream_in == st ){
			entry_params_types[st].push_back( PointerType::getUnqual( storage_llvm_type ) );
		} else {
			entry_params_types[st].push_back( storage_llvm_type );
		}
	}
	
	// Here we create packed data.
	// It is easy to compute struct layout.
	// TODO support aligned and packed layout in future.
	entry_params_structs[st].data() = StructType::get( mod_ptr()->context(), entry_params_types[st], true );
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
	*ctxt = *sc_ptr(data);

	ctxt->data().val_type = par_type;
	ctxt->data().ref_type = parref_type;
	ctxt->data().is_ref = true;

	param_ctxts[st].reset(ctxt);

	par_types.push_back(parref_type);
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
SASL_VISIT_DEF_UNIMPL( variable_expression );
SASL_VISIT_DEF_UNIMPL( identifier );

// declaration & type specifier
SASL_VISIT_DEF_UNIMPL( initializer );
SASL_VISIT_DEF_UNIMPL( expression_initializer );
SASL_VISIT_DEF_UNIMPL( member_initializer );
SASL_VISIT_DEF_UNIMPL( declaration );
SASL_VISIT_DEF( declarator ){
	sc_ptr(data)->sym = v.symbol();

	storage_si* pssi = dynamic_cast<storage_si*>( v.semantic_info().get() );

	// Local variable will call parent version.
	if( sc_inner_ptr(data)->parent_fn ){
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
	}

	if (v.init){
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	*node_ctxt(v, true) = *sc_ptr(data);
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
SASL_VISIT_DEF_UNIMPL( compound_statement );
SASL_VISIT_DEF_UNIMPL( expression_statement );
SASL_VISIT_DEF_UNIMPL( jump_statement );

// In cgllvm_vs, you would initialize entry function before call
SASL_SPECIFIC_VISIT_DEF( before_decls_visit, program ){
	// Call parent for initialization
	parent_class::before_decls_visit( v, data );

	// Create entry function
	create_entry_params();
}

SASL_SPECIFIC_VISIT_DEF( create_fnsig, function_type ){
	if( abii->is_entry( v.symbol() ) ){

		vector<Type const*> param_types;
		add_entry_param_type( data, stream_in, param_types );
		add_entry_param_type( data, buffer_in, param_types );
		add_entry_param_type( data, stream_out, param_types );
		add_entry_param_type( data, buffer_out, param_types );

		FunctionType* fntype = FunctionType::get( Type::getVoidTy(llcontext()), param_types, false );
		Function* fn = Function::Create( fntype, Function::ExternalLinkage, v.name->str, llmodule() );

		sc_inner_ptr(data)->val_type = fntype;
		sc_inner_ptr(data)->self_fn = fn;
	} else {
		parent_class::create_fnsig(v, data);
	}
}

SASL_SPECIFIC_VISIT_DEF( create_fnargs, function_type ){
	if( abii->is_entry( v.symbol() ) ){
	} else {
		parent_class::create_fnargs(v, data);
	}
}

cgllvm_vs::cgllvm_vs(){}

bool cgllvm_vs::is_entry( llvm::Function* fn ) const{
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

END_NS_SASL_CODE_GENERATOR();