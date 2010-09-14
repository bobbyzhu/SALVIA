#include <sasl/include/code_generator/llvm/cgllvm.h>
#include <sasl/include/code_generator/llvm/cgllvm_context.h>
#include <sasl/include/code_generator/llvm/cgllvm_info.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/syntax_tree/declaration.h>
#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/syntax_tree/statement.h>
#include <sasl/include/syntax_tree/program.h>

BEGIN_NS_SASL_CODE_GENERATOR();

using namespace std;
using namespace syntax_tree;
using namespace llvm;

llvm_code_generator::llvm_code_generator( )
{
}

void llvm_code_generator::visit( unary_expression& v ){
	
}

void llvm_code_generator::visit( cast_expression& v){}
void llvm_code_generator::visit( binary_expression& v ){
	//// generate left and right expr.
	//v.left_expr->accept(this);
	//v.right_expr->accept(this);

	////
	//boost::shared_ptr<symbol> lsym = v.left_expr->symbol();
	//boost::shared_ptr<symbol> rsym = v.right_expr->symbol();

	//boost::shared_ptr<type_specifier> ltype = lsym->semantic_info<value_type_semantic_info>()->value_type();
	//boost::shared_ptr<type_specifier> rtype = rsym->semantic_info<value_type_semantic_info>()->value_type();

	//boost::shared_ptr<class semantic_info> ret_syminfo = v.symbol()->get_or_create_semantic_info<class semantic_info>();

	//// generate code
	//if (v.op == operators::add){
	//	if ( ltype->type_id_of_value == buildin_type_code::_sint32 &&
	//		ltype->type_id_of_value == buildin_type_code::_sint32
	//	){
	//		llvm::Value* ret_val = cg_ctxt.builder->CreateAdd( 
	//			lsym->semantic_info<class semantic_info>()->llvm_value,
	//			rsym->semantic_info<class semantic_info>()->llvm_value,
	//			generate_temporary_name()
	//			);
	//		ret_syminfo->llvm_value = ret_val;
	//	}
	//} else {
	//	throw "cannot support yet";
	//}
}
void llvm_code_generator::visit( expression_list& v ){}
void llvm_code_generator::visit( cond_expression& v ){}
void llvm_code_generator::visit( index_expression& v ){}
void llvm_code_generator::visit( call_expression& v ){}
void llvm_code_generator::visit( member_expression& v ){}

void llvm_code_generator::visit( constant_expression& v ){
	//using sasl::semantic_analyser::value_semantic_info;
	//boost::shared_ptr<value_semantic_info> val_syminfo
	//	= get_or_create_semantic_info<value_semantic_info>(v);
	//Value* ret_v = NULL;

	//if ( v.value->valtype == literal_constant_types::real ){
	//	if ( v.value->is_single() ){
	//		ret_v = ConstantFP::get( ctxt, APFloat( (float)val_syminfo->value<double>() ) );
	//	} else {
	//		ret_v = ConstantFP::get( ctxt, APFloat(val_syminfo->value<double>()) );
	//	}
	//} else if ( v.value->valtype == literal_constant_types::integer ){
	//	if ( v.value->is_unsigned() ){
	//		ret_v = ConstantInt::get( Type::getInt64Ty(ctxt), val_syminfo->value<unsigned long>() );
	//	} else {
	//		union {
	//			unsigned long u;
	//			signed long s;
	//		} u_to_s;
	//		u_to_s.s = val_syminfo->value<long>();
	//		ret_v = ConstantInt::get( Type::getInt64Ty(ctxt), u_to_s.u, true );
	//	}
	//}

	//boost::shared_ptr< class semantic_info > syminfo = get_or_create_semantic_info<class semantic_info>(v);
	//syminfo->llvm_value = ret_v;
}

void llvm_code_generator::visit( identifier& v ){

}
void llvm_code_generator::visit( variable_expression& v ){

}
// declaration & type specifier
void llvm_code_generator::visit( initializer& v ){}
void llvm_code_generator::visit( expression_initializer& v ){}
void llvm_code_generator::visit( member_initializer& v ){}
void llvm_code_generator::visit( declaration& v ){}
void llvm_code_generator::visit( variable_declaration& v ){
	//using ::sasl::semantic::extract_semantic_info;
	//using ::sasl::semantic::variable_semantic_info;
	//using ::sasl::semantic::get_or_create_semantic_info;

	//v.type_info->accept( this );
	//boost::shared_ptr<variable_semantic_info> vsyminfo = extract_semantic_info<variable_semantic_info>(v);
	//boost::shared_ptr<llvm_semantic_info> lsyminfo = get_or_create_semantic_info<llvm_semantic_info>(v);
	//boost::shared_ptr<llvm_semantic_info> ltsyminfo = extract_semantic_info<llvm_semantic_info>(v.type_info);
	//
	////TODO: GET VALUE OF INITIALIZER

	//if ( vsyminfo->is_local() ){
	//	// TODO: GENERATE LOCAL VARIABLE	
	//} else {
	//	// generate global variable
	//	GlobalVariable* gv = cast<GlobalVariable>(ctxt->module()->getOrInsertGlobal( v.name->str, ltsyminfo->llvm_type ));
	//	// TODO: OTHER OPERATIONS. SUCH AS LINKAGE
	//	// ...
	//	lsyminfo->llvm_gvar = gv;
	//}
}

void llvm_code_generator::visit( type_definition& v ){}
void llvm_code_generator::visit( type_specifier& v ){
}
void llvm_code_generator::visit( buildin_type& v ){
	//using ::sasl::semantic::get_or_create_semantic_info;

	//const llvm::Type* ltype = NULL;
	//
	//if (v.value_typecode == buildin_type_code::_sint32 ){
	//	ltype = llvm::cast<const llvm::Type>( llvm::IntegerType::getInt32Ty(ctxt->context()) );
	//}
	//// TODO: other buildin types.

	//boost::shared_ptr<llvm_semantic_info> lsyminfo = get_or_create_semantic_info<llvm_semantic_info>(v.handle());
	//lsyminfo->llvm_type = ltype;
}
void llvm_code_generator::visit( array_type& v ){}
void llvm_code_generator::visit( struct_type& v ){}
void llvm_code_generator::visit( parameter& v ){

}

void llvm_code_generator::visit( function_type& v ){
	//// get return type
	//v.retval_type->accept( this );
	//llvm::Type* ret_type = extract_semantic_info<class semantic_info>(v.retval_type)->llvm_type;

	//// get paramenter type
	//std::vector<const llvm::Type* > param_types;
	//for( vector< boost::shared_ptr<parameter> >::iterator it = v.params.begin(); it != v.params.end(); ++it ){
	//	(*it)->accept( this );
	//	param_types.push_back( extract_semantic_info<class semantic_info>(*it)->llvm_type );
	//}

	//// create function info
	//FunctionType* funcType = FunctionType::get( ret_type, param_types, false );
	//
	//// create function and preparing parameters
	//Function* func = static_cast<Function*>( cg_ctxt.mod->getOrInsertFunction(v.name->lit, funcType) );
	//size_t arg_idx = 0;
	//for( Function::arg_iterator arg_it = func->arg_begin(); arg_it != func->arg_end(); ++arg_it, ++arg_idx){
	//	arg_it->setName( v.params[arg_idx]->ident->lit );
	//	get_or_create_semantic_info<class semantic_info>( v.params[arg_idx] )->llvm_value = arg_it;
	//}

	//// generate function code
	//BasicBlock* funcCodeBlock = BasicBlock::Create( ctxt, "entry", func );
	//cg_ctxt.builder->SetInsertPoint(funcCodeBlock);
	//for ( std::vector< boost::shared_ptr<statement> >::iterator it = v.stmts.begin();
	//	it != v.stmts.end(); ++it ){
	//	(*it)->accept( this );
	//}
}

// statement
void llvm_code_generator::visit( statement& v ){
}
void llvm_code_generator::visit( declaration_statement& v ){}
void llvm_code_generator::visit( if_statement& v ){}
void llvm_code_generator::visit( while_statement& v ){}
void llvm_code_generator::visit( dowhile_statement& v ){}
void llvm_code_generator::visit( case_label& v ){}
void llvm_code_generator::visit( switch_statement& v ){}
void llvm_code_generator::visit( compound_statement& v ){}
void llvm_code_generator::visit( expression_statement& v ){}
void llvm_code_generator::visit( jump_statement& v ){}

void llvm_code_generator::visit( ident_label& ){ }

void llvm_code_generator::visit( program& v ){
	//if ( ctxt ){
	//	return;
	//} else {
	//	ctxt.reset( new cgllvm_context(v.name) );
	//}
}

boost::shared_ptr<llvm::Module> llvm_code_generator::generated_module(){
	return ctxt->module();
}

END_NS_SASL_CODE_GENERATOR();