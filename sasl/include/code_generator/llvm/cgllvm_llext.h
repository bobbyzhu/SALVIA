#ifndef SASL_CODE_GENERATOR_LLVM_CGLLVM_LLEXT_H
#define SASL_CODE_GENERATOR_LLVM_CGLLVM_LLEXT_H

#include <sasl/include/code_generator/forward.h>

#include <eflib/include/platform/disable_warnings.h>
#include <llvm/Support/IRBuilder.h>
#include <eflib/include/platform/enable_warnings.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/for.hpp>
#include <eflib/include/platform/boost_end.h>

#include <vector>

namespace llvm{
	class Value;
	class LLVMContext;
}

BEGIN_NS_SASL_CODE_GENERATOR();

template <typename BuilderT> class llext;

template<typename BuilderT>
class llvalue{
public:
	typedef BuilderT builder_t;

	llvalue( llvm::Value* val, llext<builder_t>* ext )
		:val(val), ext(ext)
	{}

	llvalue(): val(NULL), ext(NULL){}
	llvalue( const llvalue& rhs ): val(rhs.val), ext(rhs.ext){}
	llvalue& operator = ( llvalue const& rhs ){
		val = rhs.val;
		ext = rhs.ext;
		return *this;
	}
	
	llvm::Value* v() const{
		return val;
	}
	
public:
	llvm::Value* val;
	llext<BuilderT>* ext;
};

template<typename BuilderT, unsigned int Bits, bool IsSigned>
class llv_int: public llvalue<BuilderT>{
public:
	typedef BuilderT builder_t;

	llv_int( llvm::Value* val, llext<builder_t>* ext ): llvalue( val, ext )
	{
	}

	template <typename ValueT>
	llv_int( llext<builder_t>* ext, ValueT v )
		:llvalue( ConstantInt::get( IntegerType::get(ext->ctxt, Bits ), static_cast<uint64_t>(v), IsSigned ), ext )
	{
	}

	static llv_int<BuilderT, Bits, IsSigned> null_value(){
		return llv_int<BuilderT, Bits, IsSigned>( ext, 0 );
	}
};

template<typename BuilderT>
class llv_fp: public llvalue<BuilderT>{
public:
	typedef BuilderT builder_t;

	llv_fp( llvm::Value* val, llext<builder_t>* ext )
		:llvalue(val, ext)
	{}

	llv_fp( llext<builder_t>* ext, float v )
		:llvalue( ConstantFP::get( Type::getFloatTy(ext->ctxt), v ), ext )
	{
	}
	llv_fp( llext<builder_t>* ext, double v )
		:llvalue( ConstantFP::get( Type::getDoubleTy(ext->ctxt), v ), ext )
	{
	}

	static llv_fp<builder_t> null_value(){
		return llv_fp<builder_t>( ext, 0.0 );
	}
};

template<typename ElementT>
class llvector: public llvalue< typename ElementT::builder_t >{
public:
	typedef typename ElementT::builder_t builder_t;

	typedef llvector<ElementT> this_type;

	llvector( llvm::Value* val, llext<builder_t>* ext )
		:llvalue(val, ext)
	{}

	llvector( std::vector<ElementT> const& v )
	{
		assert( !v.empty() );

		ext = v[0].ext;
		llvector<ElementT> ret( ext->null_value( v[0] ), v.size() );
		val = ret.val;

		for( size_t idx = 0; idx < v.size(); ++idx ){
			set( idx, v[idx] );
		}
	}

	template <typename ValueT>
	static llvector<ElementT> from_values( llext<builder_t>* ext, std::vector<ValueT> const& v ){
		std::vector<ElementT> elems;
		elems.reserve(v.size());
		for( size_t i = 0; i < v.size(); ++i ){
			elems.push_back( ElementT( ext, v[i] ) );
		}
		return llvector<ElementT>( elems );
	}

	llvector( ElementT const& v, size_t nvec ){
		ext = v.ext;

		vector<Constant*> vals;
		for( size_t idx = 0 ; idx < nvec; ++idx ){
			vals.push_back( cast<Constant>( ext->null_value(v).val) );
		}

		llvm::VectorType const* vtype = VectorType::get( v.val->getType(), static_cast<unsigned>( nvec ) );
		val = ConstantVector::get( vtype, vals );

		for( size_t idx = 0; idx < nvec; ++idx ){
			set( idx, v );
		}
	};

	llvector<ElementT> swizzle( std::vector<int> const& indices ){
		typedef llvector< llv_int<builder_t, 32, true> > i32vec;

		i32vec masks = i32vec::from_values( ext, indices );
		return llvector<ElementT>(
			ext->builder->CreateShuffleVector(
				val, UndefValue::get(val->getType()),
				masks.val ),
				ext
			);
	}
	
	template <typename IndexT>
	llvector<ElementT> swizzle( IndexT* indices, int max_index = 0 ){
		std::vector<int> index_values;
		int step = 0;
		while( indices[step] != -1 && ( max_index == 0 || step < max_index ) ){
			index_values.push_back( indices[step] );
		}
		return swizzle( index_values );
	}

	llvector<ElementT>& set( size_t idx, ElementT const& v ){
		val = ext->builder->CreateInsertElement( val, v.v(), llv_int<builder_t, 32, true>(ext, idx).val );
		return *this;
	}

	ElementT operator []( size_t idx ){
		return ElementT( ext->builder->CreateExtractElement( val, llv_int<builder_t, 32, true>(ext, idx).val ), ext );
	}
};

template <typename ElementT> class llvar;

template <typename ElementT>
class llptr: public llvalue<typename ElementT::builder_t>{
public:
	typedef typename ElementT::builder_t builder_t;
	
	llptr( llvm::Value* ptr, llext<builder_t>* ext )
		: llvalue<builder_t>( ptr, ext )
	{
	}

	ElementT operator *(){
		return (ElementT)( (*this)[0] );
	}

	llvar<ElementT> operator [] ( size_t index ){
		llv_int<builder_t, 32, true> index_value(ext, index);
		llvm::Value* elem_ptr = ext->builder->CreateGEP( val, index_value );
		return llvar<ElementT>( elem_ptr, ext );
	}

	llptr<ElementT> operator + ( size_t index ){
		llv_int<builder_t, 32, true> index_value(ext, index);
		llvm::Value* elem_ptr = ext->builder->CreateGEP( val, index_value );
		return llptr<ElementT>( elem_ptr, ext );
	}
};

template<typename ElementT>
class llvar: llvalue< typename ElementT::builder_t >{
public:
	typedef typename ElementT::builder_t builder_t;
	
	llvar( llvm::Type const* var_type, llext<builder_t>* ext ){
		val = ext->builder->CreateAlloca( var_type );
		cast<llvm::AllocaInst>(val)->setAlignment(16);
		this->ext = ext;
	}

	llvar( llvm::Value* val, llext<builder_t>* ext ){
		val = ext->builder->CreateAlloca( val->getType() );
		cast<llvm::AllocaInst>(val)->setAlignment(16);
		this->ext = ext;
	}

	llptr<ElementT> operator &(){
		return llptr<ElementT>( val, ext );
	}

	operator ElementT(){
		LoadInst* inst = ext->builder->CreateLoad(val);
		inst->setAlignment(4);
		return ElementT( inst, ext );
	}

	llvar<ElementT> operator = ( ElementT const& v ){
		llvm::StoreInst* inst = ext->builder->CreateStore( v.val, val );
		inst->setAlignment(4);
	}
};

template <typename BuilderT>
class llaggregated: public llvalue<BuilderT>{
public:
	typedef BuilderT builder_t;
	
	llaggregated( llvm::Value* val, llext<builder_t>* ext )
		:llvalue(val, ext)
	{}

	template <typename ElementT>
	llaggregated<BuilderT> set( size_t idx, ElementT const& v ){
		val = ext->builder->CreateInsertValue( val, v.v(), static_cast<unsigned>(idx) );
	}

	template <typename ElementT>
	ElementT get( size_t idx ){
		return ElementT( ext->builder->CreateExtractValue( val, static_cast<unsigned>(idx) ), ext );
	}

	llvalue<BuilderT> operator []( size_t idx ){
		return get< llvalue<BuilderT> >(idx);
	}
};

template<typename ElementT>
class llarray: public llvalue< typename ElementT::builder_t >{
public:
	typedef typename ElementT::builder_t builder_t;

	typedef llarray<ElementT> this_type;

	llarray( llvm::Value* val, llext<builder_t>* ext )
		:llvalue(val, ext)
	{}

	llarray<ElementT> set( size_t idx, ElementT const& v ){
		val = ext->builder->CreateInsertValue( val, v.v(), static_cast<unsigned>(idx) );
	}

	ElementT operator []( size_t idx ){
		return ElementT( ext->builder->CreateExtractValue( val, static_cast<unsigned>(idx) ), ext );
	}
};


class ll_block{
public:
	ll_block( llvm::BasicBlock* block ): block(block){}
	ll_block(): block(NULL){}

	operator bool() const{
		return block != NULL;
	}
	llvm::BasicBlock* block;
};

#define LLEXT_CONTEXT_MEMBERS() \
	((llvalue<BuilderT>,	cond_val	)) \
	((ll_block,				cond_block	)) \
	((ll_block,				then_block	)) \
	((ll_block,				else_block	))

#define LLEXT_CONTEXT_DECLARE_MEMBER( r, data, elem ) \
	BOOST_PP_TUPLE_ELEM( 2, 0, elem) BOOST_PP_TUPLE_ELEM( 2, 1, elem ) ;

#define LLEXT_CONTEXT_DECLARE_MEMBERS()	\
	BOOST_PP_SEQ_FOR_EACH( LLEXT_CONTEXT_DECLARE_MEMBER, _, LLEXT_CONTEXT_MEMBERS() )
	
#define LLEXT_CONTEXT_PUSH_MEMBER( r, data, elem )	\
	ctxts.back().BOOST_PP_TUPLE_ELEM(2, 1, elem) = BOOST_PP_TUPLE_ELEM(2, 1, elem);

#define LLEXT_CONTEXT_PUSH() \
	ctxts.push_back( llext_context<BuilderT>() );	\
	BOOST_PP_SEQ_FOR_EACH( LLEXT_CONTEXT_DECLARE_MEMBER, _, LLEXT_CONTEXT_MEMBERS() );
	
#define LLEXT_CONTEXT_POP_MEMBER( r, data, elem )	\
	BOOST_PP_TUPLE_ELEM(2, 1, elem) = ctxts.back().BOOST_PP_TUPLE_ELEM(2, 1, elem);

#define LLEXT_CONTEXT_POP() \
	BOOST_PP_SEQ_FOR_EACH( LLEXT_CONTEXT_DECLARE_MEMBER, _, LLEXT_CONTEXT_MEMBERS() ); \
	ctxts.pop_back();

template <typename BuilderT>
struct llext_context{
	LLEXT_CONTEXT_DECLARE_MEMBERS();
};

template <typename BuilderT>
class llext{
public:
	typedef llvalue<BuilderT> llvalue_t;

	llext( llvm::LLVMContext& ctxt, BuilderT* builder )
		:ctxt( ctxt ), builder( builder ){}

	llvalue<BuilderT> null_value( llvm::Type const* t ){
		return llvalue<BuilderT>( Constant::getNullValue(t), this );
	}

	template<typename T>
	T null_value( llvm::Type const* t ){
		return T( Constant::getNullValue(t), this );
	}

	template <typename ElementT>
	ElementT null_value( ElementT const& v ){
		return ElementT( Constant::getNullValue(v.val->getType()), this );
	}

	// Fake statements

	/** If ... Then ... Else ... End If statement
	@{ */
public:
	void if_( llvalue<BuilderT> const& cond ){
		assert( cond.val->getType()->isIntegerTy(1) );

		// Save cond, then, else.
		push();

		cond_block = ll_block( builder->GetInsertBlock() );
		cond_val = cond;
		else_block = then_block = ll_block();
	}
	void then_(){
		assert( cond_block );
		then_block = block( "then" );
		insert_to( then_block );
	}
	void else_(){
		assert( then_block );
		then_block = current_block();
		else_block = block( "else" );
		insert_to( else_block );
	}
	void end_if(){
		assert( cond_block && then_block );

		ll_block cur_block = block( "endif" );

		if( then_block ){
			insert_to(then_block);
			goto_(cur_block);
		}

		if( else_block ){
			insert_to(else_block);
			goto_(cur_block);
		} else {
			else_block = cur_block;
		}

		insert_to( cond_block );
		goto_( cond_val, then_block, else_block );

		insert_to( cur_block );

		// Recovery cond, then, else.
		pop();
	}
	/** @} */

	/** Jump statement @{ */ 
	void goto_( llvalue<BuilderT> const& cond_v, ll_block const& yes, ll_block const& no ){
		builder->CreateBr( cond_v.val, yes.block, no.block );
	}

	void goto_( ll_block const& block ){
		builder->CreateBr( block.block );
	}
	void return_( llvalue<BuilderT> const& v ){
		builder->CreateRet(v.val);
	}

	void return_(){
		builder->CreateRetVoid();
	}
	/** @} */

	llvm::LLVMContext& ctxt;
	BuilderT* builder;

private:

	LLEXT_CONTEXT_DECLARE_MEMBERS();

	/** Utility functions
	@{*/

	ll_block current_block(){
		return ll_block( builder->GetInsertBlock() );
	}

	void insert_to( ll_block const& block ){
		builder->SetInsertBlock( block.block );
	}

	void push_insert_point(){
		insert_points.push_back(
			make_pair( builder->GetInsertBlock(), builder->GetInsertPoint() )
			);
	}

	void pop_insert_point(){
		builder->SetInsertPoint( insert_points.back().first, insert_points.back().second );
		insert_points.pop_back();
	}

	void push(){
		LLEXT_CONTEXT_PUSH();
	}

	void pop(){
		LLEXT_CONTEXT_POP();
	}

	/** @} */

	// attributes
	std::vector< llext_context<BuilderT> > ctxts;
	std::vector< std::pair<llvm::BasicBlock*, llvm::BasicBlock::iterator> > insert_points;

	llext( llext<BuilderT> const& );
	llext& operator = ( const llext<BuilderT>& );
};

/* ------------------------------------------------------------------------- */

// Operator +
template <typename BuilderT>
llv_fp<BuilderT> operator + ( llv_fp<BuilderT> const& lhs, llv_fp<BuilderT> const& rhs ){
	llext<BuilderT>* ext = lhs.ext;
	return llv_fp<BuilderT>( ext->builder->CreateFAdd( lhs.v(), rhs.v() ), ext );
}

template <typename BuilderT>
llv_int<BuilderT, 32, true> operator + ( llv_int<BuilderT, 32, true> const& lhs, llv_int<BuilderT, 32, true> const& rhs ){
	llext<BuilderT>* ext = lhs.ext;
	return llv_int<BuilderT, 32, true>( ext->builder->CreateAdd( lhs.v(), rhs.v() ), ext );
}

template <typename BuilderT>
llvector< llv_fp<BuilderT> > operator + (
	llvector< llv_fp<BuilderT> > const& lhs,
	llvector< llv_fp<BuilderT> > const& rhs
	)
{
	llext<BuilderT>* ext = lhs.ext;
	return llvector< llv_fp<BuilderT> >( ext->builder->CreateFAdd( lhs.v(), rhs.v() ), ext );
}

/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */

// Operator *
template<typename BuilderT>
llv_fp<BuilderT> operator * ( llv_fp<BuilderT> const& lhs, llv_fp<BuilderT> const& rhs ){
	llext<BuilderT>* ext = lhs.ext;
	return llv_fp<BuilderT>( ext->builder->CreateFMul( lhs.v(), rhs.v() ), ext );
}

template<typename BuilderT>
llv_int<BuilderT, 32, true> operator * ( llv_int<BuilderT, 32, true> const& lhs, llv_int<BuilderT, 32, true> const& rhs ){
	llext<BuilderT>* ext = lhs.ext;
	return llv_int<BuilderT, 32, true>( ext->builder->CreateMul( lhs.v(), rhs.v() ), ext );
}

template <typename BuilderT>
llvector< llv_fp<BuilderT> > operator * (
	llvector< llv_fp<BuilderT> > const& lhs,
	llvector< llv_fp<BuilderT> > const& rhs
	)
{
	llext<BuilderT>* ext = lhs.ext;
	return llvector< llv_fp<BuilderT> >( ext->builder->CreateFMul( lhs.v(), rhs.v() ), ext );
}
/* ------------------------------------------------------------------------- */
END_NS_SASL_CODE_GENERATOR();

#endif