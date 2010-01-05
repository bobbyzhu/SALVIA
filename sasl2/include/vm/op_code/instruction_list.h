#ifndef SASL_VM_OP_CODE_INSTRUCTION_LIST_H
#define SASL_VM_OP_CODE_INSTRUCTION_LIST_H

#include "forward.h"
#include "instruction.h"
#include "parameter.h"
#include "storage.h"
#include "operand_type.h"

/**************************
	ָ����崦��
	ע�⣺������µ�ָ����ڴ˴����������
**************************/

#define SASL_VM_INSTRUCTIONS		\
	((add,	gr, raw, gr, raw ))		\
	((load,	gr, raw, c,  raw ))		\
	\
	/***
	***/

BEGIN_NS_SASL_VM_OP_CODE()

SASL_VM_INSTRUCTIONS_OPCODE( SASL_VM_INSTRUCTIONS );

template <int Opcode> struct typecode{
	static const int id = Opcode;
};

template <typename InstructionT, typename MachineT>
struct instruction_info{
	typedef InstructionT type;
	typedef parameter< SASL_SFN(_), typename SASL_OTFT(_, MachineT), MachineT> p0_t;
	typedef parameter< SASL_SFN(_), typename SASL_OTFT(_, MachineT), MachineT> p1_t;
};

END_NS_SASL_VM_OP_CODE()

#endif