#ifndef SASL_UNIT_H
#define SASL_UNIT_H

#include <string>
#include <boost/smart_ptr.hpp>

class context;
class scope;
class error_reporter;

/**
���뵥Ԫ��һ���ǵ���Դ�ļ���
**/
class unit
{
public:
	unit();

	///	��õ�ǰ�������ġ�
	context* get_context();

	///	���ȫ��������
	scope* get_global_scope();

	/// ����һ��Unit
	bool parse(const std::string& filename);

	/// ������Ӧ��Error Report
	error_reporter* get_error_reporter();

private:
	boost::shared_ptr<context>			hcontext_;
	boost::shared_ptr<scope>			hscope_;
	boost::shared_ptr<error_reporter>	herror_reporter_;
};

#endif // unit_H__