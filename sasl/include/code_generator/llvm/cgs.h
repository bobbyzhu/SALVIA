#ifndef SASL_CODE_GENERATOR_LLVM_CGS_H
#define SASL_CODE_GENERATOR_LLVM_CGS_H

#include <sasl/include/code_generator/forward.h>

#include <sasl/include/code_generator/llvm/cgs_objects.h>
#include <sasl/include/code_generator/llvm/cgllvm_intrins.h>
#include <sasl/include/code_generator/llvm/cg_extension.h>

#include <sasl/enums/builtin_types.h>

#include <eflib/include/utility/enable_if.h>
#include <eflib/include/diagnostics/assert.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/type_traits.hpp>
#include <eflib/include/platform/boost_end.h>

#include <vector>

namespace sasl
{
	namespace semantic
	{
		class module_semantic;
		class node_semantic;
	}
}

BEGIN_NS_SASL_CODE_GENERATOR();

class  cg_value;
class  cgllvm_module_impl;
struct node_context;

class cg_service
{
	friend class cg_value;

public:
	virtual bool initialize(
		cgllvm_module_impl* mod, module_context* ctxt,
		sasl::semantic::module_semantic* sem
		);
	virtual bool register_external_intrinsic();

	/// @name Service States
	/// @{
	virtual abis intrinsic_abi() const = 0;
	/// @}

	/// @name Value Operators
	/// @{
	virtual llvm::Value* load( cg_value const& );
	virtual llvm::Value* load( cg_value const&, abis abi );
	virtual llvm::Value* load_ref( cg_value const& );
	virtual llvm::Value* load_ref( cg_value const& v, abis abi );
	virtual void store( cg_value& lhs, cg_value const& rhs ) = 0;
	/// @}

	/** @name Emit expressions
	Some simple overload-able operators such as '+' '-' '*' '/'
	will be implemented in 'cgv_*' classes in operator overload form.
	@{ */
	virtual cg_value emit_add( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_sub( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_div( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_mod( cg_value const& lhs, cg_value const& rhs );

	virtual cg_value emit_mul_comp  ( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_mul_intrin( cg_value const& lhs, cg_value const& rhs );
	
	virtual cg_value emit_lshift ( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_rshift ( cg_value const& lhs, cg_value const& rhs );

	virtual cg_value emit_bit_and( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_bit_or ( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_bit_xor( cg_value const& lhs, cg_value const& rhs );

	virtual cg_value emit_cmp_lt ( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_cmp_le ( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_cmp_eq ( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_cmp_ne ( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_cmp_ge ( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_cmp_gt ( cg_value const& lhs, cg_value const& rhs );

	virtual cg_value emit_not( cg_value const& v );
	virtual cg_value emit_and( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_or ( cg_value const& lhs, cg_value const& rhs );

	virtual cg_value emit_call( function_t const& fn, std::vector<cg_value> const& args );
	virtual cg_value emit_call( function_t const& fn, std::vector<cg_value> const& args, cg_value const& exec_mask );
	/// @}

	/// @name Emit element extraction
	/// @{
	virtual cg_value emit_insert_val( cg_value const& lhs, cg_value const& idx, cg_value const& elem_value );
	virtual cg_value emit_insert_val( cg_value const& lhs, int index, cg_value const& elem_value );

	virtual cg_value emit_extract_val( cg_value const& lhs, int idx );
	virtual cg_value emit_extract_val( cg_value const& lhs, cg_value const& idx );
	virtual cg_value emit_extract_ref( cg_value const& lhs, int idx );
	virtual cg_value emit_extract_ref( cg_value const& lhs, cg_value const& idx );
	virtual cg_value emit_extract_elem_mask( cg_value const& vec, uint32_t mask );
	cg_value emit_extract_col( cg_value const& lhs, size_t index );

	template <typename IndexT>
	cg_value emit_extract_elem( cg_value const& vec, IndexT const& idx ){
		if( vec.storable() ){
			return emit_extract_ref( vec, idx );
		} else {
			return emit_extract_val( vec, idx );
		}
	}
	/// @}

	/// @name Intrinsics
	/// @{
	virtual cg_value emit_dot( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_abs( cg_value const& lhs );
	virtual cg_value emit_sqrt( cg_value const& lhs );
	virtual cg_value emit_cross( cg_value const& lhs, cg_value const& rhs );
	virtual cg_value emit_ddx( cg_value const& v ) = 0;
	virtual cg_value emit_ddy( cg_value const& v ) = 0;
	virtual cg_value emit_any( cg_value const& v );
	virtual cg_value emit_all( cg_value const& v );
	virtual cg_value emit_select( cg_value const& flag, cg_value const& v0, cg_value const& v1 );
	virtual cg_value emit_isinf( cg_value const& v );
	virtual cg_value emit_isfinite( cg_value const& v );
	virtual cg_value emit_isnan( cg_value const& v );
	virtual cg_value emit_sign(cg_value const& v);
	virtual cg_value emit_clamp(cg_value const& v, cg_value const& min_v, cg_value const& max_v);
	virtual cg_value emit_saturate(cg_value const& v);

	virtual cg_value emit_tex2Dlod	( cg_value const& samp, cg_value const& coord );
	virtual cg_value emit_tex2Dgrad	( cg_value const& samp, cg_value const& coord, cg_value const& ddx, cg_value const& ddy );
	virtual cg_value emit_tex2Dbias	( cg_value const& samp, cg_value const& coord );
	virtual cg_value emit_tex2Dproj	( cg_value const& samp, cg_value const& coord );

	virtual cg_value emit_texCUBElod	( cg_value const& samp, cg_value const& coord );
	virtual cg_value emit_texCUBEgrad( cg_value const& samp, cg_value const& coord, cg_value const& ddx, cg_value const& ddy );
	virtual cg_value emit_texCUBEbias( cg_value const& samp, cg_value const& coord );
	virtual cg_value emit_texCUBEproj( cg_value const& samp, cg_value const& coord );

	/// @}

	/// @name Emit type casts
	/// @{
	/// Cast between integer types.
	virtual cg_value cast_ints( cg_value const& v, cg_type* dest_tyi ) = 0;
	/// Cast integer to float.
	virtual cg_value cast_i2f ( cg_value const& v, cg_type* dest_tyi ) = 0;
	/// Cast float to integer.
	virtual cg_value cast_f2i ( cg_value const& v, cg_type* dest_tyi ) = 0;
	/// Cast between float types.
	virtual cg_value cast_f2f ( cg_value const& v, cg_type* dest_tyi ) = 0;
	/// Cast integer to bool
	virtual cg_value cast_i2b ( cg_value const& v ) = 0;
	/// Cast float to bool
	virtual cg_value cast_f2b ( cg_value const& v ) = 0;
	/// Cast scalar to vector
	virtual cg_value cast_s2v ( cg_value const& v );
	/// Cast vector to scalar
	virtual cg_value cast_v2s ( cg_value const& v );
	/// Bit casts
	virtual cg_value cast_bits(cg_value const& v, cg_type* dest_tyi);
	/// @}

	/// @name Emit statement
	/// @{
	virtual void emit_return() = 0;
	virtual void emit_return( cg_value const&, abis abi ) = 0;
	/// @}

	/// @name Context switch
	/// @{
	virtual void function_beg(){}
	virtual void function_end(){}

	virtual void for_init_beg(){}
	virtual void for_init_end(){}
	virtual void for_cond_beg(){}
	virtual void for_cond_end( cg_value const& ){}
	virtual void for_body_beg(){}
	virtual void for_body_end(){}
	virtual void for_iter_beg(){}
	virtual void for_iter_end(){}

	virtual cg_value joinable(){ return cg_value(); }

	virtual void if_beg(){}
	virtual void if_end(){}
	virtual void if_cond_beg(){}
	virtual void if_cond_end( cg_value const& ){}
	virtual void then_beg(){}
	virtual void then_end(){}
	virtual void else_beg(){}
	virtual void else_end(){}

	virtual void switch_cond_beg(){}
	virtual void switch_cond_end(){}
	virtual void switch_expr_beg(){}
	virtual void switch_expr_end(){}

	virtual void while_beg(){}
	virtual void while_end(){}
	virtual void while_cond_beg(){}
	virtual void while_cond_end( cg_value const& ){}
	virtual void while_body_beg(){}
	virtual void while_body_end(){}

	virtual void do_beg(){}
	virtual void do_end(){}
	virtual void do_body_beg(){}
	virtual void do_body_end(){}
	virtual void do_cond_beg(){}
	virtual void do_cond_end( cg_value const& ){}

	virtual void break_(){}
	virtual void continue_(){}

	virtual void push_fn(function_t* fn);
	virtual void pop_fn();
	virtual void set_insert_point( insert_point_t const& ip );
	virtual insert_point_t insert_point() const;
	/// @}

	/// @name Context queries
	/// @{
	bool in_function() const;
	function_t& fn();
	/// Get Packed Mask Which is a uint16_t.
	virtual cg_value packed_mask() = 0;
	/// @}

	/// @name Emit value and variables
	/// @{
	cg_value extend_to_vm( cg_value const&, builtin_types hint );

	function_t* fetch_function(sasl::syntax_tree::function_type* fn_node);
	
	template <typename T>
	cg_value create_constant_scalar( T const& v, cg_type* tyinfo, builtin_types hint, EFLIB_ENABLE_IF_COND( boost::is_integral<T> ) ){
		Value* ll_val = ConstantInt::get( IntegerType::get( context(), sizeof(T) * 8 ), uint64_t(v), boost::is_signed<T>::value );
		return create_scalar( ll_val, tyinfo, hint );
	}

	template <typename T>
	cg_value create_constant_scalar( T const& v, cg_type* tyinfo, builtin_types hint, EFLIB_ENABLE_IF_COND( boost::is_floating_point<T> ) ){
		Value* ll_val = ConstantFP::get( Type::getFloatTy( context() ), v );
		return create_scalar( ll_val, tyinfo, hint );
	}
	virtual cg_value create_scalar( llvm::Value* val, cg_type* tyinfo, builtin_types hint ) = 0;

	cg_value null_value( cg_type* tyinfo, abis abi );
	cg_value null_value( builtin_types bt, abis abi );
	cg_value undef_value( builtin_types bt, abis abi );
	cg_value one_value(cg_value const& proto);
	cg_value numeric_value(cg_value const& proto, double fp, uint64_t ui);

	cg_value create_constant_int( cg_type* tyinfo, builtin_types bt, abis abi, uint64_t v );

	cg_value create_value( cg_type* tyinfo, llvm::Value* val, value_kinds k, abis abi );
	cg_value create_value( builtin_types hint, llvm::Value* val, value_kinds k, abis abi );
	cg_value create_value( cg_type* tyinfo, builtin_types hint, llvm::Value* val, value_kinds k, abis abi );

	cg_value create_variable( cg_type const*, abis abi, std::string const& name );
	cg_value create_variable( builtin_types bt, abis abi, std::string const& name );

	virtual cg_value create_vector( std::vector<cg_value> const& scalars, abis abi ) = 0;
	virtual cg_value create_value_by_scalar( cg_value const& scalar, cg_type* tyinfo, builtin_types hint );
	/// @}

	/// @name Utilities
	/// @{
	/// Create a new block at the last of function
	insert_point_t new_block( std::string const& hint, bool set_insert_point );
	/// Jump to the specified block by condition.
	void jump_cond( cg_value const& cond_v, insert_point_t const & true_ip, insert_point_t const& false_ip );
	/// Jump to
	void jump_to( insert_point_t const& ip );
	void clean_empty_blocks();
	/// @}

	/// @name Type emitters
	/// @{
	/// Create tyinfo.
	cg_type* create_ty( sasl::syntax_tree::tynode* tyn );
	/// Get member type information is type is aggregated.
	cg_type* member_tyinfo( cg_type const* agg, size_t index ) const;
	/// @}
	
	/// @name Bridges
	/// @{
	llvm::Type* type_( builtin_types bt, abis abi );
	llvm::Type* type_( cg_type const*, abis abi );
	template <typename T>
	llvm::ConstantInt* int_( T v ){
		return llvm::ConstantInt::get( context(), apint(v) );
	}
	template <typename T>
	llvm::Constant* vector_( T const* vals, size_t length, EFLIB_ENABLE_IF_PRED1(is_integral, T) )
	{
		assert( vals && length > 0 );

		std::vector<llvm::Constant*> elems(length);
		for( size_t i = 0; i < length; ++i ){
			elems[i] = int_(vals[i]);
		}

		return llvm::cast<llvm::Constant>( llvm::ConstantVector::get( elems ) );
	}

	template <typename U, typename T>
	llvm::Constant* vector_( T const* vals, size_t length, EFLIB_ENABLE_IF_PRED1(is_integral, T) )
	{
		assert( vals && length > 0 );

		std::vector<llvm::Constant*> elems(length);
		for( size_t i = 0; i < length; ++i ){
			elems[i] = int_( U(vals[i]) );
		}

		return llvm::cast<llvm::Constant>( llvm::ConstantVector::get( elems ) );
	}

	llvm::Value* integer_value_( llvm::Type* ty, llvm::APInt const& );

	llvm::Value* load_as( cg_value const& v, abis abi );
	/// @}

	llvm::Module*			module () const;
	llvm::LLVMContext&		context() const;
	llvm::DefaultIRBuilder& builder() const;

	virtual bool			prefer_externals() const	= 0;
	virtual bool			prefer_scalar_code() const	= 0;

	virtual abis			param_abi( bool is_c_compatible ) const = 0;
			abis			promote_abi( abis abi0, abis abi1 );
			abis			promote_abi( abis abi0, abis abi1, abis abi2 );

	node_context*			get_node_context( sasl::syntax_tree::node* );
	node_context*			get_or_create_node_context( sasl::syntax_tree::node* );
	sasl::semantic::node_semantic*
							get_node_semantic( sasl::syntax_tree::node* );
protected:
	enum intrin_ids
	{
		exp_f32,
		exp2_f32,
		sin_f32,
		cos_f32,
		tan_f32,
		asin_f32,
		acos_f32,
		atan_f32,
		ceil_f32,
		floor_f32,
		round_f32,
		trunc_f32,
		log_f32,
		log2_f32,
		log10_f32,
		rsqrt_f32,
		ldexp_f32,
		sinh_f32,
		cosh_f32,
		tanh_f32,
		mod_f32,
		pow_f32,
		countbits_u32,
		firstbithigh_u32,
		firstbitlow_u32,
		reversebits_u32,
		tex2dlod_vs,
		tex2dlod_ps,
		tex2dgrad_ps,
		tex2dbias_ps,
		tex2dproj_ps,
		texCUBElod_vs,
		texCUBElod_ps,
		texCUBEgrad_ps,
		texCUBEbias_ps,
		texCUBEproj_ps,
		intrins_count
	};

	sasl::semantic::module_semantic*	sem_;
	cgllvm_module_impl*					llvm_mod_;
	module_context*						ctxt_;
	
	std::vector<function_t*>			fn_ctxts;
	llvm_intrin_cache					intrins;
	llvm::Function*						external_intrins[intrins_count];
	cg_value							exec_mask;
	
	cg_value emit_cmp(
		cg_value const& lhs, cg_value const& rhs,
		uint32_t pred_signed, uint32_t pred_unsigned, uint32_t pred_float
		);
	
	typedef boost::function<llvm::Value* (llvm::Value*, llvm::Value*)>	bin_fn_t;
	typedef boost::function<llvm::Value* (llvm::Value*, llvm::Type*)>	cast_fn;
	typedef boost::function<llvm::Value* (llvm::Value*)>				unary_fn_t;

	llvm::Value* call_external1_		( llvm::Function* f, llvm::Value* v );
	llvm::Value* call_external2_		( llvm::Function* f, llvm::Value* v0, llvm::Value* v1 );

	unary_fn_t	bind_unary_call_		( llvm::Function* fn );
	bin_fn_t	bind_binary_call_		( llvm::Function* fn );
	
	unary_fn_t	bind_unary_external_	( llvm::Function* fn );
	bin_fn_t	bind_binary_external_	( llvm::Function* fn );

	// LLVM have some instructions/intrinsics to support unary and binary operations.
	// But even simple instruction 'add', there are two overloads to support 'iadd' and 'fadd' separately.
	// Additional, some intrinsics are only support scalar or SIMD vector as argument.
	// In fact, For conventionally if binary operation is per-scalar, we need a function
	// which support all aggregation and all scalar types.
	// So, we could use techniques looks like high-order function to combine native intrinsics to
	// a full-scalar-typed and full-aggregation supported function.
	// But, sometimes we cannot combine them at once. Some intermediate functions/functors are generated.
	// For instance, we combine some scalar intrinsic to scalar-vector supported intrinsic
	// and later to all aggregation supported function. Also we may need some function as binder.
	// For distinguish these side-effect functors, we built a postfix system, which decorated function names to
	// indicate what the function can do.

	// Postfix of function name described the requirements of arguments of function.
	//   Component operation modes:
	//     PS: Per scalar
	//     ES: Two parameters are available and one is scalar, the other is aggregated, repeat scalar to aggregated and to PS
	//   Scalar Type Compatibility:
	//     TS: Type are Same. Type of component of Value are same as functor supported type.
	//       e.g. if sv_fn only support Singed Int or Vector of Signed Int
	//            lhs and rhs must be value of singed int or <singed int x n>.
	//     TA: Type are All. Type of component of Value are one of functor supported type.
	//       e.g. signed_sv_fn and float_sv_fn are functors to forward,
	//	          lhs and rhs could be value of signed int or <singed int x n> or float or <float x n>
	//   Aggregation Compatibility:
	//     S: Scalar
    //     V: Vector
    //     A: Aggregated (Structure, Array)
	//	   M: SIMD Vector
	//       e.g. SVA means Value could be Scalar, Vector or Aggregated.

	llvm::Value* bin_op_ps_ts_sva_		(
		llvm::Type* ret_ty, llvm::Value* lhs, llvm::Value* rhs,
		bin_fn_t sfn, bin_fn_t vfn, bin_fn_t simd_fn, bin_fn_t sv_fn,
		unary_fn_t cast_result_sv_fn
		);

	llvm::Value* unary_op_ps_ts_sva_	(
		llvm::Type* ret_ty, llvm::Value* v,
		unary_fn_t sfn, unary_fn_t vfn, unary_fn_t simd_fn, unary_fn_t sv_fn
		);

public:
	cg_value emit_bin_ps_ta_sva(
		cg_value const& lhs, cg_value const& rhs,
		bin_fn_t signed_sv_fn, bin_fn_t unsigned_sv_fn, bin_fn_t float_sv_fn
		);
	cg_value emit_bin_ps_ta_sva(
		std::string const& scalar_external_intrin_name,
		cg_value const& v0, cg_value const& v1
		);
	cg_value emit_bin_es_ta_sva(
		std::string const& scalar_external_intrin_name,
		cg_value const& lhs, cg_value const& rhs
		);
	cg_value emit_bin_es_ta_sva(
		cg_value const& lhs, cg_value const& rhs,
		bin_fn_t signed_sv_fn, bin_fn_t unsigned_sv_fn, bin_fn_t float_sv_fn
		);
	cg_value emit_unary_ps( std::string const& scalar_external_intrin_name, cg_value const& v );

protected:
	cg_value emit_bin_mm(
		cg_value const& lhs, cg_value const& rhs,
		bin_fn_t signed_fn, bin_fn_t unsigned_fn, bin_fn_t float_fn
		);
	cg_value emit_dot_vv( cg_value const& lhs, cg_value const& rhs );

	cg_value emit_mul_sv( cg_value const& lhs, cg_value const& rhs );
	cg_value emit_mul_sm( cg_value const& lhs, cg_value const& rhs );
	cg_value emit_mul_vm( cg_value const& lhs, cg_value const& rhs );
	cg_value emit_mul_mv( cg_value const& lhs, cg_value const& rhs );
	cg_value emit_mul_mm( cg_value const& lhs, cg_value const& rhs );

	virtual cg_value emit_tex_lod_impl(
		cg_value const& samp, cg_value const& coord,
		intrin_ids vs_intrin, intrin_ids ps_intrin );
	virtual cg_value emit_tex_grad_impl(
		cg_value const& samp, cg_value const& coord,
		cg_value const& ddx, cg_value const& ddy,
		intrin_ids ps_intrin );
	virtual cg_value emit_tex_bias_impl(
		cg_value const& samp, cg_value const& coord,
		intrin_ids ps_intrin );
	virtual cg_value emit_tex_proj_impl(
		cg_value const& samp, cg_value const& coord,
		intrin_ids ps_intrin );

	void merge_swizzle( cg_value const*& root, char indexes[], cg_value const& v );

	llvm::AllocaInst* alloca_(llvm::Type* ty, std::string const& name);

	llvm::Function* intrin_( int );
	template <typename FunctionT>
	llvm::Function* intrin_( int );
	
	llvm::Value* shrink_( llvm::Value* vec, size_t vsize );
	llvm::Value* extract_elements_( llvm::Value* src, size_t start_pos, size_t length );
	llvm::Value* insert_elements_ ( llvm::Value* dst, llvm::Value* src, size_t start_pos );
	llvm::Value* i8toi1_( llvm::Value* );
	llvm::Value* i1toi8_( llvm::Value* );

	enum cast_ops
	{
		cast_op_unknown,
		cast_op_f2u,
		cast_op_f2i,
		cast_op_u2f,
		cast_op_i2f,
		cast_op_bitcast,
		cast_op_i2i_signed,
		cast_op_i2i_unsigned
	};

	llvm::Value* cast_sv_( llvm::Value*, llvm::Type* elem_ty, cast_ops op );
	unary_fn_t	 bind_cast_sv_(llvm::Type* elem_ty, cast_ops op);
	llvm::Value* safe_idiv_imod_sv_( llvm::Value*, llvm::Value*, bin_fn_t div_or_mod_fn );

	llvm::Value* get_llvm_vector_(llvm::ArrayRef<llvm::Value*> const& elements);
	llvm::Value* get_llvm_struct_(llvm::Type* ty, llvm::ArrayRef<llvm::Value*> const& elements);

	llvm::Type* extract_scalar_ty_( llvm::Type* );

	cg_value inf_from_value(cg_value const& v, bool negative);
private:

	boost::scoped_ptr<cg_extension>		ext_;

	llvm::Value* load_as_llvm_c			( cg_value const& v, abis abi );
	llvm::Value* load_c_as_package		( cg_value const& v );
	llvm::Value* load_llvm_as_vec		( cg_value const& v );
	llvm::Value* load_vec_as_llvm		( cg_value const& v );
	llvm::Value* load_vec_as_package	( cg_value const& v );
};

END_NS_SASL_CODE_GENERATOR();

#endif
