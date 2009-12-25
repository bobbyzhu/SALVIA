#pragma once

#include <vector>
#include <iostream>

typedef unsigned char byte;

enum op_code{
	/*********************************
	*	����
	*********************************/
	op_nop = 0,
	// ������		��ָ��
	// ָ���ʽ��	op_nop
	// ������		��
	op_halt,
	// ������		������״̬�£���ֹ�������ִ�в�ͣ��
	// ָ���ʽ��	op_halt
	// ������		��

	/*********************************
	*	������������
	*********************************/
	op_add_si,
	// ������		�������Ĵ����е������з�����������ʽ��ӡ�
	// ָ���ʽ��	op_add_si REG0, REG1
	// ������		REG0: Ŀ��Ĵ����ţ�REG1: Դ�Ĵ�����

	/*********************************
	*	ջ
	*********************************/
	op_push,
	// ������		���Ĵ���ѹ��ջ�С�
	// ָ���ʽ��	op_push REG0
	// ������		REG0��Ҫѹ��ջ�ļĴ����š�
	op_pop,
	// ������		��ջ�����ݷ��ؼĴ���������ջ�е�����Ӧ���ݡ�
	// ָ���ʽ��	op_pop REG0
	// ������		REG0�����ܷ���ֵ�ļĴ����š�

	/*********************************
	*	����
	*********************************/
	op_load_r_si,
	// ������		��������ų������Ĵ�����
	// ָ���ʽ��	op_load_r_si REG0, CONST
	// ������		REG0��Ŀ��Ĵ����ţ�CONST���з�������
	op_load_r_ui,
	// ������		�����޷��ų������Ĵ�����
	// ָ���ʽ��	op_load_r_ui REG0, CONST
	// ������		REG0��Ŀ��Ĵ����ţ�CONST���޷�������
	op_load_r_ssi,
	// ������		��ջ�ж�ȡһ���з��������Ĵ�����
	// ָ���ʽ��	op_load_r_ssi REG0, OFFSET
	// ������		REG0��Ŀ��Ĵ�����CONST��ջ��ƫ�Ƶ�ַ

	/*********************************
	*	�ӹ��̵���
	*********************************/	
	op_call,
	// ������		����ָ����ν�ebp��eipѹջ������ת��Ŀ���ַ��
	// ָ���ʽ��	op_call ADDR
	// ������		ADDR��ָ���ַ
	op_ret
	// ������		���÷���ָ������ӹ���ջ��ʹ�ã��ָ������̵�ջ��ָ�룬����ת�Ḹָ���λ�á�
	// ָ���ʽ��	op_ret
	// ������		��
};

enum reg{
	r0 = 0,
	r1, r2, r3, r4,
	r5, r6, r7, r8, r9,
	r10, r11, r12, r13,
	r14, r15
};

struct instruction{
	instruction( op_code op ): op(op), arg0(0), arg1(0){}
	instruction( op_code op, intptr_t arg0 ): op(op), arg0(arg0), arg1(0){}
	instruction( op_code op, intptr_t arg0, intptr_t arg1 ): op(op), arg0(arg0), arg1(arg1){}

	op_code op;
	intptr_t arg0;
	intptr_t arg1;
};

class vm
{
public:
	typedef intptr_t intreg_t;
	typedef uintptr_t uintreg_t;
	typedef intptr_t regid_t;
	typedef intptr_t addr_t;
	typedef intptr_t offset_t;

	static const int register_count = 16;

	vm(void);
	~vm(void);

	/********************************************************
	// ֱ�ӵ���һ��ָ�
	// �����ָ��ۣ�eip��Ϊ0��ʼ���ã���op_halt�˳���
	// ����r[0]��ֵ��
	********************************************************/
	intptr_t raw_call(const std::vector<instruction>& ins){
		op_buffer = ins;
		eip = 0;
		ebp = 0;
		while( step() ){
			;
		}
		return r[0];
	}

	bool step(){
		/*cout << "Instruction " << eip << " Executed." 
			<< " EBP: " << ebp 
			<< " ESP: " << esp() 
			<< " EIP: " << eip 
			<< endl;*/
		if ( eip >= (intptr_t)op_buffer.size() ){
			return false;
		}

		if ( execute_op( op_buffer[eip] ) ){
			jump();
			return true;
		}

		return false;
	}

	bool execute_op( const instruction& ins ){
		return execute_op( ins.op, ins.arg0, ins.arg1 );
	}

	bool execute_op(op_code op, intptr_t arg0, intptr_t arg1){
		switch (op)
		{
		case op_halt:
			return false;
		case op_nop:
			break;
		case op_add_si:
			r[arg0] += r[arg1];
			break;
		case op_push: 
			{
				intptr_t& pushed_reg( r[arg0] );
				stack_push( pushed_reg );
				break;
			}
		case op_pop:
			{
				stack_back_value( r[arg0] );
				stack_pop( sizeof(r[arg0]) );
				break;
			}
		case op_load_r_si:
			{
				intptr_t& reg( r[arg0] );
				intptr_t val = arg1;
				reg = val;
				break;
			}
		case op_load_r_ssi:
			{
				intptr_t& reg( r[arg0] );
				ebp_based_value( reg, arg1 );
				break;
			}
		case op_call:
			{
				intptr_t tar_addr = arg0;
				stack_push( esp() );
				stack_push( eip );
				ebp = esp();
				jump_to = tar_addr;
				break;
			}
		case op_ret:
			{
				esp(ebp);
				stack_back_value( jump_to );
				stack_pop( sizeof( jump_to ) );
				stack_back_value( ebp );
				stack_pop( sizeof ( ebp ) );
				++jump_to;
				break;
			}
		}

		return true;
	}

	//instruction operators
	void jump(){
		if ( jump_to == 0 ){
			++eip;
		} else {
			eip = jump_to;
		}

		jump_to = 0;
	}

	//stack operators
	void* stack_pos( intptr_t offset ){
		return (void*)( (byte*)&(stack[0]) + offset );
	}

	template <typename ValueT>
	void stack_value(ValueT& v, intptr_t addr){
		memcpy( &v, stack_pos( addr ), sizeof(ValueT) );
	}

	template <typename ValueT>
	void ebp_based_value(ValueT& v, intptr_t offset){
		stack_value( v, ebp + offset );
	}

	template <typename ValueT>
	void esp_based_value(ValueT& v, intptr_t offset){
		stack_value( v, esp() + offset );
	}

	template <typename ValueT>
	void stack_back_value(ValueT& v){
		esp_based_value( v, - ((intptr_t)sizeof(ValueT)) );
	}

	void stack_pop(size_t size){
		esp( esp() - (intptr_t)size );
	}

	template<typename ValueT>
	void stack_push( const ValueT& v ){
		stack.insert( stack.end(), (byte*)&v, (byte*)&v + sizeof(v) );	
	}

	intptr_t esp(){
		return (intptr_t) stack.size();
	}

	void esp( intptr_t addr ){
		stack.resize(addr);
	}

	//virtual machine storage
	std::vector<byte> stack;
	std::vector<instruction> op_buffer;

	intptr_t eip;
	intptr_t ebp;

	intptr_t r[register_count];
	intptr_t jump_to;
};
