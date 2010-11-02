#ifndef SASL_ENUM_BASE_H
#define SASL_ENUM_BASE_H

#include <functional>
#include <string>
#include <eflib/include/platform/typedefs.h>

template<typename DerivedT, typename StorageT>
struct value_op{
	void from_value( StorageT val ){
		((DerivedT*)this)->val_ = val;
	}
	StorageT to_value() const{
		return ((DerivedT*)this)->val_;
	}
};

template<class DerivedT>
struct equal_op{
	bool operator == (const DerivedT& rhs) const{
		return ((DerivedT*)this)->val_ == rhs.val_;
	}

	bool operator != (const DerivedT& rhs) const{
		return ! (*this == rhs);
	}
};

template<class DerivedT>
struct compare_op{
	bool operator < ( const DerivedT& rhs) const{
		return ((DerivedT*)this)->val_ < rhs.val_;
	}

	bool operator <= ( const DerivedT& rhs) const{
		return ((DerivedT*)this)->val_ <= rhs.val_;
	}

	bool operator > ( const DerivedT& rhs) const{
		return ((DerivedT*)this)->val_ > rhs.val_;
	}

	bool operator >= ( const DerivedT& rhs) const{
		return ((DerivedT*)this)->val_ >= rhs.val_;
	}
};

template<class DerivedT>
struct bitwise_op{
private:
	template<typename ValueT>
	static DerivedT derived_obj( ValueT val )
	{
		enum_base<DerivedT, DerivedT::storage_type> tmp_obj( val );
		return *(static_cast<DerivedT*>(&tmp_obj));
	}
public:
	DerivedT operator & (const DerivedT& rhs) const{
		DerivedT::storage_type ret_val = ((DerivedT*)this)->val_ & rhs.val_;

		return derived_obj(ret_val);
	}

	DerivedT operator | (const DerivedT& rhs) const{
		DerivedT::storage_type ret_val = ((DerivedT*)this)->val_ | rhs.val_;
		return derived_obj(ret_val);
	}

	DerivedT operator ^ (const DerivedT& rhs) const{
		DerivedT::storage_type ret_val = ((DerivedT*)this)->val_ ^ rhs.val_;
		return derived_obj(ret_val);
	}

	DerivedT& operator &= ( const DerivedT& rhs) {
		((DerivedT*)this)->val_ &= rhs.val_;
		return *(static_cast<DerivedT* const>(this));
	}

	DerivedT& operator |= ( const DerivedT& rhs) {
		((DerivedT*)this)->val_ |= rhs.val_;
		return *(static_cast<DerivedT* const>(this));
	}

	DerivedT& operator ^= ( const DerivedT& rhs) {
		((DerivedT*)this)->val_ ^= rhs.val_;
		return *(static_cast<DerivedT* const>(this));
	}

	bool included( const DerivedT& rhs ) const{
		return (((DerivedT*)this)->val_ & rhs.val_) == rhs.val_;
	}

	bool excluded( const DerivedT& rhs ) const{
		return ((DerivedT*)this)->val_ & rhs.val_ == (typename Derived::storage_type)0;
	}
};

template <typename DerivedT, typename StorageT>
class enum_base{
public:
	template <class T> friend struct equal_op;
	template <class T> friend struct bitwise_op;
	template <class T> friend struct compare_op;
	template <class T, typename StroageT> friend struct value_op;
	friend struct enum_hasher;

	typedef DerivedT this_type;
	typedef StorageT storage_type;
	typedef enum_base<DerivedT, StorageT> base_type;

protected:
	StorageT val_;
	enum_base( const StorageT& val):val_(val){
	}
};


#endif