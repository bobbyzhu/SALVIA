
#ifndef SASL_LITERAL_TYPES_H
#define SASL_LITERAL_TYPES_H

#include "../enums/enum_base.h" 

struct literal_types :
	public enum_base< literal_types, int >
	, equal_op< literal_types >
{
	friend struct enum_hasher;
private:
	literal_types( const storage_type& val ): base_type( val ){}
	
public:
	literal_types( const this_type& rhs )
		:base_type(rhs.val_)
	{}
	
	this_type& operator = ( const this_type& rhs){
		val_ = rhs.val_;
		return *this;
	}

	const static this_type real;
	const static this_type integer;
	const static this_type boolean;
	const static this_type character;
	const static this_type string;


	static std::string to_name( const this_type& enum_val );
	static this_type from_name( const std::string& name );

};
#endif
