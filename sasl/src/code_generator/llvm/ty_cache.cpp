#include <sasl/include/code_generator/llvm/ty_cache.h>

#include <sasl/enums/enums_utility.h>
#include <sasl/enums/default_hasher.h>

#include <eflib/include/diagnostics/assert.h>

int const PACKAGE_SIZE = 16;
int SIMD_WIDTH_IN_BYTES(){
	return 16;
}
int SIMD_FLOAT_SIZE(){
	return SIMD_WIDTH_IN_BYTES() / sizeof(float);
}

using namespace sasl::utility;

BEGIN_NS_SASL_CODE_GENERATOR();

class ty_cache_t{
public:
	Type* type( LLVMContext& ctxt, builtin_types bt, abis abi );
	std::string const& name( builtin_types bt, abis abi );
	void initialize( LLVMContext& ctxt );
private:
	Type* create_ty( LLVMContext& ctxt, builtin_types bt, abis abi );

	unordered_map<LLVMContext*, unordered_map<builtin_types, Type*> >	cache[4];
	unordered_map<builtin_types, std::string>							ty_name[4];
};

Type* ty_cache_t::type( LLVMContext& ctxt, builtin_types bt, abis abi )
{
	if( abi == abi_unknown ) { return NULL; }

	unordered_map<builtin_types, Type*>& ty_table = cache[abi][&ctxt];
	unordered_map<builtin_types, Type*>::iterator ty_table_it = ty_table.find( bt );
	
	if ( ty_table_it != ty_table.end() ){
		return ty_table_it->second;
	}

	Type*& found_ty = ty_table[bt];
	found_ty = create_ty( ctxt, bt, abi );
	if( is_sampler(bt) ){
		cache[abi_c][&ctxt][bt]
		= cache[abi_llvm][&ctxt][bt]
		= cache[abi_vectorize][&ctxt][bt]
		= cache[abi_package][&ctxt][bt]
		= found_ty;
	}

	return found_ty;
}

std::string const& ty_cache_t::name( builtin_types bt, abis abi )
{
	std::string& ret_name = ty_name[abi][bt];
	char const* suffix = NULL;
	switch( abi )
	{
	case abi_c:
		suffix = ".c";
		break;
	case abi_llvm:
		suffix = ".l";
		break;
	case abi_vectorize:
		suffix = ".v";
		break;
	case abi_package:
		suffix = ".p";
		break;
	default:
		assert(false);
		break;
	}

	if( ret_name.empty() ){
		if( is_scalar(bt) ){
			if( bt == builtin_types::_void ){
				ret_name = "void";
			} else if ( bt == builtin_types::_sint8 ){
				ret_name = "char";
			} else if ( bt == builtin_types::_sint16 ){
				ret_name = "short";
			} else if ( bt == builtin_types::_sint32 ){
				ret_name = "int";
			} else if ( bt == builtin_types::_sint64 ){
				ret_name = "int64";
			} else if ( bt == builtin_types::_uint8 ){
				ret_name = "uchar";
			} else if ( bt == builtin_types::_uint16 ){
				ret_name = "ushort";
			} else if ( bt == builtin_types::_uint32 ){
				ret_name = "uint";
			} else if ( bt == builtin_types::_uint64 ){
				ret_name = "uint64";
			} else if ( bt == builtin_types::_float ){
				ret_name = "float";
			} else if ( bt == builtin_types::_double ){
				ret_name = "double";
			} else if ( bt == builtin_types::_boolean ){
				ret_name = "bool";
			}
			if( abi == abi_vectorize || abi == abi_package ){
				ret_name += suffix;
			}
		} else if ( is_vector(bt) ){
			ret_name = name( scalar_of(bt), abi );
			ret_name.reserve( ret_name.length() + 5 );
			ret_name += ".v";
			ret_name += lexical_cast<string>( vector_size(bt) );
			ret_name += suffix;
		} else if ( is_matrix(bt) ){
			ret_name = name( scalar_of(bt), abi );
			ret_name.reserve( ret_name.length() + 6 );
			ret_name += ".m";
			ret_name += lexical_cast<string>( vector_size(bt) );
			ret_name += lexical_cast<string>( vector_count(bt) );
			ret_name += suffix;
		}
	}
	return ret_name;
}

Type* ty_cache_t::create_ty( LLVMContext& ctxt, builtin_types bt, abis abi )
{
	if ( is_void( bt ) ){
		return Type::getVoidTy( ctxt );
	}
	if( bt == builtin_types::_sampler ){
		return Type::getInt8PtrTy(ctxt);
		// return StructType::create( ".sampler", Type::getInt8PtrTy(ctxt), Type::getInt8PtrTy(ctxt), NULL );
	}

	if( abi == abi_c || abi == abi_llvm ){
		if( is_scalar(bt) ){
			if( bt == builtin_types::_boolean ){
				return IntegerType::get( ctxt, 8 );
			}
			if( is_integer(bt) ){
				return IntegerType::get( ctxt, (unsigned int)storage_size( bt ) << 3 );
			}
			if ( bt == builtin_types::_float ){
				return Type::getFloatTy( ctxt );
			}
			if ( bt == builtin_types::_double ){
				return Type::getDoubleTy( ctxt );
			}
		}

		if( is_vector(bt) ){
			Type* elem_ty = type(ctxt, scalar_of(bt), abi );
			size_t vec_size = vector_size(bt);
			if( abi == abi_c ){
				vector<Type*> elem_tys(vec_size, elem_ty);
				return StructType::create( elem_tys, name(bt, abi) );
			} else {
				return VectorType::get( elem_ty, static_cast<unsigned int>(vec_size) );
			}
		}

		if( is_matrix(bt) ){
			Type* vec_ty = type( ctxt, vector_of( scalar_of(bt), vector_size(bt) ), abi );
			vector<Type*> row_tys( vector_count(bt), vec_ty );
			return StructType::create( row_tys, name(bt, abi) );
		}
	} else if ( abi == abi_vectorize ) {
		if( is_scalar(bt) ){
			Type* scalar_ty = get_llvm_type( ctxt, bt, abi_c );
			int vsize = static_cast<int>( SIMD_WIDTH_IN_BYTES() / storage_size( bt ) );
			vsize = std::max(vsize, 4 );
			return VectorType::get( scalar_ty, vsize );
		} else if ( is_vector(bt) ){
			Type* scalar_ty = get_llvm_type( ctxt, scalar_of(bt), abi_c );
			return VectorType::get( scalar_ty, SIMD_FLOAT_SIZE() );
		} else if ( is_matrix(bt) ){
			return get_llvm_type( ctxt, bt, abi_llvm );
		}
	} else if ( abi == abi_package ){
		if( is_scalar(bt) ){
			Type* scalar_ty = get_llvm_type( ctxt, bt, abi_c );
			return VectorType::get( scalar_ty, PACKAGE_SIZE );
		} else if ( is_vector(bt) ){
			Type* scalar_ty = get_llvm_type( ctxt, scalar_of(bt), abi_c );
			size_t padding_size = vector_size(bt);
			if( padding_size == 3 ){ padding_size = 4; } // Add padding to vec3.
			return VectorType::get( scalar_ty, padding_size * PACKAGE_SIZE );
		} else if ( is_matrix(bt) ){
			return ArrayType::get( get_llvm_type(ctxt, bt, abi_llvm), PACKAGE_SIZE );
		}
	}
	
	assert(false);
	return NULL;
}

void ty_cache_t::initialize( LLVMContext& ctxt )
{
	cache[abi_c			].erase(&ctxt);
	cache[abi_llvm		].erase(&ctxt);
	cache[abi_vectorize	].erase(&ctxt);
	cache[abi_package	].erase(&ctxt);
}

ty_cache_t cache;
Type* get_llvm_type( LLVMContext& ctxt, builtin_types bt, abis abi )
{
	return cache.type( ctxt, bt, abi );
}

void initialize_cache( LLVMContext& ctxt )
{
	cache.initialize(ctxt);
}

END_NS_SASL_CODE_GENERATOR();