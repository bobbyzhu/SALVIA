#ifndef SASL_ERROR_REPORTER_H
#define SASL_ERROR_REPORTER_H

#include <string>

class error_reporter_tags{
public:
	static std::string compiler_config_error();
	static std::string syntax_error();
};

/***
�ṩ��Error Reportor�Ľӿڣ������ṩ�˱�׼Error Reportor�ĸ�ʽ�ĸ����ַ�����Ϣ��
Error Report��ʵ����ͨ����Ҫ������¹�����
������;�����������ڵ��÷���ó���������ģ�
�������Ľ��з���������Error Content��ʽ���ɿɶ��͹淶����ʽ��
���к�����Ϊ����ִ�к�����
***/
class error_reporter
{
public:
	/**
	������������ڲ�����
	��Щ����ķ����������ɱ����������BUG����������
	**/
	virtual void report_compiler_internal_error(const std::string& error_content) = 0;

	/**
	���汻����������﷨����
	**/
	virtual void report_syntax_error(const std::string& errContent) = 0;

	/**
	����һ��error_reporter����ͬ���ͣ����ǳ�ʼ����Ϊ�ա�
	**/
	virtual error_reporter* clone() = 0;
};

#endif // error_reporter_H__