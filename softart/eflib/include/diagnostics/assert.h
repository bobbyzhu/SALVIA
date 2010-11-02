#ifndef EFLIB_DIAGNOSTICS_ASSERT_H
#define EFLIB_DIAGNOSTICS_ASSERT_H

#include <eflib/include/platform/config.h>

#include <iostream>

#ifdef _DEBUG
#	define EFLIB_ASSERT(exp, desc) \
				{\
					static bool isIgnoreAlways = false;\
					if(!isIgnoreAlways) {\
					if((*eflib::detail::ProcPreAssert)(exp?true:false, #exp, desc, __LINE__, __FILE__, __FUNCTION__, &isIgnoreAlways))\
						{ abort(); }\
					}\
				}

// e.g. EFLIB_ASSERT_AND_IF( false, "Assert!" ){
//			return 0;
//		}
#	define EFLIB_ASSERT_AND_IF( expr, desc ) \
	EFLIB_ASSERT( expr, desc );	\
	if ( !(expr) ) /* jump statement */

#else
#	define EFLIB_ASSERT(exp, desc)
#	define EFLIB_ASSERT_AND_IF( expr, desc ) \
	if ( !(expr) ) /* jump statement */
#endif
namespace eflib{
	const bool Interrupted = false;
	const bool Unimplemented = false;
}

#define EFLIB_ASSERT_UNIMPLEMENTED0( desc ) EFLIB_ASSERT( eflib::Unimplemented, desc );
#define EFLIB_ASSERT_UNIMPLEMENTED() EFLIB_ASSERT_UNIMPLEMENTED0( " An unimplemented code block was invoked! " );
#define EFLIB_INTERRUPT(desc) EFLIB_ASSERT(eflib::Interrupted, desc)

namespace eflib{
	namespace detail{
		extern bool (*ProcPreAssert)(bool exp, const char* expstr, const char* desc, int line, const char* file, const char* func, bool* ignore);

		bool ProcPreAssert_Init(bool exp, const char* expstr, const char* desc, int line, const char* file, const char* func, bool* ignore);
		bool ProcPreAssert_Defalut(bool exp, const char* expstr, const char* desc, int line, const char* file, const char* func, bool* ignore);
		bool ProcPreAssert_MsgBox(bool exp, const char* expstr, const char* desc, int line, const char* file, const char* func, bool* ignore);
	}

	template<class T>
	void print_vector(std::ostream& os, const T& v)
	{
                for(typename T::const_iterator cit = v.begin(); cit != v.end(); ++cit)
		{
			os << *cit << " ";
		}
	}
}

#endif
