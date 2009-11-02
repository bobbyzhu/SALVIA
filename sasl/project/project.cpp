#include "project.h"

#include "makefile.h"
#include "error_reporter.h"
#include "unit.h"

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace boost;
using namespace boost::filesystem;

/****
project
****/
void project::initialize(shared_ptr<preprocessor> pp, shared_ptr<error_reporter> error_reporter){
	proj().pp_ = pp;
	proj().err_reporter_ = error_reporter;
}

void project::parse(const string& config_name){
	configuration* pconf = get_config(config_name);
	if(!pconf){ return; }
	if(!parse_units(pconf)){ 
		err_reporter_->report_compiler_internal_error( "û�����ƥ�䡣" );
		return; 
	}
}

project& project::instance(){
	return prj_;
}

error_reporter* project::get_error_reporter(){
	return err_reporter_.get();
}

project::project()
	:mf_(makefile::from_directory(current_path().string()))
{
}

project::~project(){
}

project project::prj_;

/**
project Inner Implementation
**/
configuration* project::get_config(const std::string& config_name)
{
	if (!mf_){
		err_reporter_->report_compiler_internal_error("Makefile��ȡʧ��!");
		return NULL;
	}

	if (mf_->get_configurations().empty()){
		err_reporter_->report_compiler_internal_error("Makefileû���ҵ���Ч�ı������á�");
		return NULL;
	}

	if(config_name.empty())
	{
		return &(mf_->get_configurations()[0]);
	} else {
		BOOST_FOREACH(configuration& conf, mf_->get_configurations())
		{
			if(conf.get_id() == config_name){
				return &conf;
			}
		}
	}

	err_reporter_->report_compiler_internal_error(
		str( format("%s\"%s\"%s") % "Makefileû����Ϊ" % config_name.c_str() % "�ı������á�" )
		);
	return NULL;
}

//�ֵ�Ԫ(Դ�ļ�)���з���
bool project::parse_units(configuration* pconf)
{
	if( pconf->get_inputs().size() == 0){
		err_reporter_->report_compiler_internal_error("ȱ�������ļ�!");
		return false;
	}

	bool is_all_correct = true;

	BOOST_FOREACH (string filename, pconf->get_inputs())
	{
		string fileFullPath = pconf->get_work_environment().find_path(filename);
		fstream fs;
		fs.open(fileFullPath.c_str(), ios::in);

		
		if (!fs.is_open()){
			err_reporter_->report_compiler_internal_error(
				str(format("%s\"%s\"%s") % "�����ļ�" % filename.c_str() % "�޷���!")
				);
			is_all_correct = false;
		} else {
			//����Դ�ļ��Ժ󴴽�һ��Unit
			units_.push_back(shared_ptr<unit>(new unit()));
			//ִ�з�������
			is_all_correct = is_all_correct && units_.back()->parse(fileFullPath);
			fs.close();
		}
	}

	return is_all_correct;
}

std::vector<boost::shared_ptr<unit> >& project::get_units()
{
	return units_;
}