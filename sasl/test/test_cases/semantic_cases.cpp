#include <sasl/test/test_cases/semantic_cases.h>
#include <sasl/test/test_cases/syntax_cases.h>
#include <sasl/include/semantic/semantic_analyser.h>
#include <sasl/include/semantic/symbol.h>
#include <sasl/include/syntax_tree/utility.h>

#include <eflib/include/diagnostics/assert.h>
#include <eflib/include/memory/lifetime_manager.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/shared_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

#include <vector>

using namespace ::sasl::semantic;
using boost::shared_ptr;
using std::vector;

#define SYNCASE_(case_name) syntax_cases::instance().case_name ()
#define SYNCASENAME_(case_name) syntax_cases::instance().case_name##_name()

boost::mutex semantic_cases::mtx;
boost::shared_ptr<semantic_cases> semantic_cases::tcase;

void clear_semantic( SYNTAX_(node)& nd, ::boost::any* ){
	nd.semantic_info( boost::shared_ptr<SEMANTIC_(semantic_info)>() );
}

semantic_cases& semantic_cases::instance(){
	boost::mutex::scoped_lock lg(mtx);
	if ( !tcase ) {
		eflib::lifetime_manager::at_main_exit( semantic_cases::release );
		tcase.reset( new semantic_cases() );
		tcase->initialize();
	}
	return *tcase;
}

bool semantic_cases::is_avaliable()
{
	boost::mutex::scoped_lock lg(mtx);
	return tcase;
}

void semantic_cases::release(){
	boost::mutex::scoped_lock lg(mtx);
	if ( tcase ){
		if( syntax_cases::is_avaliable() ){
			follow_up_traversal( SYNCASE_(prog_for_semantic_test), clear_semantic );
			follow_up_traversal( SYNCASE_(prog_for_jit_test), clear_semantic );
		}
		tcase.reset();
	}
}

semantic_cases::semantic_cases(){
}

void semantic_cases::initialize(){
	LOCVAR_(si_root) = SEMANTIC_(semantic_analysis)( SYNCASE_(prog_for_semantic_test) );
	LOCVAR_(sym_root) = LOCVAR_(si_root)->root();

	vector< shared_ptr<symbol> > sym_fn0_sem_ol = LOCVAR_(sym_root)->find_overloads( SYNCASENAME_(fn0_sem) );
	EFLIB_ASSERT( sym_fn0_sem_ol.size() == 1, "error." );
	LOCVAR_(sym_fn0_sem) = sym_fn0_sem_ol[0];

	vector< shared_ptr<symbol> > sym_fn1_sem_ol = LOCVAR_(sym_root)->find_overloads( SYNCASENAME_(fn1_sem) );
	EFLIB_ASSERT( sym_fn1_sem_ol.size() == 1,  "error." );
	LOCVAR_(sym_fn1_sem) = sym_fn1_sem_ol[0];
}
