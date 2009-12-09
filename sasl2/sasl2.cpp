#include "include/vm/vm.h"
#include <tchar.h>
#include <iostream>
#include <string>
#include <vector>


int _tmain(int argc, _TCHAR* argv[])
{
	code_generator cg;
	
	cg
		//��ȡ��������10��67
		.op( op_loadrc, r0, 10 )
		.op( op_loadrc, r1, 67 )
		//����Ĵ���
		.op( op_push, r0 )
		.op( op_push, r1 )
		//������ѹջ
		.op( op_push, r0 )
		.op( op_push, r1 )
		// ����call��Ŀ��Ϊhalt������Ӻ���
		.op( op_call, 0x9 ) //����op_halt�����ָ��
		.op( op_nop )		//����һ�㴦����ֵ��������Ϊֱ���Ƿ���r0����ģ��ͷ��غ���
		// ��ֹ���С�����ͨ�����Խ��Ĵ�����ջ���Է��ر������ֳ���
		.op( op_halt )
		// �Ӻ���
		//��ջ�ϵĲ������ݵ��Ĵ����С�
		.op( op_loadrs, r0, -sizeof(int)*2 - sizeof(int)*2 ) //ע�⣬����Ҫ����ѹ��ջ�ϵ�ebp��eip
		.op( op_loadrs, r1, -sizeof(int)*2 - sizeof(int) )
		.op( op_add, r0, r1) //ִ�мӷ�
		.op( op_ret ) // ִ�н��ֱ����r0�з���
		;

	vm machine;
	int result = machine.raw_call( cg.codes() );

	std::cout << result << endl;

	system("pause");
	return 0;
}

