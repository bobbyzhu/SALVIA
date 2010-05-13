
#include "./compiler_informations.h" 
#include <boost/unordered_map.hpp>

using namespace boost;
using namespace std;


const compiler_informations compiler_informations::none ( 0 );
const compiler_informations compiler_informations::_error ( 131072 );
const compiler_informations compiler_informations::w_typedef_redefinition ( 16909289 );
const compiler_informations compiler_informations::_message ( 262144 );
const compiler_informations compiler_informations::_link ( 33554432 );
const compiler_informations compiler_informations::_info_level_mask ( 16711680 );
const compiler_informations compiler_informations::_stage_mask ( 4278190080 );
const compiler_informations compiler_informations::_info_id_mask ( 65535 );
const compiler_informations compiler_informations::_compile ( 16777216 );
const compiler_informations compiler_informations::_warning ( 65536 );

 
struct enum_hasher: public std::unary_function< compiler_informations, std::size_t> {
	std::size_t operator()( compiler_informations const& val) const{
		return hash_value(val.val_);
	}
};

struct dict_wrapper_compiler_informations {
private:
	boost::unordered_map< compiler_informations, std::string, enum_hasher > enum_to_name;
	boost::unordered_map< std::string, compiler_informations > name_to_enum;

public:
	dict_wrapper_compiler_informations(){
		enum_to_name.insert( std::make_pair( compiler_informations::none, "none" ) );
		enum_to_name.insert( std::make_pair( compiler_informations::_error, "Error" ) );
		enum_to_name.insert( std::make_pair( compiler_informations::w_typedef_redefinition, "Type is redefined." ) );
		enum_to_name.insert( std::make_pair( compiler_informations::_message, "Message" ) );
		enum_to_name.insert( std::make_pair( compiler_informations::_link, "Link" ) );
		enum_to_name.insert( std::make_pair( compiler_informations::_info_level_mask, "_info_level_mask" ) );
		enum_to_name.insert( std::make_pair( compiler_informations::_stage_mask, "_stage_mask" ) );
		enum_to_name.insert( std::make_pair( compiler_informations::_info_id_mask, "_info_id_mask" ) );
		enum_to_name.insert( std::make_pair( compiler_informations::_compile, "Compile" ) );
		enum_to_name.insert( std::make_pair( compiler_informations::_warning, "Warning" ) );

		name_to_enum.insert( std::make_pair( "none", compiler_informations::none ) );
		name_to_enum.insert( std::make_pair( "Error", compiler_informations::_error ) );
		name_to_enum.insert( std::make_pair( "Type is redefined.", compiler_informations::w_typedef_redefinition ) );
		name_to_enum.insert( std::make_pair( "Message", compiler_informations::_message ) );
		name_to_enum.insert( std::make_pair( "Link", compiler_informations::_link ) );
		name_to_enum.insert( std::make_pair( "_info_level_mask", compiler_informations::_info_level_mask ) );
		name_to_enum.insert( std::make_pair( "_stage_mask", compiler_informations::_stage_mask ) );
		name_to_enum.insert( std::make_pair( "_info_id_mask", compiler_informations::_info_id_mask ) );
		name_to_enum.insert( std::make_pair( "Compile", compiler_informations::_compile ) );
		name_to_enum.insert( std::make_pair( "Warning", compiler_informations::_warning ) );

	}

	std::string to_name( compiler_informations const& val ){
		boost::unordered_map< compiler_informations, std::string >::const_iterator
			find_result_it = enum_to_name.find(val);
			
		if ( find_result_it != enum_to_name.end() ){
			return find_result_it->second;
		}

		return "__unknown_enum_val__";
	}

	compiler_informations from_name( const std::string& name){
		boost::unordered_map< std::string, compiler_informations >::const_iterator
			find_result_it = name_to_enum.find(name);
			
		if ( find_result_it != name_to_enum.end() ){
			return find_result_it->second;
		}

		throw "unexcepted enumuration name!";
	}
};

static dict_wrapper_compiler_informations s_dict;

std::string compiler_informations::to_name( const compiler_informations& enum_val){
	return s_dict.to_name(enum_val);
}

compiler_informations compiler_informations::from_name( const std::string& name){
	return s_dict.from_name(name);
}

