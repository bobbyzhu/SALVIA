#include <eflib/include/platform/disable_warnings.h>
#include <LLVM/Support/IRBuilder.h>
#include <eflib/include/platform/enable_warnings.h>
#include <sasl/include/code_generator/llvm/cgllvm_type_converters.h>
#include <sasl/include/code_generator/llvm/cgllvm_contexts.h>
#include <sasl/include/semantic/semantic_infos.h>
#include <sasl/include/semantic/type_converter.h>
#include <sasl/include/semantic/type_manager.h>
#include <sasl/include/syntax_tree/node.h>
#include <sasl/include/syntax_tree/utility.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <eflib/include/platform/boost_end.h>

#include <eflib/include/diagnostics/assert.h>

using ::llvm::IRBuilderBase;
using ::llvm::IRBuilder;
using ::llvm::Type;
using ::llvm::Value;
using ::llvm::VectorType;

using ::sasl::semantic::symbol;
using ::sasl::semantic::type_converter;
using ::sasl::semantic::type_entry;
using ::sasl::semantic::type_info_si;
using ::sasl::semantic::type_manager;

using ::sasl::syntax_tree::create_builtin_type;
using ::sasl::syntax_tree::node;
using ::sasl::syntax_tree::type_specifier;

using ::boost::function;
using ::boost::make_shared;
using ::boost::shared_polymorphic_cast;
using ::boost::shared_ptr;
using ::boost::shared_static_cast;

BEGIN_NS_SASL_CODE_GENERATOR();

class cgllvm_type_converter : public type_converter{
public:
	cgllvm_type_converter(
		shared_ptr<IRBuilderBase> const& builder,
		boost::function<cgllvm_sctxt*( boost::shared_ptr<node> const& )> const& ctxt_getter,
		boost::function< llvm::Value* ( cgllvm_sctxt* ) > const& loader,
		boost::function< void (llvm::Value*, cgllvm_sctxt*) > const& storer
		)
		: builder( shared_static_cast<IRBuilder<> >(builder) ),
		get_ctxt( ctxt_getter ), load(loader), store(storer)
	{
	}

	void int2int( shared_ptr<node> dest, shared_ptr<node> src ){
		cgllvm_sctxt* dest_ctxt = get_ctxt(dest);
		cgllvm_sctxt* src_ctxt = get_ctxt(src);

		Value* dest_v = builder->CreateIntCast(
			load( src_ctxt ),
			dest_ctxt->data().val_type,
			dest_ctxt->data().is_signed
			);
		store( dest_v, dest_ctxt );
	}

	void int2float( shared_ptr<node> dest, shared_ptr<node> src ){
		cgllvm_sctxt* dest_ctxt = get_ctxt(dest);
		cgllvm_sctxt* src_ctxt = get_ctxt(src);

		Value* src_v = load( src_ctxt );
		Type const* dest_type = dest_ctxt->data().val_type;
		Value* dest_v = NULL;
		if ( src_ctxt->data().is_signed ){
			dest_v = builder->CreateSIToFP( src_v, dest_type );
		} else {
			dest_v = builder->CreateUIToFP( src_v, dest_type );
		}
		store( dest_v, dest_ctxt );
	}

	void float2int( shared_ptr<node> dest, shared_ptr<node> src ){
		cgllvm_sctxt* dest_ctxt = get_ctxt(dest);
		cgllvm_sctxt* src_ctxt = get_ctxt(src);

		Value* src_v = load( src_ctxt );
		Type const* dest_type = dest_ctxt->data().val_type;
		Value* dest_v = NULL;
		if ( dest_ctxt->data().is_signed ){
			dest_v = builder->CreateFPToSI( src_v, dest_type );
		} else {
			dest_v = builder->CreateFPToUI( src_v, dest_type );
		}
		store( dest_v, dest_ctxt );
	}

	void float2float( shared_ptr<node> dest, shared_ptr<node> src ){
		cgllvm_sctxt* dest_ctxt = get_ctxt(dest);
		cgllvm_sctxt* src_ctxt = get_ctxt(src);

		Type const* dest_type = dest_ctxt->data().val_type;
		Value* dest_v = builder->CreateFPCast( load(src_ctxt), dest_type );

		store(dest_v, dest_ctxt);
	}

	void scalar2vec1( shared_ptr<node> dest, shared_ptr<node> src ){
		EFLIB_ASSERT_UNIMPLEMENTED();
		cgllvm_sctxt* dest_ctxt = get_ctxt(dest);
		cgllvm_sctxt* src_ctxt = get_ctxt(src);

		Type const* elem_type = src_ctxt->data().val_type;
		Type const* dest_type = VectorType::get( elem_type, 1 );

		// Store value to an vector
		cgllvm_sctxt agg_ctxt;
		cgllvm_sctxt elem_ctxt;
		elem_ctxt.data().agg.is_swizzle = true;
		elem_ctxt.data().agg.swizzle = 1; /*X*/
		elem_ctxt.data().agg.parent = &agg_ctxt;
		store( load(src_ctxt), &elem_ctxt );

		store( load(&agg_ctxt), dest_ctxt );
		dest_ctxt->data().val_type = dest_type;
	}
private:
	shared_ptr< IRBuilder<> > builder;
	boost::function<cgllvm_sctxt*( boost::shared_ptr<node> const& )> get_ctxt;
	boost::function< Value* ( cgllvm_sctxt* ) > load;
	boost::function< void (Value*, cgllvm_sctxt*) > store;
};

void register_builtin_typeconv(
	shared_ptr<type_converter> typeconv,
	shared_ptr<type_manager> typemgr
	)
{
	shared_ptr<cgllvm_type_converter> cg_typeconv = shared_polymorphic_cast<cgllvm_type_converter>(typeconv);

	boost::function<
		void ( shared_ptr<node>, shared_ptr<node> ) 
	> int2int_pfn = ::boost::bind( &cgllvm_type_converter::int2int, cg_typeconv.get(), _1, _2 ) ;
	boost::function<
		void ( shared_ptr<node>, shared_ptr<node> ) 
	> int2float_pfn = ::boost::bind( &cgllvm_type_converter::int2float, cg_typeconv.get(), _1, _2 ) ;
	boost::function<
		void ( shared_ptr<node>, shared_ptr<node> ) 
	> float2int_pfn = ::boost::bind( &cgllvm_type_converter::float2int, cg_typeconv.get(), _1, _2 ) ;
	boost::function<
		void ( shared_ptr<node>, shared_ptr<node> ) 
	> float2float_pfn = ::boost::bind( &cgllvm_type_converter::float2float, cg_typeconv.get(), _1, _2 ) ;
	boost::function<
		void ( shared_ptr<node>, shared_ptr<node> ) 
	> scalar2vec1_pfn = ::boost::bind( &cgllvm_type_converter::scalar2vec1, cg_typeconv.get(), _1, _2 ) ;

	type_entry::id_t sint8_ts = typemgr->get( builtin_type_code::_sint8 );
	type_entry::id_t sint16_ts = typemgr->get( builtin_type_code::_sint16 );
	type_entry::id_t sint32_ts = typemgr->get( builtin_type_code::_sint32 );
	type_entry::id_t sint64_ts = typemgr->get( builtin_type_code::_sint64 );

	type_entry::id_t uint8_ts = typemgr->get( builtin_type_code::_uint8 );
	type_entry::id_t uint16_ts = typemgr->get( builtin_type_code::_uint16 );
	type_entry::id_t uint32_ts = typemgr->get( builtin_type_code::_uint32 );
	type_entry::id_t uint64_ts = typemgr->get( builtin_type_code::_uint64 );

	type_entry::id_t float_ts = typemgr->get( builtin_type_code::_float );
	type_entry::id_t double_ts = typemgr->get( builtin_type_code::_double );

	// type_entry::id_t bool_ts = typemgr->get( builtin_type_code::_boolean );

	cg_typeconv->register_converter( type_converter::implicit_conv, sint8_ts, sint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint8_ts, sint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint8_ts, sint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint8_ts, uint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint8_ts, uint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint8_ts, uint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint8_ts, uint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint8_ts, float_ts, int2float_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint8_ts, double_ts, int2float_pfn );
	// cg_typeconv->register_converter( type_converter::implicit_conv, sint8_ts, bool_ts, int2int_pfn );

	cg_typeconv->register_converter( type_converter::explicit_conv, sint16_ts, sint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint16_ts, sint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint16_ts, sint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint16_ts, uint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint16_ts, uint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint16_ts, uint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint16_ts, uint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint16_ts, float_ts, int2float_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint16_ts, double_ts, int2float_pfn );
	// cg_typeconv->register_converter( type_converter::implicit_conv, sint16_ts, bool_ts, default_conv );

	cg_typeconv->register_converter( type_converter::explicit_conv, sint32_ts, sint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint32_ts, sint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint32_ts, sint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint32_ts, uint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint32_ts, uint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint32_ts, uint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint32_ts, uint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::warning_conv, sint32_ts, float_ts, int2float_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, sint32_ts, double_ts, int2float_pfn );
	// cg_typeconv->register_converter( type_converter::implicit_conv, sint32_ts, bool_ts, default_conv );

	cg_typeconv->register_converter( type_converter::explicit_conv, sint64_ts, sint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint64_ts, sint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint64_ts, sint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint64_ts, uint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint64_ts, uint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint64_ts, uint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, sint64_ts, uint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::warning_conv, sint64_ts, float_ts, int2float_pfn );
	cg_typeconv->register_converter( type_converter::warning_conv, sint64_ts, double_ts, int2float_pfn );
	// cg_typeconv->register_converter( type_converter::implicit_conv, sint64_ts, bool_ts, default_conv );

	cg_typeconv->register_converter( type_converter::explicit_conv, uint8_ts, sint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, sint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, sint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, sint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, uint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, uint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, uint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, float_ts, int2float_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, double_ts, int2float_pfn );
	// cg_typeconv->register_converter( type_converter::implicit_conv, uint8_ts, bool_ts, default_conv );

	cg_typeconv->register_converter( type_converter::explicit_conv, uint16_ts, sint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint16_ts, sint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint16_ts, sint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint16_ts, sint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint16_ts, uint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint16_ts, uint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint16_ts, uint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint16_ts, float_ts, int2float_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint16_ts, double_ts, int2float_pfn );
	// cg_typeconv->register_converter( type_converter::implicit_conv, uint16_ts, bool_ts, default_conv );

	cg_typeconv->register_converter( type_converter::explicit_conv, uint32_ts, sint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint32_ts, sint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint32_ts, sint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::better_conv, uint32_ts, sint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint32_ts, uint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint32_ts, uint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint32_ts, uint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::warning_conv, uint32_ts, float_ts, int2float_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, uint32_ts, double_ts, int2float_pfn );
	// cg_typeconv->register_converter( type_converter::implicit_conv, uint32_ts, bool_ts, default_conv );

	cg_typeconv->register_converter( type_converter::explicit_conv, uint64_ts, sint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint64_ts, sint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint64_ts, sint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint64_ts, sint64_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint64_ts, uint8_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint64_ts, uint16_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, uint64_ts, uint32_ts, int2int_pfn );
	cg_typeconv->register_converter( type_converter::warning_conv, uint64_ts, float_ts, int2float_pfn );
	cg_typeconv->register_converter( type_converter::warning_conv, uint64_ts, double_ts, int2float_pfn );
	// cg_typeconv->register_converter( type_converter::implicit_conv, uint64_ts, bool_ts, default_conv );

	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, sint8_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, sint16_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, sint32_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, sint64_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, uint8_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, uint16_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, uint32_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, uint64_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::implicit_conv, float_ts, double_ts, float2float_pfn );
	// cg_typeconv->register_converter( type_converter::warning_conv, float_ts, bool_ts, default_conv );

	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, sint8_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, sint16_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, sint32_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, sint64_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, uint8_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, uint16_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, uint32_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, uint64_ts, float2int_pfn );
	cg_typeconv->register_converter( type_converter::explicit_conv, double_ts, float_ts, float2float_pfn );
	// cg_typeconv->register_converter( type_converter::warning_conv, double_ts, bool_ts, default_conv );

	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, sint8_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, sint16_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, sint32_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, sint64_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, uint8_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, uint16_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, uint32_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, uint64_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, float_ts, default_conv );
	//cg_typeconv->register_converter( type_converter::explicit_conv, bool_ts, double_ts, default_conv );

	//-------------------------------------------------------------------------
	// Register scalar <====> vector<scalar, 1>.
	type_entry::id_t float1_ts = typemgr->get( sasl_ehelper::vector_of(builtin_type_code::_float, 1) );
	cg_typeconv->register_converter( type_converter::explicit_conv, float_ts, float1_ts, scalar2vec1_pfn );
}

shared_ptr<type_converter> create_type_converter(
	boost::shared_ptr<llvm::IRBuilderBase> const& builder,
	boost::function<
		cgllvm_sctxt* ( boost::shared_ptr<sasl::syntax_tree::node> const& )
	> const& ctxt_lookup,
	boost::function< llvm::Value* ( cgllvm_sctxt* ) > const& loader,
	boost::function< void (llvm::Value*, cgllvm_sctxt*) > const& storer
	)
{
	return make_shared<cgllvm_type_converter>( builder, ctxt_lookup, loader, storer );
}

END_NS_SASL_CODE_GENERATOR();