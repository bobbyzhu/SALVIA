#ifndef SASL_VM_OP_CODE_NAMING_H
#define SASL_VM_OP_CODE_NAMING_H

#include "utility.h"

/*************************************
	���ļ��ṩ��һ��꣬���ڸ���ָ�����ơ������洢��ʽ��������Ϣ����Ψһ��ָ�����ơ�

	ע�⣺
		��������������µĴ洢��ʽ�����ͣ�
		��Ҫ�ڡ�ָ�����Ʋ����洢��ʽ���κꡱ�͡�ָ�����Ʋ����������κꡱ��������Ӷ�Ӧ���������κ꣬
		ʹ֮�ܹ���ȷ�Ĳ���ָ�����ơ�
*************************************/

/**************************************
	ָ�����Ʋ����洢��ʽ���κ�
**************************************/
#define SASL_S_NAME_MODIFIER__( NAME )			NAME
#define SASL_S_NAME_MODIFIER_stk( NAME )		BOOST_PP_CAT(NAME, _stk)
#define SASL_S_NAME_MODIFIER_gr( NAME )			BOOST_PP_CAT(NAME, _gr)
#define SASL_S_NAME_MODIFIER_fr( NAME )			BOOST_PP_CAT(NAME, _fr)
#define SASL_S_NAME_MODIFIER_dr( NAME )			BOOST_PP_CAT(NAME, _dr)
#define SASL_S_NAME_MODIFIER_a( NAME )			BOOST_PP_CAT(NAME, _a)
#define SASL_S_NAME_MODIFIER_c( NAME )			BOOST_PP_CAT(NAME, _c)
#define SASL_S_NAME_MODIFIER_ia( NAME )			BOOST_PP_CAT(NAME, _ia)
#define SASL_S_NAME_MODIFIER_igr( NAME )		BOOST_PP_CAT(NAME, _igr)


/**************************************
	ָ�����Ʋ����������κ�
**************************************/
#define SASL_T_NAME_MODIFIER__( NAME )			NAME
#define SASL_T_NAME_MODIFIER_i32( NAME )		BOOST_PP_CAT( NAME, _i32)
#define SASL_T_NAME_MODIFIER_i64( NAME )		BOOST_PP_CAT( NAME, _64)
#define SASL_T_NAME_MODIFIER_r( NAME )			BOOST_PP_CAT( NAME, _r )
#define SASL_T_NAME_MODIFIER_raw( NAME )		BOOST_PP_CAT(NAME, _raw)

/**************************************
	�������κ�׺������ȷ�����κ�
**************************************/
#define SASL_S_NAME_MODIFIER_NAME( POSTFIX )	BOOST_PP_CAT( SASL_S_NAME_MODIFIER_, POSTFIX )
#define SASL_MODIFY_NAME_S( NAME, POSTFIX )		SASL_S_NAME_MODIFIER_NAME(POSTFIX)( NAME )

#define SASL_T_NAME_MODIFIER_NAME( POSTFIX )	BOOST_PP_CAT( SASL_T_NAME_MODIFIER_, POSTFIX )
#define SASL_MODIFY_NAME_T( NAME, POSTFIX )		SASL_T_NAME_MODIFIER_NAME(POSTFIX)( NAME )

/**************************************
	���ݲ����Ĵ洢��ʽ��ֵ������������
**************************************/
#define SASL_MODIFY_NAME_ST( NAME, S, T )		SASL_MODIFY_NAME_T( SASL_MODIFY_NAME_S( NAME, S ), T )

/**************************************
	����ָ����Ϣ����ָ��ȫ��
	���� SASL_INSTRUCTION_FULL_NAME_P(add, r, i32, r, i32) ������ add_r_i32_r_i32
**************************************/
#define SASL_INSTRUCTION_FULL_NAME_P( NAME, S0, T0, S1, T1)		\
	SASL_MODIFY_NAME_ST( SASL_MODIFY_NAME_ST( NAME, S0, T0 ), S1, T1 )

/**************************************
	����Ԫ�鷽ʽ����ָ����Ϣ����ָ��ȫ��
**************************************/
#define SASL_INSTRUCTION_FULL_NAME_T( INSTRUCTION )	\
	SASL_CALL_EXPANDED_INSTRUCTION( SASL_INSTRUCTION_FULL_NAME_P, INSTRUCTION)
#define SASL_INSTRUCTION_SHORT_NAME_T( INSTRUCTION )			\
	BOOST_PP_TUPLE_ELEM( 5, 0, INSTRUCTION )

// SASL_INSTRUCTION_FULL_NAME_T �ı���
#define SASL_ISN( INSTRUCTION ) SASL_INSTRUCTION_SHORT_NAME_T( INSTRUCTION )
#define SASL_IFN( INSTRUCTION ) SASL_INSTRUCTION_FULL_NAME_T( INSTRUCTION )

#define SASL_ADD_UNDERLINE_PREFIX( NAME )	\
	BOOST_PP_CAT(_, NAME)

#define SASL_NAMESAPCED_IFN( INSTRUCTION ) NS_SASL_VM_OP_CODE( SASL_IFN(INSTRUCTION) )
#define SASL_UNDERLINE_PREFIXED_IFN( INSTRUCTION ) SASL_ADD_UNDERLINE_PREFIX( SASL_IFN(INSTRUCTION) )
#define SASL_UNDERLINE_PREFIXED_ISN( INSTRUCTION ) SASL_ADD_UNDERLINE_PREFIX( SASL_ISN(INSTRUCTION) )

#endif