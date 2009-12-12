#include "enums/operators.h"
#include "include/code_generator/vm_codegen.h"
#include "include/syntax_tree/expression.h"
#include "include/syntax_tree/constant.h"
#include "include/parser/binary_expression.h"
#include "include/parser/token.h"

#include <tchar.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>

using namespace std;
using namespace boost;

struct token_printer{
	template <typename TokenT>
	bool operator()( const TokenT& tok ){
		cout << "token " << get<token_attr>( tok.value() ).lit << " " << "at " << get<token_attr>( tok.value() ).column << endl;
		return true;
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	binary_expression bin_expr;

	std::string str("3+  2");
	char const* first = str.c_str();
	char const* last = &first[str.size()];

	sasl_tokenizer sasl_tok;
	binary_expression_grammar<sasl_token_iterator, sasl_skipper> g( sasl_tok );

	try{
		bool r = boost::spirit::lex::tokenize_and_phrase_parse( first, last, sasl_tok, g, SKIPPER( sasl_tok ), bin_expr );
		if (r){
			cout << "ok" << endl;
		} else {
			cout << "fail" << endl;
		}
	} catch (const std::runtime_error& e){
		cout << e.what() << endl;
	}

	//bin_expr.op = operators::add;
	//bin_expr.left_expr.reset( new constant( 10 ) );
	//bin_expr.right_expr.reset( new constant( 67 ) );

	vm_codegen vm_cg;
	vm_cg
		.emit_expression( bin_expr );
	//code_generator cg;

	//cg
	//	//��ȡ��������10��67
	//	.op( op_loadrc, r0, 10 )
	//	.op( op_loadrc, r1, 67 )
	//	//����Ĵ���
	//	.op( op_push, r0 )
	//	.op( op_push, r1 )
	//	//������ѹջ
	//	.op( op_push, r0 )
	//	.op( op_push, r1 )
	//	// ����call��Ŀ��Ϊhalt������Ӻ���
	//	.op( op_call, 0x9 ) //����op_halt�����ָ��
	//	.op( op_nop )		//����һ�㴦����ֵ��������Ϊֱ���Ƿ���r0����ģ��ͷ��غ���
	//	// ��ֹ���С�����ͨ�����Խ��Ĵ�����ջ���Է��ر������ֳ���
	//	.op( op_halt )
	//	// �Ӻ���
	//	//��ջ�ϵĲ������ݵ��Ĵ����С�
	//	.op( op_loadrs, r0, -sizeof(int)*2 - sizeof(int)*2 ) //ע�⣬����Ҫ����ѹ��ջ�ϵ�ebp��eip
	//	.op( op_loadrs, r1, -sizeof(int)*2 - sizeof(int) )
	//	.op( op_add, r0, r1) //ִ�мӷ�
	//	.op( op_ret ) // ִ�н��ֱ����r0�з���
	//	;

	vm machine;
	int result = machine.raw_call( vm_cg.codes() );

	std::cout << result << endl;

	system("pause");
	return 0;
}

