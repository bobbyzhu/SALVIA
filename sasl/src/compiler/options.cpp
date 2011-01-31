#include <sasl/include/compiler/options.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/foreach.hpp>
#include <eflib/include/platform/boost_end.h>

#include <iostream>

using boost::to_lower;

using std::cout;
using std::endl;
using std::string;
using std::vector;

BEGIN_NS_SASL_COMPILER();

options_manager options_manager::inst;

options_manager& options_manager::instance()
{
	return inst;
}

void options_manager::parse( int argc, char** argv )
{
	po::parsed_options parsed = po::command_line_parser(argc, argv).options( desc ).allow_unregistered().run();
	std::vector<std::string> unrecg = po::collect_unrecognized( parsed.options, po::include_positional );

	if( !unrecg.empty() ){
		cout << "Warning: options ";
		BOOST_FOREACH( std::string const & str, unrecg ){
			cout << str << " ";
		}
		cout << "are invalid. They were ignored." << endl;
	}

	po::store( parsed, vm );
	po::notify(vm);

	opt_disp.filterate(vm);
	opt_io.filterate(vm);
}

options_manager::options_manager()
{
	opt_disp.fill_desc(desc);
	opt_global.fill_desc(desc);
	opt_io.fill_desc(desc);
}

void options_manager::process( bool& abort )
{
	abort = false;

	opt_disp.process(abort);
	if( abort ){ return; }

	opt_global.process(abort);
	if( abort ){ return; }

	opt_io.process(abort);
	if( abort ){ return; }
}

po::variables_map const & options_manager::variables() const
{
	return vm;
}

options_display_info const & options_manager::display_info() const
{
	return opt_disp;
}

options_io const & options_manager::io_info() const
{
	return opt_io;
}

//////////////////////////////////////////////////////////////////////////
// display info

const char* options_display_info::version_tag = "version,v";
const char* options_display_info::version_desc = "Show version and copyright information";
const char* options_display_info::version_info = 
	"SoftArt/Salvia Shading Language Compiler(sac) 1.0 pre-alpha\r\n"
	"Copyright (C) 2010 SoftArt/Salvia Development Group."
	"This software and its full source code copyright is GPLv2.";

const char* options_display_info::help_tag = "help,h";
const char* options_display_info::help_desc = "Display this information.";

options_display_info::options_display_info()
	: h(false), v(false)
{}

void options_display_info::fill_desc( po::options_description& desc )
{
	desc.add_options()
		( help_tag, help_desc)
		( version_tag, version_desc )
		;
	pdesc = &desc;
}

void options_display_info::filterate( po::variables_map const & vm )
{
	h = ( vm.empty() || vm.count("help") > 0 );
	v = ( vm.count("version") > 0 );
}

void options_display_info::process( bool& abort )
{
	if( h ){
		cout << *pdesc << endl;
		abort = true;
		return;
	}

	if( v ){
		cout << version_info << endl;
		abort = false;
	}
}

bool options_display_info::help_enabled() const
{
	return h;
}

bool options_display_info::version_enabled() const
{
	return v;
}

//////////////////////////////////////////////////////////////////////////
// output

const char* options_io::out_tag = "out,o";
const char* options_io::out_desc = "File name of output.";

const char* options_io::export_as_tag = "export-as";
const char* options_io::export_as_desc = "Specifies the content of output file that the compiler should generate.";

options_io::options_io() : fmt(none)
{
}

void options_io::fill_desc( po::options_description& desc )
{
	desc.add_options()
		( out_tag, po::value< string >(&fname), out_desc )
		( export_as_tag, po::value< string >(&fmt_str), export_as_desc )
		;
}

void options_io::filterate( po::variables_map const & vm )
{
	if( !vm.count("out") ){
		// Guess output from input.
	}

	if( !vm.count("export-as") ){
		fmt = llvm_ir;
	} else {
		to_lower(fmt_str);
		if( fmt_str == "llvm" || fmt_str == "llvm_ir" ){
			fmt = llvm_ir;
		}
	}
}

void options_io::process( bool& abort )
{
	if( fmt == llvm_ir ){
		cout << "Output format is: LLVM IR" << endl;
	}
}

options_io::export_format options_io::format() const
{
	return fmt;
}

std::string options_io::file_name() const
{
	return fname;
}

//////////////////////////////////////////////////////////////////////////
// options global
void options_global::fill_desc( po::options_description& desc )
{
	desc.add_options()
		(
		"detail-level", po::value<string>(&detail_lvl_str),
		"Specify the detail level of compiler output."
		"The optional items are: quite(q), brief(b), normal(n), verbose(v)."
		"Default is normal"
		);
}

void options_global::filterate( po::variables_map const & vm )
{
	if( vm.count("detail-level") ){

		to_lower( detail_lvl_str );

		detail_lvl = none;
		if( detail_lvl_str == "quite" || detail_lvl_str == "q" ){
			detail_lvl = quite;
		} else if( detail_lvl_str == "brief" || detail_lvl_str == "b" ){
			detail_lvl = brief;
		} else if( detail_lvl_str == "normal" || detail_lvl_str == "n" ){
			detail_lvl = normal;
		} else if( detail_lvl_str == "verbose" || detail_lvl_str == "v" ){
			detail_lvl = verbose;
		} else if( detail_lvl_str == "debug" || detail_lvl_str == "d" ){
			detail_lvl = debug;
		}

	}
}

void options_global::process( bool& abort )
{
	abort = true;
	if( detail_lvl == none ){
		cout << "Detail level is an invalid value. Ignore it." << endl;
	}
}

options_global::detail_level options_global::detail() const
{
	return detail_lvl;
}

END_NS_SASL_COMPILER();