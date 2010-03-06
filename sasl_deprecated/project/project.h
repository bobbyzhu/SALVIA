#ifndef PROJECT_H_INCLUDED
#define PROJECT_H_INCLUDED

#include <string>
#include <vector>

class preprocessor;
class error_reporter;
class makefile;
class configuration;
class unit;

#include <boost/smart_ptr.hpp>

/***
һ��������һ������������̵���С��λ����������һ����Դ�ļ���·�����Լ����������ڲ�ͬ���õı�����������
ÿ��������Ӧ�ó���ʵ��ֻ������һ�����̡�
�������ܹ���֧���ļ�����Ĳ����﷨������������������ӽ׶���Ȼֻ�ܴ�����ɡ�
Ŀǰ��ʵ��Ϊ���̷߳�ʽ��
***/
class project
{
public:
	static project& instance();

	/**
	����preprocessor��ErrorReportִ�г�ʼ�����̡�
	�ú�������������������ǰ������Ҫ��ִ��һ�Ρ�
	**/
	static void initialize(boost::shared_ptr<preprocessor> pp, boost::shared_ptr<error_reporter> err_reporter);
	std::vector<boost::shared_ptr<unit> >& get_units();

	/**
		�Կ�ܽ����﷨�����������Ӧ���﷨����
		�������config_name�����ѡ����Ӧ�����ý��з�����
		��������config_name�����ڣ����߷������̳��ִ���������Զ�ֹͣ��
		��������config_nameΪ�գ���ѡȡ���̵ĵ�һ��Config��
	**/
	void parse(const std::string& config_name = "");

	///����Project��preprocessor
	preprocessor* get_pp();

	///����Project��Error Reporter
	error_reporter* get_error_reporter();

private:
	/**
	��ȡһ����֪ID�ı��������á���������ļ��������򷵻ؿա�
	�����ɿ�ܵ��á���������������saslc�����е���������
	**/
	configuration* get_config(const std::string& config_name);

	///���ݴ�������ý��з�����
	bool parse_units(configuration* pconf);

	///���ӣ�����Ŀ����롣
	bool link(configuration* pconf);

	project();
	~project();

	static project prj_;

	boost::shared_ptr<preprocessor>			pp_;
	boost::shared_ptr<error_reporter>		err_reporter_;
	boost::shared_ptr<makefile>				mf_;
	std::vector<boost::shared_ptr<unit> >	units_;
};

//project::Instance��һ���򻯵��÷�װ��
inline project& proj()
{
	return project::instance();
}

#endif // PROJECT_H_INCLUDED
