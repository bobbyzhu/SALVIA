#include "../include/debug_helper.h"
#include "../include/detail/initialize.h"
#include "../include/config.h"

namespace efl{
	namespace detail{
		void do_init()
		{
			//�˴���ӳ�ʼ������

			//initialize debug helper
			#ifndef EFLIB_CONFIG_MSVC
			efl::detail::ProcPreAssert = &efl::detail::ProcPreAssert_Defalut;
			#else
			efl::detail::ProcPreAssert = &efl::detail::ProcPreAssert_MsgBox;
			#endif
		}
	}
}