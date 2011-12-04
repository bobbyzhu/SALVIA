#ifndef SASL_CODE_GENERATOR_LLVM_CGLLVM_SERVICE_H
#define SASL_CODE_GENERATOR_LLVM_CGLLVM_SERVICE_H

#include <sasl/include/code_generator/forward.h>

#include <sasl/enums/builtin_types.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

namespace llvm{
	class LLVMContext;
	class Module;
	class Type;
	class Value;

	template <bool preserveNames> class IRBuilderDefaultInserter;
	template< bool preserveNames, typename T, typename Inserter
	> class IRBuilder;
	class ConstantFolder;
	typedef IRBuilder<true, ConstantFolder, IRBuilderDefaultInserter<true> >
		DefaultIRBuilder;
}

namespace sasl{
	namespace syntax_tree{
		struct node;
		struct tynode;
	}
}

BEGIN_NS_SASL_CODE_GENERATOR();

class value_t;
class cgllvm_sctxt;
class llvm_module_impl;
class cg_service;

enum abis{
	abi_c,
	abi_llvm,
	abi_unknown
};

enum value_kinds{
	vkind_unknown = 0,
	vkind_tyinfo_only = 1,
	vkind_swizzle = 2,

	vkind_value = 4,
	/// \brief Does treat type as reference if ABI is C compatible.
	///  
	/// An important fact is LLVM's ABI is not same as C API.
	/// If structure was passed into function by value,
	/// C compiler will copy a temporary instance and pass in its pointer on x64 calling convention.
	/// But LLVM will push the instance to stack.
	/// So this varaible will qualify the type of arguments/parameters indicates the compiler.
	/// For e.g. we have a prototype:
	///		void foo( struct S );
	/// If is only called by LLVM code, the IR signature will be 
	///		def foo( %S %arg );
	/// But if it maybe called by external function as convention as "C" code,
	/// The IR signature will be generated as following:
	///		def foo( %S* %arg );
	/// And 'kind' the parameter/argument 'arg' is set to 'vkind_ref'.
	vkind_ref = 8
};

class value_tyinfo{
public:
	friend class cgs_sisd;

	enum classifications{
		unknown_type,
		builtin,
		aggregated
	};

	value_tyinfo(
		sasl::syntax_tree::tynode* sty,
		llvm::Type* cty,
		llvm::Type* llty
		);

	value_tyinfo( value_tyinfo const& );
	value_tyinfo& operator = ( value_tyinfo const& );

	sasl::syntax_tree::tynode* tyn_ptr() const;
	boost::shared_ptr<sasl::syntax_tree::tynode> tyn_shared() const;
	builtin_types hint() const;
	llvm::Type* ty( abis abi ) const;

protected:
	value_tyinfo();

	llvm::Type*					tys[2];
	sasl::syntax_tree::tynode*	tyn;
	classifications				cls;
};

class value_t{
public:
	friend class cgs_sisd;

	value_t();
	value_t( value_t const& );
	value_t& operator = ( value_t const& );

	/// @name State queriers 
	/// @{
	/// Get service.
	cg_service* service() const;
	/// Return internal llvm value.
	llvm::Value* raw() const;

	/// Load llvm value from value_t.
	llvm::Value* load() const;
	llvm::Value* load( abis abi ) const;
	llvm::Value* load_ref() const;

	void store( value_t const& ) const;

	/// Store llvm value to value_t
	void emplace( value_t const& );
	void emplace( llvm::Value* v, value_kinds k, abis abi );
	void set_parent( value_t const& v );
	void set_parent( value_t const* v );

	bool storable() const;
	bool load_only() const;

	value_t as_ref() const;
	/// Get type information of value.
	value_tyinfo* get_tyinfo() const;
	/// Get type hint. if type is not built-in type it returns builtin_type::none.
	builtin_types get_hint() const;
	/// Set type hint.
	void set_hint( builtin_types bt );
	/// Get kind.
	value_kinds get_kind() const;
	/// Get parent. If value is not a member of aggragation, it return NULL.
	value_t* get_parent() const;
	/// Get ABI.
	abis get_abi() const;
	/// Set ABI
	void set_abi( abis abi );
	/// Set Index. It is only make sense if parent is available.
	void set_index( size_t index );
	/// Get masks
	uint32_t get_masks() const;
	/// Set kind
	void set_kind( value_kinds vkind );
	/// @}

	/// @name Operators
	/// @{
	value_t swizzle( size_t swz_code ) const;
	value_t to_rvalue() const;
	/// @}
protected:
	/// @name Constructor, Destructor, Copy constructor and assignment operator
	/// @{
	value_t(
		value_tyinfo* tyinfo,
		llvm::Value* val, value_kinds k, abis abi,
		cg_service* cg
		);
	value_t(
		builtin_types hint,
		llvm::Value* val, value_kinds k, abis abi,
		cg_service* cg
		);

	static value_t slice( value_t const& vec, uint32_t masks );
	/// @}

	/// @name Members
	/// @{
	boost::scoped_ptr<value_t>	parent; // For write mask and swizzle.
	uint32_t					masks;

	value_kinds		kind;
	llvm::Value*	val;
	/// Branch execution tag, for SIMD.
	llvm::Value*	bet;

	/// Type information
	value_tyinfo*	tyinfo;
	builtin_types	hint;

	/// ABI
	abis			abi;

	cg_service*		cg;
	/// @}
};

class cg_service
{
public:
	typedef boost::function< cgllvm_sctxt* (sasl::syntax_tree::node*, bool) > node_ctxt_fn;
	virtual bool initialize( llvm_module_impl* mod, node_ctxt_fn const& fn );

	/// @name Value Operators
	/// @{
	virtual llvm::Value* load( value_t const& ) = 0;
	virtual llvm::Value* load( value_t const& , abis abi ) = 0;
	virtual llvm::Value* load_ref( value_t const& ) = 0;
	virtual void store( value_t& lhs, value_t const& rhs ) = 0;
	/// @}

	llvm::Module*			module () const;
	llvm::LLVMContext&		context() const;
	llvm::DefaultIRBuilder& builder() const;

protected:
	node_ctxt_fn			node_ctxt;
	llvm_module_impl*		mod_impl;
};

END_NS_SASL_CODE_GENERATOR();

#endif