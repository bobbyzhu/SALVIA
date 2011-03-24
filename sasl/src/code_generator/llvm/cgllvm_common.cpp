#include <sasl/include/code_generator/llvm/cgllvm_common.h>

#include <sasl/enums/enums_helper.h>
#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/include/code_generator/llvm/cgllvm_globalctxt.h>
#include <sasl/include/code_generator/llvm/cgllvm_type_converters.h>
#include <sasl/include/semantic/name_mangler.h>
#include <sasl/include/semantic/abi_analyser.h>
#include <sasl/include/semantic/semantic_infos.imp.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/semantic/type_checker.h>
#include <sasl/include/semantic/type_converter.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/syntax_tree/statement.h>
#include <sasl/include/syntax_tree/program.h>

#include <softart/include/enums.h>

#include <eflib/include/diagnostics/assert.h>
#include <eflib/include/metaprog/util.h>
#include <eflib/include/platform/boost_begin.h>
#include <boost/assign/std/vector.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <eflib/include/platform/boost_end.h>

#include <string>

BEGIN_NS_SASL_CODE_GENERATOR();

using namespace syntax_tree;
using namespace boost::assign;
using namespace llvm;

using semantic::abi_info;
using semantic::const_value_si;
using semantic::extract_semantic_info;
using semantic::module_si;
using semantic::storage_si;
using semantic::operator_name;
using semantic::statement_si;
using semantic::symbol;
using semantic::type_converter;
using semantic::type_entry;
using semantic::type_equal;
using semantic::type_info_si;

using boost::shared_ptr;
using boost::any;
using boost::any_cast;

using std::vector;

typedef cgllvm_common_context* common_ctxt_handle;

#define is_node_class( handle_of_node, typecode ) ( (handle_of_node)->node_class() == syntax_node_types::typecode )

cgllvm_common_context const * any_to_cgctxt_ptr( const any& any_val  ){
	return any_cast<cgllvm_common_context>(&any_val);
}

cgllvm_common_context* any_to_cgctxt_ptr( any& any_val ){
	return any_cast<cgllvm_common_context>(&any_val);
}

#define data_as_cgctxt_ptr() ( any_to_cgctxt_ptr(*data) )

//////////////////////////////////////////////////////////////////////////
//
#define SASL_VISITOR_TYPE_NAME cgllvm_common

cgllvm_common::cgllvm_common()
	: msi( NULL ), abii( NULL )
{}

bool cgllvm_common::generate(
	module_si* mod,
	abi_info const* abii
	)
{
	this->msi = mod;
	this->abii = abii;

	if ( msi ){
		msi->root()->node()->accept( this, NULL );
		return true;
	}

	return false;
}

template <typename NodeT> any& cgllvm_common::visit_child( any& child_ctxt, const any& init_data, shared_ptr<NodeT> child )
{
	child_ctxt = init_data;
	EFLIB_ASSERT( !child_ctxt.empty(), "" );
	return visit_child( child_ctxt, child );
}

template <typename NodeT> any& cgllvm_common::visit_child( any& child_ctxt, shared_ptr<NodeT> child )
{
	child->accept( this, &child_ctxt );
	return child_ctxt;
}

template<typename NodeT>
common_ctxt_handle cgllvm_common::node_ctxt( boost::shared_ptr<NodeT> const& nd, bool create_if_need ){
	if ( !nd ){ return NULL; }

	node* ptr = static_cast<node*>(nd.get());
	ctxts_t::iterator it = ctxts.find( ptr );
	if ( it == ctxts.end() ){
		if( create_if_need ){
			shared_ptr<cgllvm_common_context> const& ret = ctxts[ptr] = create_codegen_context<cgllvm_common_context>( nd->handle() );
			return ret.get();
		}
		return NULL;
	}

	return (it->second).get();
}

common_ctxt_handle cgllvm_common::node_ctxt( node& nd, bool create_if_need ){
	return node_ctxt(nd.handle(), create_if_need);
}

// Process assign
void cgllvm_common::do_assign( any* data, shared_ptr<expression> lexpr, shared_ptr<expression> rexpr )
{
	shared_ptr<type_info_si> larg_tsi = extract_semantic_info<type_info_si>(lexpr);
	shared_ptr<type_info_si> rarg_tsi = extract_semantic_info<type_info_si>(rexpr);

	if ( larg_tsi->entry_id() != rarg_tsi->entry_id() ){
		if( typeconv->implicit_convertible( larg_tsi->entry_id(), rarg_tsi->entry_id() ) ){
			typeconv->convert( larg_tsi->type_info(), rexpr );
		} else {
			assert( !"Expression could not converted to storage type." );
		}
	}

	Value* addr = node_ctxt(lexpr)->addr;
	Value* val = node_ctxt(rexpr)->val;

	mctxt->builder()->CreateStore( val, addr );

	data_as_cgctxt_ptr()->type = node_ctxt(lexpr)->type;
	data_as_cgctxt_ptr()->addr = addr;
	data_as_cgctxt_ptr()->val = val;
}

Constant* cgllvm_common::get_zero_filled_constant( boost::shared_ptr<type_specifier> typespec )
{
	if( typespec->node_class() == syntax_node_types::builtin_type ){
		builtin_type_code btc = typespec->value_typecode;
		if( sasl_ehelper::is_integer( btc ) ){
			return ConstantInt::get( node_ctxt(typespec)->type, 0, sasl_ehelper::is_signed(btc) );
		}
		if( sasl_ehelper::is_real( btc ) ){
			return ConstantFP::get( node_ctxt(typespec)->type, 0.0 );
		}
	}

	EFLIB_ASSERT_UNIMPLEMENTED();
	return NULL;
}

llvm::Type const* cgllvm_common::create_builtin_type( builtin_type_code const& btc, bool& sign ){

	if ( sasl_ehelper::is_void( btc ) ){
		return Type::getVoidTy( mctxt->context() );
	}
	
	if( sasl_ehelper::is_scalar(btc) ){
		if( btc == builtin_type_code::_boolean ){
			return IntegerType::get( mctxt->context(), 1 );
		}
		if( sasl_ehelper::is_integer(btc) ){
			sign = sasl_ehelper::is_signed( btc );
			return IntegerType::get( mctxt->context(), (unsigned int)sasl_ehelper::storage_size( btc ) << 3 );
		}
		if ( btc == builtin_type_code::_float ){
			return Type::getFloatTy( mctxt->context() );
		}
		if ( btc == builtin_type_code::_double ){
			return Type::getDoubleTy( mctxt->context() );
		}
	} 
	
	if( sasl_ehelper::is_vector( btc) ){
		builtin_type_code scalar_btc = sasl_ehelper::scalar_of( btc );
		Type const* inner_type = create_builtin_type(scalar_btc, sign);
		return VectorType::get( inner_type, static_cast<uint32_t>(sasl_ehelper::len_0(btc)) );
	}
	
	if( sasl_ehelper::is_matrix( btc ) ){
		builtin_type_code scalar_btc = sasl_ehelper::scalar_of( btc );
		Type const* row_type =
			create_builtin_type( sasl_ehelper::vector_of(scalar_btc, sasl_ehelper::len_1(btc)), sign );
		return ArrayType::get( row_type, sasl_ehelper::len_0(btc) );
	}

	return NULL;
}

void cgllvm_common::restart_block( boost::any* data ){
	BasicBlock* restart = BasicBlock::Create( mctxt->context(), "", data_as_cgctxt_ptr()->parent_func );
	mctxt->builder()->SetInsertPoint(restart);
}

SASL_VISIT_DEF_UNIMPL( unary_expression );
SASL_VISIT_DEF( cast_expression ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.casted_type );
	visit_child( child_ctxt, child_ctxt_init, v.expr );

	shared_ptr<type_info_si> src_tsi = extract_semantic_info<type_info_si>( v.expr );
	shared_ptr<type_info_si> casted_tsi = extract_semantic_info<type_info_si>( v.casted_type );

	if( src_tsi->entry_id() != casted_tsi->entry_id() ){
		if( typeconv->convertible( casted_tsi->entry_id(), src_tsi->entry_id() ) == type_converter::cannot_conv ){
			// Here is code error. Compiler should report it.
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
		node_ctxt(v, true)->type = node_ctxt(v.casted_type)->type;
		typeconv->convert( v.handle(), v.expr );
	}

	data_as_cgctxt_ptr()->type = node_ctxt(v, true)->type;
	data_as_cgctxt_ptr()->val = node_ctxt(v, true)->val;
}

SASL_VISIT_DEF( binary_expression ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.left_expr );
	visit_child( child_ctxt, child_ctxt_init, v.right_expr );

	if( v.op == operators::assign ){
		do_assign( data, v.left_expr, v.right_expr );
	} else {
		shared_ptr<type_info_si> larg_tsi = extract_semantic_info<type_info_si>(v.left_expr);
		shared_ptr<type_info_si> rarg_tsi = extract_semantic_info<type_info_si>(v.right_expr);

		//////////////////////////////////////////////////////////////////////////
		// type conversation for matching the operator prototype

		// get an overloadable prototype.
		std::vector< shared_ptr<expression> > args;
		args += v.left_expr, v.right_expr;

		symbol::overloads_t overloads
			= data_as_cgctxt_ptr()->sym.lock()->find_overloads( operator_name( v.op ), typeconv, args );

		EFLIB_ASSERT_AND_IF( !overloads.empty(), "Error report: no prototype could match the expression." ){
			return;
		}
		EFLIB_ASSERT_AND_IF( overloads.size() == 1, "Error report: prototype was ambigous." ){
			return;
		}

		boost::shared_ptr<function_type> op_proto = overloads[0]->node()->typed_handle<function_type>();

		shared_ptr<type_info_si> p0_tsi = extract_semantic_info<type_info_si>( op_proto->params[0] );
		shared_ptr<type_info_si> p1_tsi = extract_semantic_info<type_info_si>( op_proto->params[1] );

		// convert value type to match proto type.
		if( p0_tsi->entry_id() != larg_tsi->entry_id() ){
			if( ! node_ctxt( p0_tsi->type_info() ) ){
				visit_child( child_ctxt, child_ctxt_init,  op_proto->params[0]->param_type );
			}
			node_ctxt(v.left_expr)->type = node_ctxt( p0_tsi->type_info() )->type;
			typeconv->convert( p0_tsi->type_info(), v.left_expr );
		}
		if( p1_tsi->entry_id() != rarg_tsi->entry_id() ){
			if( ! node_ctxt( p1_tsi->type_info() ) ){
				visit_child( child_ctxt, child_ctxt_init, op_proto->params[1]->param_type );
			}
			node_ctxt(v.right_expr)->type = node_ctxt( p1_tsi->type_info() )->type;
			typeconv->convert( p1_tsi->type_info(), v.right_expr );
		}

		// use type-converted value to generate code.
		Value* lval = node_ctxt( v.left_expr )->val;
		Value* rval = node_ctxt( v.right_expr )->val;

		Value* retval = NULL;
		if( lval && rval ){

			builtin_type_code lbtc = p0_tsi->type_info()->value_typecode;
			builtin_type_code rbtc = p1_tsi->type_info()->value_typecode;

			if (v.op == operators::add){
				if( sasl_ehelper::is_real(lbtc) ){
					retval = mctxt->builder()->CreateFAdd( lval, rval, "" );
				} else if( sasl_ehelper::is_integer(lbtc) ){
					retval = mctxt->builder()->CreateAdd( lval, rval, "" );
				}
			} else if ( v.op == operators::sub ){
				retval = mctxt->builder()->CreateSub( lval, rval, "" );
			} else if ( v.op == operators::mul ){
				retval = mctxt->builder()->CreateMul( lval, rval, "" );
			} else if ( v.op == operators::div ){
				EFLIB_INTERRUPT( "Division is not supported yet." );
			} else if ( v.op == operators::less ){
				if(sasl_ehelper::is_real(lbtc)){
					retval = mctxt->builder()->CreateFCmpULT( lval, rval );
				} else if ( sasl_ehelper::is_integer(lbtc) ){
					if( sasl_ehelper::is_signed(lbtc) ){
						retval = mctxt->builder()->CreateICmpSLT(lval, rval);
					}
					if( sasl_ehelper::is_unsigned(lbtc) ){
						retval = mctxt->builder()->CreateICmpULT(lval, rval);
					}
				}
			} else {
				EFLIB_INTERRUPT( (boost::format("Operator %s is not supported yet.") % v.op.name() ).str().c_str() );
			}
		}

		data_as_cgctxt_ptr()->val = retval;
	}

	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}

SASL_VISIT_DEF_UNIMPL( expression_list );
SASL_VISIT_DEF_UNIMPL( cond_expression );
SASL_VISIT_DEF_UNIMPL( index_expression );
SASL_VISIT_DEF_UNIMPL( call_expression );
SASL_VISIT_DEF_UNIMPL( member_expression );

SASL_VISIT_DEF( constant_expression ){

	any child_ctxt_init = *data;
	any child_ctxt;

	boost::shared_ptr<const_value_si> c_si = extract_semantic_info<const_value_si>(v);
	if( ! node_ctxt( c_si->type_info() ) ){
		visit_child( child_ctxt, child_ctxt_init, c_si->type_info() );
	}

	Value* retval = NULL;
	if( c_si->value_type() == builtin_type_code::_sint32 ){
		retval = ConstantInt::get( node_ctxt( c_si->type_info() )->type, uint64_t( c_si->value<int32_t>() ), true );
	} else if ( c_si->value_type() == builtin_type_code::_uint32 ) {
		retval = ConstantInt::get( node_ctxt( c_si->type_info() )->type, uint64_t( c_si->value<uint32_t>() ), false );
	} else if ( c_si->value_type() == builtin_type_code::_float ) {
		retval = ConstantFP::get( node_ctxt( c_si->type_info() )->type, c_si->value<double>() );
	} else {
		EFLIB_ASSERT_UNIMPLEMENTED();
	}

	data_as_cgctxt_ptr()->val = retval;
	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}

SASL_VISIT_DEF_UNIMPL( identifier );
SASL_VISIT_DEF( variable_expression ){

	shared_ptr<symbol> declsym = data_as_cgctxt_ptr()->sym.lock()->find( v.var_name->str );
	assert( declsym && declsym->node() );

	data_as_cgctxt_ptr()->addr = node_ctxt( declsym->node() )->addr;
	data_as_cgctxt_ptr()->type = node_ctxt( declsym->node() )->type;
	if ( data_as_cgctxt_ptr()->addr ){
		data_as_cgctxt_ptr()->val = mctxt->builder()->CreateLoad( data_as_cgctxt_ptr()->addr, v.var_name->str.c_str() );
	} else {
		data_as_cgctxt_ptr()->val = node_ctxt( declsym->node() )->val;
	}
	
	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}

// declaration & type specifier
SASL_VISIT_DEF_UNIMPL( initializer );
SASL_VISIT_DEF( expression_initializer ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.init_expr );

	shared_ptr<type_info_si> init_tsi = extract_semantic_info<type_info_si>(v.handle());
	shared_ptr<type_info_si> var_tsi = extract_semantic_info<type_info_si>(data_as_cgctxt_ptr()->variable_to_fill.lock());

	if( init_tsi->entry_id() != var_tsi->entry_id() ){
		typeconv->convert( var_tsi->type_info(), v.init_expr );
	}

	data_as_cgctxt_ptr()->type = any_to_cgctxt_ptr( child_ctxt )->type;
	data_as_cgctxt_ptr()->val = any_to_cgctxt_ptr( child_ctxt )->val;

	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}

SASL_VISIT_DEF_UNIMPL( member_initializer );
SASL_VISIT_DEF_UNIMPL( declaration );
SASL_VISIT_DEF( declarator ){
	any child_ctxt_init = *data;
	any_to_cgctxt_ptr( child_ctxt_init )->variable_to_fill = v.handle();
	any child_ctxt;

	Function* parent_func = data_as_cgctxt_ptr()->parent_func;

	IRBuilder<> vardecl_builder( mctxt->context() ) ;

	if( parent_func ){
		vardecl_builder.SetInsertPoint( &parent_func->getEntryBlock(), parent_func->getEntryBlock().begin() );
		data_as_cgctxt_ptr()->addr 
			= vardecl_builder.CreateAlloca( data_as_cgctxt_ptr()->type, 0, v.name->str.c_str() );
	} else {
		data_as_cgctxt_ptr()->addr
			= mctxt->module()->getOrInsertGlobal( v.name->str.c_str(), data_as_cgctxt_ptr()->type );
	}

	if ( v.init ){
		if (parent_func){
			visit_child( child_ctxt, child_ctxt_init, v.init );

			assert( any_to_cgctxt_ptr(child_ctxt)->val );
			vardecl_builder.CreateStore( any_to_cgctxt_ptr(child_ctxt)->val, data_as_cgctxt_ptr()->addr );
		} else {
			// Here is global variable initialization.
			EFLIB_ASSERT_UNIMPLEMENTED();
		}
	}

	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}
SASL_VISIT_DEF( variable_declaration ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.type_info );
	assert( any_to_cgctxt_ptr(child_ctxt)->type );

	any_to_cgctxt_ptr(child_ctxt_init)->type = any_to_cgctxt_ptr(child_ctxt)->type;

	BOOST_FOREACH( shared_ptr<declarator> decl, v.declarators ){
		visit_child( child_ctxt, child_ctxt_init, decl );
	}

	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}

SASL_VISIT_DEF_UNIMPL( type_definition );
SASL_VISIT_DEF_UNIMPL( type_specifier );
SASL_VISIT_DEF( builtin_type ){

	shared_ptr<type_info_si> tisi = extract_semantic_info<type_info_si>( v );

	if ( node_ctxt( tisi->type_info() ) ){
		*data = *node_ctxt( tisi->type_info() );
		return;
	}

	bool sign = false;
	Type const* ret_type = create_builtin_type(v.value_typecode, sign);

	std::string tips = v.value_typecode.name() + std::string(" was not supported yet.");
	EFLIB_ASSERT_AND_IF( ret_type, tips.c_str() ){
		return;
	}

	data_as_cgctxt_ptr()->type = ret_type;
	data_as_cgctxt_ptr()->is_signed = sign;

	*node_ctxt( tisi->type_info(), true ) = *(data_as_cgctxt_ptr());
}

SASL_VISIT_DEF_UNIMPL( array_type );
SASL_VISIT_DEF_UNIMPL( struct_type );
SASL_VISIT_DEF( parameter ){

	data_as_cgctxt_ptr()->sym = v.symbol();

	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.param_type );
	data_as_cgctxt_ptr()->type = any_to_cgctxt_ptr(child_ctxt)->type;
	data_as_cgctxt_ptr()->is_signed = any_to_cgctxt_ptr(child_ctxt)->is_signed;
	if (v.init){
		visit_child( child_ctxt, child_ctxt_init, v.init );
	} 

	*node_ctxt(v, true) = *(data_as_cgctxt_ptr());

}

SASL_VISIT_DEF( function_type ){
	
	data_as_cgctxt_ptr()->sym = v.symbol();

	any child_ctxt_init = *data;
	any child_ctxt;

	// Generate return types.
	visit_child( child_ctxt, child_ctxt_init, v.retval_type );
	const llvm::Type* ret_type = any_to_cgctxt_ptr(child_ctxt)->type;

	EFLIB_ASSERT_AND_IF( ret_type, "ret_type" ){
		return;
	}

	// Generate paramenter types.
	vector< const llvm::Type*> param_types;
	for( vector< boost::shared_ptr<parameter> >::iterator it = v.params.begin(); it != v.params.end(); ++it ){
		visit_child( child_ctxt, child_ctxt_init, *it );
		if ( any_to_cgctxt_ptr(child_ctxt)->type ){
			param_types.push_back( any_to_cgctxt_ptr(child_ctxt)->type );
		} else {
			EFLIB_ASSERT_AND_IF( ret_type, "Error occurs while parameter parsing." ){
				return;
			}
		}
	}

	// Create function.
	llvm::FunctionType* ftype = llvm::FunctionType::get( ret_type, param_types, false );
	data_as_cgctxt_ptr()->func_type = ftype;
	llvm::Function* fn = 
		Function::Create( ftype, Function::ExternalLinkage, v.name->str, mctxt->module() );
	data_as_cgctxt_ptr()->func = fn;
	any_to_cgctxt_ptr(child_ctxt_init)->parent_func = fn;

	// Register parameter names.
	llvm::Function::arg_iterator arg_it = fn->arg_begin();
	for( size_t arg_idx = 0; arg_idx < fn->arg_size(); ++arg_idx, ++arg_it){
		boost::shared_ptr<parameter> par = v.params[arg_idx];
		arg_it->setName( par->symbol()->unmangled_name() );
		common_ctxt_handle par_ctxt = node_ctxt( par );
		par_ctxt->val = arg_it;
	}


	// Create function body.
	if ( v.body ){
		visit_child( child_ctxt, child_ctxt_init, v.body );

		//////////////////////////////////////////////////////////////////////////
		// Process empty block.

		// Inner empty block, insert an br instruction for jumping to next block.
		for( Function::BasicBlockListType::iterator it = fn->getBasicBlockList().begin();
			it != fn->getBasicBlockList().end(); ++it
			)
		{
			if( it->empty() ){
				Function::BasicBlockListType::iterator next_it = it;
				++next_it;

				if( next_it != fn->getBasicBlockList().end() ){
					mctxt->builder()->CreateBr( &(*next_it) );
				} else {
					Value* val = get_zero_filled_constant( v.retval_type );
					mctxt->builder()->CreateRet(val);
				}
			}
		}
	}

	*node_ctxt(v, true) = *( data_as_cgctxt_ptr() );
}

// statement
SASL_VISIT_DEF_UNIMPL( statement );
SASL_VISIT_DEF( declaration_statement ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.decl );

	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}
SASL_VISIT_DEF( if_statement ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.cond );
	type_entry::id_t cond_tid = extract_semantic_info<type_info_si>(v.cond)->entry_id();
	type_entry::id_t bool_tid = msi->type_manager()->get( builtin_type_code::_boolean );
	if( cond_tid != bool_tid ){
		typeconv->convert( msi->type_manager()->get(bool_tid), v.cond );
	}
	BasicBlock* cond_block = mctxt->builder()->GetInsertBlock();

	// Generate 'then' branch code.
	BasicBlock* yes_block = BasicBlock::Create( mctxt->context(), v.yes_stmt->symbol()->mangled_name(), data_as_cgctxt_ptr()->parent_func );
	mctxt->builder()->SetInsertPoint( yes_block );
	visit_child( child_ctxt, child_ctxt_init, v.yes_stmt );
	BasicBlock* after_yes_block = mctxt->builder()->GetInsertBlock();
	
	// Generate 'else' branch code.
	BasicBlock* no_block = NULL;
	if( v.no_stmt ){
		no_block = BasicBlock::Create( mctxt->context(), v.no_stmt->symbol()->mangled_name(), data_as_cgctxt_ptr()->parent_func );
		mctxt->builder()->SetInsertPoint( no_block );
		visit_child( child_ctxt, child_ctxt_init, v.no_stmt );
	}
	BasicBlock* after_no_block = mctxt->builder()->GetInsertBlock();

	// Generate aggragate block
	BasicBlock* aggregate_block = BasicBlock::Create(
			mctxt->context(),
			extract_semantic_info<statement_si>(v)->exit_point().c_str(),
			data_as_cgctxt_ptr()->parent_func
		);
	
	// Fill back if-jump instruction
	mctxt->builder()->SetInsertPoint( cond_block );
	mctxt->builder()->CreateCondBr( node_ctxt(v.cond)->val, yes_block, no_block ? no_block : aggregate_block );

	// Fill back jump out instruct of each branch.
	mctxt->builder()->SetInsertPoint( after_yes_block );
	mctxt->builder()->CreateBr( aggregate_block );
	
	mctxt->builder()->SetInsertPoint( after_no_block );
	mctxt->builder()->CreateBr( aggregate_block );

	// Set insert point to end of code.
	mctxt->builder()->SetInsertPoint( aggregate_block );

	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}

SASL_VISIT_DEF_UNIMPL( while_statement );
SASL_VISIT_DEF_UNIMPL( dowhile_statement );
SASL_VISIT_DEF_UNIMPL( case_label );
SASL_VISIT_DEF_UNIMPL( switch_statement );

SASL_VISIT_DEF( compound_statement ){
	data_as_cgctxt_ptr()->sym = v.symbol();

	any child_ctxt_init = *data;
	any child_ctxt;

	BasicBlock* bb = NULL;
	// If instruction block is the first block of function, we must create it.
	if ( data_as_cgctxt_ptr()->parent_func->getBasicBlockList().empty() ){
		bb = BasicBlock::Create(
				mctxt->context(),
				v.symbol()->mangled_name(),
				data_as_cgctxt_ptr()->parent_func
				);
	}

	data_as_cgctxt_ptr()->block = bb;

	if(bb){
		mctxt->builder()->SetInsertPoint(bb);
	}

	for ( std::vector< boost::shared_ptr<statement> >::iterator it = v.stmts.begin();
		it != v.stmts.end(); ++it)
	{
		visit_child( child_ctxt, child_ctxt_init, *it );
	}

	*node_ctxt(v, true) = *( data_as_cgctxt_ptr() );
}

SASL_VISIT_DEF( expression_statement ){
	any child_ctxt_init = *data;
	any child_ctxt;

	visit_child( child_ctxt, child_ctxt_init, v.expr );

	*node_ctxt(v, true) = *( data_as_cgctxt_ptr() );
}

SASL_VISIT_DEF( jump_statement ){

	any child_ctxt_init = *data;
	any child_ctxt;

	if (v.jump_expr){
		visit_child( child_ctxt, child_ctxt_init, v.jump_expr );
	}

	if ( v.code == jump_mode::_return ){
		if ( !v.jump_expr ){
			data_as_cgctxt_ptr()->return_inst = mctxt->builder()->CreateRetVoid();
		} else {
			data_as_cgctxt_ptr()->return_inst = mctxt->builder()->CreateRet( any_to_cgctxt_ptr(child_ctxt)->val );
		}
	} else if ( v.code == jump_mode::_continue ){
		assert( data_as_cgctxt_ptr()->continue_to );
		mctxt->builder()->CreateBr( data_as_cgctxt_ptr()->continue_to );
	} else if ( v.code == jump_mode::_break ){
		assert( data_as_cgctxt_ptr()->break_to );
		mctxt->builder()->CreateBr( data_as_cgctxt_ptr()->break_to );
	}

	// Restart a new block for sealing the old block.
	restart_block(data);

	*node_ctxt(v, true) = *data_as_cgctxt_ptr();
}

SASL_VISIT_DEF_UNIMPL( ident_label );

SASL_VISIT_DEF( program ){
	UNREF_PARAM( data );
	if ( mctxt ){
		return;
	}

	mctxt = create_codegen_context<cgllvm_global_context>( v.handle() );
	mctxt->create_module( v.name );

	ctxt_getter = boost::bind( &cgllvm_common::node_ctxt<node>, this, _1, false );
	typeconv = create_type_converter( mctxt->builder(), ctxt_getter );
	register_builtin_typeconv( typeconv, msi->type_manager() );

	any child_ctxt = cgllvm_common_context();

	vector<Function*> proc_fns;

	for( vector< boost::shared_ptr<declaration> >::iterator
		it = v.decls.begin(); it != v.decls.end(); ++it )
	{
		visit_child( child_ctxt, (*it) );
	}
}

SASL_VISIT_DEF_UNIMPL( for_statement );

boost::shared_ptr<llvm_code> cgllvm_common::module(){
	return boost::shared_polymorphic_cast<llvm_code>(mctxt);
}

END_NS_SASL_CODE_GENERATOR();
