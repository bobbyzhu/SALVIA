// sasl_syntax_tree_generator.cpp : ���� DLL Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "sasl_syntax_tree_generator.h"


#ifdef _MANAGED
#pragma managed(push, off)
#endif

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

// ���ǵ���������һ��ʾ��
SASL_SYNTAX_TREE_GENERATOR_API int nsasl_syntax_tree_generator=0;

// ���ǵ���������һ��ʾ����
SASL_SYNTAX_TREE_GENERATOR_API int fnsasl_syntax_tree_generator(void)
{
	return 42;
}

// �����ѵ�����Ĺ��캯����
// �й��ඨ�����Ϣ������� sasl_syntax_tree_generator.h
Csasl_syntax_tree_generator::Csasl_syntax_tree_generator()
{
	return;
}
