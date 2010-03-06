
#include <tchar.h>

#include "parsers.h"

#include "../project/project.h"
#include "../project/error_reporters.h"

#include "../project/unit.h"
#include "../project/context.h"

using namespace std;

#include "boost/spirit/include/classic_ast.hpp"

int _tmain(int argc, _TCHAR* argv[])
{
	//cout <<
	//	( ast_parse( "2, 3", expression(), white_space() ).full ? "full matched" : "not full matched" )
	//	<< endl;
	proj().initialize(
		boost::shared_ptr<preprocessor>(),
		boost::shared_ptr<error_reporter>(new stdout_error_reporter())
		);
	proj().parse("");
	
	//cout 
	//	<< "�ļ�" 
	//	<< proj().get_units()[0]->get_context()->get_code_pos().filename
	//	<< "����" 
	//	<< proj().get_units()[0]->get_context()->get_current_line_idx() 
	//	<< "��" 
	//	<< endl;

	//cout 
	//	<< "��ƥ����"
	//	<< proj().get_units()[0]->get_context()->get_node_stack().size()
	//	<< "��������Ϣ"
	//	<< endl;

	system("pause");
	return 0;
}

